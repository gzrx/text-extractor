// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace textract {

/**
 * Places `text` on the system clipboard.
 *
 * Does nothing when `text` is empty: the spec forbids clobbering the user's
 * existing clipboard contents on a failure path.
 */
void copyToClipboard(const QString &text);

} // namespace textract
