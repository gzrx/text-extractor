// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * Scores the fixture corpus across every supported upscale factor.
 *
 * The spec commits to a 3x default on reasoning alone. This is the tool that
 * turns that into a measurement: run it once the corpus has real captures in
 * it and pick the factor with the best mean, then record the answer.
 *
 *   ./build/bin/textract-fixture-report [--binarize] [manifest.json]
 */

#include <cstdio>
#include <map>

#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QStringList>

#include "app/extraction.h"
#include "fixturecorpus.h"
#include "ocr/tesseractengine.h"

using namespace textract;
using namespace textract::fixtures;

namespace {

QString defaultManifest()
{
    return QStringLiteral(TEXTRACT_FIXTURE_DIR "/manifest.json");
}

/// Returns the score for one fixture at one upscale factor, or -1 if the
/// fixture could not be run at all.
double scoreAt(TesseractEngine &engine, const Fixture &fixture,
               const QImage &crop, const QString &expected,
               int upscale, bool binarize)
{
    PreprocessOptions options;
    options.upscale = upscale;
    options.binarize = binarize;

    const auto result = extractText(engine, crop, fixture.langs,
                                    fixture.layout, options);
    return similarity(expected, result.text);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    bool binarize = false;
    QString manifest = defaultManifest();
    for (const QString &argument : app.arguments().mid(1)) {
        if (argument == QLatin1String("--binarize")) {
            binarize = true;
        } else {
            manifest = argument;
        }
    }

    QString error;
    const std::vector<Fixture> fixtures = loadManifest(manifest, &error);
    if (!error.isEmpty()) {
        std::fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    if (fixtures.empty()) {
        std::fprintf(stderr,
                     "no fixtures in %s\n"
                     "Capture some first - see tests/fixtures/README.md\n",
                     qPrintable(manifest));
        return 1;
    }

    std::printf("corpus: %s%s\n\n", qPrintable(manifest),
                binarize ? "  (binarisation ON)" : "");
    std::printf("%-30s", "fixture");
    for (int factor = kMinUpscale; factor <= kMaxUpscale; ++factor) {
        std::printf("%8dx", factor);
    }
    std::printf("   langs\n");

    std::map<int, double> totals;
    int scored = 0;

    TesseractEngine engine;
    for (const Fixture &fixture : fixtures) {
        if (!langdataAvailable(fixture.langs)) {
            std::printf("%-30s   skipped: no traineddata for %s\n",
                        qPrintable(fixture.name), qPrintable(fixture.langs));
            continue;
        }

        QImage crop;
        if (!crop.load(fixture.imagePath)) {
            std::printf("%-30s   skipped: cannot load %s\n",
                        qPrintable(fixture.name), qPrintable(fixture.imagePath));
            continue;
        }

        QFile expectedFile(fixture.expectedPath);
        if (!expectedFile.open(QIODevice::ReadOnly)) {
            std::printf("%-30s   skipped: cannot read %s\n",
                        qPrintable(fixture.name), qPrintable(fixture.expectedPath));
            continue;
        }
        const QString expected = QString::fromUtf8(expectedFile.readAll());

        std::printf("%-30s", qPrintable(fixture.name));
        for (int factor = kMinUpscale; factor <= kMaxUpscale; ++factor) {
            const double score = scoreAt(engine, fixture, crop, expected,
                                         factor, binarize);
            totals[factor] += score;
            std::printf("%9.4f", score);
        }
        std::printf("   %s\n", qPrintable(fixture.langs));
        ++scored;
    }

    if (scored == 0) {
        std::fprintf(stderr, "\nno fixture could be scored\n");
        return 1;
    }

    std::printf("\n%-30s", "mean");
    int best = kMinUpscale;
    for (int factor = kMinUpscale; factor <= kMaxUpscale; ++factor) {
        std::printf("%9.4f", totals[factor] / scored);
        if (totals[factor] > totals[best]) {
            best = factor;
        }
    }
    std::printf("\n\nbest mean over %d fixture(s): %dx\n", scored, best);
    return 0;
}
