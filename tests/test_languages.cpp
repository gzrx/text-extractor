// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "ocr/tesseractengine.h"

using namespace textract;

class TestLanguages : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void findsTheLanguageTheCorpusRequires();
    void excludesOsd();
    void isSortedAndFreeOfDuplicates();
};

/// eng is not optional: it is the compiled-in default and eight of the ten
/// corpus fixtures are scored with it. If this fails, tesseract-data-eng is
/// missing and much more than this test is broken.
void TestLanguages::findsTheLanguageTheCorpusRequires()
{
    const QStringList langs = availableLanguages();
    QVERIFY2(!langs.isEmpty(), "no tessdata found at all");
    QVERIFY(langs.contains(QStringLiteral("eng")));
}

/// osd is orientation and script detection, not a language. Offering it in the
/// dialog would let a user select something that loads successfully and then
/// recognises nothing, which is far more confusing than an error.
void TestLanguages::excludesOsd()
{
    QVERIFY(!availableLanguages().contains(QStringLiteral("osd")));
}

void TestLanguages::isSortedAndFreeOfDuplicates()
{
    const QStringList langs = availableLanguages();
    QStringList expected = langs;
    expected.sort();
    expected.removeDuplicates();
    QCOMPARE(langs, expected);
}

QTEST_GUILESS_MAIN(TestLanguages)
#include "test_languages.moc"
