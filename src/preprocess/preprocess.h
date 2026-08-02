// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>

namespace textract {

/// Knobs for preprocess(). Defaults are the ones the spec commits to.
struct PreprocessOptions {
    /// Upscale factor, clamped to [2, 4]. See effectiveUpscale().
    ///
    /// The spec calls for Lanczos. Leptonica 1.87.0 ships no Lanczos kernel —
    /// its whole scaling family is sampling, linear interpolation, area map and
    /// smoothing — so upscaling uses pixScaleGrayLI(). Adding OpenCV for one
    /// resample was already rejected as a ~100MB dependency. If the corpus ever
    /// shows the interpolator is the limiting factor, that is the moment to
    /// revisit, with evidence.
    int upscale{3};

    /// Otsu binarisation. Deliberately off: screen glyphs are subpixel
    /// antialiased and often light-on-dark, and Tesseract 5's LSTM engine
    /// already does its own adaptive thresholding. Kept for low-contrast
    /// edge cases only.
    bool binarize{false};
};

/// Lowest and highest upscale factors preprocess() will apply.
inline constexpr int kMinUpscale = 2;
inline constexpr int kMaxUpscale = 4;

/// The factor preprocess() will actually apply for `options`, after clamping.
///
/// Callers MUST pass this — not `options.upscale` — to
/// OcrEngine::setUpscaleFactor(), or bounding boxes come back in the wrong
/// coordinate space whenever the requested factor was out of range.
int effectiveUpscale(const PreprocessOptions &options);

/// Conditions a crop for OCR. Pure.
QImage preprocess(const QImage &crop, const PreprocessOptions &options);

} // namespace textract
