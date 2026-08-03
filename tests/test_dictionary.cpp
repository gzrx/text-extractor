// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "correct/dictionary.h"

class TestDictionary : public QObject
{
    Q_OBJECT
private Q_SLOTS:

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
    /// missed it would answer the hyphen question wrongly for every one of
    /// them.
    void findsACapitalisedWord()
    {
        const textract::Dictionary dictionary(QStringLiteral("en_US"));
        QVERIFY(dictionary.contains(QStringLiteral("Optical")));
    }

    /// Every machine this runs on will not have every dictionary, and a
    /// missing one must fall back to the unaided reglue rather than fail the
    /// extraction.
    void reportsUnavailableForALanguageWithNoDictionary()
    {
        const textract::Dictionary dictionary(QStringLiteral("zz_ZZ"));
        QVERIFY(!dictionary.available());
    }

    /// An unavailable dictionary must not claim a word is missing either: a
    /// caller that acted on that would take a decision about text it knows
    /// nothing about.
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
};

QTEST_MAIN(TestDictionary)
#include "test_dictionary.moc"
