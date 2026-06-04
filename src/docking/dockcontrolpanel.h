// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef KMAG_DOCKCONTROLPANEL_H
#define KMAG_DOCKCONTROLPANEL_H

#include "dockingmanager.h"
#include <QWidget>

class QLabel;
class QComboBox;
class QSpinBox;
class QPushButton;

// Shown in the main window's central area while the zoom view is docked. Lets the
// user change edge/thickness and undock, mirroring the View-menu edge group.
class DockControlPanel : public QWidget
{
    Q_OBJECT
public:
    using Edge = DockingManager::Edge;

    explicit DockControlPanel(QWidget *parent = nullptr);

    // Reflect external state changes (menu edge group, config) without emitting.
    void setState(Edge edge, int thickness);

Q_SIGNALS:
    void edgeChangeRequested(DockingManager::Edge edge);
    void thicknessChangeRequested(int thickness);
    void undockRequested();

private Q_SLOTS:
    void onEdgeIndexChanged(int index);
    void onThicknessChanged(int value);

private:
    QLabel *m_status;
    QComboBox *m_edgeBox;
    QSpinBox *m_thicknessSpin;
    QPushButton *m_undockButton;

    void updateStatusText(Edge edge);
};

#endif // KMAG_DOCKCONTROLPANEL_H
