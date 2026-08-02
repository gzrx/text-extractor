#pragma once

#include <QByteArray>
#include <QImage>

namespace textract {

/**
 * Decodes the raw pixel buffer produced by org.kde.KWin.ScreenShot2 into a
 * QImage that owns its memory.
 *
 * Returns a null QImage if the parameters are inconsistent with the buffer
 * size or the format value is not a valid QImage::Format.
 */
QImage imageFromRaw(const QByteArray &bytes,
                    quint32 width,
                    quint32 height,
                    quint32 stride,
                    quint32 format);

} // namespace textract
