// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * The corpus gate.
 *
 * Runs every captured fixture through the same pipeline the daemon runs and
 * reports a per-fixture accuracy score. A fixture only *fails* if it drops
 * below its own recorded `minScore`, so the corpus can grow with hard cases
 * that are not yet solved without turning the suite red.
 *
 * `textract-fixture-report` prints the same scores across upscale factors.
 */

#include <QTest>
#include <QFile>
#include <QImage>

#include "app/extraction.h"
#include "fixturecorpus.h"
#include "ocr/tesseractengine.h"

using namespace textract::fixtures;

// Must precede any QFETCH/addColumn use, and must be fully qualified.
Q_DECLARE_METATYPE(textract::fixtures::Fixture)

namespace {

QString manifestPath()
{
    if (const QByteArray override = qgetenv("TEXTRACT_FIXTURES");
        !override.isEmpty()) {
        return QString::fromLocal8Bit(override);
    }
    return QStringLiteral(TEXTRACT_FIXTURE_DIR "/manifest.json");
}

QString layoutName(textract::LayoutKind kind)
{
    switch (kind) {
    case textract::LayoutKind::Raw:   return QStringLiteral("raw");
    case textract::LayoutKind::Code:  return QStringLiteral("code");
    case textract::LayoutKind::Prose: return QStringLiteral("prose");
    case textract::LayoutKind::Table: return QStringLiteral("table");
    }
    return QStringLiteral("?");
}

} // namespace

class TestFixtures : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QString error;
        m_fixtures = loadManifest(manifestPath(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        if (m_fixtures.empty()) {
            QSKIP("no fixtures captured yet - see tests/fixtures/README.md");
        }
    }

    void matchesExpectedText_data()
    {
        QTest::addColumn<Fixture>("fixture");
        for (const Fixture &fixture : m_fixtures) {
            QTest::newRow(qPrintable(fixture.name)) << fixture;
        }
    }

    void matchesExpectedText()
    {
        QFETCH(Fixture, fixture);

        if (!langdataAvailable(fixture.langs)) {
            QSKIP(qPrintable(QStringLiteral("traineddata for \"%1\" not installed")
                                 .arg(fixture.langs)));
        }

        QImage crop;
        QVERIFY2(crop.load(fixture.imagePath),
                 qPrintable(QStringLiteral("cannot load %1").arg(fixture.imagePath)));

        QFile expectedFile(fixture.expectedPath);
        QVERIFY2(expectedFile.open(QIODevice::ReadOnly),
                 qPrintable(QStringLiteral("cannot read %1").arg(fixture.expectedPath)));
        const QString expected = QString::fromUtf8(expectedFile.readAll());

        const auto result = textract::extractText(m_engine, crop, fixture.langs,
                                                  fixture.layout, {});
        const double score = similarity(expected, result.text);

        // Printed on pass as well as failure: the direction each fixture moved
        // is the signal, and a boolean would hide it.
        qInfo("%-28s score %.4f  (floor %.4f, mean confidence %.2f)",
              qPrintable(fixture.name), score, fixture.minScore,
              double(result.meanConfidence));

        QVERIFY2(score >= fixture.minScore,
                 qPrintable(QStringLiteral("%1 scored %2, below its floor of %3")
                                .arg(fixture.name)
                                .arg(score, 0, 'f', 4)
                                .arg(fixture.minScore, 0, 'f', 4)));
    }

    void classifiesWellEnoughToHitTheSameFloor_data() { matchesExpectedText_data(); }

    /**
     * The daemon does not know a region's layout, so it runs the unforced
     * path: classify(), then assemble by whatever came back. matchesExpectedText
     * pins the declared layout and therefore scores assembly alone — useful,
     * but it is not what the user gets.
     *
     * Held to the same floor as the pinned run. A classifier that picks the
     * "wrong" kind and still reaches the floor has not hurt anyone; one that
     * drops below it has, and no amount of agreeing with the manifest would
     * make up for that.
     */
    void classifiesWellEnoughToHitTheSameFloor()
    {
        QFETCH(Fixture, fixture);

        if (!langdataAvailable(fixture.langs)) {
            QSKIP(qPrintable(QStringLiteral("traineddata for \"%1\" not installed")
                                 .arg(fixture.langs)));
        }

        QImage crop;
        QVERIFY2(crop.load(fixture.imagePath),
                 qPrintable(QStringLiteral("cannot load %1").arg(fixture.imagePath)));

        QFile expectedFile(fixture.expectedPath);
        QVERIFY2(expectedFile.open(QIODevice::ReadOnly),
                 qPrintable(QStringLiteral("cannot read %1").arg(fixture.expectedPath)));
        const QString expected = QString::fromUtf8(expectedFile.readAll());

        const auto result = textract::extractText(m_engine, crop, fixture.langs,
                                                  std::nullopt, {});
        const double score = similarity(expected, result.text);

        qInfo("%-28s score %.4f  (floor %.4f, classified %s at %.2f, declared %s)",
              qPrintable(fixture.name), score, fixture.minScore,
              qPrintable(layoutName(result.kind)),
              double(result.layoutConfidence),
              qPrintable(layoutName(fixture.layout)));

        QVERIFY2(score >= fixture.minScore,
                 qPrintable(QStringLiteral("%1 classified as %2 scored %3, "
                                           "below its floor of %4")
                                .arg(fixture.name, layoutName(result.kind))
                                .arg(score, 0, 'f', 4)
                                .arg(fixture.minScore, 0, 'f', 4)));
    }

private:
    std::vector<Fixture>      m_fixtures;
    textract::TesseractEngine m_engine;
};

QTEST_MAIN(TestFixtures)
#include "test_fixtures.moc"
