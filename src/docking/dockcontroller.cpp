// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#include "dockcontroller.h"

#include "dockcontrolpanel.h"
#include "kmagzoomview.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QContextMenuEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QMainWindow>
#include <QScreen>
#include <QWindow>

DockController::DockController(IDockHost *host, KMagZoomView *view, QObject *parent)
    : QObject(parent)
    , m_host(host)
    , m_view(view)
    , m_backend(DockingManager::create())
{
}

DockController::~DockController() = default;

void DockController::configure(Edge edge, int thickness, const QString &screenName)
{
    m_edge = edge;
    m_thickness = thickness < 100 ? 100 : thickness;
    m_screenName = screenName;
}

QString DockController::screenName() const
{
    return m_screen ? m_screen->name() : m_screenName;
}

QScreen *DockController::resolveScreen() const
{
    if (!m_screenName.isEmpty()) {
        const auto screens = QGuiApplication::screens();
        for (QScreen *s : screens) {
            if (s->name() == m_screenName) {
                return s;
            }
        }
    }
    return QGuiApplication::primaryScreen();
}

QRect DockController::basedAvailableGeometry(QScreen *screen) const
{
    const QRect g = screen->geometry();
    return g.adjusted(m_resLeft, m_resTop, -m_resRight, -m_resBottom);
}

void DockController::enterDock()
{
    if (!m_backend || m_docked || !m_view) {
        return;
    }
    QScreen *screen = resolveScreen();
    if (!screen) {
        return;
    }
    m_screen = screen;

    // Capture the panel-only reserved insets now, before we apply our own strut.
    // Once docked, availableGeometry() would also exclude our strip, so we must
    // remember the panel-only work area and reuse it for all later placement.
    {
        const QRect g0 = screen->geometry();
        const QRect a0 = screen->availableGeometry();
        m_resTop    = a0.top() - g0.top();
        m_resBottom = g0.bottom() - a0.bottom();
        m_resLeft   = a0.left() - g0.left();
        m_resRight  = g0.right() - a0.right();
    }

    // One-time warning if the chosen edge is interior on a multi-monitor X11
    // desktop (KWin will ignore the strut there). (spec §9.4)
    if (!m_interiorEdgeWarned
        && !KMagDock::edgeIsOnOuterBorder(screen->geometry(), screen->virtualGeometry(), m_edge)) {
        m_interiorEdgeWarned = true;
        KMessageBox::information(m_host->dockMainWindow(),
            i18n("This screen edge is on the interior of a multi-monitor desktop. "
                 "The window manager may not reserve space there; the magnifier will "
                 "still dock but other windows might overlap it."),
            i18n("Docked Mode"),
            QStringLiteral("kmag_dock_interior_edge"));
    }

    // Preserve the source mode's fit setting, then force fit-to-strip.
    m_savedFitToWindow = m_view->getFitToWindow();
    m_view->setFitToWindow(true);

    // Swap the view out for the control panel.
    QMainWindow *mw = m_host->dockMainWindow();
    mw->takeCentralWidget(); // detaches m_view without deleting it
    m_panel = new DockControlPanel(mw);
    m_panel->setState(m_edge, m_thickness);
    mw->setCentralWidget(m_panel);
    connect(m_panel, &DockControlPanel::edgeChangeRequested, this, &DockController::setEdge);
    connect(m_panel, &DockControlPanel::thicknessChangeRequested, this, &DockController::setThickness);
    connect(m_panel, &DockControlPanel::undockRequested, this, [this] { m_host->setDockToggleChecked(false); exitDock(); });

    // Reparent the view to a frameless top-level and realize its native surface.
    m_view->setParent(nullptr, Qt::Window | Qt::FramelessWindowHint);
    m_view->setGeometry(KMagDock::stripRect(basedAvailableGeometry(screen), m_edge, m_thickness));
    m_view->createWinId();
    QWindow *win = m_view->windowHandle();

    if (!win || !m_backend->dock(win, m_edge, m_thickness, screen, basedAvailableGeometry(screen))) {
        rollbackFailedDock();
        return;
    }

    // Right-click on the strip routes to the host's XmlGui context menu.
    // QAbstractScrollArea forwards the viewport's context-menu event up to the
    // scroll-area widget, so an event filter on both is the reliable hook.
    m_view->installEventFilter(this);
    m_view->viewport()->installEventFilter(this);

    m_view->show();
    m_host->addStripActions(m_view);
    connectScreenSignals();
    m_host->setDockEdgeChecked(m_edge);
    m_host->setDockEdgeActionsEnabled(true);
    m_docked = true;
}

void DockController::rollbackFailedDock()
{
    m_view->removeEventFilter(this);
    if (m_view->viewport()) {
        m_view->viewport()->removeEventFilter(this);
    }
    m_host->removeStripActions(m_view);

    QMainWindow *mw = m_host->dockMainWindow();
    if (m_panel) {
        mw->takeCentralWidget();
        m_panel->deleteLater();
        m_panel.clear();
    }
    m_view->setParent(mw);
    mw->setCentralWidget(m_view);
    m_view->setFitToWindow(m_savedFitToWindow);
    m_view->show();

    m_host->setDockToggleChecked(false);
    m_host->setDockEdgeActionsEnabled(false);
    m_docked = false;

    KMessageBox::error(mw, i18n("Could not dock the magnifier to the screen edge."),
                       i18n("Docked Mode"));
}

