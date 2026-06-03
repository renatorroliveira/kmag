// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "framecrop.h"

#include <QtGlobal>

namespace KMagCapture
{

QImage cropGlobalRegion(const QImage &frame,
                        const QRect &outputGeometryLogical,
                        const QRect &globalRegion)
{
    if (frame.isNull() || outputGeometryLogical.isEmpty() || globalRegion.isEmpty()) {
        return QImage();
    }

    // logical (global) -> logical (output-local)
    const QRect localLogical = globalRegion.translated(-outputGeometryLogical.topLeft());

    // physical-pixels-per-logical-pixel for this output (its device-pixel-ratio).
    // Derived from the actual frame rather than a trusted scalar so a frame that
    // is delivered at a slightly different resolution than nominal still maps
    // correctly.
    const qreal sx = qreal(frame.width()) / qreal(outputGeometryLogical.width());
    const qreal sy = qreal(frame.height()) / qreal(outputGeometryLogical.height());

    QRect localPhysical(qRound(localLogical.x() * sx),
                        qRound(localLogical.y() * sy),
                        qRound(localLogical.width() * sx),
                        qRound(localLogical.height() * sy));
    localPhysical &= frame.rect();
    if (localPhysical.isEmpty()) {
        return QImage();
    }

    QImage crop = frame.copy(localPhysical);
    // Tag the crop with the output scale so its device-independent size matches
    // globalRegion (grabWindow parity); KMagZoomView divides width()/height() by
    // this ratio when centring the magnified content.
    crop.setDevicePixelRatio((sx + sy) / 2.0);
    return crop;
}

} // namespace KMagCapture
