// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "analyze/analyze.h"

#include <algorithm>
#include <optional>

#include <QRect>

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

struct Run {
    int left;  ///< inclusive, in content-relative pixels
    int right; ///< inclusive

    int width() const { return right - left + 1; }
};

QRect contentRect(const std::vector<Word> &words)
{
    QRect content;
    for (const Word &word : words) {
        content = content.united(word.bbox);
    }
    return content;
}

/**
 * Maximal runs of x with no word over them.
 *
 * Column occupancy rather than a per-line scan on purpose: a run that survives
 * here is one that *no* word crosses at *any* height, which is what "full
 * height gutter" means. Testing lines individually would need a separate
 * height check and would be fooled by a short paragraph.
 */
std::vector<Run> whitespaceRuns(const std::vector<Word> &words,
                                const QRect &content)
{
    std::vector<bool> occupied(size_t(content.width()), false);
    for (const Word &word : words) {
        const int from = std::max(0, word.bbox.left() - content.left());
        const int to = std::min(content.width() - 1,
                                word.bbox.right() - content.left());
        for (int x = from; x <= to; ++x) {
            occupied[size_t(x)] = true;
        }
    }

    std::vector<Run> runs;
    int start = -1;
    for (int x = 0; x < content.width(); ++x) {
        if (!occupied[size_t(x)]) {
            if (start < 0) {
                start = x;
            }
        } else if (start >= 0) {
            runs.push_back({start, x - 1});
            start = -1;
        }
    }
    // A trailing run cannot exist: content is the union of the boxes, so its
    // last column is occupied by definition.

    return runs;
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

    std::vector<Run> gutters;
    for (const Run &run : whitespaceRuns(words, content)) {
        if (run.width() >= minGutter) {
            gutters.push_back(run);
        }
    }

    // Exactly one: none means a single column, and several mean cells. A table
    // has full-height gutters too, and reading one as a set of columns emits it
    // column by column, which is the defect this whole module exists to avoid.
    if (gutters.size() != 1) {
        return std::nullopt;
    }

    const int boundaryLeft = content.left() + gutters.front().left;
    const int boundaryRight = content.left() + gutters.front().right;

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
