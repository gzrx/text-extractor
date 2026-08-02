// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include <QImage>
#include <QPainter>
#include <QSet>

#include "preprocess/preprocess.h"

namespace {

/// A 60x60 field of `background` with a 20x20 block of `foreground` at (10,10),
/// so the background is ~89% of the area and decides the polarity.
QImage twoTone(Qt::GlobalColor background, Qt::GlobalColor foreground)
{
    QImage image(60, 60, QImage::Format_RGB32);
    image.fill(background);

    QPainter painter(&image);
    painter.fillRect(QRect(10, 10, 20, 20), foreground);
    return image;
}

int grayAt(const QImage &image, int x, int y)
{
    return qGray(image.pixel(x, y));
}

QSet<int> uniqueGrays(const QImage &image)
{
    QSet<int> levels;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            levels.insert(grayAt(image, x, y));
        }
    }
    return levels;
}

} // namespace

class TestPreprocess : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void upscalesByTheDefaultFactor()
    {
        QImage crop(100, 50, QImage::Format_RGB32);
        crop.fill(Qt::white);

        const QImage out = textract::preprocess(crop, {});

        QCOMPARE(out.width(), 300);
        QCOMPARE(out.height(), 150);
    }

    void upscalesByAConfiguredFactor()
    {
        QImage crop(100, 50, QImage::Format_RGB32);
        crop.fill(Qt::white);

        textract::PreprocessOptions options;
        options.upscale = 4;

        const QImage out = textract::preprocess(crop, options);

        QCOMPARE(out.width(), 400);
        QCOMPARE(out.height(), 200);
    }

    void clampsUpscaleToTheSupportedRange_data()
    {
        QTest::addColumn<int>("requested");
        QTest::addColumn<int>("effective");

        QTest::newRow("below range") << 1 << 2;
        QTest::newRow("zero")        << 0 << 2;
        QTest::newRow("negative")    << -3 << 2;
        QTest::newRow("low bound")   << 2 << 2;
        QTest::newRow("high bound")  << 4 << 4;
        QTest::newRow("above range") << 9 << 4;
    }

    void clampsUpscaleToTheSupportedRange()
    {
        QFETCH(int, requested);
        QFETCH(int, effective);

        textract::PreprocessOptions options;
        options.upscale = requested;

        QCOMPARE(textract::effectiveUpscale(options), effective);

        QImage crop(100, 50, QImage::Format_RGB32);
        crop.fill(Qt::white);
        QCOMPARE(textract::preprocess(crop, options).width(), 100 * effective);
    }

    /// Tesseract must always be handed dark text on a light background, so a
    /// dark-mode terminal and a light document have to leave here identical in
    /// polarity. Sampling at 3x: the foreground block covers x,y in [30, 90).
    void normalisesPolarityToDarkOnLight_data()
    {
        QTest::addColumn<QImage>("crop");

        QTest::newRow("light background") << twoTone(Qt::white, Qt::black);
        QTest::newRow("dark background")  << twoTone(Qt::black, Qt::white);
    }

    void normalisesPolarityToDarkOnLight()
    {
        QFETCH(QImage, crop);

        const QImage out = textract::preprocess(crop, {});

        QVERIFY2(grayAt(out, 150, 150) > 200, "background should be light");
        QVERIFY2(grayAt(out, 60, 60) < 60, "foreground should be dark");
    }

    /// Otsu is standard advice for scanned pages and wrong for screen text:
    /// glyphs are subpixel antialiased and the LSTM engine thresholds
    /// adaptively itself. The soft edges must survive the default path.
    void keepsIntermediateGraysByDefault()
    {
        const QImage out = textract::preprocess(twoTone(Qt::white, Qt::black), {});

        QVERIFY(!uniqueGrays(out).empty());
        QVERIFY2(uniqueGrays(out).size() > 2,
                 "default path must not collapse to two levels");
    }

    void binarisesOnlyWhenRequested()
    {
        textract::PreprocessOptions options;
        options.binarize = true;

        const QImage out = textract::preprocess(twoTone(Qt::white, Qt::black),
                                                options);

        const QSet<int> levels = uniqueGrays(out);
        QCOMPARE(levels.size(), 2);
        QVERIFY(levels.contains(0));
        QVERIFY(levels.contains(255));
    }

    void producesGrayscaleOutput()
    {
        const QImage out = textract::preprocess(twoTone(Qt::white, Qt::black), {});

        QCOMPARE(out.format(), QImage::Format_Grayscale8);
    }

    void returnsNullForNullInput()
    {
        QVERIFY(textract::preprocess(QImage(), {}).isNull());
    }
};

QTEST_MAIN(TestPreprocess)
#include "test_preprocess.moc"
