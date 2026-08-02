// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/extraction.h"

namespace textract {

Extraction extractText(OcrEngine &engine,
                       const QImage &crop,
                       const QString &langs,
                       LayoutKind kind,
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

    result.words = engine.recognize(conditioned, langs);
    if (result.words.empty()) {
        return result;
    }

    result.text = assemble(result.words, kind);

    float total = 0.0f;
    for (const Word &word : result.words) {
        total += word.confidence;
    }
    result.meanConfidence = total / float(result.words.size());

    return result;
}

} // namespace textract
