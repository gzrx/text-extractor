// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include <QImage>

#include "app/extraction.h"
#include "ocr/tesseractengine.h"
#include "testimages.h"

using namespace textract::testimages;

class TestExtraction : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY2(m_engine.initialize(QStringLiteral("eng")),
                 "tesseract eng data missing; install tesseract-data-eng");
    }

    /// The coordinate contract: preprocessing hands the engine a 3x image, so
    /// the engine must divide its boxes back down. If this regresses, M4's
    /// column clustering silently operates on 3x-inflated x-positions.
    void reportsBoxesInOriginalCropCoordinates()
    {
        const QImage crop = renderText(QStringLiteral("Hello world"),
                                       Qt::white, Qt::black);

        const auto result = textract::extractText(m_engine, crop,
                                                  QStringLiteral("eng"),
                                                  textract::LayoutKind::Raw, {});

        QVERIFY(!result.words.empty());
        for (const auto &word : result.words) {
            QVERIFY2(crop.rect().contains(word.bbox),
                     qPrintable(QStringLiteral("%1 at %2,%3 %4x%5 escapes the crop")
                                    .arg(word.text)
                                    .arg(word.bbox.x()).arg(word.bbox.y())
                                    .arg(word.bbox.width()).arg(word.bbox.height())));
        }
    }

    /// A dark-mode terminal is a primary use case, and it only works because
    /// preprocess() flips polarity before the engine sees the crop.
    void readsLightTextOnADarkBackground()
    {
        const QImage crop = renderText(QStringLiteral("Hello world"),
                                       Qt::black, Qt::white);

        const auto result = textract::extractText(m_engine, crop,
                                                  QStringLiteral("eng"),
                                                  textract::LayoutKind::Raw, {});

        QCOMPARE(result.text, QStringLiteral("Hello world"));
    }

    void reportsMeanConfidenceOverAllWords()
    {
        const QImage crop = renderText(QStringLiteral("Hello world"),
                                       Qt::white, Qt::black);

        const auto result = textract::extractText(m_engine, crop,
                                                  QStringLiteral("eng"),
                                                  textract::LayoutKind::Raw, {});

        QCOMPARE(result.words.size(), size_t(2));
        QVERIFY(result.meanConfidence > 0.5f);
        QVERIFY(result.meanConfidence <= 1.0f);
    }

    void reportsNothingForABlankCrop()
    {
        QImage blank(400, 200, QImage::Format_RGB32);
        blank.fill(Qt::white);

        const auto result = textract::extractText(m_engine, blank,
                                                  QStringLiteral("eng"),
                                                  textract::LayoutKind::Raw, {});

        QVERIFY(result.isEmpty());
        QVERIFY(result.text.isEmpty());
        QCOMPARE(result.meanConfidence, 0.0f);
    }

    /// The daemon does not know the layout, so extractText() must classify it.
    /// Wiring this in the controller instead would mean the fixture harness
    /// scored a path the daemon never runs.
    void classifiesTheLayoutWhenTheCallerForcesNone()
    {
        const QImage crop = renderText(QStringLiteral("Hello world"),
                                       Qt::white, Qt::black);

        const auto result = textract::extractText(m_engine, crop,
                                                  QStringLiteral("eng"),
                                                  std::nullopt, {});

        QCOMPARE(result.kind, textract::LayoutKind::Raw);
    }

    /// The escape hatch: a forced kind wins outright, so a misclassification is
    /// always recoverable. classify() must not run at all in this case.
    void usesTheForcedLayoutInsteadOfClassifying()
    {
        const QImage crop = renderText(QStringLiteral("Hello world"),
                                       Qt::white, Qt::black);

        const auto result = textract::extractText(m_engine, crop,
                                                  QStringLiteral("eng"),
                                                  textract::LayoutKind::Code, {});

        QCOMPARE(result.kind, textract::LayoutKind::Code);
    }

    /// The layout has to reach the engine, not just the assembler. Prose is
    /// the multi-column kind, so a two-column crop forced to Prose must come
    /// back one whole column at a time.
    void segmentsProseWithFullLayoutAnalysis()
    {
        const QImage crop = renderTwoColumns(columnLines(QStringLiteral("west")),
                                             columnLines(QStringLiteral("east")));

        const auto result = textract::extractText(m_engine, crop,
                                                  QStringLiteral("eng"),
                                                  textract::LayoutKind::Prose, {});

        const int lastWest = lastIndexOf(result.words, QStringLiteral("west"));
        const int firstEast = firstIndexOf(result.words, QStringLiteral("east"));
        QVERIFY(lastWest >= 0 && firstEast >= 0);
        QVERIFY2(lastWest < firstEast,
                 "Prose should read the left column through before the right");
    }

    /// The same crop forced to Raw must read line by line instead. This is the
    /// reading-order defect the classifier exists to fix: on a terminal or a
    /// spreadsheet the gutters are alignment, not column boundaries.
    void segmentsRawAsASingleBlock()
    {
        const QImage crop = renderTwoColumns(columnLines(QStringLiteral("west")),
                                             columnLines(QStringLiteral("east")));

        const auto result = textract::extractText(m_engine, crop,
                                                  QStringLiteral("eng"),
                                                  textract::LayoutKind::Raw, {});

        const int lastWest = lastIndexOf(result.words, QStringLiteral("west"));
        const int firstEast = firstIndexOf(result.words, QStringLiteral("east"));
        QVERIFY(lastWest >= 0 && firstEast >= 0);
        QVERIFY2(firstEast < lastWest,
                 "Raw should interleave the two columns row by row");
    }

    /// The factor actually applied is the clamped one, so an out-of-range
    /// request must not desynchronise the image from the box divisor.
    void survivesAnOutOfRangeUpscaleRequest()
    {
        textract::PreprocessOptions options;
        options.upscale = 99; // clamps to 4

        const QImage crop = renderText(QStringLiteral("Hello world"),
                                       Qt::white, Qt::black);

        const auto result = textract::extractText(m_engine, crop,
                                                  QStringLiteral("eng"),
                                                  textract::LayoutKind::Raw,
                                                  options);

        QVERIFY(!result.words.empty());
        for (const auto &word : result.words) {
            QVERIFY(crop.rect().contains(word.bbox));
        }
    }

private:
    textract::TesseractEngine m_engine;
};

QTEST_MAIN(TestExtraction)
#include "test_extraction.moc"
