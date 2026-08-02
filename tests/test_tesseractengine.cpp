// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include <QFont>
#include <QImage>
#include <QPainter>

#include "ocr/tesseractengine.h"

namespace {

/// Renders `text` as black on white at a size Tesseract reads reliably.
QImage renderText(const QString &text)
{
    QImage image(900, 160, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    QFont font(QStringLiteral("DejaVu Sans"), 48);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(QRect(20, 20, 860, 120), Qt::AlignLeft | Qt::AlignVCenter,
                     text);
    return image;
}

} // namespace

class TestTesseractEngine : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void readsSimpleText()
    {
        textract::TesseractEngine engine;
        QVERIFY2(engine.initialize(QStringLiteral("eng")),
                 "tesseract eng data missing; install tesseract-data-eng");

        const auto words = engine.recognize(renderText(QStringLiteral("Hello world")),
                                            QStringLiteral("eng"));

        QCOMPARE(words.size(), size_t(2));
        QCOMPARE(words[0].text, QStringLiteral("Hello"));
        QCOMPARE(words[1].text, QStringLiteral("world"));
    }

    void reportsPlausibleGeometryAndConfidence()
    {
        textract::TesseractEngine engine;
        QVERIFY(engine.initialize(QStringLiteral("eng")));

        const QImage image = renderText(QStringLiteral("Hello world"));
        const auto words = engine.recognize(image, QStringLiteral("eng"));

        QVERIFY(!words.empty());
        for (const auto &word : words) {
            QVERIFY(word.confidence > 0.5f);
            QVERIFY(word.confidence <= 1.0f);
            QVERIFY(image.rect().contains(word.bbox));
        }
        // Reading order: "Hello" is left of "world".
        QVERIFY(words[0].bbox.left() < words[1].bbox.left());
    }

    void isWarmOnlyAfterInitialize()
    {
        textract::TesseractEngine engine;
        QVERIFY(!engine.isWarm());
        QVERIFY(engine.initialize(QStringLiteral("eng")));
        QVERIFY(engine.isWarm());
    }

    void returnsNothingForBlankImage()
    {
        textract::TesseractEngine engine;
        QVERIFY(engine.initialize(QStringLiteral("eng")));

        QImage blank(400, 200, QImage::Format_RGB32);
        blank.fill(Qt::white);

        QVERIFY(engine.recognize(blank, QStringLiteral("eng")).empty());
    }
};

QTEST_MAIN(TestTesseractEngine)
#include "test_tesseractengine.moc"
