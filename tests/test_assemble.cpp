// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include "assemble/assemble.h"
#include "correct/dictionary.h"

namespace {

textract::Word makeWord(const QString &text, int x, int line)
{
    textract::Word word;
    word.text = text;
    word.bbox = QRect(x, line * 20, 10 * text.size(), 16);
    word.confidence = 0.9f;
    word.line = line;
    word.block = 1;
    return word;
}

/// One line of space-separated words starting at `x`, laid out on a 10px
/// character cell so that x-offsets are readable as character columns.
void appendLine(std::vector<textract::Word> &words, const QString &text,
                int x, int line, int block = 1)
{
    int cursor = x;
    for (const QString &token : text.split(QLatin1Char(' '),
                                           Qt::SkipEmptyParts)) {
        textract::Word word = makeWord(token, cursor, line);
        word.block = block;
        words.push_back(word);
        cursor += 10 * token.size() + 10; // one blank cell between words
    }
}

/// One table row. Each cell starts at a fixed 200px pitch, so the gaps between
/// them run the full height of the region whatever the cells contain.
void appendCells(std::vector<textract::Word> &words, const QStringList &cells,
                 int line)
{
    for (int column = 0; column < cells.size(); ++column) {
        int cursor = column * 200;
        for (const QString &token : cells.at(column).split(QLatin1Char(' '),
                                                           Qt::SkipEmptyParts)) {
            textract::Word word = makeWord(token, cursor, line);
            words.push_back(word);
            cursor += 10 * token.size() + 10;
        }
    }
}

} // namespace

