// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "order/order.h"

#include <algorithm>
#include <map>
#include <optional>

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

/// At most this share of lines may cross a band for it to still be a gutter.
///
/// Not zero, because real two-column pages put a full-width heading or intro
/// paragraph above their columns and a footer below, and a gutter defined as
/// "unoccupied at every height" is erased by a single one of those.
constexpr double kGutterCrossingShare = 0.2;

/// Each side of a gutter must hold at least this share of the lines.
///
/// This is what stops single-column prose finding a false gutter near its
/// right margin, where the short last line of each paragraph leaves a band
/// that few lines cross but almost nothing sits to the right of.
constexpr double kGutterSideShare = 0.25;

/// The x centre of the page's one gutter, or nothing.
std::optional<int> findGutter(const std::vector<Line> &lines, const QRect &content)
{
    if (lines.size() < 4 || content.width() <= 0) {
        return std::nullopt;
    }

    std::vector<int> crossings(size_t(content.width()), 0);
    for (const Line &line : lines) {
        const int from = std::max(0, line.bounds.left() - content.left());
        const int to = std::min(content.width() - 1,
                                line.bounds.right() - content.left());
        for (int x = from; x <= to; ++x) {
            ++crossings[size_t(x)];
        }
    }

    const int maxCrossings = int(kGutterCrossingShare * double(lines.size()));
    const int minSide = std::max(1, int(kGutterSideShare * double(lines.size())));
    const double cell = characterWidth(wordsOf(lines));
    const int minWidth = std::max(3, int(qRound(0.5 * cell)));

    std::vector<Gap> candidates;
    int start = -1;
    for (int x = 0; x < content.width(); ++x) {
        if (crossings[size_t(x)] <= maxCrossings) {
            if (start < 0) {
                start = x;
            }
        } else if (start >= 0) {
            candidates.push_back({content.left() + start, content.left() + x - 1});
            start = -1;
        }
    }
    if (start >= 0) {
        candidates.push_back({content.left() + start,
                              content.left() + content.width() - 1});
    }

    std::optional<int> found;
    for (const Gap &gap : candidates) {
        if (gap.width() < minWidth) {
            continue;
        }
        int left = 0;
        int right = 0;
        for (const Line &line : lines) {
            if (line.bounds.right() < gap.left) {
                ++left;
            } else if (line.bounds.left() > gap.right) {
                ++right;
            }
        }
        if (left < minSide || right < minSide) {
            continue;
        }
        if (found) {
            return std::nullopt; // Two gutters is a table, not a column break.
        }
        found = gap.centre();
    }
    return found;
}

/// Splits `lines` into column runs, in reading order.
///
/// Measured on pdf-two-column, a full-width intro paragraph above two columns:
/// requiring a band unoccupied at EVERY height found no gutter at all and the
/// fixture scored 0.3083 with its columns interleaved line by line, against
/// tier 1's 0.9975.
///
/// So the gutter is found with a tolerance for the few lines that cross it,
/// and those crossers then cut the page into regions. Regions come out in y
/// order and a split region emits left before right, which is reading order:
/// intro, then the left column, then the right, then the footer.
std::vector<std::vector<Line>> splitIntoColumns(std::vector<Line> lines)
{
    QRect content;
    for (const Line &line : lines) {
        content = content.isNull() ? line.bounds : content.united(line.bounds);
    }

    const std::optional<int> gutter = findGutter(lines, content);
    if (!gutter) {
        return {std::move(lines)};
    }

    std::sort(lines.begin(), lines.end(),
              [](const Line &a, const Line &b) { return a.top() < b.top(); });

    std::vector<std::vector<Line>> columns;
    std::vector<Line> band;

    auto flushBand = [&columns, &band, &gutter]() {
        if (band.empty()) {
            return;
        }
        std::vector<Line> left;
        std::vector<Line> right;
        for (Line &line : band) {
            (line.bounds.center().x() < *gutter ? left : right)
                .push_back(std::move(line));
        }
        band.clear();
        if (!left.empty()) {
            columns.push_back(std::move(left));
        }
        if (!right.empty()) {
            columns.push_back(std::move(right));
        }
    };

    for (Line &line : lines) {
        if (line.bounds.left() < *gutter && line.bounds.right() > *gutter) {
            // Crosses the gutter, so it belongs to neither column and ends the
            // band above it.
            flushBand();
            std::vector<Line> spanning;
            spanning.push_back(std::move(line));
            columns.push_back(std::move(spanning));
        } else {
            band.push_back(std::move(line));
        }
    }
    flushBand();

    return columns;
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
