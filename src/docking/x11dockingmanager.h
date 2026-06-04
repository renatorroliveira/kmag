// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef KMAG_X11DOCKINGMANAGER_H
#define KMAG_X11DOCKINGMANAGER_H

#include "dockingmanager.h"
#include <QPointer>
#include <QRect>
#include <QWindow>

// Edge reservation via _NET_WM_STRUT_PARTIAL + _NET_WM_WINDOW_TYPE_DOCK, using
// KX11Extras (ships in KF6::WindowSystem). Strut/type/state are no-ops off X11,
// so this file always compiles. (findings §C)
class X11DockingManager : public DockingManager
{
public:
    bool dock(QWindow *win, Edge edge, int thickness, QScreen *screen, const QRect &availGeom) override;
    void updateDock(Edge edge, int thickness, QScreen *screen, const QRect &availGeom) override;
    void releaseDock() override;

private:
    void applyStrutAndGeometry(Edge edge, int thickness, QScreen *screen, const QRect &availGeom);

    QPointer<QWindow> m_window;
};

#endif // KMAG_X11DOCKINGMANAGER_H
