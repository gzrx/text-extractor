// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>

namespace textract {

struct Settings;

/**
 * Runs the modal settings dialog.
 *
 * Writes the user's choices into `*settings` and returns true if accepted;
 * returns false and leaves `*settings` untouched on cancel. Reads and writes no
 * file -- the caller owns persistence.
 *
 * `languages` is what availableLanguages() found. Preprocessing is deliberately
 * absent from this dialog; see Settings::preprocess.
 *
 * A language present in `settings->langs` but not in `languages` gets no
 * checkbox, and is therefore silently dropped from `langs` on accept -- even
 * if the user only opened the dialog to edit the model directory. This is
 * deliberate: the dialog must not offer a language warmUp() would reject.
 *
 * Requires a QApplication (not merely a QGuiApplication) to already exist.
 */
bool runConfigDialog(Settings *settings, const QStringList &languages);

} // namespace textract
