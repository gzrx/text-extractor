// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "analyze/analyze.h"

#include <algorithm>
#include <optional>

#include <QRect>

#include "assemble/columns.h"

namespace textract {

namespace {

/// Below this a whitespace gutter proves nothing: a couple of short lines
/// with a gap between them is a label beside a value, not two columns.
///
/// It gates only the column test. The punctuation ratio needs no such floor —
/// it is stable on a short snippet, and on input this small Code and Raw
/// assemble to the same thing anyway.
constexpr size_t kMinWordsForColumns = 40;

/// A gutter narrower than this fraction of the content width is inter-word
/// spacing or a wide indent, not a column boundary.
constexpr double kMinGutterFraction = 0.04;

/// Each side of a real column boundary carries a real share of the text. This
/// rejects margin notes, line numbers and gutter-adjacent stray marks.
constexpr double kMinSideFraction = 0.2;

/// Structural punctuation: what brackets and terminates statements. Not every
/// mark — a comma or a full stop is as much prose as code.
constexpr QLatin1StringView kCodePunctuation("{}();");

/**
 * Fraction of structural punctuation above which text reads as source.
 *
 * Set from the corpus, which separates cleanly: 0.068 for the code fixture and
 * 0.006 for the next highest, a build log. This threshold sits about three and
 * a half times above one and below the other.
 *
 * The spec proposed glyph advance-width variance instead, on the theory that
 * monospace implies code. It does not discriminate here: a terminal build log,
 * a monospaced table and a source listing are all monospace, and two of the
 * three are not code. Punctuation is what actually separates them.
 */
constexpr double kCodePunctuationRatio = 0.02;

/**
 * Column gaps needed before a region reads as a table.
 *
 * Two, because one is a column boundary and none is a paragraph. The corpus
 * separates cleanly at that line: both table fixtures show four full-height
 * gaps and nothing else shows more than one.
 */
constexpr size_t kMinTableGaps = 2;

/// Rows needed before "full height" means anything. Any two short lines leave
/// bands that no word happens to cross.
constexpr size_t kMinTableRows = 3;

/// Distinct text lines in the word stream.
size_t countLines(const std::vector<Word> &words)
{
    size_t lines = 0;
    int previousLine = -1;
    int previousBlock = -1;
    for (const Word &word : words) {
        if (word.line != previousLine || word.block != previousBlock) {
            ++lines;
        }
        previousLine = word.line;
        previousBlock = word.block;
    }
    return lines;
}

/// Several full-height gaps over enough rows: cells, not paragraphs.
std::optional<LayoutClass> detectTable(const std::vector<Word> &words)
{
    if (countLines(words) < kMinTableRows) {
        return std::nullopt;
    }

    const size_t gaps = columnGaps(words).size();
    if (gaps < kMinTableGaps) {
        return std::nullopt;
    }

    LayoutClass result;
    result.kind = LayoutKind::Table;
    // Margin over the threshold, saturating at twice it.
    result.confidence = float(std::min(1.0, double(gaps)
                                                / (2.0 * double(kMinTableGaps))));
    return result;
}

/// Share of all recognised characters that is structural punctuation.
double codePunctuationRatio(const std::vector<Word> &words)
{
    qsizetype total = 0;
    qsizetype structural = 0;
    for (const Word &word : words) {
        total += word.text.size();
        for (const QChar character : word.text) {
            if (kCodePunctuation.contains(character)) {
                ++structural;
            }
        }
    }
    return total > 0 ? double(structural) / double(total) : 0.0;
}

/**
 * A single wide full-height gutter with real text on both sides.
 *
 * Returns nothing when the evidence is not there, which is the common case:
 * most of a desktop is one column.
 */
std::optional<LayoutClass> detectTwoColumns(const std::vector<Word> &words)
{
    if (words.size() < kMinWordsForColumns) {
        return std::nullopt;
    }

    const QRect content = contentRect(words);
    if (content.width() <= 0) {
        return std::nullopt;
    }

    const int minGutter = std::max(1, int(content.width() * kMinGutterFraction));

    std::vector<Gap> gutters;
    for (const Gap &gap : verticalGaps(words, content)) {
        if (gap.width() >= minGutter) {
            gutters.push_back(gap);
        }
    }

    // Exactly one: none means a single column, and several mean cells. A table
    // has full-height gutters too, and reading one as a set of columns emits it
    // column by column, which is the defect this whole module exists to avoid.
    if (gutters.size() != 1) {
        return std::nullopt;
    }

    const int boundaryLeft = gutters.front().left;
    const int boundaryRight = gutters.front().right;

    size_t before = 0;
    size_t after = 0;
    for (const Word &word : words) {
        if (word.bbox.right() < boundaryLeft) {
            ++before;
        } else if (word.bbox.left() > boundaryRight) {
            ++after;
        }
    }

    const size_t lighter = std::min(before, after);
    if (double(lighter) < kMinSideFraction * double(words.size())) {
        return std::nullopt;
    }

    LayoutClass result;
    result.kind = LayoutKind::Prose;
    // How evenly the two columns are filled. The thresholds above already
    // decided; this reports the margin by which they were cleared.
    result.confidence = float(2.0 * double(lighter) / double(before + after));
    return result;
}

} // namespace

LayoutClass classify(const std::vector<Word> &words, const QImage &image)
{
    Q_UNUSED(image)

    if (words.empty()) {
        return {};
    }

    // Multi-column is tested first because it is the only decision that
    // changes page segmentation, and segmentation decides reading order.
    // Getting it wrong costs more than any assembly choice can.
    if (const auto columns = detectTwoColumns(words)) {
        return *columns;
    }

    // Mutually exclusive with the column test by construction: that one wants
    // exactly one gap, this one wants several.
    if (const auto table = detectTable(words)) {
        return *table;
    }

    if (const double ratio = codePunctuationRatio(words);
        ratio >= kCodePunctuationRatio) {
        LayoutClass result;
        result.kind = LayoutKind::Code;
        result.confidence = float(std::min(1.0, ratio
                                                    / (2.0 * kCodePunctuationRatio)));
        return result;
    }

    // Nothing carried enough evidence. Raw is never the best rendering of
    // anything and never the wrong one either.
    return {};
}

Segmentation segmentationFor(LayoutKind kind)
{
    switch (kind) {
    case LayoutKind::Prose:
        return Segmentation::Auto;
    case LayoutKind::Raw:
    case LayoutKind::Code:
    case LayoutKind::Table:
        return Segmentation::SingleBlock;
    }
    return Segmentation::SingleBlock;
}

} // namespace textract
