// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include <QString>

#include "ocr/ocrengine.h"

namespace textract {

/**
 * Tier-2 engine: PP-OCRv6_small under ONNX Runtime, CPU.
 *
 * A deliberate escalation, not a replacement. It beats Tesseract on CJK, code
 * and both table fixtures, and loses to it on dark-terminal-buildlog -- small
 * monospace punctuation is still tier 1's. Which is why there are two tiers.
 *
 * Disables itself when its models are missing, the way Dictionary does without
 * Hunspell langdata: available() returns false, recognize() returns nothing,
 * and no exception escapes into the daemon's shortcut handler. ONNX Runtime
 * itself is a hard link-time dependency; only the model files are optional.
 */
class OnnxPaddleEngine : public OcrEngine
{
public:
    /// `modelDir` holds ppocrv6_small_{det,rec}.onnx and their .yml files.
    explicit OnnxPaddleEngine(const QString &modelDir);
    ~OnnxPaddleEngine() override;

    OnnxPaddleEngine(const OnnxPaddleEngine &) = delete;
    OnnxPaddleEngine &operator=(const OnnxPaddleEngine &) = delete;

    /// True once both sessions and the charset loaded.
    bool available() const;

    /// The default model directory: $XDG_DATA_HOME/textract/models.
    static QString defaultModelDir();

    std::vector<Word> recognize(const QImage &image,
                                const QString &langs,
                                Segmentation mode) override;
    bool isWarm() const override;
    void setUpscaleFactor(int factor) override;

    /// A detector-plus-recogniser reports where text is, not what order to read
    /// it in. order/ supplies both that and the line and block numbering.
    bool providesReadingOrder() const override { return false; }

private:
    // ONNX Runtime headers stay out of this header so textract_lib's consumers
    // do not need them on their include path.
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace textract
