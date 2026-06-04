// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#include "waylanddockingmanager.h"

#if HAVE_LAYERSHELLQT

#include <LayerShellQt/Window>
#include <QScreen>

using LSWindow = LayerShellQt::Window;

bool WaylandDockingManager::dock(QWindow *win, Edge edge, int thickness, QScreen *screen, const QRect &availGeom)
{
    Q_UNUSED(availGeom);
    if (!win) {
        return false;
    }
    m_layer = LSWindow::get(win);
    if (!m_layer) {
        return false;
    }
    m_window = win;

    if (screen) {
        win->setScreen(screen);
    }
    m_layer->setLayer(LSWindow::LayerTop);
    m_layer->setScope(QStringLiteral("dock"));
    m_layer->setKeyboardInteractivity(LSWindow::KeyboardInteractivityOnDemand);
    applyAnchors(edge, thickness);
    return true;
}

void WaylandDockingManager::updateDock(Edge edge, int thickness, QScreen *screen, const QRect &availGeom)
{
    Q_UNUSED(availGeom);
    if (!m_layer || !m_window) {
        return;
    }
    if (screen) {
        m_window->setScreen(screen);
    }
    applyAnchors(edge, thickness);
}

void WaylandDockingManager::applyAnchors(Edge edge, int thickness)
{
    LSWindow::Anchors anchors;
    LSWindow::Anchor exclusiveEdge = LSWindow::AnchorTop;
    switch (edge) {
    case Edge::Top:
        anchors = LSWindow::Anchors(LSWindow::AnchorTop | LSWindow::AnchorLeft | LSWindow::AnchorRight);
        exclusiveEdge = LSWindow::AnchorTop;
        break;
    case Edge::Bottom:
        anchors = LSWindow::Anchors(LSWindow::AnchorBottom | LSWindow::AnchorLeft | LSWindow::AnchorRight);
        exclusiveEdge = LSWindow::AnchorBottom;
        break;
    case Edge::Left:
        anchors = LSWindow::Anchors(LSWindow::AnchorLeft | LSWindow::AnchorTop | LSWindow::AnchorBottom);
        exclusiveEdge = LSWindow::AnchorLeft;
        break;
    case Edge::Right:
        anchors = LSWindow::Anchors(LSWindow::AnchorRight | LSWindow::AnchorTop | LSWindow::AnchorBottom);
        exclusiveEdge = LSWindow::AnchorRight;
        break;
    }
    m_layer->setAnchors(anchors);
    m_layer->setExclusiveEdge(exclusiveEdge);
    m_layer->setExclusiveZone(thickness);
}

void WaylandDockingManager::releaseDock()
{
    // The controller hides + destroys the surface on undock; nothing to release
    // at the protocol level here.
    m_layer = nullptr;
    m_window.clear();
}

#endif // HAVE_LAYERSHELLQT
