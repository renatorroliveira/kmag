// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#include "x11dockingmanager.h"

#include <KX11Extras>
#include <netwm_def.h>

#include <QScreen>

namespace {
constexpr NET::States kDockStates =
    NET::States(NET::SkipTaskbar | NET::SkipPager | NET::SkipSwitcher | NET::Sticky | NET::KeepAbove);
}

bool X11DockingManager::dock(QWindow *win, Edge edge, int thickness, QScreen *screen, const QRect &availGeom)
{
    if (!win || !screen) {
        return false;
    }
    if (m_window && m_window != win) {
        releaseDock();
    }
    m_window = win;
    const WId wid = win->winId();

    KX11Extras::setType(wid, NET::Dock);
    KX11Extras::setState(wid, kDockStates);
    KX11Extras::setOnAllDesktops(wid, true);
    applyStrutAndGeometry(edge, thickness, screen, availGeom);
    return true;
}

void X11DockingManager::updateDock(Edge edge, int thickness, QScreen *screen, const QRect &availGeom)
{
    if (!m_window) {
        return;
    }
    applyStrutAndGeometry(edge, thickness, screen, availGeom);
}

void X11DockingManager::applyStrutAndGeometry(Edge edge, int thickness, QScreen *screen, const QRect &availGeom)
{
    if (!m_window || !screen) {
        return;
    }
    const WId wid = m_window->winId();
    const QRect g = screen->geometry();
    const QRect v = screen->virtualGeometry();
    const QRect a = availGeom;

    m_window->setGeometry(KMagDock::stripRect(a, edge, thickness));

    const KMagDock::Strut s = KMagDock::extendedStrut(g, a, v, edge, thickness);
    KX11Extras::setExtendedStrut(wid,
        s.v[0], s.v[1], s.v[2],
        s.v[3], s.v[4], s.v[5],
        s.v[6], s.v[7], s.v[8],
        s.v[9], s.v[10], s.v[11]);
}

void X11DockingManager::releaseDock()
{
    if (!m_window) {
        return;
    }
    const WId wid = m_window->winId();
    KX11Extras::setExtendedStrut(wid, 0,0,0, 0,0,0, 0,0,0, 0,0,0);
    KX11Extras::clearState(wid, kDockStates);
    KX11Extras::setType(wid, NET::Normal);
    KX11Extras::setOnAllDesktops(wid, false);
    m_window.clear();
}
