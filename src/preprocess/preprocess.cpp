// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "preprocess/preprocess.h"

#include <algorithm>
#include <memory>

#include <leptonica/allheaders.h>

namespace textract {

namespace {

struct PixDeleter {
    void operator()(PIX *pix) const { pixDestroy(&pix); }
};
using PixPtr = std::unique_ptr<PIX, PixDeleter>;

/// Copies an 8-bit grayscale QImage into a fresh 8bpp Pix.
///
/// Leptonica packs four 8-bit samples into each 32-bit word in a fixed byte
/// order, so the rows cannot be memcpy'd; SET_DATA_BYTE hides the packing.
PixPtr toGrayPix(const QImage &image)
{
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    PixPtr pix(pixCreate(gray.width(), gray.height(), 8));
    if (!pix) {
        return nullptr;
    }

    l_uint32 *data = pixGetData(pix.get());
    const int wpl = pixGetWpl(pix.get());

    for (int y = 0; y < gray.height(); ++y) {
        const uchar *src = gray.constScanLine(y);
        l_uint32 *line = data + y * wpl;
        for (int x = 0; x < gray.width(); ++x) {
            SET_DATA_BYTE(line, x, src[x]);
        }
    }
    return pix;
}

/// Copies an 8bpp Pix back into a grayscale QImage.
QImage fromGrayPix(PIX *pix)
{
    const int width = pixGetWidth(pix);
    const int height = pixGetHeight(pix);

    QImage image(width, height, QImage::Format_Grayscale8);
    const l_uint32 *data = pixGetData(pix);
    const int wpl = pixGetWpl(pix);

    for (int y = 0; y < height; ++y) {
        uchar *dst = image.scanLine(y);
        const l_uint32 *line = data + y * wpl;
        for (int x = 0; x < width; ++x) {
            dst[x] = uchar(GET_DATA_BYTE(line, x));
        }
    }
    return image;
}

/// True when the crop is light text on a dark background — a dark-mode
/// terminal or editor. Text occupies far less area than its background, so the
/// mean sample value tracks the background.
bool hasDarkBackground(PIX *pix)
{
    l_float32 mean = 0.0f;
    if (pixGetAverageMasked(pix, nullptr, 0, 0, 1, L_MEAN_ABSVAL, &mean) != 0) {
        return false; // unmeasurable: leave the crop as it is
    }
    return mean < 128.0f;
}

/// Global Otsu threshold, returned as an 8bpp black-on-white image so the rest
/// of the pipeline keeps seeing one depth. Passing the full image extent as the
/// tile size makes the threshold global rather than adaptive.
PixPtr otsuBinarize(PIX *pix)
{
    PIX *binary = nullptr;
    const l_ok rc = pixOtsuAdaptiveThreshold(pix,
                                             pixGetWidth(pix),
                                             pixGetHeight(pix),
                                             0, 0, // no tile smoothing
                                             0.0f, // no score-based bias
                                             nullptr,
                                             &binary);
    PixPtr owned(binary);
    if (rc != 0 || !owned) {
        return nullptr;
    }
    // 1bpp Leptonica convention: a set bit is foreground, so it becomes black.
    return PixPtr(pixConvert1To8(nullptr, owned.get(), 255, 0));
}

} // namespace

int effectiveUpscale(const PreprocessOptions &options)
{
    return std::clamp(options.upscale, kMinUpscale, kMaxUpscale);
}

QImage preprocess(const QImage &crop, const PreprocessOptions &options)
{
    if (crop.isNull()) {
        return QImage();
    }

    // The spec states the order as polarity, upscale, grayscale. Grayscale is
    // hoisted to the front here: luminance conversion, inversion and linear
    // interpolation are all linear in the samples, so they commute, and doing
    // it first means the interpolation runs over one channel instead of three.
    PixPtr pix = toGrayPix(crop);
    if (!pix) {
        return QImage();
    }

    if (hasDarkBackground(pix.get())) {
        pixInvert(pix.get(), pix.get());
    }

    const float factor = float(effectiveUpscale(options));
    PixPtr scaled(pixScaleGrayLI(pix.get(), factor, factor));
    if (!scaled) {
        return QImage();
    }

    if (options.binarize) {
        PixPtr binary = otsuBinarize(scaled.get());
        if (binary) {
            scaled = std::move(binary);
        }
        // A failed threshold falls through to the grayscale result rather than
        // failing the extraction outright.
    }

    return fromGrayPix(scaled.get());
}

} // namespace textract
