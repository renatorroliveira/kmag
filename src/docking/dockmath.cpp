// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#include "dockmath.h"

namespace KMagDock {

QRect stripRect(const QRect &a, Edge edge, int thickness)
{
    switch (edge) {
    case Edge::Top:    return QRect(a.x(), a.y(), a.width(), thickness);
    case Edge::Bottom: return QRect(a.x(), a.y() + a.height() - thickness, a.width(), thickness);
    case Edge::Left:   return QRect(a.x(), a.y(), thickness, a.height());
    case Edge::Right:  return QRect(a.x() + a.width() - thickness, a.y(), thickness, a.height());
    }
    return QRect();
}

Strut extendedStrut(const QRect &g, const QRect &a, const QRect &v, Edge edge, int thickness)
{
    Strut s{};
    const int resTop    = a.y() - g.y();
    const int resBottom = (g.y() + g.height()) - (a.y() + a.height());
    const int resLeft   = a.x() - g.x();
    const int resRight  = (g.x() + g.width()) - (a.x() + a.width());
    switch (edge) {
    case Edge::Top:
        s.v[6] = (g.y() - v.y()) + resTop + thickness;   // top_width (root-relative)
        s.v[7] = a.x();                                  // top_start_x
        s.v[8] = a.x() + a.width() - 1;                  // top_end_x
        break;
    case Edge::Bottom:
        s.v[9]  = (v.y() + v.height()) - (g.y() + g.height()) + resBottom + thickness; // bottom_width
        s.v[10] = a.x();                                 // bottom_start_x
        s.v[11] = a.x() + a.width() - 1;                 // bottom_end_x
        break;
    case Edge::Left:
        s.v[0] = (g.x() - v.x()) + resLeft + thickness;  // left_width
        s.v[1] = a.y();                                  // left_start_y
        s.v[2] = a.y() + a.height() - 1;                 // left_end_y
        break;
    case Edge::Right:
        s.v[3] = (v.x() + v.width()) - (g.x() + g.width()) + resRight + thickness; // right_width
        s.v[4] = a.y();                                  // right_start_y
        s.v[5] = a.y() + a.height() - 1;                 // right_end_y
        break;
    }
    return s;
}

bool edgeIsOnOuterBorder(const QRect &g, const QRect &v, Edge edge)
{
    switch (edge) {
    case Edge::Top:    return g.y() == v.y();
    case Edge::Bottom: return (g.y() + g.height()) == (v.y() + v.height());
    case Edge::Left:   return g.x() == v.x();
    case Edge::Right:  return (g.x() + g.width()) == (v.x() + v.width());
    }
    return false;
}

} // namespace KMagDock
