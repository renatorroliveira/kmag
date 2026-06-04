// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#include "dockingmanager.h"
#include "x11dockingmanager.h"
#include "dockconfig.h"
#if HAVE_LAYERSHELLQT
#include "waylanddockingmanager.h"
#endif

#include <KWindowSystem>
#include <QGuiApplication>

std::unique_ptr<DockingManager> DockingManager::create()
{
    // XWayland trap: an xcb client inside a Wayland session. KWin ignores X11
    // struts for it and LayerShellQt needs a native wayland surface, so neither
    // backend works -> docking is unavailable. (findings §B)
    const bool waylandSession = qEnvironmentVariableIsSet("WAYLAND_DISPLAY")
        || qgetenv("XDG_SESSION_TYPE") == QByteArrayLiteral("wayland");
    if (QGuiApplication::platformName() == QLatin1String("xcb") && waylandSession) {
        return nullptr;
    }

    if (KWindowSystem::isPlatformX11()) {
        return std::make_unique<X11DockingManager>();
    }

#if HAVE_LAYERSHELLQT
    if (KWindowSystem::isPlatformWayland()) {
        return std::make_unique<WaylandDockingManager>();
    }
#endif
    return nullptr;
}
