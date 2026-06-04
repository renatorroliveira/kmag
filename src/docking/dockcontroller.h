// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef KMAG_DOCKCONTROLLER_H
#define KMAG_DOCKCONTROLLER_H

#include "dockingmanager.h"
#include <QObject>
#include <QPointer>
#include <QRect>
#include <memory>

class QMainWindow;
class QScreen;
class QWidget;
class KMagZoomView;
class DockControlPanel;

// Callbacks the controller uses to keep host UI (menu actions, dock toggle) in
// sync and to reach the host's XmlGui context menu. KmagApp implements this.
class IDockHost
{
public:
    virtual ~IDockHost() = default;
    virtual QMainWindow *dockMainWindow() = 0;
    virtual void setDockToggleChecked(bool checked) = 0;   // sync m_dockMode (no signal)
    virtual void setDockEdgeChecked(DockingManager::Edge e) = 0; // sync edge group (no signal)
    virtual void setDockEdgeActionsEnabled(bool enabled) = 0;
    virtual void reassertSourceMode() = 0;                 // re-run slotModeChanged()
    virtual void popupDockContextMenu(const QPoint &globalPos) = 0;
    virtual void addStripActions(QWidget *strip) = 0;
    virtual void removeStripActions(QWidget *strip) = 0;
};

// Orchestrates docking: reparents the zoom view out to a frameless top-level,
// swaps a DockControlPanel into the main window, drives the backend, persists and
// reacts to screen changes.
class DockController : public QObject
{
    Q_OBJECT
public:
    using Edge = DockingManager::Edge;

    DockController(IDockHost *host, KMagZoomView *view, QObject *parent = nullptr);
    ~DockController() override;

    bool isAvailable() const { return m_backend != nullptr; }
    bool isDocked() const { return m_docked; }
    Edge edge() const { return m_edge; }
    int thickness() const { return m_thickness; }

    // Apply persisted config before any docking. Clamps thickness (>=100).
    void configure(Edge edge, int thickness, const QString &screenName);
    QString screenName() const;

    void enterDock();   // rolls back + leaves m_dockMode unchecked on failure
    void exitDock();
    // Close-path teardown: detach the dock machinery WITHOUT reparenting the
    // view (reparenting a widget that owns a native dock/LayerShell surface
    // corrupts the heap and double-frees at app teardown). The host then deletes
    // the orphaned top-level view; the control panel is destroyed with the
    // main window as its central-widget child.
    void prepareForClose();
    void setEdge(Edge edge);        // live
    void setThickness(int thickness); // live

public Q_SLOTS:
    void onScreenGeometryChanged();
    void onScreenRemoved(QScreen *screen);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QScreen *resolveScreen() const;        // saved name, else primary
    QRect basedAvailableGeometry(QScreen *screen) const;
    void connectScreenSignals();
    void disconnectScreenSignals();
    void rollbackFailedDock();

    IDockHost *m_host;
    QPointer<KMagZoomView> m_view;
    QPointer<DockControlPanel> m_panel;
    std::unique_ptr<DockingManager> m_backend;

    bool m_docked = false;
    Edge m_edge = Edge::Top;
    int m_thickness = 300;
    QPointer<QScreen> m_screen;
    QString m_screenName;       // persisted preference ("" => primary/active)
    bool m_savedFitToWindow = true;
    bool m_interiorEdgeWarned = false;
    int m_resTop = 0;
    int m_resBottom = 0;
    int m_resLeft = 0;
    int m_resRight = 0;
};

#endif // KMAG_DOCKCONTROLLER_H