class TestAssemble : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void joinsWordsOnOneLineWithSpaces()
    {
        const std::vector<textract::Word> words{
            makeWord(QStringLiteral("Hello"), 0, 1),
            makeWord(QStringLiteral("world"), 60, 1),
        };

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Raw),
                 QStringLiteral("Hello world"));
    }

    void separatesLinesWithNewlines()
    {
        const std::vector<textract::Word> words{
            makeWord(QStringLiteral("first"), 0, 1),
            makeWord(QStringLiteral("second"), 0, 2),
        };

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Raw),
                 QStringLiteral("first\nsecond"));
    }

    void separatesBlocksWithBlankLine()
    {
        std::vector<textract::Word> words{
            makeWord(QStringLiteral("para1"), 0, 1),
            makeWord(QStringLiteral("para2"), 0, 2),
        };
        words[1].block = 2;

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Raw),
                 QStringLiteral("para1\n\npara2"));
    }

    void returnsEmptyForNoWords()
    {
        QCOMPARE(textract::assemble({}, textract::LayoutKind::Raw), QString());
    }

    void handlesSingleWord()
    {
        const std::vector<textract::Word> words{
            makeWord(QStringLiteral("alone"), 0, 1),
        };

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Raw),
                 QStringLiteral("alone"));
    }

    void assemblesMultipleWordsAcrossMultipleLines()
    {
        const std::vector<textract::Word> words{
            makeWord(QStringLiteral("the"), 0, 1),
            makeWord(QStringLiteral("quick"), 40, 1),
            makeWord(QStringLiteral("brown"), 100, 2),
            makeWord(QStringLiteral("fox"), 160, 2),
        };

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Raw),
                 QStringLiteral("the quick\nbrown fox"));
    }

    /// Tesseract emits Chinese one character per word. Joining those with
    /// spaces the way Latin words are joined inserts a character between every
    /// glyph on screen, which is the single largest error on the CJK fixture —
    /// and it applies to every layout, not just Prose.
    void joinsCjkCharactersOnOneLineWithoutSpaces()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("这 个 程 序"), 0, 1);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Raw),
                 QStringLiteral("这个程序"));
    }

    /// A Latin word beside a Chinese one still needs its space: only a run of
    /// CJK on both sides of the join means no space belongs there.
    void keepsTheSpaceBetweenLatinAndCjk()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("使用 OCR 技术"), 0, 1);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Raw),
                 QStringLiteral("使用 OCR 技术"));
    }

    // --- Table -------------------------------------------------------------

    /// Tabs, not aligned padding. A captured table is nearly always on its way
    /// into a spreadsheet, and tab-separated values paste there as real cells;
    /// re-emitting the on-screen padding produces one text cell per row.
    void separatesTableCellsWithTabs()
    {
        std::vector<textract::Word> words;
        appendCells(words, {QStringLiteral("Module"), QStringLiteral("Files"),
                            QStringLiteral("Tested")}, 1);
        appendCells(words, {QStringLiteral("capture"), QStringLiteral("4"),
                            QStringLiteral("yes")}, 2);
        appendCells(words, {QStringLiteral("overlay"), QStringLiteral("12"),
                            QStringLiteral("no")}, 3);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Table),
                 QStringLiteral("Module\tFiles\tTested\n"
                                "capture\t4\tyes\n"
                                "overlay\t12\tno"));
    }

    /// A cell holding two words is one cell, not two. Only a gap that runs the
    /// full height of the region separates columns.
    void keepsAMultiWordCellTogether()
    {
        std::vector<textract::Word> words;
        appendCells(words, {QStringLiteral("Module name"), QStringLiteral("Files")}, 1);
        appendCells(words, {QStringLiteral("capture lib"), QStringLiteral("4")}, 2);
        appendCells(words, {QStringLiteral("overlay lib"), QStringLiteral("12")}, 3);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Table),
                 QStringLiteral("Module name\tFiles\n"
                                "capture lib\t4\n"
                                "overlay lib\t12"));
    }

    /// Text with no column structure has one cell per row, which is the same
    /// thing Raw would have said. Table must not invent columns.
    void emitsOneCellPerRowWhenThereAreNoColumns()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("just a sentence"), 0, 1);
        appendLine(words, QStringLiteral("and another one"), 0, 2);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Table),
                 QStringLiteral("just a sentence\nand another one"));
    }

    // --- Code --------------------------------------------------------------

    /// Indentation is the whole point of copying code, and Tesseract does not
    /// report it — it reports where each word starts. Dividing that offset by
    /// the measured character cell recovers the columns.
    void reconstructsIndentationFromXOffsets()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("if (x) {"), 0, 1);
        appendLine(words, QStringLiteral("return;"), 40, 2);
        appendLine(words, QStringLiteral("}"), 0, 3);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Code),
                 QStringLiteral("if (x) {\n    return;\n}"));
    }

    /// Indentation is relative to the leftmost line, not to the crop, so a
    /// selection that starts mid-indent does not gain a margin.
    void measuresIndentationFromTheLeftmostLine()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("first"), 40, 1);
        appendLine(words, QStringLiteral("second"), 80, 2);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Code),
                 QStringLiteral("first\n    second"));
    }

    /// A line-end hyphen in code is an operator or part of an identifier. Code
    /// never joins lines, whatever the line ends with.
    void neverJoinsLinesEvenAfterAHyphen()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("total = a -"), 0, 1);
        appendLine(words, QStringLiteral("b;"), 0, 2);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Code),
                 QStringLiteral("total = a -\nb;"));
    }

    /// A blank line in source has no words in it, so the engine cannot report
    /// one. The vertical gap is the only evidence it was there.
    void restoresABlankLineFromAVerticalGap()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("alpha"), 0, 1);
        appendLine(words, QStringLiteral("bravo"), 0, 2);
        appendLine(words, QStringLiteral("charlie"), 0, 3);
        appendLine(words, QStringLiteral("delta"), 0, 5);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Code),
                 QStringLiteral("alpha\nbravo\ncharlie\n\ndelta"));
    }

    // --- Prose -------------------------------------------------------------

    /// Rendered line breaks inside a paragraph are an artefact of the width
    /// the text happened to be laid out at. Pasting them into a document
    /// carries someone else's column width along with the words.
    void joinsWrappedLinesIntoOneParagraph()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("the quick brown"), 0, 1);
        appendLine(words, QStringLiteral("fox jumps over"), 0, 2);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Prose),
                 QStringLiteral("the quick brown fox jumps over"));
    }

    /// A hyphen at a line end is hyphenation, so the word is put back together
    /// without it. With no dictionary to consult this is the only thing that
    /// can be done, and it is right far more often than it is wrong.
    void regluesAWordBrokenByAHyphenAtALineEnd()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("optical character recog-"), 0, 1);
        appendLine(words, QStringLiteral("nition is useful"), 0, 2);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Prose),
                 QStringLiteral("optical character recognition is useful"));
    }

    /// The same decision with a dictionary available: "recognition" is a word
    /// and "recog"/"nition" are not, so the hyphen was hyphenation.
    void stillRegluesARealHyphenBreakWhenADictionaryIsAvailable()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("optical character recog-"), 0, 1);
        appendLine(words, QStringLiteral("nition is useful"), 0, 2);

        const textract::Dictionary dictionary;
        QCOMPARE(textract::assemble(words, textract::LayoutKind::Prose,
                                    &dictionary),
                 QStringLiteral("optical character recognition is useful"));
    }

    /// The standing corpus case. "bleed-through" is genuinely hyphenated and
    /// merely happened to break at its own hyphen; both halves are words while
    /// the glued form is not, which is exactly the evidence that the hyphen is
    /// content rather than typesetting.
    void keepsTheHyphenOfAGenuinelyHyphenatedWord()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("texture skew and bleed-"), 0, 1);
        appendLine(words, QStringLiteral("through from the reverse"), 0, 2);

        const textract::Dictionary dictionary;
        QCOMPARE(textract::assemble(words, textract::LayoutKind::Prose,
                                    &dictionary),
                 QStringLiteral("texture skew and bleed-through from the reverse"));
    }

    /// A word neither form of which is in the dictionary — a proper noun, an
    /// identifier, a misrecognition — falls back to regluing. The dictionary
    /// only ever overrides the default when it has positive evidence.
    void regluesWhenTheDictionaryKnowsNeitherForm()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("built on Layer-"), 0, 1);
        appendLine(words, QStringLiteral("ShellQt directly"), 0, 2);

        const textract::Dictionary dictionary;
        QCOMPARE(textract::assemble(words, textract::LayoutKind::Prose,
                                    &dictionary),
                 QStringLiteral("built on LayerShellQt directly"));
    }

    void keepsParagraphsApart()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("first paragraph"), 0, 1, 1);
        appendLine(words, QStringLiteral("still first"), 0, 2, 1);
        appendLine(words, QStringLiteral("second paragraph"), 0, 3, 2);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Prose),
                 QStringLiteral("first paragraph still first\n\n"
                                "second paragraph"));
    }

    /// Chinese, Japanese and Korean text does not separate words with spaces,
    /// so the space that joins two Latin lines would be an inserted character
    /// that was never on screen.
    void joinsCjkLinesWithoutInsertingASpace()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("这个程序可以从屏幕上读取文字。"), 0, 1);
        appendLine(words, QStringLiteral("用户先用鼠标选择一个区域"), 0, 2);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Prose),
                 QStringLiteral("这个程序可以从屏幕上读取文字。用户先用鼠标选择一个区域"));
    }
};

QTEST_MAIN(TestAssemble)
#include "test_assemble.moc"
