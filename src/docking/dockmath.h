// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef KMAG_DOCKMATH_H
#define KMAG_DOCKMATH_H

#include <QRect>

// Pure geometry for docked mode. No widget / KWindowSystem dependencies so it
// can be unit-checked by a standalone harness. See docs/research/
// docked-mode-implementation-findings.md section C for the strut conventions.
namespace KMagDock {

enum class Edge { Top, Bottom, Left, Right };

// On-screen rectangle the docked strip occupies, in the coordinate space of
// `availGeom` (the work area / QScreen::availableGeometry()), so the strip sits
// adjacent to (outside) any existing panels rather than over them.
QRect stripRect(const QRect &availGeom, Edge edge, int thickness);

// _NET_WM_STRUT_PARTIAL values laid out in the exact argument order of
// KX11Extras::setExtendedStrut(WId, ...):
//   [0..2]  left_width,  left_start,  left_end
//   [3..5]  right_width, right_start, right_end
//   [6..8]  top_width,   top_start,   top_end
//   [9..11] bottom_width,bottom_start,bottom_end
// Struts are root-window-relative, hence `virtualGeom` (the full virtual desktop).
struct Strut {
    long v[12];
};
// `screenGeom` anchors the strut to the true screen edge; `availGeom` is the
// panel-only work area (its inset past `screenGeom` is the space already
// reserved by other panels, which this strut spans through); `virtualGeom`
// makes widths root-relative on multi-monitor.
Strut extendedStrut(const QRect &screenGeom, const QRect &availGeom,
                    const QRect &virtualGeom, Edge edge, int thickness);

// True when `edge` of `screenGeom` is flush with the outer border of the virtual
// desktop. KWin ignores struts on interior multi-monitor edges, so the caller
// warns when this is false. (findings §C, pitfall)
bool edgeIsOnOuterBorder(const QRect &screenGeom, const QRect &virtualGeom, Edge edge);

} // namespace KMagDock

#endif // KMAG_DOCKMATH_H
