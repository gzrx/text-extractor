// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "analyze/analyze.h"

namespace textract {

LayoutClass classify(const std::vector<Word> &words, const QImage &image)
{
    Q_UNUSED(words)
    Q_UNUSED(image)

    // No signals yet. This exists so the seam can be wired and measured before
    // any heuristic is added: if the corpus moves now, the wiring is wrong, and
    // there is no heuristic to blame it on.
    return {};
}

Segmentation segmentationFor(LayoutKind kind)
{
    switch (kind) {
    case LayoutKind::Prose:
        return Segmentation::Auto;
    case LayoutKind::Raw:
    case LayoutKind::Code:
    case LayoutKind::Table:
        return Segmentation::SingleBlock;
    }
    return Segmentation::SingleBlock;
}

} // namespace textract
