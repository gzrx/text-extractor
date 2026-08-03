// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "order/order.h"

#include <algorithm>
#include <map>

#include "assemble/columns.h"

namespace textract {

namespace {

/// Two detection boxes belong to the same screen line when they overlap
/// vertically by more than this fraction of the SHORTER box's height.
///
/// Measured against the shorter box on purpose: a short fragment -- a stray
/// period, a "[ 50%]" prefix -- has to be able to merge into a tall neighbour.
/// Against the taller box the same pair would never reach any sane threshold
/// and the fragment would be stranded on a line of its own, which is exactly
/// the interleaving that costs dark-terminal-buildlog its score.
constexpr double kLineOverlapRatio = 0.4;

/// A vertical gap larger than this many median line pitches starts a new block.
///
/// Set from the corpus. Sweep it separately from kLineOverlapRatio: merging
/// changes the line pitch this measures against, so tuning both at once
/// measures neither.
constexpr double kBlockGapRatio = 1.6;

/// One visual line: the words on it and the union of their boxes.
struct Line {
    QRect               bounds;
    std::vector<Word *> words;

    int top() const { return bounds.top(); }
};

/// Groups by the incoming detection id, preserving nothing about order.
std::vector<Line> groupBySourceLine(std::vector<Word> &words)
{
    std::map<int, Line> byId;
    for (Word &word : words) {
        Line &line = byId[word.line];
        line.bounds = line.bounds.isNull() ? word.bbox
                                           : line.bounds.united(word.bbox);
        line.words.push_back(&word);
    }

    std::vector<Line> lines;
    lines.reserve(byId.size());
    for (auto &[id, line] : byId) {
        lines.push_back(std::move(line));
    }
    return lines;
}

bool sharesAScreenLine(const QRect &a, const QRect &b)
{
    const int overlap = std::min(a.bottom(), b.bottom())
                        - std::max(a.top(), b.top()) + 1;
    if (overlap <= 0) {
        return false;
    }
    const int shorter = std::min(a.height(), b.height());
    return shorter > 0 && double(overlap) > kLineOverlapRatio * double(shorter);
}

/// Merges lines that share a screen line. Lines arrive sorted by top, so a
/// single forward pass against the accumulating bounds is enough.
std::vector<Line> mergeSharedLines(std::vector<Line> lines)
{
    if (lines.empty()) {
        return lines;
    }

    std::vector<Line> merged;
    merged.push_back(std::move(lines.front()));
    for (size_t i = 1; i < lines.size(); ++i) {
        Line &current = lines[i];
        Line &last = merged.back();
        if (sharesAScreenLine(last.bounds, current.bounds)) {
            last.bounds = last.bounds.united(current.bounds);
            last.words.insert(last.words.end(), current.words.begin(),
                              current.words.end());
        } else {
            merged.push_back(std::move(current));
        }
    }
    return merged;
}

/// The typical line-to-line step, as a median of the gaps between successive
/// line tops. A median rather than a mean because one paragraph break would
/// drag a mean up and then mask every other break.
double medianLinePitch(const std::vector<Line> &lines)
{
    if (lines.size() < 2) {
        return 0.0;
    }
    std::vector<double> pitches;
    pitches.reserve(lines.size() - 1);
    for (size_t i = 1; i < lines.size(); ++i) {
        pitches.push_back(double(lines[i].top() - lines[i - 1].top()));
    }
    const size_t middle = pitches.size() / 2;
    std::nth_element(pitches.begin(), pitches.begin() + long(middle),
                     pitches.end());
    return pitches[middle];
}

/// Words of every line in `lines`, flattened, for the column geometry helpers
/// that take a word vector.
std::vector<Word> wordsOf(const std::vector<Line> &lines)
{
    std::vector<Word> out;
    for (const Line &line : lines) {
        for (const Word *word : line.words) {
            out.push_back(*word);
        }
    }
    return out;
}

/// Splits `lines` into two columns at a single prose gutter, or returns one
/// column when there is no such gutter.
///
/// Exactly one qualifying gap is required. Two or more is a table, which
/// classify() would not have called Prose, and zero is ordinary single-column
/// prose. Both fall through to one column.
std::vector<std::vector<Line>> splitIntoColumns(std::vector<Line> lines)
{
    const std::vector<Gap> gaps = columnGaps(wordsOf(lines));
    if (gaps.size() != 1) {
        return {std::move(lines)};
    }

    const int split = gaps.front().centre();
    std::vector<Line> left;
    std::vector<Line> right;
    for (Line &line : lines) {
        if (line.bounds.center().x() < split) {
            left.push_back(std::move(line));
        } else {
            right.push_back(std::move(line));
        }
    }
    if (left.empty() || right.empty()) {
        std::vector<Line> all;
        for (Line &line : left) {
            all.push_back(std::move(line));
        }
        for (Line &line : right) {
            all.push_back(std::move(line));
        }
        return {std::move(all)};
    }
    return {std::move(left), std::move(right)};
}

} // namespace

void orderWords(std::vector<Word> &words, LayoutKind kind)
{
    if (words.empty()) {
        return;
    }

    std::vector<Line> lines = groupBySourceLine(words);

    // Split BEFORE merging. Lines in the left and right columns of a
    // two-column page share a y range, so merging first would fuse them across
    // the gutter and leave nothing to split.
    //
    // Only Prose may be genuinely multi-column. Table, Code and Raw are all
    // single-origin: a table's inter-cell gap runs the full height exactly like
    // a prose gutter, and splitting on it reads the table column-major.
    std::vector<std::vector<Line>> columns =
        kind == LayoutKind::Prose ? splitIntoColumns(std::move(lines))
                                  : std::vector<std::vector<Line>>{std::move(lines)};

    std::vector<Word> ordered;
    ordered.reserve(words.size());
    int lineIndex = 0;
    int blockIndex = 0;

    for (std::vector<Line> &column : columns) {
        std::sort(column.begin(), column.end(),
                  [](const Line &a, const Line &b) { return a.top() < b.top(); });
        column = mergeSharedLines(std::move(column));

        const double pitch = medianLinePitch(column);
        for (size_t i = 0; i < column.size(); ++i) {
            Line &line = column[i];
            if (i > 0 && pitch > 0.0) {
                const double gap = double(line.top() - column[i - 1].top());
                if (gap > kBlockGapRatio * pitch) {
                    ++blockIndex;
                }
            }

            std::sort(line.words.begin(), line.words.end(),
                      [](const Word *a, const Word *b) {
                          return a->bbox.left() < b->bbox.left();
                      });
            for (Word *word : line.words) {
                Word copy = *word;
                copy.line = lineIndex;
                copy.block = blockIndex;
                ordered.push_back(std::move(copy));
            }
            ++lineIndex;
        }
        // A column boundary is always a block boundary: the last paragraph of
        // one column and the first of the next are not the same paragraph.
        ++blockIndex;
    }

    words = std::move(ordered);
}

} // namespace textract
