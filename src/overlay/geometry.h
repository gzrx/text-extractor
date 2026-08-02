#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>

namespace textract {

/// Selections smaller than this in either dimension are treated as accidental.
inline constexpr int kMinSelectionPixels = 8;

/**
 * Converts a selection expressed in overlay-window logical coordinates into
 * physical-pixel coordinates within the full workspace capture.
 *
 * `screenLogicalOrigin` is the logical position of the overlay's screen in the
 * virtual desktop. The result is clamped to `imageSize`.
 */
QRect mapSelectionToPhysical(const QRect &selectionInWindow,
                             const QPoint &screenLogicalOrigin,
                             qreal devicePixelRatio,
                             const QSize &imageSize);

/// True if the selection is large enough to be a deliberate drag.
bool isSelectionUsable(const QRect &physical);

} // namespace textract
