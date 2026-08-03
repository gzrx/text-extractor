// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

// The full header, not a forward declaration: WriteConfigFlags is a nested
// enum and cannot be forward-declared.
#include <KConfigBase>

#include "preprocess/preprocess.h"

class KConfigGroup;

namespace textract {

/**
 * Everything the daemon reads from ~/.config/textractrc.
 *
 * Every default below equals what was compiled in before M7a, which is what
 * makes an absent config file ordinary rather than a special case.
 *
 * Shortcuts are deliberately NOT here. They live in kglobalshortcutsrc, owned
 * by KGlobalAccel and edited in System Settings; storing them a second time
 * would create two sources of truth with no rule for which wins.
 */
struct Settings {
    /// Tesseract language string, '+'-joined. Tier 1 only -- PP-OCRv6_small
    /// carries one fixed 18708-character charset and takes no language.
    QString langs{QStringLiteral("eng")};

    /// Not exposed in the dialog on purpose. HANDOFF 9.2 measured 3x as better
    /// than 2x by 0.0051 and binarisation as costing 0.0054 at that default, so
    /// a control offering these invites a user to undo a measured result. The
    /// file keeps the escape hatch for content the corpus does not cover.
    PreprocessOptions preprocess{};

    /// Empty means OnnxPaddleEngine::defaultModelDir(). Resolve with
    /// resolveModelDir(), never by reading this field directly.
    QString modelDir;

    friend bool operator==(const Settings &a, const Settings &b)
    {
        return a.langs == b.langs
            && a.preprocess.upscale == b.preprocess.upscale
            && a.preprocess.binarize == b.preprocess.binarize
            && a.modelDir == b.modelDir;
    }
};

/// Reads `root`'s subgroups. Pure: opens nothing and consults no environment.
Settings loadSettings(const KConfigGroup &root);

/**
 * Writes into `root`'s subgroups. The caller syncs.
 *
 * `flags` must be KConfigBase::Notify for a write a running daemon should
 * notice: KConfigWatcher only emits configChanged() when the writer set that
 * flag, so a plain writeEntry() followed by sync() updates the file and
 * silently reloads nothing.
 *
 * It defaults to Normal so that tests -- which write a throwaway file that
 * happens to share the real one's name -- cannot poke a daemon running on the
 * same session bus.
 */
void saveSettings(KConfigGroup &root, const Settings &settings,
                  KConfigBase::WriteConfigFlags flags = KConfigBase::Normal);

/**
 * The model directory actually to use, highest precedence first:
 *
 *   1. the TEXTRACT_MODELS environment variable
 *   2. Settings::modelDir
 *   3. OnnxPaddleEngine::defaultModelDir()
 *
 * Separate from loadSettings() because it is the only part that reads the
 * environment. Env stays on top so the fixture harness override keeps working.
 */
QString resolveModelDir(const Settings &settings);

} // namespace textract
