// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "assemble/columns.h"

#include <algorithm>

namespace textract {

QRect contentRect(const std::vector<Word> &words)
{
    QRect content;
    for (const Word &word : words) {
        content = content.united(word.bbox);
    }
    return content;
}

std::vector<Gap> verticalGaps(const std::vector<Word> &words,
                              const QRect &content)
{
    if (content.width() <= 0) {
        return {};
    }

    std::vector<bool> occupied(size_t(content.width()), false);
    for (const Word &word : words) {
        const int from = std::max(0, word.bbox.left() - content.left());
        const int to = std::min(content.width() - 1,
                                word.bbox.right() - content.left());
        for (int x = from; x <= to; ++x) {
            occupied[size_t(x)] = true;
        }
    }

    std::vector<Gap> gaps;
    int start = -1;
    for (int x = 0; x < content.width(); ++x) {
        if (!occupied[size_t(x)]) {
            if (start < 0) {
                start = x;
            }
        } else if (start >= 0) {
            gaps.push_back({content.left() + start, content.left() + x - 1});
            start = -1;
        }
    }
    // A trailing gap cannot exist: content is the union of the boxes, so its
    // last column is occupied by definition.

    return gaps;
}

double characterWidth(const std::vector<Word> &words);

namespace {

/// Narrowest usable column gap, in character cells. See columnGaps().
constexpr double kMinColumnGap = 0.25;

} // namespace

std::vector<Gap> columnGaps(const std::vector<Word> &words)
{
    const double cell = characterWidth(words);
    const int minimum = std::max(2, int(qRound(kMinColumnGap * cell)));

    std::vector<Gap> columns;
    for (const Gap &gap : verticalGaps(words, contentRect(words))) {
        if (gap.width() >= minimum) {
            columns.push_back(gap);
        }
    }
    return columns;
}

double characterWidth(const std::vector<Word> &words)
{
    std::vector<double> advances;
    for (const Word &word : words) {
        if (!word.text.isEmpty() && word.bbox.width() > 0) {
            advances.push_back(double(word.bbox.width())
                               / double(word.text.size()));
        }
    }
    if (advances.empty()) {
        return 0.0;
    }

    const size_t middle = advances.size() / 2;
    std::nth_element(advances.begin(), advances.begin() + long(middle),
                     advances.end());
    return advances[middle];
}

} // namespace textract