void DockController::exitDock()
{
    if (!m_docked || !m_view) {
        return;
    }
    disconnectScreenSignals();

    m_view->removeEventFilter(this);
    if (m_view->viewport()) {
        m_view->viewport()->removeEventFilter(this);
    }
    m_host->removeStripActions(m_view);

    if (m_backend) {
        m_backend->releaseDock();
    }

    QMainWindow *mw = m_host->dockMainWindow();
    if (m_panel) {
        mw->takeCentralWidget();
        m_panel->deleteLater();
        m_panel.clear();
    }

    // Reparent the view back into the main window. (Destroys the dock surface.)
    m_view->setParent(mw);
    mw->setCentralWidget(m_view);
    m_view->setFitToWindow(m_savedFitToWindow);
    m_host->reassertSourceMode();
    m_view->show();

    m_host->setDockEdgeActionsEnabled(false);
    m_docked = false;
}

void DockController::prepareForClose()
{
    if (!m_docked || !m_view) {
        return;
    }
    // Detach the dock machinery only. NO reparenting: while docked the view is a
    // frameless top-level that owns a native dock/LayerShell surface; reparenting
    // it back into the main window is what corrupts the heap and double-frees on
    // teardown. The host deletes this orphaned top-level view; the control panel
    // stays as the main window's central widget and is destroyed with it.
    disconnectScreenSignals();
    m_view->removeEventFilter(this);
    if (m_view->viewport()) {
        m_view->viewport()->removeEventFilter(this);
    }
    m_host->removeStripActions(m_view);
    if (m_backend) {
        m_backend->releaseDock();
    }
    m_docked = false;
}

void DockController::setEdge(Edge edge)
{
    if (edge == m_edge && m_docked) {
        return;
    }
    m_edge = edge;
    if (!m_docked) {
        return;
    }
    QScreen *screen = m_screen ? m_screen.data() : resolveScreen();
    const QRect avail = basedAvailableGeometry(screen);
    m_view->setGeometry(KMagDock::stripRect(avail, m_edge, m_thickness));
    m_backend->updateDock(m_edge, m_thickness, screen, avail);
    if (m_panel) {
        m_panel->setState(m_edge, m_thickness);
    }
    m_host->setDockEdgeChecked(m_edge);
}

void DockController::setThickness(int thickness)
{
    thickness = thickness < 100 ? 100 : thickness;
    if (thickness == m_thickness) {
        return;
    }
    m_thickness = thickness;
    if (!m_docked) {
        return;
    }
    QScreen *screen = m_screen ? m_screen.data() : resolveScreen();
    const QRect avail = basedAvailableGeometry(screen);
    m_view->setGeometry(KMagDock::stripRect(avail, m_edge, m_thickness));
    m_backend->updateDock(m_edge, m_thickness, screen, avail);
    if (m_panel) {
        m_panel->setState(m_edge, m_thickness);
    }
}

bool DockController::eventFilter(QObject *watched, QEvent *event)
{
    if (m_docked && event->type() == QEvent::ContextMenu
        && (watched == m_view || watched == m_view->viewport())) {
        auto *cme = static_cast<QContextMenuEvent *>(event);
        m_host->popupDockContextMenu(cme->globalPos());
        cme->accept();
        return true;
    }
    if (m_docked && event->type() == QEvent::Close && watched == m_view) {
        // Closing the strip must behave like closing the app while docked:
        // route through the main window so queryClose() persists the docked
        // state (Docked=true) before teardown. Swallow the strip's own close.
        event->ignore();
        if (QMainWindow *mw = m_host->dockMainWindow()) {
            mw->close();
        }
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void DockController::connectScreenSignals()
{
    connect(qApp, &QGuiApplication::screenRemoved, this, &DockController::onScreenRemoved);
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, &DockController::onScreenGeometryChanged);
    if (m_screen) {
        connect(m_screen, &QScreen::geometryChanged, this, &DockController::onScreenGeometryChanged);
    }
}

void DockController::disconnectScreenSignals()
{
    disconnect(qApp, &QGuiApplication::screenRemoved, this, &DockController::onScreenRemoved);
    disconnect(qApp, &QGuiApplication::primaryScreenChanged, this, &DockController::onScreenGeometryChanged);
    if (m_screen) {
        disconnect(m_screen, &QScreen::geometryChanged, this, &DockController::onScreenGeometryChanged);
    }
}

void DockController::onScreenGeometryChanged()
{
    if (!m_docked) {
        return;
    }
    QScreen *screen = m_screen ? m_screen.data() : resolveScreen();
    const QRect avail = basedAvailableGeometry(screen);
    m_view->setGeometry(KMagDock::stripRect(avail, m_edge, m_thickness));
    m_backend->updateDock(m_edge, m_thickness, screen, avail);
}

void DockController::onScreenRemoved(QScreen *screen)
{
    if (!m_docked || screen != m_screen) {
        return;
    }
    QScreen *fallback = QGuiApplication::primaryScreen();
    if (!fallback) {
        return; // application is shutting down
    }
    disconnect(m_screen, &QScreen::geometryChanged, this, &DockController::onScreenGeometryChanged);
    m_screen = fallback;
    connect(m_screen, &QScreen::geometryChanged, this, &DockController::onScreenGeometryChanged);
    const QRect avail = basedAvailableGeometry(m_screen);
    m_view->setGeometry(KMagDock::stripRect(avail, m_edge, m_thickness));
    m_backend->updateDock(m_edge, m_thickness, m_screen, avail);
}
