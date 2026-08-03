// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace textract {

/// One decoded text line.
struct Recognition {
    QString text;
    float   confidence{0.0f}; ///< 0.0 - 1.0, mean over emitted characters.
};

/**
 * Greedy CTC decode of one line's logits.
 *
 * `logits` is row-major, `timesteps` by `classes`. `charset` must have one
 * entry per class in PaddleOCR's CTCLabelDecode order: the blank at index 0,
 * then the model's dictionary, then a space. Index 0 is never emitted.
 *
 * Confidence is the mean of the winning score over *emitted* characters only.
 * Blanks usually dominate a CTC sequence, so averaging over all timesteps
 * would report the model's confidence that most of the line is empty.
 *
 * Per-timestep positions are discarded here. They are the raw material for
 * per-character x-offsets, which would give wordsFromLines() true proportional
 * geometry instead of a uniform split -- cheap to add later precisely because
 * the alignment is already computed at this point.
 */
Recognition decodeCtc(const float *logits, int timesteps, int classes,
                      const QStringList &charset);

} // namespace textract
