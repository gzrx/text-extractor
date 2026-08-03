// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "correct/correct.h"
#include "correct/dictionary.h"

namespace {

/// One recognised word. Geometry is irrelevant to correction — only the text,
/// the confidence and the layout decide anything — so it is left nominal.
textract::Word makeWord(const QString &text, float confidence)
{
    textract::Word word;
    word.text = text;
    word.bbox = QRect(0, 0, 10 * text.size(), 16);
    word.confidence = confidence;
    word.line = 1;
    word.block = 1;
    return word;
}

/// The text of every word, so a test asserts on the whole vector rather than
/// on one index and silently ignores collateral damage to its neighbours.
QStringList textOf(const std::vector<textract::Word> &words)
{
    QStringList out;
    for (const textract::Word &word : words) {
        out << word.text;
    }
    return out;
}

/// Below whatever threshold correction uses; the value is deliberately far
/// from it so that tuning the constant does not silently disarm these tests.
constexpr float kLow = 0.35f;
constexpr float kHigh = 0.99f;

} // namespace

class TestCorrect : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    // --- Dictionary ---------------------------------------------------------

    void findsAWordThatIsInTheDictionary()
    {
        const textract::Dictionary dictionary(QStringLiteral("en_US"));
        QVERIFY(dictionary.available());
        QVERIFY(dictionary.contains(QStringLiteral("recognition")));
    }

    /// The standing corpus case: "bleed-through" breaks across a line and the
    /// Prose branch reglues it. Only a dictionary can tell that apart from
    /// "recog-" + "nition", and it turns on this lookup returning false.
    void rejectsAWordThatIsNotInTheDictionary()
    {
        const textract::Dictionary dictionary(QStringLiteral("en_US"));
        QVERIFY(!dictionary.contains(QStringLiteral("bleedthrough")));
    }

    /// A word at the start of a sentence is capitalised, and a lookup that
    /// missed it would offer to "correct" every one of them.
    void findsACapitalisedWord()
    {
        const textract::Dictionary dictionary(QStringLiteral("en_US"));
        QVERIFY(dictionary.contains(QStringLiteral("Optical")));
    }

    /// Every machine this runs on will not have every dictionary, and a
    /// missing one must disable correction rather than fail the extraction.
    void reportsUnavailableForALanguageWithNoDictionary()
    {
        const textract::Dictionary dictionary(QStringLiteral("zz_ZZ"));
        QVERIFY(!dictionary.available());
    }

    /// An unavailable dictionary must not claim a word is missing either: a
    /// caller that acted on that would "correct" text it knows nothing about.
    void matchesNothingWhenUnavailable()
    {
        const textract::Dictionary dictionary(QStringLiteral("zz_ZZ"));
        QVERIFY(!dictionary.contains(QStringLiteral("recognition")));
    }

    void treatsAnEmptyStringAsNotAWord()
    {
        const textract::Dictionary dictionary(QStringLiteral("en_US"));
        QVERIFY(!dictionary.contains(QString()));
    }

    // --- correct(): what it must not touch ----------------------------------

    /// Source code is full of identifiers a dictionary would love to "fix",
    /// and none of them are dictionary words. Correction is confined to Prose
    /// for exactly this reason.
    void leavesCodeUntouched()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{
            makeWord(QStringLiteral("0ptical"), kLow),
            makeWord(QStringLiteral("rnodern"), kLow),
        };

        textract::correct(words, textract::LayoutKind::Code, &dictionary);

        QCOMPARE(textOf(words), (QStringList{QStringLiteral("0ptical"),
                                             QStringLiteral("rnodern")}));
    }

    void leavesTablesUntouched()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("0ptical"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Table, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("0ptical"));
    }

    /// Raw is the escape hatch. A user who held Shift asked for what was on
    /// screen, not for an improved version of it.
    void leavesRawUntouched()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("0ptical"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Raw, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("0ptical"));
    }

    void doesNothingWithoutADictionary()
    {
        std::vector<textract::Word> words{makeWord(QStringLiteral("0ptical"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Prose, nullptr);

        QCOMPARE(words.front().text, QStringLiteral("0ptical"));
    }

    /// Confidence is half the rule. A word the engine was sure about is what
    /// was on screen, whatever a dictionary thinks of it.
    void leavesAConfidentlyRecognisedWordAlone()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("0ptical"),
                                                   kHigh)};

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("0ptical"));
    }

    /// The other half of the rule. "dear" is a word, so the fact that cl<->d
    /// could turn it into "clear" is not evidence of anything.
    void leavesAWordTheDictionaryAlreadyKnows()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("dear"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("dear"));
    }

    /// A proper noun is not in the dictionary and no substitution rescues it.
    /// This module must never invent text, so it leaves it exactly as read.
    void leavesAnUnknownWordAloneWhenNoSubstitutionHelps()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("Wayland"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("Wayland"));
    }

    // --- correct(): the confusion set ---------------------------------------

    void substitutesTheDigitZeroForTheLetterO()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("0ptical"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("Optical"));
    }

    void substitutesTheDigitOneForALowercaseL()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("1ayout"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("layout"));
    }

    /// The multi-character pairs are the ones a per-character confusion set
    /// would miss, and "rn" read as "m" is the classic small-text failure.
    void substitutesRnForM()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("rnodern"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("modern"));
    }

    void substitutesVvForW()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("vvindow"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("window"));
    }

    /// Two errors in one word is common at small point sizes, so candidates
    /// have to combine substitutions rather than try one at a time.
    void substitutesAtTwoPositionsInOneWord()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("0ptica1"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("Optical"));
    }

    /// Punctuation is not part of the word being looked up, and it has to
    /// survive the round trip: a sentence that lost its full stop would be a
    /// worse result than one that kept a misread letter.
    void keepsPunctuationAttachedToACorrectedWord()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("(0ptical),"),
                                                   kLow)};

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().text, QStringLiteral("(Optical),"));
    }

    /// Correction rewrites text and nothing else. A downstream stage reading
    /// confidence to decide anything must see what the engine actually said.
    void doesNotAlterConfidenceOrGeometry()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words{makeWord(QStringLiteral("0ptical"),
                                                   kLow)};
        const QRect before = words.front().bbox;

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QCOMPARE(words.front().confidence, kLow);
        QCOMPARE(words.front().bbox, before);
    }

    void handlesAnEmptyWordList()
    {
        const textract::Dictionary dictionary;
        std::vector<textract::Word> words;

        textract::correct(words, textract::LayoutKind::Prose, &dictionary);

        QVERIFY(words.empty());
    }
};

QTEST_MAIN(TestCorrect)
#include "test_correct.moc"
