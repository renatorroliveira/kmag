// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SCREENCAPTUREMANAGER_H
#define SCREENCAPTUREMANAGER_H

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QString>

/**
 * Abstract, synchronous screen-capture backend.
 *
 * KMag pulls one frame per refresh tick via getFrame(). The X11 backend
 * captures on demand; the PipeWire backend keeps a mutex-protected latest
 * frame filled by its own thread and getFrame() returns a crop of it. On an
 * unrecoverable error a backend returns a null QImage from getFrame() and emits
 * captureError() exactly once.
 */
class ScreenCaptureManager : public QObject
{
    Q_OBJECT
public:
    explicit ScreenCaptureManager(QObject *parent = nullptr);
    ~ScreenCaptureManager() override;

    /// Bring the backend up. Returns false on failure (and emits captureError()).
    virtual bool start() = 0;

    /// Tear the backend down and release streams/resources.
    virtual void stop() = 0;

    /// True once the backend can return real frames.
    virtual bool isReady() const = 0;

    /// Pixels of @p globalRegion (global virtual-desktop coordinates) as a
    /// QImage anchored at @p globalRegion.topLeft(). Null QImage when no frame
    /// is available or on error. The X11 backend always returns the full
    /// requested region; the PipeWire backend (single picked output, v1)
    /// may return a smaller crop when @p globalRegion straddles the captured
    /// output's edge.
    virtual QImage getFrame(const QRect &globalRegion) = 0;

    /// Short backend name, for logging.
    virtual QString backendName() const = 0;

    /// Current global (virtual-desktop, logical) pointer position, used by
    /// follow-mouse to centre the magnified region. The default reads
    /// QCursor::pos(), which is correct on X11. Wayland never hands a client the
    /// global pointer position while it is over another surface, so the PipeWire
    /// backend overrides this with the per-frame cursor metadata from the
    /// ScreenCast stream.
    virtual QPoint getCursorPosition() const;

    /// Construct the backend appropriate for the running session. Ownership is
    /// the caller's (or @p parent's). Never returns nullptr.
    static ScreenCaptureManager *create(QObject *parent = nullptr);

Q_SIGNALS:
    /// Emitted once when the backend hits an unrecoverable capture error.
    void captureError(const QString &message);
};

#endif // SCREENCAPTUREMANAGER_H
