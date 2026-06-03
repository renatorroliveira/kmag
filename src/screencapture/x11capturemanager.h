// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef X11CAPTUREMANAGER_H
#define X11CAPTUREMANAGER_H

#include "screencapturemanager.h"

/// Capture via QScreen::grabWindow(0, ...). Works on X11 (xcb); the default
/// non-Wayland backend.
class X11CaptureManager : public ScreenCaptureManager
{
    Q_OBJECT
public:
    explicit X11CaptureManager(QObject *parent = nullptr);
    ~X11CaptureManager() override;

    bool start() override;
    void stop() override;
    bool isReady() const override;
    QImage getFrame(const QRect &globalRegion) override;
    QString backendName() const override;
};

#endif // X11CAPTUREMANAGER_H
