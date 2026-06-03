// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PIPEWIRECAPTUREMANAGER_H
#define PIPEWIRECAPTUREMANAGER_H

#include "screencapturemanager.h"

#include <QImage>
#include <QMutex>
#include <QPoint>
#include <QRect>
#include <atomic>
#include <cstdint>

// Opaque, file-scope private state (hides all pipewire/spa C types). Defined in
// the .cpp at file scope so file-local callbacks can name it without poking at a
// private nested type.
struct PipeWireBackendPrivate;

/// Capture via the XDG Desktop Portal ScreenCast interface + PipeWire. The
/// portable, upstream-acceptable Wayland path.
///
/// v1 limitation: a single picked output is streamed. getFrame() crops the
/// requested global region out of that output's latest frame (scale-aware:
/// logical region -> physical pixels, result tagged with the output's
/// device-pixel-ratio), matching the output to a QScreen by physical frame size.
/// This is still ambiguous on identical-resolution monitors, and a region
/// straddling the output edge is clamped (so the returned crop may be smaller
/// than the requested region, unlike the X11 backend). See TODO.md for
/// follow-ups.
class PipeWireCaptureManager : public ScreenCaptureManager
{
    Q_OBJECT
public:
    explicit PipeWireCaptureManager(QObject *parent = nullptr);
    ~PipeWireCaptureManager() override;

    bool start() override;
    void stop() override;
    bool isReady() const override;
    QImage getFrame(const QRect &globalRegion) override;
    QString backendName() const override;

    // Wayland global pointer position, derived from the stream's per-frame
    // cursor metadata (physical stream pixels -> global logical coords). Falls
    // back to the base QCursor::pos() until a frame with valid cursor metadata
    // has arrived.
    QPoint getCursorPosition() const override;

    // Called from the PipeWire thread to publish the newest full-output frame
    // together with that buffer's cursor position (in physical stream pixels;
    // @p cursorValid is false when the buffer carried no valid cursor metadata).
    void publishFrame(const QImage &fullFrame, const QPoint &cursorPhysical, bool cursorValid);
    void publishStreamError(const QString &message);

private:
    PipeWireBackendPrivate *d = nullptr;

    bool runPortalHandshake(int *pwFd, std::uint32_t *nodeId);
    bool startPipeWire(int fd, std::uint32_t nodeId);

    QString readRestoreToken() const;
    void writeRestoreToken(const QString &token);

    // Resolve which QScreen this stream is (output geometry, logical coords) by
    // matching the frame's physical size. Caches into m_outputGeometry. Caller
    // must hold m_mutex and m_latestFrame must be non-null.
    void resolveOutputGeometryLocked() const;

    // latest-frame buffer (written by PW thread, read by GUI thread)
    mutable QMutex m_mutex;
    QImage m_latestFrame;        // full streamed output, deep-copied
    mutable QRect m_outputGeometry; // global logical coords of the streamed output (lazy cache)
    std::atomic<bool> m_haveFrame{false};

    // latest cursor position, in physical stream pixels (guarded by m_mutex)
    QPoint m_cursorPhysical;
    bool m_haveCursor = false;
};

#endif // PIPEWIRECAPTUREMANAGER_H
