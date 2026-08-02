// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QImage>
#include <QString>

#include "ocr/word.h"

namespace textract {

/// Common interface for tier-1 (Tesseract) and tier-2 (ONNX) engines.
class OcrEngine
{
public:
    virtual ~OcrEngine() = default;

    /// Recognises `image`, returning words in reading order.
    virtual std::vector<Word> recognize(const QImage &image,
                                        const QString &langs) = 0;

    /// True once the engine's models are loaded and a call will be fast.
    virtual bool isWarm() const = 0;
};

} // namespace textract
