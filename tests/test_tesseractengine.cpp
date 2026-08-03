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

/// Two text columns separated by a wide blank gutter — the layout that makes
/// page segmentation observable, because the two modes read it in different
/// orders.
QImage renderTwoColumns(const QStringList &left, const QStringList &right)
{
    QImage image(1400, 900, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    QFont font(QStringLiteral("DejaVu Serif"), 28);
    painter.setFont(font);
    painter.setPen(Qt::black);
    // A gutter roughly a third of a column wide, and enough lines either side
    // for Tesseract to be confident: its column detector wants evidence, and a
    // handful of short lines does not supply it.
    painter.drawText(QRect(60, 40, 540, 820), Qt::AlignLeft | Qt::AlignTop,
                     left.join(QLatin1Char('\n')));
    painter.drawText(QRect(800, 40, 540, 820), Qt::AlignLeft | Qt::AlignTop,
                     right.join(QLatin1Char('\n')));
    return image;
}

/// Twelve lines of plausible column text, distinct between the two columns so
/// that reading order is unambiguous in the assertion.
QStringList columnLines(const QString &tag)
{
    QStringList lines;
    for (int i = 1; i <= 12; ++i) {
        lines << QStringLiteral("%1 line number %2 of the column")
                     .arg(tag).arg(i);
    }
    return lines;
}

/// Index of the first word containing `tag`, or -1. Asserting on tag positions
/// rather than on the whole joined string keeps the test about reading order
/// and not about whether Tesseract misread one glyph out of a hundred.
int firstIndexOf(const std::vector<textract::Word> &words, const QString &tag)
{
    for (size_t i = 0; i < words.size(); ++i) {
        if (words[i].text.contains(tag)) {
            return int(i);
        }
    }
    return -1;
}

int lastIndexOf(const std::vector<textract::Word> &words, const QString &tag)
{
    for (int i = int(words.size()) - 1; i >= 0; --i) {
        if (words[size_t(i)].text.contains(tag)) {
            return i;
        }
    }
    return -1;
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
                                            QStringLiteral("eng"),
                                            textract::Segmentation::Auto);

        QCOMPARE(words.size(), size_t(2));
        QCOMPARE(words[0].text, QStringLiteral("Hello"));
        QCOMPARE(words[1].text, QStringLiteral("world"));
    }

    void reportsPlausibleGeometryAndConfidence()
    {
        textract::TesseractEngine engine;
        QVERIFY(engine.initialize(QStringLiteral("eng")));

        const QImage image = renderText(QStringLiteral("Hello world"));
        const auto words = engine.recognize(image, QStringLiteral("eng"),
                                            textract::Segmentation::Auto);

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

        QVERIFY(engine.recognize(blank, QStringLiteral("eng"),
                                 textract::Segmentation::Auto).empty());
    }

    /// Segmentation::Auto is Tesseract's PSM 3, which runs multi-column layout
    /// analysis: a whitespace gutter reads as a column boundary, so each column
    /// is emitted in full before the next one starts.
    void readsColumnByColumnUnderAutoSegmentation()
    {
        textract::TesseractEngine engine;
        QVERIFY(engine.initialize(QStringLiteral("eng")));

        const QImage image = renderTwoColumns(columnLines(QStringLiteral("west")),
                                              columnLines(QStringLiteral("east")));

        const auto words = engine.recognize(image, QStringLiteral("eng"),
                                            textract::Segmentation::Auto);

        const int lastWest = lastIndexOf(words, QStringLiteral("west"));
        const int firstEast = firstIndexOf(words, QStringLiteral("east"));
        QVERIFY(lastWest >= 0 && firstEast >= 0);
        QVERIFY2(lastWest < firstEast,
                 "the whole left column should precede the right one");
    }

    /// Segmentation::SingleBlock is PSM 6: one uniform block, read line by
    /// line. The same gutter is now just wide spacing inside a row, which is
    /// the correct reading of a terminal, a table or a spreadsheet.
    void readsLineByLineUnderSingleBlockSegmentation()
    {
        textract::TesseractEngine engine;
        QVERIFY(engine.initialize(QStringLiteral("eng")));

        const QImage image = renderTwoColumns(columnLines(QStringLiteral("west")),
                                              columnLines(QStringLiteral("east")));

        const auto words = engine.recognize(image, QStringLiteral("eng"),
                                            textract::Segmentation::SingleBlock);

        const int lastWest = lastIndexOf(words, QStringLiteral("west"));
        const int firstEast = firstIndexOf(words, QStringLiteral("east"));
        QVERIFY(lastWest >= 0 && firstEast >= 0);
        QVERIFY2(firstEast < lastWest,
                 "rows should interleave the two columns, not separate them");
    }
};

QTEST_MAIN(TestTesseractEngine)
#include "test_tesseractengine.moc"
