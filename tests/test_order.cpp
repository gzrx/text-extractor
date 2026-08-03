// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "order/order.h"

namespace {

/// A word on a detection line. `sourceLine` is the id paddlewords assigns:
/// the index of the PP-OCR box the word came out of.
textract::Word makeWord(const QString &text, int x, int y, int width,
                        int height, int sourceLine)
{
    textract::Word word;
    word.text = text;
    word.bbox = QRect(x, y, width, height);
    word.confidence = 0.9f;
    word.line = sourceLine;
    word.block = 0;
    return word;
}

QStringList textsInOrder(const std::vector<textract::Word> &words)
{
    QStringList out;
    for (const textract::Word &word : words) {
        out << word.text;
    }
    return out;
}

} // namespace

class TestOrder : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    /// PP-OCR splits one screen line into several boxes -- "[ 50%]" and the
    /// "Building CXX object ..." after it. They must come back as one line, in
    /// left-to-right order, or the buildlog fixture reads as interleaved noise.
    void mergesBoxesThatShareAScreenLine()
    {
        std::vector<textract::Word> words{
            makeWord(QStringLiteral("Building"), 200, 10, 80, 16, 1),
            makeWord(QStringLiteral("[50%]"), 10, 11, 50, 15, 0),
        };

        textract::orderWords(words, textract::LayoutKind::Raw);

        QCOMPARE(textsInOrder(words),
                 QStringList({QStringLiteral("[50%]"), QStringLiteral("Building")}));
        QCOMPARE(words[0].line, words[1].line);
    }

    /// Boxes on genuinely different screen lines stay separate and sort by top.
    void ordersSeparateLinesTopToBottom()
    {
        std::vector<textract::Word> words{
            makeWord(QStringLiteral("second"), 10, 40, 60, 16, 1),
            makeWord(QStringLiteral("first"), 10, 10, 50, 16, 0),
        };

        textract::orderWords(words, textract::LayoutKind::Raw);

        QCOMPARE(textsInOrder(words),
                 QStringList({QStringLiteral("first"), QStringLiteral("second")}));
        QVERIFY(words[0].line < words[1].line);
    }

    /// Line ids arrive as arbitrary detection indices; downstream
    /// groupIntoLines() only cares that they change between lines, but a dense
    /// 0..n-1 numbering is what every other engine produces.
    void renumbersLinesFromZero()
    {
        std::vector<textract::Word> words{
            makeWord(QStringLiteral("c"), 10, 70, 20, 16, 42),
            makeWord(QStringLiteral("a"), 10, 10, 20, 16, 7),
            makeWord(QStringLiteral("b"), 10, 40, 20, 16, 19),
        };

        textract::orderWords(words, textract::LayoutKind::Raw);

        QCOMPARE(words[0].line, 0);
        QCOMPARE(words[1].line, 1);
        QCOMPARE(words[2].line, 2);
    }

    /// A short fragment must merge into a tall neighbour, which is why overlap
    /// is measured against the SHORTER box. Measured against the taller one,
    /// this fragment would be stranded on a line of its own.
    void mergesAShortFragmentIntoATallNeighbour()
    {
        std::vector<textract::Word> words{
            makeWord(QStringLiteral("tall"), 10, 10, 40, 30, 0),
            makeWord(QStringLiteral("."), 60, 30, 6, 8, 1),
        };

        textract::orderWords(words, textract::LayoutKind::Raw);

        QCOMPARE(words[0].line, words[1].line);
    }

    void handlesEmptyInput()
    {
        std::vector<textract::Word> words;
        textract::orderWords(words, textract::LayoutKind::Raw);
        QVERIFY(words.empty());
    }

    /// A vertical gap much larger than the line pitch is a paragraph break.
    /// assembleProse() turns a block change into a paragraph, and assembleRaw()
    /// into a blank line, so this is what recovers both.
    void startsANewBlockAcrossALargeVerticalGap()
    {
        std::vector<textract::Word> words{
            makeWord(QStringLiteral("a"), 10, 0, 20, 16, 0),
            makeWord(QStringLiteral("b"), 10, 20, 20, 16, 1),
            makeWord(QStringLiteral("c"), 10, 40, 20, 16, 2),
            // Three line pitches down: a paragraph break.
            makeWord(QStringLiteral("d"), 10, 120, 20, 16, 3),
        };

        textract::orderWords(words, textract::LayoutKind::Prose);

        QCOMPARE(words[0].block, words[1].block);
        QCOMPARE(words[1].block, words[2].block);
        QVERIFY(words[3].block > words[2].block);
    }

    /// Evenly-spaced lines are one block. A false paragraph break in the middle
    /// of a paragraph is as damaging as a missing one.
    void keepsEvenlySpacedLinesInOneBlock()
    {
        std::vector<textract::Word> words;
        for (int i = 0; i < 6; ++i) {
            words.push_back(makeWord(QStringLiteral("line"), 10, i * 20, 40, 16, i));
        }

        textract::orderWords(words, textract::LayoutKind::Prose);

        for (const textract::Word &word : words) {
            QCOMPARE(word.block, words.front().block);
        }
    }

    /// Two prose columns: the whole left column is read before the right one.
    ///
    /// Note the left and right lines share a y range, which is the normal case
    /// on a two-column page and the trap in this module: if merging runs before
    /// the column split, every L fuses to its R and the split finds nothing.
    /// This test fails outright under that ordering.
    void readsProseColumnsLeftColumnFirst()
    {
        std::vector<textract::Word> words;
        int id = 0;
        for (int i = 0; i < 5; ++i) {
            words.push_back(makeWord(QStringLiteral("L"), 10, i * 20, 60, 16, id++));
            words.push_back(makeWord(QStringLiteral("R"), 200, i * 20, 60, 16, id++));
        }

        textract::orderWords(words, textract::LayoutKind::Prose);

        // Five L then five R, not L R L R interleaved by row.
        for (int i = 0; i < 5; ++i) {
            QCOMPARE(words[size_t(i)].text, QStringLiteral("L"));
        }
        for (int i = 5; i < 10; ++i) {
            QCOMPARE(words[size_t(i)].text, QStringLiteral("R"));
        }
    }

    /// A full-width intro paragraph above two columns.
    ///
    /// A gutter is a band unoccupied at EVERY height, so the single spanning
    /// line erases it for the whole page. Searching the page as a whole finds
    /// no gutter and interleaves the columns line by line -- measured on
    /// pdf-two-column, which scored 0.3083 that way against tier 1's 0.9975.
    /// Reading order here is intro, then all of the left column, then all of
    /// the right.
    void readsAFullWidthLineThenTheColumnsBelowIt()
    {
        std::vector<textract::Word> words;
        int id = 0;
        // Spans both columns and the gutter between them.
        words.push_back(makeWord(QStringLiteral("intro"), 10, 0, 250, 16, id++));
        for (int i = 0; i < 4; ++i) {
            const int y = 30 + i * 20;
            words.push_back(makeWord(QStringLiteral("L"), 10, y, 60, 16, id++));
            words.push_back(makeWord(QStringLiteral("R"), 200, y, 60, 16, id++));
        }

        textract::orderWords(words, textract::LayoutKind::Prose);

        QCOMPARE(words[0].text, QStringLiteral("intro"));
        for (int i = 1; i <= 4; ++i) {
            QCOMPARE(words[size_t(i)].text, QStringLiteral("L"));
        }
        for (int i = 5; i <= 8; ++i) {
            QCOMPARE(words[size_t(i)].text, QStringLiteral("R"));
        }
    }

    /// The same geometry classified as a Table must stay row-major. Splitting
    /// on a table's inter-cell gap is what dropped spreadsheet-table from
    /// 1.0000 to 0.4348 during the M6 probe.
    void neverSplitsATableIntoColumns()
    {
        std::vector<textract::Word> words;
        int id = 0;
        for (int i = 0; i < 5; ++i) {
            words.push_back(makeWord(QStringLiteral("L"), 10, i * 20, 60, 16, id++));
            words.push_back(makeWord(QStringLiteral("R"), 200, i * 20, 60, 16, id++));
        }

        textract::orderWords(words, textract::LayoutKind::Table);

        QCOMPARE(words[0].text, QStringLiteral("L"));
        QCOMPARE(words[1].text, QStringLiteral("R"));
        QCOMPARE(words[0].line, words[1].line);
    }

    /// Code is single-column too: indentation depends on x-offsets within one
    /// coordinate origin, and a column split would rebase half the file.
    void neverSplitsCodeIntoColumns()
    {
        std::vector<textract::Word> words;
        int id = 0;
        for (int i = 0; i < 5; ++i) {
            words.push_back(makeWord(QStringLiteral("L"), 10, i * 20, 60, 16, id++));
            words.push_back(makeWord(QStringLiteral("R"), 200, i * 20, 60, 16, id++));
        }

        textract::orderWords(words, textract::LayoutKind::Code);

        QCOMPARE(words[0].line, words[1].line);
    }
};

QTEST_MAIN(TestOrder)
#include "test_order.moc"
