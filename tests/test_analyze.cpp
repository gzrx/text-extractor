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

/// Words laid out from real text, one line per entry, on a 10px character
/// cell. Enough words to clear classify()'s evidence threshold.
std::vector<textract::Word> fromLines(const QStringList &lines)
{
    std::vector<textract::Word> words;
    for (int line = 0; line < lines.size(); ++line) {
        const QString &source = lines.at(line);
        int cursor = 10 * int(source.size() - source.trimmed().size());
        for (const QString &token : source.split(QLatin1Char(' '),
                                                 Qt::SkipEmptyParts)) {
            words.push_back(makeWord(token, cursor, line * 20,
                                     10 * int(token.size())));
            cursor += 10 * int(token.size()) + 10;
        }
    }
    return words;
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

    /// Brackets and semicolons are what separate source from every other
    /// monospaced thing on a desktop. Advance-width variance does not: a build
    /// log and a source listing are both monospace, so it cannot tell them
    /// apart, and on this corpus punctuation density can — 0.068 for the code
    /// fixture against 0.006 for the next highest.
    void classifiesBracketDenseTextAsCode()
    {
        const auto result = textract::classify(fromLines({
            QStringLiteral("QImage preprocess(const QImage &crop)"),
            QStringLiteral("{"),
            QStringLiteral("    if (crop.isNull()) {"),
            QStringLiteral("        return QImage();"),
            QStringLiteral("    }"),
            QStringLiteral("    QImage gray = convert(crop);"),
            QStringLiteral("    if (mean(gray) < threshold) {"),
            QStringLiteral("        gray.invertPixels();"),
            QStringLiteral("    }"),
            QStringLiteral("    return gray;"),
            QStringLiteral("}"),
        }), QImage());

        QCOMPARE(result.kind, textract::LayoutKind::Code);
        QVERIFY(result.confidence > 0.0f);
    }

    /// Terminal output has some punctuation but nowhere near a source file's.
    /// Calling it Code would reconstruct indentation that was column padding.
    void doesNotCallOccasionalPunctuationCode()
    {
        const auto result = textract::classify(fromLines({
            QStringLiteral("Test project /build/textract"),
            QStringLiteral("    Start 1: appstreamtest"),
            QStringLiteral("1/8 Test #1: appstreamtest ... Passed 0.01 sec"),
            QStringLiteral("    Start 2: test_rawimage"),
            QStringLiteral("2/8 Test #2: test_rawimage ... Passed 0.05 sec"),
            QStringLiteral("    Start 3: test_geometry"),
            QStringLiteral("3/8 Test #3: test_geometry ... Passed 0.04 sec"),
            QStringLiteral("100% tests passed out of 8"),
        }), QImage());

        QVERIFY(result.kind != textract::LayoutKind::Code);
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

    /// Several full-height gaps mean cells. One means a column boundary.
    /// Reading a table as two columns of prose would emit it column by column,
    /// which is the exact defect this module exists to prevent, so the two
    /// tests are mutually exclusive by construction.
    ///
    /// The corpus separates as sharply: both table fixtures show four gaps and
    /// nothing else shows more than one.
    void classifiesSeveralFullHeightGapsAsTable()
    {
        const auto result = textract::classify(grid({0, 300, 600, 900}, 100, 12),
                                               QImage());

        QCOMPARE(result.kind, textract::LayoutKind::Table);
        QVERIFY(result.confidence > 0.0f);
    }

    /// A single gap is not a table. It is either a column boundary or nothing.
    void doesNotCallOneGapATable()
    {
        std::vector<int> lefts = columnStarts(0, 540, 60);
        const std::vector<int> right = columnStarts(700, 1240, 60);
        lefts.insert(lefts.end(), right.begin(), right.end());

        QVERIFY(textract::classify(grid(lefts, 50, 12), QImage()).kind
                != textract::LayoutKind::Table);
    }

    /// Two rows are not enough for "full height" to mean anything: any pair of
    /// short lines leaves gaps that no word happens to cross.
    void needsEnoughRowsBeforeTrustingAGap()
    {
        QVERIFY(textract::classify(grid({0, 300, 600, 900}, 100, 2), QImage()).kind
                != textract::LayoutKind::Table);
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

    /// Holding Shift through the drag forces Raw, so a misclassification is
    /// always recoverable on the spot rather than through a config change.
    void treatsShiftDuringTheDragAsForcedRaw()
    {
        QCOMPARE(textract::forcedLayoutFor(Qt::ShiftModifier),
                 std::optional<textract::LayoutKind>(textract::LayoutKind::Raw));
    }

    /// Shift alongside another modifier still means Raw: the user held it.
    void acceptsShiftCombinedWithOtherModifiers()
    {
        QCOMPARE(textract::forcedLayoutFor(Qt::ShiftModifier | Qt::ControlModifier),
                 std::optional<textract::LayoutKind>(textract::LayoutKind::Raw));
    }

    /// Anything else leaves the decision to classify().
    void forcesNothingWithoutShift()
    {
        QVERIFY(!textract::forcedLayoutFor(Qt::NoModifier).has_value());
        QVERIFY(!textract::forcedLayoutFor(Qt::ControlModifier).has_value());
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
