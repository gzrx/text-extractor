// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "correct/correct.h"

#include <array>

namespace textract {

namespace {

/**
 * Below this the engine is admitting it guessed, and a dictionary is allowed
 * an opinion. Above it, what the engine read is what was on screen.
 *
 * Tesseract 5 reports well above this for correctly recognised screen text, so
 * the gate is not as tight as it looks; the corpus is the arbiter if it moves.
 */
constexpr float kConfidenceThreshold = 0.80f;

/// Bound on the candidate expansion. A word with many confusable characters
/// would otherwise generate exponentially many forms for no benefit — the
/// answer, if there is one, is always within a couple of substitutions.
constexpr size_t kMaxCandidates = 128;

/// The shortest core worth correcting. A one-character token is as likely to
/// be an initial or a list marker as a misread word, and "5" -> "S" is a
/// change no dictionary check can justify.
constexpr int kMinCoreLength = 2;

struct Confusion {
    QLatin1StringView from;
    QLatin1StringView to;
};

/**
 * The confusion set, expanded to one entry per direction.
 *
 * These are the substitutions that survive rendering at screen resolution:
 * shapes that differ by a hairline at 7pt (`0`/`O`, `1`/`l`/`I`, `5`/`S`) and
 * ligature-like runs that merge or split under antialiasing (`rn`/`m`,
 * `cl`/`d`, `vv`/`w`). Anything wider than this is guesswork.
 *
 * Letter-to-digit directions are kept even though a dictionary word can never
 * contain a digit — they cost one failed lookup and keep the set symmetric,
 * which is how the spec states it and how a reader will check it.
 */
constexpr std::array kConfusions{
    Confusion{QLatin1StringView("0"), QLatin1StringView("O")},
    Confusion{QLatin1StringView("O"), QLatin1StringView("0")},
    Confusion{QLatin1StringView("1"), QLatin1StringView("l")},
    Confusion{QLatin1StringView("l"), QLatin1StringView("1")},
    Confusion{QLatin1StringView("1"), QLatin1StringView("I")},
    Confusion{QLatin1StringView("I"), QLatin1StringView("1")},
    Confusion{QLatin1StringView("l"), QLatin1StringView("I")},
    Confusion{QLatin1StringView("I"), QLatin1StringView("l")},
    Confusion{QLatin1StringView("5"), QLatin1StringView("S")},
    Confusion{QLatin1StringView("S"), QLatin1StringView("5")},
    Confusion{QLatin1StringView("rn"), QLatin1StringView("m")},
    Confusion{QLatin1StringView("m"), QLatin1StringView("rn")},
    Confusion{QLatin1StringView("cl"), QLatin1StringView("d")},
    Confusion{QLatin1StringView("d"), QLatin1StringView("cl")},
    Confusion{QLatin1StringView("vv"), QLatin1StringView("w")},
    Confusion{QLatin1StringView("w"), QLatin1StringView("vv")},
};

/**
 * Every form of `word` reachable by applying confusion-set substitutions.
 *
 * Depth-first with the "change nothing here" branch taken first, so the
 * expansion is ordered by how much it disturbs what the engine reported and
 * the caller can stop at the first form that is a word. The unmodified word is
 * therefore always the first element.
 */
void expand(const QString &word, int position, const QString &accumulated,
            std::vector<QString> &out)
{
    if (out.size() >= kMaxCandidates) {
        return;
    }
    if (position == word.size()) {
        out.push_back(accumulated);
        return;
    }

    expand(word, position + 1, accumulated + word.at(position), out);

    for (const Confusion &confusion : kConfusions) {
        const int span = confusion.from.size();
        if (QStringView(word).sliced(position).startsWith(confusion.from)) {
            expand(word, position + span, accumulated + confusion.to, out);
        }
    }
}

/// The letters and digits of `token`, with any surrounding punctuation and
/// quoting split off so it can be put back afterwards.
struct Split {
    QString prefix;
    QString core;
    QString suffix;
};

Split splitToken(const QString &token)
{
    int first = 0;
    while (first < token.size() && !token.at(first).isLetterOrNumber()) {
        ++first;
    }
    int last = token.size();
    while (last > first && !token.at(last - 1).isLetterOrNumber()) {
        --last;
    }
    return Split{token.left(first), token.mid(first, last - first),
                 token.mid(last)};
}

bool hasLetter(const QString &text)
{
    for (const QChar character : text) {
        if (character.isLetter()) {
            return true;
        }
    }
    return false;
}

/// The corrected form of `core`, or an empty string when nothing in the
/// confusion set turns it into a word.
QString substituted(const QString &core, const Dictionary &dictionary)
{
    std::vector<QString> candidates;
    expand(core, 0, QString(), candidates);

    for (const QString &candidate : candidates) {
        // The first element is `core` itself, and the rest are ordered by how
        // little they change it, so the first dictionary hit is the least
        // invasive repair available. Where two are equally good this prefers
        // the one that leaves the earlier characters as the engine read them.
        if (candidate != core && dictionary.contains(candidate)) {
            return candidate;
        }
    }
    return QString();
}

} // namespace

void correct(std::vector<Word> &words, LayoutKind kind,
             const Dictionary *dictionary)
{
    if (kind != LayoutKind::Prose || !dictionary || !dictionary->available()) {
        return;
    }

    for (Word &word : words) {
        if (word.confidence >= kConfidenceThreshold) {
            continue;
        }

        const Split split = splitToken(word.text);
        if (split.core.size() < kMinCoreLength || !hasLetter(split.core)) {
            continue;
        }

        // Condition 2's second half: a word the dictionary already knows is
        // not evidence of a misreading, however unsure the engine was.
        if (dictionary->contains(split.core)) {
            continue;
        }

        const QString repaired = substituted(split.core, *dictionary);
        if (!repaired.isEmpty()) {
            word.text = split.prefix + repaired + split.suffix;
        }
    }
}

} // namespace textract
