// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef KMAG_WAYLANDDOCKINGMANAGER_H
#define KMAG_WAYLANDDOCKINGMANAGER_H

#include "dockconfig.h"

#if HAVE_LAYERSHELLQT

#include "dockingmanager.h"
#include <QPointer>
#include <QWindow>

namespace LayerShellQt { class Window; }

// Edge reservation via wlr-layer-shell (anchors + exclusive zone) using
// LayerShellQt. (findings §D)
class WaylandDockingManager : public DockingManager
{
public:
    bool dock(QWindow *win, Edge edge, int thickness, QScreen *screen, const QRect &availGeom) override;
    void updateDock(Edge edge, int thickness, QScreen *screen, const QRect &availGeom) override;
    void releaseDock() override;

private:
    void applyAnchors(Edge edge, int thickness);

    QPointer<QWindow> m_window;
    LayerShellQt::Window *m_layer = nullptr; // owned by the QWindow, not us
};

#endif // HAVE_LAYERSHELLQT
#endif // KMAG_WAYLANDDOCKINGMANAGER_H
