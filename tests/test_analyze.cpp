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

/// Rows of words laid out at the given left edges, `count` lines deep. The
/// generator for both shapes that produce full-height whitespace gutters: two
/// text columns, and a table.
std::vector<textract::Word> grid(const std::vector<int> &lefts, int wordWidth,
                                 int lines)
{
    std::vector<textract::Word> words;
    for (int line = 0; line < lines; ++line) {
        for (const int left : lefts) {
            words.push_back(makeWord(QStringLiteral("word"), left, line * 20,
                                     wordWidth));
        }
    }
    return words;
}

/// Word left edges for a densely-set text column: words butted up against each
/// other across the column's whole width.
std::vector<int> columnStarts(int from, int to, int step)
{
    std::vector<int> lefts;
    for (int x = from; x + step <= to; x += step) {
        lefts.push_back(x);
    }
    return lefts;
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

    /// One wide full-height gutter with dense text either side is a
    /// two-column document. This is the signal that keeps a two-column PDF on
    /// full page layout analysis; without it the columns are read as rows and
    /// the text comes out interleaved.
    void classifiesOneWideGutterBetweenDenseColumnsAsProse()
    {
        std::vector<int> lefts = columnStarts(0, 540, 60);
        const std::vector<int> right = columnStarts(700, 1240, 60);
        lefts.insert(lefts.end(), right.begin(), right.end());

        const auto result = textract::classify(grid(lefts, 50, 12), QImage());

        QCOMPARE(result.kind, textract::LayoutKind::Prose);
        QVERIFY(result.confidence > 0.5f);
    }

    /// A table has full-height gutters too, and misreading it as two columns
    /// would emit it column by column — the exact defect this module exists to
    /// prevent. Several gutters mean cells, not columns of prose.
    void doesNotMistakeTableGuttersForColumns()
    {
        const auto result = textract::classify(grid({0, 300, 600, 900}, 100, 12),
                                               QImage());

        QVERIFY(result.kind != textract::LayoutKind::Prose);
    }

    /// A gutter with almost nothing on one side is a stray margin note or a
    /// line number, not a column.
    void ignoresAGutterWithNoRealContentOnOneSide()
    {
        std::vector<textract::Word> words = grid(columnStarts(700, 1240, 60),
                                                 50, 12);
        words.push_back(makeWord(QStringLiteral("1"), 0, 0, 20));

        QVERIFY(textract::classify(words, QImage()).kind
                != textract::LayoutKind::Prose);
    }

    /// A handful of words is not evidence of anything.
    void treatsTooFewWordsAsUncertain()
    {
        const auto result = textract::classify(grid({0, 700}, 50, 2), QImage());

        QCOMPARE(result.kind, textract::LayoutKind::Raw);
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
