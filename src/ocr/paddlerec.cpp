// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocr/paddlerec.h"

namespace textract {

Recognition decodeCtc(const float *logits, int timesteps, int classes,
                      const QStringList &charset)
{
    Recognition result;
    if (logits == nullptr || timesteps <= 0 || classes <= 0) {
        return result;
    }

    double total = 0.0;
    int emitted = 0;
    int previous = -1;

    for (int t = 0; t < timesteps; ++t) {
        const float *row = logits + size_t(t) * size_t(classes);

        int best = 0;
        float score = row[0];
        for (int c = 1; c < classes; ++c) {
            if (row[c] > score) {
                score = row[c];
                best = c;
            }
        }

        // Emit on a change to a non-blank class. A blank between two identical
        // characters is exactly what keeps them distinct.
        if (best != previous && best != 0) {
            if (best < charset.size()) {
                result.text += charset.at(best);
                total += double(score);
                ++emitted;
            }
        }
        previous = best;
    }

    if (emitted > 0) {
        result.confidence = float(total / double(emitted));
    }
    return result;
}

} // namespace textract
