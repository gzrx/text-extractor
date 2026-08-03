// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <vector>

#include <QImage>
#include <Qt>

#include "assemble/assemble.h"
#include "ocr/ocrengine.h"
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

/**
 * The page segmentation a layout wants.
 *
 * `Prose` is the only kind that may be genuinely multi-column, so it is the
 * only one that gets full layout analysis. Raw, Code and Table are all
 * whitespace-aligned single blocks, where column detection is precisely the
 * thing that ruins them — it turns alignment gutters into column boundaries
 * and emits the region block by block instead of line by line.
 */
Segmentation segmentationFor(LayoutKind kind);

/**
 * The layout the user demanded with modifier keys, if any.
 *
 * Holding Shift through the drag forces Raw. Every heuristic in this module
 * is wrong sometimes, and when one is, the user should be able to take it back
 * on the next attempt rather than go looking for a setting.
 */
std::optional<LayoutKind> forcedLayoutFor(Qt::KeyboardModifiers modifiers);

} // namespace textract
