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
    void selected(const QRect &physicalRect);
    void cancelled();

private:
    void finishWith(const QRect &physicalRect);
    void tearDown();

    QImage m_workspace;
    QList<OverlayWindow *> m_windows;
    bool m_finished{false};
};

} // namespace textract
