// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QString>

#include "assemble/assemble.h"

namespace textract::fixtures {

/// One corpus entry: a captured PNG paired with the text it should produce.
struct Fixture {
    QString    name;
    QString    imagePath;    ///< absolute, resolved against the manifest
    QString    expectedPath; ///< absolute, resolved against the manifest
    QString    langs{QStringLiteral("eng")};
    LayoutKind layout{LayoutKind::Raw};
    double     minScore{0.0}; ///< tier-1 regression floor; 0 means report only

    /// Tier-2 regression floor; 0 means report only.
    ///
    /// Separate from minScore because the two engines have genuinely different
    /// strengths: PP-OCR fixes the full-width comma in dark-terminal-cjk and
    /// both tables, and loses to Tesseract on dark-terminal-buildlog's small
    /// monospace punctuation. One shared floor would either hide that or block
    /// on a known, measured loss.
    double     minScoreTier2{0.0};
    QString    notes;
};

/**
 * Reads the corpus manifest, resolving relative paths against its directory.
 *
 * An absent manifest and an empty fixture list both return empty with no
 * error: that is the normal state before any fixture has been captured.
 * Malformed JSON and incomplete entries do set `*error`.
 */
std::vector<Fixture> loadManifest(const QString &manifestPath, QString *error);

/// Canonical form for comparison: LF line endings, no trailing whitespace on
/// any line, no leading or trailing blank lines.
QString normalise(const QString &text);

/**
 * Character-level accuracy in [0, 1], as 1 - levenshtein / max(length), over
 * the normalised strings.
 *
 * Deliberately a score rather than a verdict. A boolean corpus tells you a
 * change broke something; a scored one tells you which direction every fixture
 * moved, which is what makes accuracy work tractable.
 */
double similarity(const QString &expected, const QString &actual);

/// True when traineddata for every component of `langs` ("eng+msa") is present.
bool langdataAvailable(const QString &langs);

} // namespace textract::fixtures
