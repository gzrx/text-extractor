// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/extraction.h"

#include "analyze/analyze.h"
#include "correct/correct.h"
#include "order/order.h"

namespace textract {

namespace {

/**
 * The dictionary to post-correct with, or nullptr when there is none.
 *
 * Loaded once and shared. Hunspell parses the whole .dic on construction,
 * which is far too expensive to repeat per extraction on a path the user is
 * meant to reach for reflexively.
 *
 * Correction is enabled only for languages the machine has *both* Tesseract
 * langdata and a Hunspell dictionary for, which today means English alone. A
 * Malay or Chinese capture is left exactly as recognised rather than checked
 * against the wrong language — that would not be correction, it would be
 * noise.
 */
const Dictionary *dictionaryFor(const QString &langs)
{
    // Tesseract takes '+'-joined language codes; a mixed capture that includes
    // English still gets the English check, and the non-English words in it
    // are protected by the same rule that protects proper nouns.
    if (!langs.split(QLatin1Char('+')).contains(QLatin1String("eng"))) {
        return nullptr;
    }
    static const Dictionary english(QStringLiteral("en_US"));
    return english.available() ? &english : nullptr;
}

} // namespace

Extraction extractText(OcrEngine &engine,
                       const QImage &crop,
                       const QString &langs,
                       std::optional<LayoutKind> forcedKind,
                       const PreprocessOptions &options)
{
    Extraction result;
    if (crop.isNull()) {
        return result;
    }

    const QImage conditioned = preprocess(crop, options);

    // effectiveUpscale(), not options.upscale: an out-of-range request is
    // clamped, and the divisor has to match the image the engine actually got.
    engine.setUpscaleFactor(effectiveUpscale(options));

    // Segmentation has to be chosen before recognition, but classification
    // needs recognised words to look at — so an unforced call recognises
    // twice. The first pass asks for a single block, which is the common
    // desktop case (a terminal, a table, a dialog) and therefore the one that
    // should not pay for a second pass. Only a multi-column Prose result
    // re-runs. Word geometry is reliable under either mode, so the classifier
    // is not misled by the first pass's reading order.
    const Segmentation firstPass = forcedKind ? segmentationFor(*forcedKind)
                                              : Segmentation::SingleBlock;

    result.words = engine.recognize(conditioned, langs, firstPass);
    if (result.words.empty()) {
        return result;
    }

    if (forcedKind) {
        result.kind = *forcedKind;
        result.layoutConfidence = 1.0f;
    } else {
        const LayoutClass layout = classify(result.words, conditioned);
        result.kind = layout.kind;
        result.layoutConfidence = layout.confidence;

        // Only an engine with a page-segmentation stage can act on the hint,
        // so only such an engine can gain anything from a second pass. For a
        // detector-plus-recogniser the mode is ignored, and re-running would
        // buy byte-identical words at the price of a whole inference.
        if (engine.providesReadingOrder()) {
            if (const Segmentation wanted = segmentationFor(result.kind);
                wanted != firstPass) {
                std::vector<Word> resegmented = engine.recognize(conditioned,
                                                                 langs, wanted);
                // Keep the first pass rather than return nothing: a mode that
                // recognises less is worse than a mode that ordered it oddly.
                if (!resegmented.empty()) {
                    result.words = std::move(resegmented);
                }
            }
        }
    }

    // An engine that does not number its own lines and blocks gets them here.
    // This runs after classification on purpose: column-versus-table cannot be
    // decided from geometry alone, and classify() has already decided it.
    if (!engine.providesReadingOrder()) {
        orderWords(result.words, result.kind);
    }

    // Correction runs on words rather than on the assembled string, because
    // both of its inputs are per word: the engine's confidence, and — for the
    // hyphen decision inside the Prose branch — which two tokens a line break
    // fell between. Neither survives assembly.
    const Dictionary *dictionary = dictionaryFor(langs);
    correct(result.words, result.kind, dictionary);

    result.text = assemble(result.words, result.kind, dictionary);

    float total = 0.0f;
    for (const Word &word : result.words) {
        total += word.confidence;
    }
    result.meanConfidence = total / float(result.words.size());

    return result;
}

} // namespace textract
