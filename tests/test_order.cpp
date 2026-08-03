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
};

QTEST_MAIN(TestOrder)
#include "test_order.moc"
