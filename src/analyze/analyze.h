// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QImage>

#include "assemble/assemble.h"
#include "ocr/word.h"

namespace textract {

/// What classify() decided, and how sure it is.
struct LayoutClass {
    LayoutKind kind{LayoutKind::Raw};

    /// 0.0 - 1.0. Reported rather than folded into the decision so a caller can
    /// tell "confidently Raw" from "gave up and said Raw".
    float confidence{0.0f};
};

/**
 * Decides how a recognised region is laid out. Pure.
 *
 * `words` carry geometry in original crop coordinates and `image` is the
 * conditioned crop the engine saw. Signals are geometric, so they survive the
 * reading-order defect that motivates the whole classification step: a word's
 * box is right even when the order the words arrive in is wrong.
 *
 * Falls back to `Raw` whenever the evidence is weak. Raw is never wrong, only
 * unambitious, whereas a confident wrong answer reorders the user's text.
 */
LayoutClass classify(const std::vector<Word> &words, const QImage &image);

} // namespace textract
