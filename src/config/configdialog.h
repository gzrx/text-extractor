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
 * Requires a QApplication (not merely a QGuiApplication) to already exist.
 */
bool runConfigDialog(Settings *settings, const QStringList &languages);

} // namespace textract
