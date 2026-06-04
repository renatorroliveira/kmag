// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef KMAG_DOCKINGMANAGER_H
#define KMAG_DOCKINGMANAGER_H

#include "dockmath.h"
#include <memory>

class QWindow;
class QScreen;
class QRect;

// WM-level edge reservation strategy. Knows nothing about widgets; operates on an
// already-created, not-yet-shown frameless top-level QWindow.
class DockingManager
{
public:
    using Edge = KMagDock::Edge;

    virtual ~DockingManager() = default;

    // Apply reservation to `win`. Returns false on failure (caller rolls back).
    virtual bool dock(QWindow *win, Edge edge, int thickness, QScreen *screen, const QRect &availGeom) = 0;
    // Reconfigure a live docked window (edge/size/screen) without recreating it.
    virtual void updateDock(Edge edge, int thickness, QScreen *screen, const QRect &availGeom) = 0;
    // Release reservation. X11: zero strut + NET::Normal + clearState.
    // Wayland: no-op (the controller tears the surface down).
    virtual void releaseDock() = 0;

    // Returns the X11 or Wayland backend, or nullptr when docking cannot work in
    // this session (X11-only build on Wayland, or XWayland).
    static std::unique_ptr<DockingManager> create();
};

#endif // KMAG_DOCKINGMANAGER_H
