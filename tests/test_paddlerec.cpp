// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include <vector>

#include "ocr/paddlerec.h"

namespace {

/// Charset in the layout PaddleOCR's CTCLabelDecode uses: the blank first,
/// then the dictionary, then a space.
QStringList charset()
{
    return {QStringLiteral("<blank>"), QStringLiteral("a"),
            QStringLiteral("b"), QStringLiteral("c"), QStringLiteral(" ")};
}

/// Logits with `winner` at probability 1.0 for each timestep.
std::vector<float> logitsFor(const std::vector<int> &winners, int classes)
{
    std::vector<float> logits(winners.size() * size_t(classes), 0.0f);
    for (size_t t = 0; t < winners.size(); ++t) {
        logits[t * size_t(classes) + size_t(winners[t])] = 1.0f;
    }
    return logits;
}

} // namespace

class TestPaddleRec : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void decodesASimpleSequence()
    {
        const auto logits = logitsFor({1, 2, 3}, 5);

        const auto result = textract::decodeCtc(logits.data(), 3, 5, charset());

        QCOMPARE(result.text, QStringLiteral("abc"));
    }

    /// Class 0 is the CTC blank and is never emitted.
    void dropsBlanks()
    {
        const auto logits = logitsFor({0, 1, 0, 2, 0}, 5);

        const auto result = textract::decodeCtc(logits.data(), 5, 5, charset());

        QCOMPARE(result.text, QStringLiteral("ab"));
    }

    /// A character held across timesteps is one character.
    void collapsesRepeatedTimesteps()
    {
        const auto logits = logitsFor({1, 1, 1, 2, 2}, 5);

        const auto result = textract::decodeCtc(logits.data(), 5, 5, charset());

        QCOMPARE(result.text, QStringLiteral("ab"));
    }

    /// A blank between two identical characters is what separates them --
    /// without it "aa" would collapse to "a". This is the case that breaks if
    /// the previous-class check is written wrongly.
    void keepsRepeatsSeparatedByABlank()
    {
        const auto logits = logitsFor({1, 0, 1}, 5);

        const auto result = textract::decodeCtc(logits.data(), 3, 5, charset());

        QCOMPARE(result.text, QStringLiteral("aa"));
    }

    /// The space lives at the end of the charset, after the dictionary.
    void decodesTheSpaceAtTheEndOfTheCharset()
    {
        const auto logits = logitsFor({1, 4, 2}, 5);

        const auto result = textract::decodeCtc(logits.data(), 3, 5, charset());

        QCOMPARE(result.text, QStringLiteral("a b"));
    }

    void reportsTheMeanConfidenceOfEmittedCharacters()
    {
        std::vector<float> logits(3 * 5, 0.0f);
        logits[0 * 5 + 1] = 0.8f; // 'a'
        logits[1 * 5 + 0] = 0.9f; // blank, must not count
        logits[2 * 5 + 2] = 0.6f; // 'b'

        const auto result = textract::decodeCtc(logits.data(), 3, 5, charset());

        QCOMPARE(result.text, QStringLiteral("ab"));
        QVERIFY(qAbs(result.confidence - 0.7f) < 0.001f);
    }

    void returnsEmptyForAnAllBlankSequence()
    {
        const auto logits = logitsFor({0, 0, 0}, 5);

        const auto result = textract::decodeCtc(logits.data(), 3, 5, charset());

        QVERIFY(result.text.isEmpty());
        QCOMPARE(result.confidence, 0.0f);
    }

    /// A class index past the end of the charset must be skipped, not used to
    /// index out of bounds.
    void survivesAnOutOfRangeClassIndex()
    {
        const auto logits = logitsFor({1, 4}, 5);
        const QStringList shortCharset{QStringLiteral("<blank>"),
                                       QStringLiteral("a")};

        const auto result = textract::decodeCtc(logits.data(), 2, 5, shortCharset);

        QCOMPARE(result.text, QStringLiteral("a"));
    }

    void survivesDegenerateInput()
    {
        const auto logits = logitsFor({1}, 5);

        QVERIFY(textract::decodeCtc(nullptr, 3, 5, charset()).text.isEmpty());
        QVERIFY(textract::decodeCtc(logits.data(), 0, 5, charset()).text.isEmpty());
        QVERIFY(textract::decodeCtc(logits.data(), 3, 0, charset()).text.isEmpty());
    }
};

QTEST_MAIN(TestPaddleRec)
#include "test_paddlerec.moc"
