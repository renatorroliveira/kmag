// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#include "dockcontrolpanel.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
// Combo index order matches KMagDock::Edge (Top=0, Bottom=1, Left=2, Right=3).
DockControlPanel::Edge edgeForIndex(int i)
{
    switch (i) {
    case 1: return KMagDock::Edge::Bottom;
    case 2: return KMagDock::Edge::Left;
    case 3: return KMagDock::Edge::Right;
    default: return KMagDock::Edge::Top;
    }
}
int indexForEdge(DockControlPanel::Edge e)
{
    switch (e) {
    case KMagDock::Edge::Bottom: return 1;
    case KMagDock::Edge::Left:   return 2;
    case KMagDock::Edge::Right:  return 3;
    default: return 0;
    }
}
QString edgeName(DockControlPanel::Edge e)
{
    switch (e) {
    case KMagDock::Edge::Bottom: return i18n("Bottom");
    case KMagDock::Edge::Left:   return i18n("Left");
    case KMagDock::Edge::Right:  return i18n("Right");
    default: return i18n("Top");
    }
}
}

DockControlPanel::DockControlPanel(QWidget *parent)
    : QWidget(parent)
{
    m_status = new QLabel(this);
    m_status->setWordWrap(true);

    m_edgeBox = new QComboBox(this);
    m_edgeBox->addItems({i18n("Top"), i18n("Bottom"), i18n("Left"), i18n("Right")});

    m_thicknessSpin = new QSpinBox(this);
    m_thicknessSpin->setRange(100, 4000);
    m_thicknessSpin->setSingleStep(10);
    m_thicknessSpin->setSuffix(i18n(" px"));

    m_undockButton = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-close")),
                                     i18n("Undock"), this);

    auto *form = new QFormLayout;
    form->addRow(i18n("Edge:"), m_edgeBox);
    form->addRow(i18n("Thickness:"), m_thicknessSpin);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_status);
    layout->addLayout(form);
    layout->addWidget(m_undockButton);
    layout->addStretch();

    connect(m_edgeBox, &QComboBox::currentIndexChanged, this, &DockControlPanel::onEdgeIndexChanged);
    connect(m_thicknessSpin, &QSpinBox::valueChanged, this, &DockControlPanel::onThicknessChanged);
    connect(m_undockButton, &QPushButton::clicked, this, &DockControlPanel::undockRequested);
}

void DockControlPanel::setState(Edge edge, int thickness)
{
    QSignalBlocker b1(m_edgeBox);
    QSignalBlocker b2(m_thicknessSpin);
    m_edgeBox->setCurrentIndex(indexForEdge(edge));
    m_thicknessSpin->setValue(thickness);
    updateStatusText(edge);
}

void DockControlPanel::onEdgeIndexChanged(int index)
{
    const Edge e = edgeForIndex(index);
    updateStatusText(e);
    Q_EMIT edgeChangeRequested(e);
}

void DockControlPanel::onThicknessChanged(int value)
{
    Q_EMIT thicknessChangeRequested(value);
}

void DockControlPanel::updateStatusText(Edge edge)
{
    m_status->setText(i18n("Magnifier docked to the <b>%1</b> screen edge.", edgeName(edge)));
}
