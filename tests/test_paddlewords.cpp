// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "ocr/paddlewords.h"

using textract::TextLine;

class TestPaddleWords : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    /// On a monospaced face the split is exact: "ab cd" over 50px of 10px
    /// cells puts "ab" at 0..19 and "cd" at 30..49.
    void splitsAMonospacedLineExactly()
    {
        const std::vector<TextLine> lines{
            {QStringLiteral("ab cd"), QRect(0, 0, 50, 16), 0.9f}};

        const auto words = textract::wordsFromLines(lines);

        QCOMPARE(words.size(), size_t(2));
        QCOMPARE(words[0].text, QStringLiteral("ab"));
        QCOMPARE(words[0].bbox.left(), 0);
        QCOMPARE(words[0].bbox.width(), 20);
        QCOMPARE(words[1].text, QStringLiteral("cd"));
        QCOMPARE(words[1].bbox.left(), 30);
        QCOMPARE(words[1].bbox.width(), 20);
    }

    /// The Code branch takes indentation from the first word's left edge, so
    /// the line box's own left edge has to survive untouched.
    void preservesTheLineLeftEdgeAsIndentation()
    {
        const std::vector<TextLine> lines{
            {QStringLiteral("indented"), QRect(80, 0, 80, 16), 0.9f}};

        const auto words = textract::wordsFromLines(lines);

        QCOMPARE(words.size(), size_t(1));
        QCOMPARE(words[0].bbox.left(), 80);
    }

    /// Every word carries the source line's index so order/ can group them.
    void tagsEachWordWithItsSourceLineIndex()
    {
        const std::vector<TextLine> lines{
            {QStringLiteral("one"), QRect(0, 0, 30, 16), 0.9f},
            {QStringLiteral("two"), QRect(0, 20, 30, 16), 0.9f}};

        const auto words = textract::wordsFromLines(lines);

        QCOMPARE(words.size(), size_t(2));
        QCOMPARE(words[0].line, 0);
        QCOMPARE(words[1].line, 1);
        QCOMPARE(words[0].block, 0);
        QCOMPARE(words[1].block, 0);
    }

    /// PP-OCR reports one confidence per line; every word on it inherits that.
    /// A known coarsening -- there is no per-word confidence to be had.
    void givesEveryWordTheLineConfidence()
    {
        const std::vector<TextLine> lines{
            {QStringLiteral("a b"), QRect(0, 0, 30, 16), 0.75f}};

        const auto words = textract::wordsFromLines(lines);

        QCOMPARE(words.size(), size_t(2));
        QCOMPARE(words[0].confidence, 0.75f);
        QCOMPARE(words[1].confidence, 0.75f);
    }

    /// CJK arrives with no spaces at all: one word spanning the whole line.
    /// assemble/ already knows not to space CJK runs.
    void keepsAnUnspacedLineAsOneWord()
    {
        const std::vector<TextLine> lines{
            {QStringLiteral("这个程序"), QRect(0, 0, 64, 16), 0.9f}};

        const auto words = textract::wordsFromLines(lines);

        QCOMPARE(words.size(), size_t(1));
        QCOMPARE(words[0].bbox.width(), 64);
    }

    void dropsEmptyAndWhitespaceOnlyLines()
    {
        const std::vector<TextLine> lines{
            {QString(), QRect(0, 0, 10, 16), 0.9f},
            {QStringLiteral("   "), QRect(0, 20, 10, 16), 0.9f}};

        QVERIFY(textract::wordsFromLines(lines).empty());
    }

    /// A zero-width box cannot be divided; it must not produce a division by
    /// zero or a negative-width Word.
    void survivesADegenerateBox()
    {
        const std::vector<TextLine> lines{
            {QStringLiteral("x"), QRect(5, 0, 0, 16), 0.9f}};

        const auto words = textract::wordsFromLines(lines);

        for (const textract::Word &word : words) {
            QVERIFY(word.bbox.width() >= 0);
        }
    }
};

QTEST_MAIN(TestPaddleWords)
#include "test_paddlewords.moc"
