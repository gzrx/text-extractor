// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "assemble/assemble.h"

#include <algorithm>

#include <QStringList>

#include "assemble/columns.h"

namespace textract {

namespace {

bool isCjk(QChar character);

/**
 * Appends `text` to `line` with the separator the scripts call for.
 *
 * Latin words take a space. A join inside a run of CJK takes none: Tesseract
 * emits Chinese one character per word, so spacing them the way Latin words
 * are spaced inserts a character between every glyph that was on screen.
 */
void appendWord(QString &line, const QString &text)
{
    if (line.isEmpty() || text.isEmpty()) {
        line += text;
        return;
    }
    if (!(isCjk(line.back()) && isCjk(text.front()))) {
        line += QLatin1Char(' ');
    }
    line += text;
}

/// The words of one recognised text line, in the order the engine emitted them.
struct Line {
    int                       block{0};
    std::vector<const Word *> words;

    QString joined() const
    {
        QString text;
        for (const Word *word : words) {
            appendWord(text, word->text);
        }
        return text;
    }
};

/// Splits the word stream on the engine's own line and block numbering. Every
/// branch works from this rather than from raw word order.
std::vector<Line> groupIntoLines(const std::vector<Word> &words)
{
    std::vector<Line> lines;
    int previousLine = -1;
    int previousBlock = -1;

    for (const Word &word : words) {
        if (lines.empty() || word.line != previousLine
            || word.block != previousBlock) {
            lines.push_back(Line{word.block, {}});
        }
        lines.back().words.push_back(&word);
        previousLine = word.line;
        previousBlock = word.block;
    }

    return lines;
}

/**
 * True for Han, kana, Hangul and the full-width and CJK punctuation blocks.
 *
 * These scripts do not separate words with spaces, so the space that joins two
 * wrapped Latin lines would be a character that was never on screen.
 */
bool isCjk(QChar character)
{
    const char32_t code = character.unicode();
    return (code >= 0x1100 && code <= 0x11FF)   // Hangul Jamo
        || (code >= 0x2E80 && code <= 0x2EFF)   // CJK radicals
        || (code >= 0x3000 && code <= 0x303F)   // CJK symbols and punctuation
        || (code >= 0x3040 && code <= 0x30FF)   // kana
        || (code >= 0x3130 && code <= 0x318F)   // Hangul compatibility jamo
        || (code >= 0x3400 && code <= 0x4DBF)   // CJK extension A
        || (code >= 0x4E00 && code <= 0x9FFF)   // CJK unified ideographs
        || (code >= 0xAC00 && code <= 0xD7AF)   // Hangul syllables
        || (code >= 0xF900 && code <= 0xFAFF)   // compatibility ideographs
        || (code >= 0xFF00 && code <= 0xFFEF);  // half and full width forms
}

/// Reading order with the engine's own line breaks. The escape hatch: never
/// the best rendering of anything, never wrong about anything either.
QString assembleRaw(const std::vector<Word> &words)
{
    QString out;
    int previousBlock = -1;

    for (const Line &line : groupIntoLines(words)) {
        if (!out.isEmpty()) {
            out += line.block != previousBlock ? QStringLiteral("\n\n")
                                               : QStringLiteral("\n");
        }
        out += line.joined();
        previousBlock = line.block;
    }

    return out;
}

int lineTop(const Line &line)
{
    int top = line.words.front()->bbox.top();
    for (const Word *word : line.words) {
        top = std::min(top, word->bbox.top());
    }
    return top;
}

/// Median vertical distance between consecutive lines.
double linePitch(const std::vector<Line> &lines)
{
    std::vector<double> gaps;
    for (size_t i = 1; i < lines.size(); ++i) {
        const double gap = lineTop(lines[i]) - lineTop(lines[i - 1]);
        if (gap > 0.0) {
            gaps.push_back(gap);
        }
    }
    if (gaps.empty()) {
        return 0.0;
    }
    const size_t middle = gaps.size() / 2;
    std::nth_element(gaps.begin(), gaps.begin() + long(middle), gaps.end());
    return gaps[middle];
}

/**
 * Rows of tab-separated cells.
 *
 * Tabs rather than the on-screen padding, and rather than Markdown: a captured
 * table is nearly always on its way into a spreadsheet, where tab-separated
 * values land as real cells. Re-emitting the padding would produce one text
 * cell per row, and Markdown would need escaping rules for content the user
 * cannot see or correct.
 *
 * Columns come from whitespace bands that run the full height of the region,
 * so a gap inside one cell — a two-word heading — does not split it.
 */
QString assembleTable(const std::vector<Word> &words)
{
    const std::vector<Line> lines = groupIntoLines(words);
    if (lines.empty()) {
        return QString();
    }

    std::vector<int> boundaries;
    for (const Gap &gap : columnGaps(words)) {
        boundaries.push_back(gap.centre());
    }

    QStringList rows;
    for (const Line &line : lines) {
        std::vector<QString> cells(boundaries.size() + 1);
        for (const Word *word : line.words) {
            // By centre, not by left edge: a column of right-aligned numerics
            // starts at a different x on every row.
            const int centre = word->bbox.center().x();
            size_t column = 0;
            while (column < boundaries.size() && centre > boundaries[column]) {
                ++column;
            }
            appendWord(cells[column], word->text);
        }

        while (cells.size() > 1 && cells.back().isEmpty()) {
            cells.pop_back();
        }
        rows << QStringList(cells.cbegin(), cells.cend())
                    .join(QLatin1Char('\t'));
    }

    return rows.join(QLatin1Char('\n'));
}

/// A vertical gap this much larger than the usual pitch means blank lines.
constexpr double kBlankLineGap = 1.5;

/// Cap on reconstructed blank lines. A misjudged pitch would otherwise turn
/// one wide gap into a screenful of nothing.
constexpr int kMaxBlankLines = 2;

/**
 * Source code, with its indentation put back.
 *
 * The engine reports where each word starts, never how many spaces preceded
 * it, so indentation is recovered by dividing the offset from the leftmost
 * line by the measured character cell. Lines are never joined: in code a line
 * break is content, and a trailing hyphen is an operator rather than
 * hyphenation.
 */
QString assembleCode(const std::vector<Word> &words)
{
    const std::vector<Line> lines = groupIntoLines(words);
    if (lines.empty()) {
        return QString();
    }

    const double cell = characterWidth(words);
    const double pitch = linePitch(lines);

    int origin = lines.front().words.front()->bbox.left();
    for (const Line &line : lines) {
        origin = std::min(origin, line.words.front()->bbox.left());
    }

    QString out;
    for (size_t i = 0; i < lines.size(); ++i) {
        const Line &line = lines[i];

        if (i > 0) {
            out += QLatin1Char('\n');
            if (pitch > 0.0) {
                // A blank line in source contains no words, so the engine
                // cannot report one. The gap is the only evidence it existed.
                const double gap = lineTop(line) - lineTop(lines[i - 1]);
                if (gap >= kBlankLineGap * pitch) {
                    const int blanks = std::min(kMaxBlankLines,
                                                int(qRound(gap / pitch)) - 1);
                    out += QString(blanks, QLatin1Char('\n'));
                }
            }
        }

        if (cell > 0.0) {
            const int indent = int(qRound((line.words.front()->bbox.left()
                                           - origin) / cell));
            if (indent > 0) {
                out += QString(indent, QLatin1Char(' '));
            }
        }
        out += line.joined();
    }

    return out;
}

/**
 * Paragraphs, unwrapped.
 *
 * A rendered line break inside a paragraph records the width the text happened
 * to be laid out at, which is not something the user asked to copy. Block
 * boundaries are kept — those are real paragraph breaks.
 */
QString assembleProse(const std::vector<Word> &words)
{
    QStringList paragraphs;
    QString current;
    int currentBlock = -1;

    for (const Line &line : groupIntoLines(words)) {
        const QString text = line.joined();
        if (text.isEmpty()) {
            continue;
        }

        if (current.isEmpty() || line.block != currentBlock) {
            if (!current.isEmpty()) {
                paragraphs << current;
            }
            current = text;
            currentBlock = line.block;
            continue;
        }

        if (current.endsWith(QLatin1Char('-'))) {
            // Hyphenation: put the word back together without the hyphen.
            //
            // This also swallows the hyphen of a genuinely hyphenated word
            // that happened to break at the same place ("bleed-through" ->
            // "bleedthrough"). Telling the two apart needs a dictionary, which
            // is M5's job; inventing one here would be guesswork.
            current.chop(1);
            current += text;
        } else if (isCjk(current.back()) && isCjk(text.front())) {
            current += text;
        } else {
            current += QLatin1Char(' ');
            current += text;
        }
    }

    if (!current.isEmpty()) {
        paragraphs << current;
    }

    return paragraphs.join(QStringLiteral("\n\n"));
}

} // namespace

QString assemble(const std::vector<Word> &words, LayoutKind kind)
{
    switch (kind) {
    case LayoutKind::Code:
        return assembleCode(words);
    case LayoutKind::Prose:
        return assembleProse(words);
    case LayoutKind::Table:
        return assembleTable(words);
    case LayoutKind::Raw:
        return assembleRaw(words);
    }
    return QString();
}

} // namespace textract
