// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include <QDir>
#include <QTemporaryDir>

#include "fixturecorpus.h"

using namespace textract::fixtures;

namespace {

/// Writes `contents` to `dir/name` and returns the full path.
QString writeFile(const QTemporaryDir &dir, const QString &name,
                  const QByteArray &contents)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(contents);
    file.close();
    return path;
}

} // namespace

class TestFixtureCorpus : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // ---- scoring -------------------------------------------------------

    void scoresAnExactMatchAsOne()
    {
        QCOMPARE(similarity(QStringLiteral("Hello world"),
                            QStringLiteral("Hello world")), 1.0);
    }

    void scoresFullyDisjointTextAsZero()
    {
        QCOMPARE(similarity(QStringLiteral("abc"), QStringLiteral("xyz")), 0.0);
    }

    /// A per-fixture score, not a boolean, is the whole point: one wrong glyph
    /// in ten has to read as 0.9 so improvements and regressions are visible.
    void scoresOneSubstitutionInTenProportionally()
    {
        const double score = similarity(QStringLiteral("abcdefghij"),
                                        QStringLiteral("abcdefghiX"));
        QVERIFY(qFuzzyCompare(score, 0.9));
    }

    void scoresAMissingTailProportionally()
    {
        const double score = similarity(QStringLiteral("abcdefghij"),
                                        QStringLiteral("abcde"));
        QVERIFY(qFuzzyCompare(score, 0.5));
    }

    void scoresTwoEmptyStringsAsOne()
    {
        QCOMPARE(similarity(QString(), QString()), 1.0);
    }

    void scoresEmptyOutputAgainstExpectedTextAsZero()
    {
        QCOMPARE(similarity(QStringLiteral("Hello"), QString()), 0.0);
    }

    /// Editors and captures disagree about trailing whitespace and line
    /// endings; that must not show up as an accuracy regression.
    void ignoresLineEndingAndTrailingWhitespaceDifferences()
    {
        QCOMPARE(similarity(QStringLiteral("one\r\ntwo  \r\n\r\n"),
                            QStringLiteral("one\ntwo\n")), 1.0);
    }

    void preservesInteriorLineStructure()
    {
        QVERIFY(similarity(QStringLiteral("one\ntwo"),
                           QStringLiteral("one two")) < 1.0);
    }

    // ---- manifest ------------------------------------------------------

    void loadsFixtureEntriesWithPathsRelativeToTheManifest()
    {
        QTemporaryDir dir;
        const QString manifest = writeFile(dir, QStringLiteral("manifest.json"), R"({
            "fixtures": [
                {
                    "name": "dark-terminal",
                    "image": "dark-terminal.png",
                    "expected": "dark-terminal.txt",
                    "langs": "eng+msa",
                    "layout": "code",
                    "minScore": 0.95,
                    "notes": "polarity path"
                }
            ]
        })");

        QString error;
        const auto fixtures = loadManifest(manifest, &error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(fixtures.size(), size_t(1));
        QCOMPARE(fixtures[0].name, QStringLiteral("dark-terminal"));
        QCOMPARE(fixtures[0].imagePath, dir.filePath(QStringLiteral("dark-terminal.png")));
        QCOMPARE(fixtures[0].expectedPath, dir.filePath(QStringLiteral("dark-terminal.txt")));
        QCOMPARE(fixtures[0].langs, QStringLiteral("eng+msa"));
        QCOMPARE(fixtures[0].layout, textract::LayoutKind::Code);
        QCOMPARE(fixtures[0].minScore, 0.95);
        QCOMPARE(fixtures[0].notes, QStringLiteral("polarity path"));
    }

    void appliesDefaultsForOptionalFields()
    {
        QTemporaryDir dir;
        const QString manifest = writeFile(dir, QStringLiteral("manifest.json"), R"({
            "fixtures": [ { "name": "plain", "image": "a.png", "expected": "a.txt" } ]
        })");

        QString error;
        const auto fixtures = loadManifest(manifest, &error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(fixtures.size(), size_t(1));
        QCOMPARE(fixtures[0].langs, QStringLiteral("eng"));
        QCOMPARE(fixtures[0].layout, textract::LayoutKind::Raw);
        QCOMPARE(fixtures[0].minScore, 0.0);
    }

    /// An empty corpus is the normal state until fixtures are captured, and it
    /// must not look like a failure.
    void reportsAnEmptyCorpusWithoutError()
    {
        QTemporaryDir dir;
        const QString manifest = writeFile(dir, QStringLiteral("manifest.json"),
                                           R"({ "fixtures": [] })");

        QString error;
        QVERIFY(loadManifest(manifest, &error).empty());
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }

    void reportsAMissingManifestWithoutError()
    {
        QTemporaryDir dir;

        QString error;
        QVERIFY(loadManifest(dir.filePath(QStringLiteral("absent.json")),
                             &error).empty());
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }

    /// Malformed JSON is a real mistake and must be loud, unlike an absent one.
    void reportsMalformedJsonAsAnError()
    {
        QTemporaryDir dir;
        const QString manifest = writeFile(dir, QStringLiteral("manifest.json"),
                                           "{ not json");

        QString error;
        QVERIFY(loadManifest(manifest, &error).empty());
        QVERIFY(!error.isEmpty());
    }

    void reportsAnEntryMissingRequiredFieldsAsAnError()
    {
        QTemporaryDir dir;
        const QString manifest = writeFile(dir, QStringLiteral("manifest.json"), R"({
            "fixtures": [ { "name": "no-image" } ]
        })");

        QString error;
        QVERIFY(loadManifest(manifest, &error).empty());
        QVERIFY(error.contains(QStringLiteral("no-image")));
    }

    // ---- langdata availability ----------------------------------------

    void detectsInstalledLanguageData()
    {
        QVERIFY(langdataAvailable(QStringLiteral("eng")));
    }

    void detectsMissingLanguageData()
    {
        QVERIFY(!langdataAvailable(QStringLiteral("nosuchlang")));
    }

    /// Multi-language fixtures need every component present.
    void requiresEveryComponentOfACompoundLangSpec()
    {
        QVERIFY(!langdataAvailable(QStringLiteral("eng+nosuchlang")));
    }
};

QTEST_MAIN(TestFixtureCorpus)
#include "test_fixturecorpus.moc"
