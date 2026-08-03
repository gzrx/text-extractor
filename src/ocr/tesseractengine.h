// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include <QStringList>

#include "ocr/ocrengine.h"

namespace tesseract {
class TessBaseAPI;
}

namespace textract {

/**
 * Language codes with traineddata installed, sorted, excluding `osd`.
 *
 * Built from what is actually on disk rather than from a fixed table, so the
 * config dialog cannot offer a language whose warmUp() would then fail.
 * `osd` is orientation and script detection, not a recognisable language.
 */
QStringList availableLanguages();

/// Tier-1 engine: a long-lived TessBaseAPI kept warm by the daemon.
class TesseractEngine : public OcrEngine
{
public:
    TesseractEngine();
    ~TesseractEngine() override;

    /// Loads `langs` (e.g. "eng" or "eng+msa"). Returns false if data is missing.
    bool initialize(const QString &langs);

    std::vector<Word> recognize(const QImage &image,
                                const QString &langs,
                                Segmentation mode) override;
    bool isWarm() const override;

    /// Tesseract runs full page segmentation and numbers its own lines and
    /// blocks, which is exactly what Segmentation selects between.
    bool providesReadingOrder() const override { return true; }

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
