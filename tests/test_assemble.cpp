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
};

QTEST_MAIN(TestAssemble)
#include "test_assemble.moc"
