// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QImage>
#include <QString>

#include "assemble/assemble.h"
#include "ocr/ocrengine.h"
#include "ocr/word.h"
#include "preprocess/preprocess.h"

namespace textract {

/// The outcome of running one crop through the pipeline.
struct Extraction {
    QString           text;
    std::vector<Word> words;
    float             meanConfidence{0.0f};

    bool isEmpty() const { return words.empty(); }
};

/**
 * Runs one crop through preprocess -> recognise -> assemble.
 *
 * This is the whole pipeline between capture and clipboard, factored out so
 * that the daemon and the fixture harness exercise exactly the same code. A
 * harness that scored a different path would measure nothing useful.
 *
 * `crop` is in original capture pixels and every returned Word::bbox is in
 * those same coordinates, whatever upscale factor was applied internally.
 */
Extraction extractText(OcrEngine &engine,
                       const QImage &crop,
                       const QString &langs,
                       LayoutKind kind,
                       const PreprocessOptions &options);

} // namespace textract
