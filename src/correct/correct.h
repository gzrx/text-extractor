// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "assemble/assemble.h"
#include "correct/dictionary.h"
#include "ocr/word.h"

namespace textract {

/**
 * Repairs confusable characters in recognised prose, in place.
 *
 * A no-op for `Code`, `Table` and `Raw`. Code is full of identifiers that are
 * not dictionary words and must not be made into them; a table cell is as
 * likely to be a part number as a word; and `Raw` is the escape hatch, where
 * the user asked for what was on screen rather than an improved version of it.
 *
 * Within `Prose`, a substitution is made only where **both** hold:
 *
 *   1. the engine's confidence for that word is below threshold, and
 *   2. the substituted form is a dictionary word while the original is not.
 *
 * That conjunction is the whole safety argument. Either half alone would
 * rewrite proper nouns, identifiers and technical terms into whatever they
 * happen to be one confusable character away from. Substitutions are drawn
 * from a fixed confusion set, so this never invents text — every output
 * character is one the engine reported or one the set maps it to.
 *
 * Runs before assemble(): the confidence this reads is per word, and the
 * assembled string no longer has it.
 *
 * Passing nullptr, or a dictionary with no langdata behind it, disables
 * correction entirely.
 */
void correct(std::vector<Word> &words, LayoutKind kind,
             const Dictionary *dictionary);

} // namespace textract
