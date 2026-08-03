// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QRect>

class OverlayWindow;

namespace textract {

/**
 * Displays the frozen workspace capture across every screen and lets the user
 * drag a rectangle. Emits exactly one of selected() or cancelled() per start().
 */
class SelectionOverlay : public QObject
{
    Q_OBJECT
public:
    explicit SelectionOverlay(QObject *parent = nullptr);
    ~SelectionOverlay() override;

    /// Shows one layer-shell surface per screen. `workspace` is physical pixels.
    void start(const QImage &workspace);

Q_SIGNALS:
    /// `modifiers` are those held at any point during the drag, so the caller
    /// can honour an override such as Shift for forced Raw.
    void selected(const QRect &physicalRect, Qt::KeyboardModifiers modifiers);
    void cancelled();

private:
    void finishWith(const QRect &physicalRect, Qt::KeyboardModifiers modifiers);
    void tearDown();

    QImage m_workspace;
    QList<OverlayWindow *> m_windows;
    bool m_finished{false};
};

} // namespace textract
