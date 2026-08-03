// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTemporaryDir>
#include <QTest>

#include <KConfig>
#include <KConfigGroup>

#include "config/settings.h"

using namespace textract;

class TestSettings : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultsMatchTheCompiledInBehaviour();
    void absentFileYieldsDefaults();
    void roundTripsEveryField();
    void eachKeyDefaultsIndependently();
    void upscaleIsClampedAtBothEnds();
};

/// The whole "an absent config file is not a special case" property rests on
/// this: every default here must equal what main.cpp and ExtractorController
/// hardcoded before M7a existed.
void TestSettings::defaultsMatchTheCompiledInBehaviour()
{
    const Settings s;
    QCOMPARE(s.langs, QStringLiteral("eng"));
    QCOMPARE(s.preprocess.upscale, 3);
    QCOMPARE(s.preprocess.binarize, false);
    QVERIFY2(s.modelDir.isEmpty(), "empty means defaultModelDir()");
}

void TestSettings::absentFileYieldsDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("nonexistentrc")),
                   KConfig::SimpleConfig);

    const Settings s = loadSettings(config.group(QString()));
    QCOMPARE(s, Settings());
}

void TestSettings::roundTripsEveryField()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("textractrc")),
                   KConfig::SimpleConfig);

    Settings written;
    written.langs = QStringLiteral("eng+msa");
    written.preprocess.upscale = 4;
    written.preprocess.binarize = true;
    written.modelDir = QStringLiteral("/opt/models");

    KConfigGroup root = config.group(QString());
    saveSettings(root, written);
    config.sync();

    KConfig reopened(dir.filePath(QStringLiteral("textractrc")),
                     KConfig::SimpleConfig);
    QCOMPARE(loadSettings(reopened.group(QString())), written);
}

/// A file written by an older version, or hand-edited to set one key, must not
/// drag the others away from their defaults.
void TestSettings::eachKeyDefaultsIndependently()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("textractrc")),
                   KConfig::SimpleConfig);
    config.group(QStringLiteral("General"))
        .writeEntry("Languages", QStringLiteral("chi_sim"));
    config.sync();

    const Settings s = loadSettings(config.group(QString()));
    QCOMPARE(s.langs, QStringLiteral("chi_sim"));
    QCOMPARE(s.preprocess.upscale, 3);
    QCOMPARE(s.preprocess.binarize, false);
    QVERIFY(s.modelDir.isEmpty());
}

/// Clamped rather than rejected, to the same [2, 4] effectiveUpscale() already
/// enforces. A second, stricter rule here would mean a config file that loads
/// as one factor and preprocesses as another.
void TestSettings::upscaleIsClampedAtBothEnds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    KConfig low(dir.filePath(QStringLiteral("lowrc")), KConfig::SimpleConfig);
    low.group(QStringLiteral("Preprocess")).writeEntry("Upscale", 1);
    low.sync();
    QCOMPARE(loadSettings(low.group(QString())).preprocess.upscale, kMinUpscale);

    KConfig high(dir.filePath(QStringLiteral("highrc")), KConfig::SimpleConfig);
    high.group(QStringLiteral("Preprocess")).writeEntry("Upscale", 99);
    high.sync();
    QCOMPARE(loadSettings(high.group(QString())).preprocess.upscale, kMaxUpscale);
}

QTEST_GUILESS_MAIN(TestSettings)
#include "test_settings.moc"
