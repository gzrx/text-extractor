#include "overlay/geometry.h"

#include <QtGlobal>

namespace textract {

QRect mapSelectionToPhysical(const QRect &selectionInWindow,
                             const QPoint &screenLogicalOrigin,
                             qreal devicePixelRatio,
                             const QSize &imageSize)
{
    if (selectionInWindow.isEmpty() || devicePixelRatio <= 0.0) {
        return QRect();
    }

    // Translate into virtual-desktop logical space, then scale to physical.
    const QRect logical = selectionInWindow.translated(screenLogicalOrigin);

    const int left   = qRound(logical.left()   * devicePixelRatio);
    const int top    = qRound(logical.top()    * devicePixelRatio);
    const int width  = qRound(logical.width()  * devicePixelRatio);
    const int height = qRound(logical.height() * devicePixelRatio);

    const QRect physical(left, top, width, height);
    return physical.intersected(QRect(QPoint(0, 0), imageSize));
}

bool isSelectionUsable(const QRect &physical)
{
    return physical.width()  >= kMinSelectionPixels
        && physical.height() >= kMinSelectionPixels;
}

} // namespace textract
