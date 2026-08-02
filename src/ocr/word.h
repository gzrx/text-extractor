#pragma once

#include <QRect>
#include <QString>

namespace textract {

/// One recognised word, with geometry in ORIGINAL crop coordinates.
struct Word {
    QString text;
    QRect   bbox;
    float   confidence{0.0f}; ///< 0.0 - 1.0
    int     line{0};
    int     block{0};
};

} // namespace textract
