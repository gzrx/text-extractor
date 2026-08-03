// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QRect>
#include <QString>

#include "ocr/word.h"

namespace textract {

/// One recognised text line, as a detector-plus-recogniser reports it: a
/// string, the box it came from, and one confidence for the whole line.
struct TextLine {
    QString text;
    QRect   bbox;
    float   confidence{0.0f};
};

/**
 * Splits recognised text lines into Words with manufactured geometry.
 *
 * PP-OCR reports no per-word boxes, but assemble/ needs them: the Code branch
 * divides x-offsets by the character cell to rebuild indentation, and the
 * Table branch splits cells on the gaps between word boxes -- a whole-line box
 * would yield no gaps at all and collapse every table to a single cell.
 *
 * Each word gets a slice of its line's box proportional to its character
 * count. Exact on a monospaced face, which is what terminals, code and tables
 * are; approximate on a proportional one, where the Prose branch reflows lines
 * into paragraphs and word x-positions barely matter.
 *
 * `line` is set to the index of the source TextLine and `block` left at 0;
 * orderWords() replaces both.
 */
std::vector<Word> wordsFromLines(const std::vector<TextLine> &lines);

} // namespace textract
