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
    /// coordinates. M2 leaves this at 1; M3's preprocessing will set it.
    void setUpscaleFactor(int factor) { m_upscale = factor > 0 ? factor : 1; }

private:
    std::unique_ptr<tesseract::TessBaseAPI> m_api;
    QString m_langs;
    bool    m_warm{false};
    int     m_upscale{1};
};

} // namespace textract
