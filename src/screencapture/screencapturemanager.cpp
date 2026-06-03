// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "screencapturemanager.h"
#include "screencaptureconfig.h"
#include "x11capturemanager.h"

#if KMAG_WITH_PIPEWIRE
#include "pipewirecapturemanager.h"
#endif

#include <KWindowSystem>
#include <QCursor>
#include <QtGlobal>

#if !KMAG_WITH_PIPEWIRE
namespace
{
/// Placeholder used on Wayland when KMag was built without the PipeWire backend
/// (KMAG_WITH_PIPEWIRE=OFF). There is no other Wayland capture path, so start()
/// reports the same recoverable captureError() as a denied screen-capture
/// permission: the magnifier comes up, shows the "Screen Capture Unavailable"
/// notice once, and renders blank rather than crashing.
class UnavailableCaptureManager : public ScreenCaptureManager
{
public:
    explicit UnavailableCaptureManager(QObject *parent = nullptr)
        : ScreenCaptureManager(parent)
    {
    }

    bool start() override
    {
        Q_EMIT captureError(QStringLiteral(
            "This build of KMag has no Wayland screen-capture backend "
            "(compiled without PipeWire support)."));
        return false;
    }
    void stop() override {}
    bool isReady() const override { return false; }
    QImage getFrame(const QRect &) override { return QImage(); }
    QString backendName() const override
    {
        return QStringLiteral("Wayland/unavailable (built without PipeWire)");
    }
};
} // namespace
#endif

ScreenCaptureManager::ScreenCaptureManager(QObject *parent)
    : QObject(parent)
{
}

ScreenCaptureManager::~ScreenCaptureManager() = default;

QPoint ScreenCaptureManager::getCursorPosition() const
{
    // Correct on X11. The PipeWire backend overrides this because Wayland does
    // not expose the global pointer position when the cursor is over another
    // surface.
    return QCursor::pos();
}

ScreenCaptureManager *ScreenCaptureManager::create(QObject *parent)
{
    ScreenCaptureManager *mgr = nullptr;

    if (!KWindowSystem::isPlatformWayland()) {
        // X11 (xcb) or other: QScreen::grabWindow works.
        mgr = new X11CaptureManager(parent);
    } else {
        // Wayland: PipeWire (XDG ScreenCast portal) is the only backend.
#if KMAG_WITH_PIPEWIRE
        mgr = new PipeWireCaptureManager(parent);
#else
        mgr = new UnavailableCaptureManager(parent);
#endif
    }

    qInfo("KMag screen capture backend: %s", qPrintable(mgr->backendName()));
    return mgr;
}
