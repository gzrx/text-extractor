// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "analyze/analyze.h"

namespace {

textract::Word makeWord(const QString &text, int x, int y, int width)
{
    textract::Word word;
    word.text = text;
    word.bbox = QRect(x, y, width, 16);
    word.confidence = 0.9f;
    word.line = y / 20 + 1;
    word.block = 1;
    return word;
}

} // namespace

class TestAnalyze : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    /// Nothing to go on is the clearest case of "uncertain", and the contract
    /// is that uncertainty falls back to Raw rather than guessing.
    void classifiesAnEmptyPageAsRaw()
    {
        const auto result = textract::classify({}, QImage());

        QCOMPARE(result.kind, textract::LayoutKind::Raw);
        QCOMPARE(result.confidence, 0.0f);
    }

    void classifiesASingleLineAsRaw()
    {
        const std::vector<textract::Word> words{
            makeWord(QStringLiteral("Hello"), 0, 0, 50),
            makeWord(QStringLiteral("world"), 60, 0, 50),
        };

        QCOMPARE(textract::classify(words, QImage()).kind,
                 textract::LayoutKind::Raw);
    }

    /// Prose is the only kind that may be genuinely multi-column, so it is the
    /// only one that gets full page layout analysis.
    void asksForFullLayoutAnalysisOnlyForProse()
    {
        QCOMPARE(textract::segmentationFor(textract::LayoutKind::Prose),
                 textract::Segmentation::Auto);
    }

    /// Raw, Code and Table are whitespace-aligned single blocks, where column
    /// detection is precisely what ruins them.
    void asksForASingleBlockForEveryOtherKind()
    {
        QCOMPARE(textract::segmentationFor(textract::LayoutKind::Raw),
                 textract::Segmentation::SingleBlock);
        QCOMPARE(textract::segmentationFor(textract::LayoutKind::Code),
                 textract::Segmentation::SingleBlock);
        QCOMPARE(textract::segmentationFor(textract::LayoutKind::Table),
                 textract::Segmentation::SingleBlock);
    }
};

QTEST_MAIN(TestAnalyze)
#include "test_analyze.moc"
