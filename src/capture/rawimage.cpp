// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "capture/rawimage.h"

namespace textract {

QImage imageFromRaw(const QByteArray &bytes,
                    quint32 width,
                    quint32 height,
                    quint32 stride,
                    quint32 format)
{
    if (width == 0 || height == 0 || stride == 0) {
        return QImage();
    }
    if (format == 0 || format >= quint32(QImage::NImageFormats)) {
        return QImage();
    }
    if (quint64(stride) * height > quint64(bytes.size())) {
        return QImage();
    }

    const QImage view(reinterpret_cast<const uchar *>(bytes.constData()),
                      int(width),
                      int(height),
                      int(stride),
                      QImage::Format(format));

    // Deep copy: the view does not own `bytes`.
    return view.copy();
}

} // namespace textract
