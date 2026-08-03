// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QString>

#include "correct/dictionary.h"
#include "ocr/word.h"

namespace textract {

/**
 * How a recognised region should be turned back into text.
 * M2 implements Raw only; Code, Prose, and Table arrive with M4.
 */
enum class LayoutKind {
    Raw,   ///< Reading order, Tesseract's own line breaks, no correction.
    Code,  ///< Preserve indentation and line breaks exactly.
    Prose, ///< Join wrapped lines, re-glue hyphenated words.
    Table, ///< Cluster columns, emit Markdown or TSV.
};

/**
 * Converts recognised words into text according to `kind`.
 *
 * `dictionary` is consulted by the `Prose` branch alone, and only to decide
 * whether a hyphen at a line end was typesetting or content. That decision has
 * to be made here rather than in a later pass: once the two halves are glued
 * the evidence that a hyphen was ever there is gone. Passing nullptr, or a
 * dictionary with no langdata behind it, keeps the unaided behaviour of always
 * regluing.
 */
QString assemble(const std::vector<Word> &words, LayoutKind kind,
                 const Dictionary *dictionary = nullptr);

} // namespace textract
