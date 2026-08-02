// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboard/clipboard.h"

#include <KSystemClipboard>

#include <QMimeData>

namespace textract {

void copyToClipboard(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    // setMimeData takes ownership of the QMimeData.
    auto *mime = new QMimeData;
    mime->setText(text);
    KSystemClipboard::instance()->setMimeData(mime, QClipboard::Clipboard);
}

} // namespace textract
