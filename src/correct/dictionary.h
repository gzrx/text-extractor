// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include <QString>

class Hunspell;

namespace textract {

/**
 * Hunspell-backed word lookup.
 *
 * A missing dictionary disables the lookup rather than failing construction:
 * the one thing it decides — whether a hyphen at a line end was typesetting or
 * content — is an improvement on the extracted text, never a precondition for
 * producing it, and the tool has to keep working on a machine that has no
 * langdata for the language in front of it.
 *
 * `contains()` therefore answers false in both directions when unavailable —
 * "not a word" and "no opinion" are the same answer here only because every
 * caller is required to consult `available()` before acting on a negative.
 */
class Dictionary
{
public:
    /// `language` is a Hunspell basename, e.g. "en_US" for en_US.{aff,dic}.
    explicit Dictionary(const QString &language = QStringLiteral("en_US"));
    ~Dictionary();

    Dictionary(const Dictionary &) = delete;
    Dictionary &operator=(const Dictionary &) = delete;

    bool available() const { return m_hunspell != nullptr; }

    /// True when `word` is spelled correctly according to the loaded
    /// dictionary. Always false when `available()` is false.
    bool contains(const QString &word) const;

private:
    /// mutable because Hunspell::spell() is non-const — it maintains internal
    /// caches — while looking a word up is observably const from out here.
    mutable std::unique_ptr<Hunspell> m_hunspell;

    /// Whatever the loaded .aff declared in its SET line.
    bool m_utf8{true};
};

} // namespace textract
