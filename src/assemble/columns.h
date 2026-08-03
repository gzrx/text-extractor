// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QRect>

#include "ocr/word.h"

namespace textract {

/// A maximal band of x that no word overlaps, in absolute crop coordinates.
struct Gap {
    int left;  ///< inclusive
    int right; ///< inclusive

    int width() const { return right - left + 1; }
    int centre() const { return left + width() / 2; }
};

/// The union of every word's box.
QRect contentRect(const std::vector<Word> &words);

/**
 * Vertical whitespace bands crossing the whole content height.
 *
 * Occupancy is accumulated over all words at once rather than line by line, so
 * a band that survives is one no word crosses at any height. That is what
 * makes it usable for both jobs that need it: the column boundary between two
 * blocks of prose, and the separator between table cells.
 */
std::vector<Gap> verticalGaps(const std::vector<Word> &words,
                              const QRect &content);

/// Median width of one character, in pixels. Exact on a monospaced face, and
/// still the best available ruler on a proportional one.
double characterWidth(const std::vector<Word> &words);

/**
 * The gaps wide enough to be column boundaries.
 *
 * One definition, used by both the classifier deciding a region is a table and
 * the assembler deciding where its cells are, so the two can never disagree
 * about where the columns fall.
 *
 * The width guard is deliberately far below one space, because width is not
 * what identifies a column — running the full height of the region is. The
 * corpus makes that concrete: the monospaced table separates its columns by 34
 * to 47px, but the spreadsheet manages only 3 to 12px against a 9px character,
 * because its headers are left-aligned while its numerics are right-aligned so
 * the two nearly touch. Both are real column boundaries and both are
 * unoccupied at every row. What the guard rejects is an antialiasing sliver.
 */
std::vector<Gap> columnGaps(const std::vector<Word> &words);

} // namespace textract
