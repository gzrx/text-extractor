// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocr/tesseractengine.h"

#include <tesseract/baseapi.h>
#include <tesseract/publictypes.h>
#include <tesseract/resultiterator.h>

#include <QImage>

namespace textract {

TesseractEngine::TesseractEngine()
    : m_api(std::make_unique<tesseract::TessBaseAPI>())
{
}

TesseractEngine::~TesseractEngine()
{
    if (m_warm) {
        m_api->End();
    }
}

bool TesseractEngine::initialize(const QString &langs)
{
    if (m_warm && langs == m_langs) {
        return true;
    }
    if (m_warm) {
        m_api->End();
        m_warm = false;
    }

    // OEM_LSTM_ONLY: the neural recogniser, which is what the spec relies on
    // for doing its own adaptive thresholding.
    const int rc = m_api->Init(nullptr,
                               langs.toUtf8().constData(),
                               tesseract::OEM_LSTM_ONLY);
    if (rc != 0) {
        return false;
    }

    m_api->SetPageSegMode(tesseract::PSM_AUTO);
    m_langs = langs;
    m_warm = true;
    return true;
}

std::vector<Word> TesseractEngine::recognize(const QImage &image,
                                             const QString &langs)
{
    if (!initialize(langs)) {
        return {};
    }
    if (image.isNull()) {
        return {};
    }

    // Format_RGB888 guarantees a tightly packed 3-byte layout, which is what
    // SetImage's pointer overload expects.
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);

    m_api->SetImage(rgb.constBits(),
                    rgb.width(),
                    rgb.height(),
                    3,                        // bytes per pixel
                    int(rgb.bytesPerLine())); // bytes per line
    m_api->SetSourceResolution(96 * m_upscale);

    if (m_api->Recognize(nullptr) != 0) {
        return {};
    }

    std::unique_ptr<tesseract::ResultIterator> it(m_api->GetIterator());
    if (!it) {
        return {};
    }

    std::vector<Word> words;
    int block = 0;
    int line = 0;

    do {
        if (it->IsAtBeginningOf(tesseract::RIL_BLOCK)) {
            ++block;
        }
        if (it->IsAtBeginningOf(tesseract::RIL_TEXTLINE)) {
            ++line;
        }

        std::unique_ptr<char[]> raw(it->GetUTF8Text(tesseract::RIL_WORD));
        if (!raw) {
            continue;
        }
        const QString text = QString::fromUtf8(raw.get()).trimmed();
        if (text.isEmpty()) {
            continue;
        }

        int left = 0, top = 0, right = 0, bottom = 0;
        if (!it->BoundingBox(tesseract::RIL_WORD, &left, &top, &right, &bottom)) {
            continue;
        }

        Word word;
        word.text = text;
        // Scale boxes back into original crop coordinates. M2 does not upscale,
        // so the divisor is 1; M3's preprocessing will set it.
        word.bbox = QRect(QPoint(left / m_upscale, top / m_upscale),
                          QPoint(right / m_upscale, bottom / m_upscale));
        word.confidence = it->Confidence(tesseract::RIL_WORD) / 100.0f;
        word.line = line;
        word.block = block;
        words.push_back(word);
    } while (it->Next(tesseract::RIL_WORD));

    return words;
}

bool TesseractEngine::isWarm() const
{
    return m_warm;
}

} // namespace textract
