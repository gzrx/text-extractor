// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QString>

namespace textract {

struct CaptureResult {
    QImage image;      ///< Physical-pixel image of the whole workspace.
    qreal  scale{1.0}; ///< Native-to-logical ratio reported by KWin.

    bool isValid() const { return !image.isNull(); }
};

/**
 * Captures the entire workspace via org.kde.KWin.ScreenShot2.
 *
 * Requests native-resolution output, so the returned image is in physical
 * pixels. On failure returns an invalid result and, if `error` is non-null,
 * sets it to a human-readable message.
 */
CaptureResult captureWorkspace(QString *error = nullptr);

/**
 * True when this process's executable has been unlinked or replaced on disk.
 *
 * Rebuilding relinks `bin/textract`, which replaces the inode underneath any
 * already-running process. Linux then reports `/proc/self/exe` with a
 * " (deleted)" suffix, and KWin — which authorises a caller by matching that
 * path against `Exec=` lines — can no longer match it against anything.
 */
bool executableWasReplaced();

/**
 * Builds the guidance shown when KWin refuses a screenshot as unauthorised.
 *
 * Split by cause because the two remedies are unrelated. A missing or
 * mismatched .desktop file needs the file installed; a replaced binary needs
 * the process restarted, and re-installing the desktop file achieves nothing.
 * Kept pure so it can be tested without a compositor.
 */
QString authorisationErrorText(const QString &executablePath,
                               bool executableReplaced);

} // namespace textract
