// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocr/paddledet.h"

#include <algorithm>
#include <vector>

namespace textract {

namespace {

/// One connected component being accumulated.
struct Blob {
    QRect  bounds;
    double total{0.0}; ///< Sum of probabilities, for the mean.
    int    pixels{0};
};

} // namespace

std::vector<QRect> detectTextLines(const float *map, int width, int height,
                                   const DetectOptions &options)
{
    if (map == nullptr || width <= 0 || height <= 0) {
        return {};
    }

    const size_t count = size_t(width) * size_t(height);
    std::vector<int> label(count, -1);
    std::vector<Blob> blobs;
    std::vector<int> stack;

    auto isText = [&](int x, int y) {
        return map[size_t(y) * size_t(width) + size_t(x)] > options.thresh;
    };

    // Flood fill, 4-connected. Iterative: a text region can span the whole map
    // and recursion would risk the stack on a large capture.
    for (int y = 0; y < height && int(blobs.size()) < options.maxCandidates; ++y) {
        for (int x = 0; x < width && int(blobs.size()) < options.maxCandidates; ++x) {
            const size_t seed = size_t(y) * size_t(width) + size_t(x);
            if (label[seed] >= 0 || !isText(x, y)) {
                continue;
            }

            const int id = int(blobs.size());
            blobs.push_back({});
            Blob &blob = blobs.back();
            blob.bounds = QRect(x, y, 1, 1);

            stack.clear();
            stack.push_back(int(seed));
            label[seed] = id;

            while (!stack.empty()) {
                const int index = stack.back();
                stack.pop_back();
                const int cx = index % width;
                const int cy = index / width;

                blob.bounds = blob.bounds.united(QRect(cx, cy, 1, 1));
                blob.total += double(map[size_t(index)]);
                ++blob.pixels;

                const int neighbours[4][2] = {
                    {cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1}};
                for (const auto &neighbour : neighbours) {
                    const int nx = neighbour[0];
                    const int ny = neighbour[1];
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                        continue;
                    }
                    const size_t at = size_t(ny) * size_t(width) + size_t(nx);
                    if (label[at] < 0 && isText(nx, ny)) {
                        label[at] = id;
                        stack.push_back(int(at));
                    }
                }
            }
        }
    }

    std::vector<QRect> boxes;
    for (const Blob &blob : blobs) {
        if (blob.pixels <= 0) {
            continue;
        }
        if (std::min(blob.bounds.width(), blob.bounds.height()) < options.minSide) {
            continue;
        }
        if (blob.total / double(blob.pixels) < double(options.boxThresh)) {
            continue;
        }

        // DB shrinks the drawn region during training, so the predicted blob is
        // smaller than the glyphs. Dilate it back proportionally to its size.
        const double grow = (options.unclipRatio - 1.0) / 2.0;
        const int dx = int(double(blob.bounds.width()) * grow);
        const int dy = int(double(blob.bounds.height()) * grow);

        QRect box = blob.bounds.adjusted(-dx, -dy, dx, dy);
        box &= QRect(0, 0, width, height);
        if (!box.isEmpty()) {
            boxes.push_back(box);
        }
    }

    return boxes;
}

} // namespace textract
