// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "ocr/ocrengine.h"

namespace tesseract {
class TessBaseAPI;
}

namespace textract {

/// Tier-1 engine: a long-lived TessBaseAPI kept warm by the daemon.
class TesseractEngine : public OcrEngine
{
public:
    TesseractEngine();
    ~TesseractEngine() override;

    /// Loads `langs` (e.g. "eng" or "eng+msa"). Returns false if data is missing.
    bool initialize(const QString &langs);

    std::vector<Word> recognize(const QImage &image,
                                const QString &langs) override;
    bool isWarm() const override;

    /// Divisor applied to reported boxes so they land in original crop
    /// coordinates. extractText() sets this from effectiveUpscale().
    void setUpscaleFactor(int factor) override
    {
        m_upscale = factor > 0 ? factor : 1;
    }

private:
    std::unique_ptr<tesseract::TessBaseAPI> m_api;
    QString m_langs;
    bool    m_warm{false};
    int     m_upscale{1};
};

} // namespace textract
