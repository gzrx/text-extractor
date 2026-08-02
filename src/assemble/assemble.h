#pragma once

#include <vector>

#include <QString>

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

/// Converts recognised words into text according to `kind`.
QString assemble(const std::vector<Word> &words, LayoutKind kind);

} // namespace textract
