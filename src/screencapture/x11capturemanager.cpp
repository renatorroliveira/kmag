// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "x11capturemanager.h"

#include <QApplication>
#include <QCursor>
#include <QPixmap>
#include <QScreen>

X11CaptureManager::X11CaptureManager(QObject *parent)
    : ScreenCaptureManager(parent)
{
}

X11CaptureManager::~X11CaptureManager() = default;

bool X11CaptureManager::start()
{
    return true; // grabWindow is always available on xcb; nothing to set up
}

void X11CaptureManager::stop()
{
}

bool X11CaptureManager::isReady() const
{
    return true;
}

QString X11CaptureManager::backendName() const
{
    return QStringLiteral("X11/grabWindow");
}

QImage X11CaptureManager::getFrame(const QRect &globalRegion)
{
    // Pick the screen under the (clamped) cursor, exactly as the old grabFrame did.
    QPoint cursorPos = QCursor::pos();
    const QRect vg = qApp->primaryScreen()->virtualGeometry();
    cursorPos.rx() = qBound(vg.left(), cursorPos.x(), vg.right());
    cursorPos.ry() = qBound(vg.top(), cursorPos.y(), vg.bottom());

    QScreen *screen = qApp->primaryScreen();
    const auto screens = qApp->screens();
    for (QScreen *s : screens) {
        if (s->geometry().contains(cursorPos)) {
            screen = s;
            break;
        }
    }
    if (!screen) {
        return QImage();
    }

    // grabWindow takes device-independent (logical) coordinates and, on a scaled
    // display, returns a pixmap whose physical size is larger by the screen's
    // device-pixel-ratio with that ratio set on it. QPixmap::toImage() preserves
    // the ratio, so the returned image already has the right scale tag -- no
    // manual scaling needed here (the Wayland backends reproduce this parity).
    const QPixmap pm = screen->grabWindow(
        0, // WId == 0 -> root window / desktop
        globalRegion.x() - screen->geometry().left(),
        globalRegion.y() - screen->geometry().top(),
        globalRegion.width(), globalRegion.height());
    return pm.toImage();
}
