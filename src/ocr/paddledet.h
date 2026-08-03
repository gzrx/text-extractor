// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QRect>

namespace textract {

/**
 * DB post-processing thresholds.
 *
 * Defaults are PP-OCRv6_small's, from the inference.yml the model ships with.
 * They differ from PP-OCRv5's (0.3 / 0.6 / 1.5), so read them from the model
 * rather than trusting a value found in a tutorial.
 */
struct DetectOptions {
    float  thresh{0.2f};       ///< Probability above which a pixel is text.
    float  boxThresh{0.45f};   ///< Mean probability a whole blob must reach.
    double unclipRatio{1.4};   ///< Outward dilation, as a fraction of size.
    int    maxCandidates{3000};
    int    minSide{3};         ///< Blobs thinner than this are antialiasing.
};

/**
 * Text-line boxes from a DB probability map.
 *
 * `map` is row-major, `width` by `height`, values in [0, 1]. Returned boxes are
 * in map coordinates and clamped to the map; the caller rescales them to image
 * coordinates.
 *
 * Boxes are axis-aligned. The reference implementation fits rotated rectangles,
 * but screen captures are never rotated and Word::bbox is a QRect, so an
 * oriented box would be flattened on the next line anyway.
 */
std::vector<QRect> detectTextLines(const float *map, int width, int height,
                                   const DetectOptions &options);

} // namespace textract
