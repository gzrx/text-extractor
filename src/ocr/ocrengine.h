// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QImage>
#include <QString>

#include "ocr/word.h"

namespace textract {

/**
 * How the engine should segment the page before recognising it.
 *
 * This is the single highest-value knob in the pipeline, because it decides
 * reading *order* rather than recognition. On whitespace-aligned content — a
 * terminal, a table, a spreadsheet — full page analysis mistakes the gutters
 * for column boundaries and emits the content column-block by column-block.
 * The symptom is distinctive: high mean confidence next to a low score, i.e.
 * the characters were read correctly and then ordered wrongly.
 *
 * It is per call, not per engine, because no single mode is right for a
 * desktop: `Auto` is correct for a genuinely multi-column document and wrong
 * for everything else, and `SingleBlock` is the reverse.
 */
enum class Segmentation {
    Auto,        ///< Full layout analysis, including multi-column (PSM 3).
    SingleBlock, ///< One uniform block, read line by line (PSM 6).
};

/// Common interface for tier-1 (Tesseract) and tier-2 (ONNX) engines.
class OcrEngine
{
public:
    virtual ~OcrEngine() = default;

    /// Recognises `image`, returning words in reading order.
    ///
    /// `mode` is a hint an engine may ignore if its architecture has no
    /// equivalent — a detector-plus-recogniser model has no page segmentation
    /// stage to configure.
    virtual std::vector<Word> recognize(const QImage &image,
                                        const QString &langs,
                                        Segmentation mode) = 0;

    /// True once the engine's models are loaded and a call will be fast.
    virtual bool isWarm() const = 0;

    /// True if the engine returns words already in reading order, with line and
    /// block indices it derived itself.
    ///
    /// An engine returning false is also declaring the Segmentation hint
    /// meaningless to it: both properties come from having a page-segmentation
    /// stage, and a detector-plus-recogniser has neither. The pipeline then
    /// owns ordering, via order/.
    ///
    /// Pure virtual for the same reason setUpscaleFactor() is — an engine must
    /// answer this deliberately rather than inherit a default that silently
    /// mis-orders the user's text.
    virtual bool providesReadingOrder() const = 0;

    /// Tells the engine what factor preprocessing upscaled the image by, so it
    /// can divide reported boxes back into original crop coordinates.
    ///
    /// Pure virtual on purpose: every engine owns this conversion, and
    /// `analyze/` and `assemble/` must never learn that upscaling happened.
    virtual void setUpscaleFactor(int factor) = 0;
};

} // namespace textract
