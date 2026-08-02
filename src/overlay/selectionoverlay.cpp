#include "overlay/selectionoverlay.h"
#include "overlay/geometry.h"

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRasterWindow>
#include <QScreen>

/// A borderless layer-shell surface covering one screen.
class OverlayWindow : public QRasterWindow
{
    Q_OBJECT
public:
    OverlayWindow(const QImage &workspace, QScreen *targetScreen)
        : m_workspace(workspace)
    {
        setScreen(targetScreen);
        setGeometry(targetScreen->geometry());

        auto *layer = LayerShellQt::Window::get(this);
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        LayerShellQt::Window::Anchors anchors(LayerShellQt::Window::AnchorTop);
        anchors |= LayerShellQt::Window::AnchorBottom;
        anchors |= LayerShellQt::Window::AnchorLeft;
        anchors |= LayerShellQt::Window::AnchorRight;
        layer->setAnchors(anchors);
        layer->setExclusiveZone(-1); // ignore panels; cover the whole screen
        layer->setKeyboardInteractivity(
            LayerShellQt::Window::KeyboardInteractivityExclusive);
        layer->setScope(QStringLiteral("textract-selection"));
        layer->setScreen(targetScreen);
    }

Q_SIGNALS:
    void regionChosen(const QRect &physicalRect);
    void aborted();

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);

        // QWindow has no rect(); build it from the current logical size.
        const QRect surface(0, 0, width(), height());

        // Draw this screen's slice of the workspace capture, scaled to fit.
        painter.drawImage(surface, m_workspace, physicalScreenRect());

        // Dim everything, then punch the live selection back to full brightness.
        painter.fillRect(surface, QColor(0, 0, 0, 110));
        if (!m_selection.isNull() && !m_selection.isEmpty()) {
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.drawImage(m_selection, m_workspace, mapSelection(m_selection));
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(QColor(80, 160, 255), 1));
            painter.drawRect(m_selection.adjusted(0, 0, -1, -1));
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_origin = event->position().toPoint();
            m_dragging = true;
            m_selection = QRect(m_origin, QSize());
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging) {
            m_selection = QRect(m_origin, event->position().toPoint()).normalized();
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton || !m_dragging) {
            return;
        }
        m_dragging = false;

        const QRect physical = mapSelection(m_selection);
        if (textract::isSelectionUsable(physical)) {
            Q_EMIT regionChosen(physical);
        } else {
            Q_EMIT aborted();
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            Q_EMIT aborted();
        }
    }

private:
    /// This screen's area within the physical workspace image.
    QRect physicalScreenRect() const
    {
        return textract::mapSelectionToPhysical(
            QRect(QPoint(0, 0), screen()->geometry().size()),
            screen()->geometry().topLeft(),
            screen()->devicePixelRatio(),
            m_workspace.size());
    }

    QRect mapSelection(const QRect &windowRect) const
    {
        return textract::mapSelectionToPhysical(windowRect,
                                                screen()->geometry().topLeft(),
                                                screen()->devicePixelRatio(),
                                                m_workspace.size());
    }

    QImage m_workspace;
    QPoint m_origin;
    QRect  m_selection;
    bool   m_dragging{false};
};

namespace textract {

SelectionOverlay::SelectionOverlay(QObject *parent)
    : QObject(parent)
{
}

SelectionOverlay::~SelectionOverlay()
{
    tearDown();
}

void SelectionOverlay::start(const QImage &workspace)
{
    // NOTE: LayerShellQt::Shell::useLayerShell() is NOT called here. It swaps
    // the Wayland shell integration and must run before any window is created,
    // so main() calls it once at startup.
    m_workspace = workspace;
    m_finished = false;

    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        auto *window = new OverlayWindow(m_workspace, screen);
        connect(window, &OverlayWindow::regionChosen, this,
                &SelectionOverlay::finishWith);
        connect(window, &OverlayWindow::aborted, this, [this] {
            finishWith(QRect());
        });
        window->show();
        m_windows.append(window);
    }
}

void SelectionOverlay::finishWith(const QRect &physicalRect)
{
    if (m_finished) {
        return; // another screen's window already resolved this session
    }
    m_finished = true;
    tearDown();

    if (physicalRect.isNull()) {
        Q_EMIT cancelled();
    } else {
        Q_EMIT selected(physicalRect);
    }
}

void SelectionOverlay::tearDown()
{
    qDeleteAll(m_windows);
    m_windows.clear();
}

} // namespace textract

#include "selectionoverlay.moc"
