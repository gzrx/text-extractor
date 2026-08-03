// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
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

    /// The layout used, whether classified or forced by the caller.
    LayoutKind kind{LayoutKind::Raw};

    /// classify()'s confidence, or 1.0 when the caller forced the kind.
    float layoutConfidence{0.0f};

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
 *
 * `forcedKind` overrides classification: the daemon leaves it unset, the
 * Shift-held override pins it to Raw, and the fixture harness pins it to each
 * fixture's declared layout so that assembly is scored without classifier
 * noise. classify() is measured separately, against the same corpus.
 */
Extraction extractText(OcrEngine &engine,
                       const QImage &crop,
                       const QString &langs,
                       std::optional<LayoutKind> forcedKind,
                       const PreprocessOptions &options);

} // namespace textract
