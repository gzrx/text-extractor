// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/extraction.h"

#include "analyze/analyze.h"

namespace textract {

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

        if (const Segmentation wanted = segmentationFor(result.kind);
            wanted != firstPass) {
            std::vector<Word> resegmented = engine.recognize(conditioned, langs,
                                                             wanted);
            // Keep the first pass rather than return nothing: a mode that
            // recognises less is worse than a mode that ordered it oddly.
            if (!resegmented.empty()) {
                result.words = std::move(resegmented);
            }
        }
    }

    result.text = assemble(result.words, result.kind);

    float total = 0.0f;
    for (const Word &word : result.words) {
        total += word.confidence;
    }
    result.meanConfidence = total / float(result.words.size());

    return result;
}

} // namespace textract
