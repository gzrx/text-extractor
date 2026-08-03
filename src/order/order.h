// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "assemble/assemble.h"
#include "ocr/word.h"

namespace textract {

/**
 * Assigns final line and block indices and sorts `words` into reading order.
 *
 * For engines that return text-line boxes with no ordering of their own. A
 * detector-plus-recogniser reports where text is, not what order to read it
 * in, but groupIntoLines() in assemble/ splits on `line` and `block` changing
 * and trusts the vector's order within a line.
 *
 * MUST run after classify(): column-versus-table cannot be settled from
 * geometry alone, because a prose gutter and a table's inter-cell gap are both
 * unoccupied at every row. Splitting on the wrong one reads a table
 * column-major -- measured, and it dropped spreadsheet-table from 1.0000 to
 * 0.4348. classify() has already drawn that distinction, so this consumes the
 * answer rather than guessing it again.
 *
 * Input:  `line` is a stable per-detection-box id, `block` unused.
 * Output: sorted; `line` renumbered 0..n-1 in reading order; `block` assigned.
 */
void orderWords(std::vector<Word> &words, LayoutKind kind);

} // namespace textract
