// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KMAG_FRAMECROP_H
#define KMAG_FRAMECROP_H

#include <QImage>
#include <QRect>

namespace KMagCapture
{

/// Crop the logical-coordinate @p globalRegion out of a captured full-frame.
///
/// @p frame holds the raw physical pixels of one output (devicePixelRatio 1),
/// e.g. 3440x1440 for a monitor driven at scale 2. @p outputGeometryLogical is
/// that output's geometry in global, device-independent (logical) coordinates
/// as reported by QScreen::geometry(), e.g. (0,0 1720x720). @p globalRegion is
/// the wanted region in that same logical coordinate space (KMag's selRect).
///
/// The region is translated into output-local logical coordinates, scaled to
/// physical pixels by the per-axis ratio frame.size()/outputGeometryLogical.size()
/// (the output's effective device-pixel-ratio), clamped to the frame, and
/// cropped. The returned image carries devicePixelRatio == that ratio, so its
/// device-independent size equals @p globalRegion -- matching what
/// QScreen::grabWindow() returns on a scaled display, which KMagZoomView's
/// paintEvent() relies on (it centres via width()/devicePixelRatio()).
///
/// Returns a null QImage if any input is degenerate or the mapped region is
/// empty. Shared by PipeWireCaptureManager::getFrame() and the crop harness so
/// both exercise identical coordinate math.
QImage cropGlobalRegion(const QImage &frame,
                        const QRect &outputGeometryLogical,
                        const QRect &globalRegion);

} // namespace KMagCapture

#endif // KMAG_FRAMECROP_H
