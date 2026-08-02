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

} // namespace textract
