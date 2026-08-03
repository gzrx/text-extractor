// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include "assemble/assemble.h"

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
    /// without it.
    void regluesAWordBrokenByAHyphenAtALineEnd()
    {
        std::vector<textract::Word> words;
        appendLine(words, QStringLiteral("optical character recog-"), 0, 1);
        appendLine(words, QStringLiteral("nition is useful"), 0, 2);

        QCOMPARE(textract::assemble(words, textract::LayoutKind::Prose),
                 QStringLiteral("optical character recognition is useful"));
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
