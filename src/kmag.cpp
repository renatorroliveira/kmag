// SPDX-FileCopyrightText: 2001-2003 Sarang Lakare <sarang@users.sourceforge.net>
// SPDX-FileCopyrightText: 2003-2004 Olaf Schmidt <ojschmidt@kde.org>
// SPDX-FileCopyrightText: 2008 Matthew Woehlke <mw_triad@users.sourceforge.net>
// SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kmag.h"

// include files for Qt
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFileDialog>
#include <QImageWriter>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPrintDialog>
#include <QScreen>
#include <QTemporaryFile>

// include files for KF5
#include <KActionCollection>
#include <KConfigGroup>
#include <KEditToolBar>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSelectAction>
#include <KShortcutsDialog>
#include <KToggleAction>
#include <KXMLGUIFactory>
#include <KIO/FileCopyJob>

// application specific includes
#include "kmagselrect.h"


KmagApp::KmagApp(QWidget*)
  : KXmlGuiWindow(nullptr) // Qt::WStyle_MinMax | Qt::WType_TopLevel | Qt::WDestructiveClose | Qt::WStyle_ContextHelp | Qt::WindowCloseButtonHint | Qt::WStyle_StaysOnTop
  , m_defaultMouseCursorType(2)
{
  setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  config=KSharedConfig::openConfig();

  zoomArrayString << QStringLiteral( "5:1" ) << QStringLiteral( "2:1" ) << QStringLiteral( "1:1" ) << QStringLiteral( "1:1.5" ) << QStringLiteral( "1:2" ) << QStringLiteral( "1:3" ) << QStringLiteral( "1:4" ) << QStringLiteral( "1:5" )
    << QStringLiteral( "1:6" ) << QStringLiteral( "1:7" ) << QStringLiteral( "1:8" ) << QStringLiteral( "1:9" ) << QStringLiteral( "1:10" ) << QStringLiteral( "1:11" ) << QStringLiteral( "1:12" ) << QStringLiteral( "1:16" ) << QStringLiteral( "1:20" );

  zoomArray << 0.2 <<0.5 << 1.0;
  zoomArray << 1.5 << 2.0 << 3.0;
  zoomArray << 4.0 << 5.0 << 6.0 << 7.0;
  zoomArray << 8.0 << 9.0 << 10.0 << 11.0 << 12.0 << 16.0 << 20.0;

  fpsArrayString << i18nc("Zoom at very low", "&Very Low") << i18nc("Zoom at low", "&Low") << i18nc("Zoom at medium", "&Medium") << i18nc("Zoom at high", "&High") << i18nc("Zoom at very high", "V&ery High");

  fpsArray.append(5); // very low
  fpsArray.append(10); // low
  fpsArray.append(20); // medium
  fpsArray.append(30); // high
  fpsArray.append(60); // very high

  colorArrayString << i18nc("No color-blindness simulation, i.e. 'normal' vision", "&Normal") << i18n("&Protanopia") << i18n("&Deuteranopia") << i18n("&Tritanopia") << i18n("&Achromatopsia");

  colorArray.append(0);
  colorArray.append(1);
  colorArray.append(2);
  colorArray.append(3);
  colorArray.append(4);

  rotationArrayString << i18n("&No Rotation (0 Degrees)") << i18n("&Left (90 Degrees)") << i18n("&Upside Down (180 Degrees)") << i18n("&Right (270 Degrees)");

  rotationArray.append(0); // no rotation
  rotationArray.append(90); // left
  rotationArray.append(180); // upside down
  rotationArray.append(270); // right

  // call inits to invoke all other construction parts
  initView();
  initActions();
  initConnections();

  // Docked mode controller: requires both the zoom view (initView) and the
  // dock actions (initActions) to already exist.
  m_dockController = new DockController(this, m_zoomView, this);
  if (!m_dockController->isAvailable()) {
      m_dockMode->setEnabled(false);
      m_dockMode->setToolTip(i18n("Docked mode is not available in this session "
                                  "(needs X11, or a Wayland build with LayerShellQt)."));
  }
  setDockEdgeActionsEnabled(false);

  // read options from config file
  readOptions();
}

/**
 * Default destructor.
 */
KmagApp::~KmagApp()
{
    // m_zoomView is null here if we closed while docked (queryClose deleted the
    // orphaned top-level view and cleared the pointer); guard against that.
    if (m_zoomView) {
        m_zoomView->showSelRect(false);
    }

    #ifndef QT_NO_PRINTER
    delete m_printer;
    #endif // QT_NO_PRINTER
}

void KmagApp::initActions()
{
  fileNewWindow = actionCollection()->addAction(QStringLiteral( "new_window" ));
  fileNewWindow->setIcon(QIcon::fromTheme(QStringLiteral( "window-new" )));
  fileNewWindow->setText(i18n("New &Window"));
  connect(fileNewWindow, &QAction::triggered, this, &KmagApp::slotFileNewWindow);
  actionCollection()->setDefaultShortcuts(fileNewWindow, KStandardShortcut::openNew());
  fileNewWindow->setToolTip(i18n("Open a new KMagnifier window"));

  refreshSwitch = actionCollection()->addAction(QStringLiteral( "start_stop_refresh" ));
  refreshSwitch->setIcon(QIcon::fromTheme(QStringLiteral( "process-stop" )));
  refreshSwitch->setText(i18n("&Stop"));
  connect(refreshSwitch, &QAction::triggered, this, &KmagApp::slotToggleRefresh);
  actionCollection()->setDefaultShortcuts(refreshSwitch, KStandardShortcut::reload());
  refreshSwitch->setToolTip(i18n("Click to stop window refresh"));
  refreshSwitch->setWhatsThis(i18n("Clicking on this icon will <b>start</b> / <b>stop</b> "
                                   "updating of the display. Stopping the update will zero the processing power "
                                   "required (CPU usage)"));

  m_pSnapshot = actionCollection()->addAction(QStringLiteral( "snapshot" ));
  m_pSnapshot->setIcon(QIcon::fromTheme(QStringLiteral( "ksnapshot" )));
  m_pSnapshot->setText(i18n("&Save Snapshot As..."));
  connect(m_pSnapshot, &QAction::triggered, this, &KmagApp::saveZoomPixmap);
  actionCollection()->setDefaultShortcuts(m_pSnapshot, KStandardShortcut::save());
  m_pSnapshot->setWhatsThis(i18n("Saves the zoomed view to an image file."));
  m_pSnapshot->setToolTip(i18n("Save image to a file"));

  m_pPrint = actionCollection()->addAction(KStandardAction::Print, this, SLOT(slotFilePrint()));
  m_pPrint->setWhatsThis(i18n("Click on this button to print the current zoomed view."));

  m_pQuit = actionCollection()->addAction(KStandardAction::Quit, this, SLOT(slotFileQuit()));
  m_pQuit->setToolTip(i18n("Quits the application"));
  m_pQuit->setWhatsThis (i18n("Quits the application"));

  m_pCopy = actionCollection()->addAction(KStandardAction::Copy, this, SLOT(copyToClipBoard()));
  m_pCopy->setWhatsThis(i18n("Click on this button to copy the current zoomed view to the clipboard which you can paste in other applications."));
  m_pCopy->setToolTip(i18n("Copy zoomed image to clipboard"));

  m_pShowMenu = KStandardAction::showMenubar(this, SLOT(slotShowMenu()), actionCollection());

  setStandardToolBarMenuEnabled(true);

  m_modeFollowMouse = new KToggleAction(QIcon::fromTheme(QStringLiteral( "followmouse" )), i18n("&Follow Mouse Mode"), this);
  actionCollection()->addAction(QStringLiteral( "mode_followmouse" ), m_modeFollowMouse);
  connect(m_modeFollowMouse, &QAction::triggered, this, &KmagApp::slotModeChanged);
  actionCollection()->setDefaultShortcut(m_modeFollowMouse, Qt::Key_F2);
  m_modeFollowMouse->setIconText(i18n("Mouse"));
  m_modeFollowMouse->setToolTip(i18n("Magnify around the mouse cursor"));
  m_modeFollowMouse->setWhatsThis(i18n("If selected, the area around the mouse cursor is magnified"));

#if HAVE_QACCESSIBILITYCLIENT

  m_modeFollowFocus = new KToggleAction(QIcon::fromTheme(QStringLiteral( "view-restore" )), i18n("&Follow Focus Mode"), this);
  actionCollection()->addAction(QStringLiteral( "mode_followfocus" ), m_modeFollowFocus);
  connect(m_modeFollowFocus, SIGNAL(triggered(bool)), SLOT(slotModeChanged()));
  actionCollection()->setDefaultShortcut(m_modeFollowFocus, Qt::Key_F8);
  m_modeFollowFocus->setIconText(i18n("Focus"));
  m_modeFollowFocus->setToolTip(i18n("Magnify around the keyboard focus"));
  m_modeFollowFocus->setWhatsThis(i18n("If selected, the area around the keyboard cursor is magnified"));

#endif

  m_modeSelWin = new KToggleAction(QIcon::fromTheme(QStringLiteral( "window" )), i18n("Se&lection Window Mode"), this);
  actionCollection()->addAction(QStringLiteral( "mode_selectionwindow" ), m_modeSelWin);
  connect(m_modeSelWin, &QAction::triggered, this, &KmagApp::slotModeSelWin);
  actionCollection()->setDefaultShortcut(m_modeSelWin, Qt::Key_F3);
  m_modeSelWin->setIconText(i18n("Window"));
  m_modeSelWin->setToolTip(i18n("Show a window for selecting the magnified area"));

  m_modeWholeScreen = new KToggleAction(QIcon::fromTheme(QStringLiteral( "view-fullscreen" )), i18n("&Whole Screen Mode"), this);
  actionCollection()->addAction(QStringLiteral( "mode_wholescreen" ), m_modeWholeScreen);
  connect(m_modeWholeScreen, &QAction::triggered, this, &KmagApp::slotModeWholeScreen);
  actionCollection()->setDefaultShortcut(m_modeWholeScreen, Qt::Key_F4);
  m_modeWholeScreen->setIconText(i18n("Screen"));
  m_modeWholeScreen->setToolTip(i18n("Magnify the whole screen"));
  m_modeWholeScreen->setWhatsThis(i18n("Click on this button to fit the zoom view to the zoom window."));

  m_hideCursor = new KToggleAction(QIcon::fromTheme(QStringLiteral( "hidemouse" )), i18n("Hide Mouse &Cursor"), this);
  actionCollection()->addAction(QStringLiteral( "hidecursor" ), m_hideCursor);
  connect(m_hideCursor, &QAction::triggered, this, &KmagApp::slotToggleHideCursor);
  actionCollection()->setDefaultShortcut(m_hideCursor, Qt::Key_F6);
  #ifdef havesetCheckedStatef
  m_hideCursor->setCheckedState(KGuiItem(i18n("Show Mouse &Cursor")));
  #endif
  m_hideCursor->setIconText(i18n("Hide"));
  m_hideCursor->setToolTip(i18n("Hide the mouse cursor"));

  m_staysOnTop = new KToggleAction(QIcon::fromTheme(QStringLiteral( "go-top" )), i18n("Stays On Top"), this);
  actionCollection()->addAction(QStringLiteral( "staysontop" ), m_staysOnTop);
  connect(m_staysOnTop, &QAction::triggered, this, &KmagApp::slotStaysOnTop);
  actionCollection()->setDefaultShortcut(m_staysOnTop, Qt::Key_F7);
  m_staysOnTop->setToolTip(i18n("The KMagnifier Window stays on top of other windows."));

  m_pZoomIn = actionCollection()->addAction(KStandardAction::ZoomIn, this, SLOT(zoomIn()));
  m_pZoomIn->setWhatsThis(i18n("Click on this button to <b>zoom-in</b> on the selected region."));

  m_pZoomBox = new KSelectAction(i18n("&Zoom"), this);
  actionCollection()->addAction(QStringLiteral( "zoom" ), m_pZoomBox);
  m_pZoomBox->setItems(zoomArrayString);
  m_pZoomBox->setWhatsThis(i18n("Select the zoom factor."));
  m_pZoomBox->setToolTip(i18n("Zoom factor"));

  m_pZoomOut = actionCollection()->addAction(KStandardAction::ZoomOut, this, SLOT(zoomOut()));
  m_pZoomOut->setWhatsThis(i18n("Click on this button to <b>zoom-out</b> on the selected region."));

  m_pRotationBox = new KSelectAction(i18n("&Rotation"),this);
  actionCollection()->addAction(QStringLiteral( "rotation" ), m_pRotationBox);
  m_pRotationBox->setItems(rotationArrayString);
  m_pRotationBox->setWhatsThis(i18n("Select the rotation degree."));
  m_pRotationBox->setToolTip(i18n("Rotation degree"));

  m_keyConf = actionCollection()->addAction(KStandardAction::KeyBindings, this, SLOT(slotConfKeys()));
  m_toolConf = actionCollection()->addAction(KStandardAction::ConfigureToolbars, this, SLOT(slotEditToolbars()));

  m_pFPSBox = new KSelectAction(i18n("&Refresh"),this);
  actionCollection()->addAction(QStringLiteral( "fps_selector" ), m_pFPSBox);
  m_pFPSBox->setItems(fpsArrayString);
  m_pFPSBox->setWhatsThis(i18n("Select the refresh rate. The higher the rate, the more computing power (CPU) will be needed."));
  m_pFPSBox->setToolTip(i18n("Refresh rate"));

  m_pColorBox = new KSelectAction(i18nc("Color-blindness simulation mode", "&Color"),this);
  actionCollection()->addAction(QStringLiteral( "color_mode" ), m_pColorBox);
  m_pColorBox->setItems(colorArrayString);
  m_pColorBox->setWhatsThis(i18n("Select a mode to simulate various types of color-blindness."));
  m_pColorBox->setToolTip(i18n("Color-blindness Simulation Mode"));

  // --- Docked mode ---
  m_dockMode = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")),
                                 i18n("Dock to Screen &Edge"), this);
  actionCollection()->addAction(QStringLiteral("mode_dock"), m_dockMode);
  connect(m_dockMode, &QAction::toggled, this, &KmagApp::slotToggleDock);
  actionCollection()->setDefaultShortcut(m_dockMode, Qt::Key_F9);
  // The docked strip is a NET::Dock top-level that cannot take keyboard focus
  // on X11, so F9 must be application-wide to remain usable while docked.
  m_dockMode->setShortcutContext(Qt::ApplicationShortcut);
  m_dockMode->setIconText(i18n("Dock"));
  m_dockMode->setToolTip(i18n("Dock the magnified view to a screen edge and reserve that space"));
  m_dockMode->setWhatsThis(i18n("If selected, the magnified view is docked to a screen edge "
                                "and reserves that strip of screen space, like a panel."));

  m_dockEdgeGroup = new QActionGroup(this);
  m_dockEdgeGroup->setExclusive(true);

  m_dockEdgeTop = new KToggleAction(i18n("Dock to &Top Edge"), this);
  actionCollection()->addAction(QStringLiteral("dock_edge_top"), m_dockEdgeTop);
  m_dockEdgeGroup->addAction(m_dockEdgeTop);
  m_dockEdgeTop->setChecked(true);

  m_dockEdgeBottom = new KToggleAction(i18n("Dock to &Bottom Edge"), this);
  actionCollection()->addAction(QStringLiteral("dock_edge_bottom"), m_dockEdgeBottom);
  m_dockEdgeGroup->addAction(m_dockEdgeBottom);

  m_dockEdgeLeft = new KToggleAction(i18n("Dock to &Left Edge"), this);
  actionCollection()->addAction(QStringLiteral("dock_edge_left"), m_dockEdgeLeft);
  m_dockEdgeGroup->addAction(m_dockEdgeLeft);

  m_dockEdgeRight = new KToggleAction(i18n("Dock to &Right Edge"), this);
  actionCollection()->addAction(QStringLiteral("dock_edge_right"), m_dockEdgeRight);
  m_dockEdgeGroup->addAction(m_dockEdgeRight);

  connect(m_dockEdgeGroup, &QActionGroup::triggered, this, &KmagApp::slotDockEdgeChanged);

  setupGUI(ToolBar | Keys | Save | Create);
}

void KmagApp::initView()
{
  m_zoomView = new KMagZoomView( this, "ZoomView" );
//m_zoomView->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)7, m_zoomView->sizePolicy().hasHeightForWidth() ) );
  m_zoomView->setFrameShape( QFrame::NoFrame );

  setCentralWidget(m_zoomView);
}


void KmagApp::slotChangeZoomBoxIndex(int index)
{
   m_pZoomBox->setCurrentItem(index);
}

void KmagApp::slotChangeRotationBoxIndex(int index)
{
  m_pRotationBox->setCurrentItem(index);
}

void KmagApp::slotChangeFPSIndex(int index)
{
  m_pFPSBox->setCurrentItem(index);
}

void KmagApp::slotChangeColorIndex(int index)
{
  m_pColorBox->setCurrentItem(index);
}

/**
 * Initialize all connections.
 */
void KmagApp::initConnections()
{
  // change in zoom value -> update the view
  connect(this, &KmagApp::updateZoomValue, m_zoomView, &KMagZoomView::setZoom);
  connect(this, &KmagApp::updateRotationValue, m_zoomView, &KMagZoomView::setRotation);
  connect(this, &KmagApp::updateFPSValue, m_zoomView, &KMagZoomView::setRefreshRate);
  connect(this, &KmagApp::updateColorValue, m_zoomView, &KMagZoomView::setColorMode);

  // change in zoom index -> update the selector
  connect(this, &KmagApp::updateZoomIndex, this, &KmagApp::slotChangeZoomBoxIndex);
  connect(this, &KmagApp::updateRotationIndex, this, &KmagApp::slotChangeRotationBoxIndex);
  connect(this, &KmagApp::updateFPSIndex, this, &KmagApp::slotChangeFPSIndex);
  connect(this, &KmagApp::updateColorIndex, this, &KmagApp::slotChangeColorIndex);

  // selector selects a zoom index -> set the zoom index
  connect(m_pZoomBox, &KSelectAction::indexTriggered, this, &KmagApp::setZoomIndex);
  connect(m_pRotationBox, &KSelectAction::indexTriggered, this, &KmagApp::setRotationIndex);
  connect(m_pFPSBox, &KSelectAction::indexTriggered, this, &KmagApp::setFPSIndex);
  connect(m_pColorBox, &KSelectAction::indexTriggered, this, &KmagApp::setColorIndex);
}

/**
 * Save options to config file.
 */
void KmagApp::saveOptions()
{
  KConfigGroup cg( config, QStringLiteral("General Options"));
  cg.writeEntry("Geometry", size());
  cg.writeEntry("ZoomIndex", m_zoomIndex);
  cg.writeEntry("RotationIndex", m_rotationIndex);
  cg.writeEntry("FPSIndex", m_fpsIndex);
  cg.writeEntry("ColorIndex", m_colorIndex);
  cg.writeEntry("SelRect", m_zoomView->getSelRectPos());
  cg.writeEntry("ShowMouse", m_zoomView->getShowMouseType());
  cg.writeEntry("StaysOnTop", m_staysOnTop->isChecked());

  if (m_modeFollowMouse->isChecked())
     cg.writeEntry("Mode", "followmouse");
#if HAVE_QACCESSIBILITYCLIENT
  else if (m_modeFollowFocus->isChecked())
     cg.writeEntry("Mode", "followfocus");
#endif
  else if (m_modeWholeScreen->isChecked())
     cg.writeEntry("Mode", "wholescreen");
  else if (m_modeSelWin->isChecked())
     cg.writeEntry("Mode", "selectionwindow");

  if (m_dockController) {
    cg.writeEntry("Docked", m_dockController->isDocked());
    cg.writeEntry("DockEdge", static_cast<int>(m_dockController->edge()));
    cg.writeEntry("DockThickness", m_dockController->thickness());
    cg.writeEntry("DockScreen", m_dockController->screenName());
  }
}


/**
 * Read settings from config file.
 */
void KmagApp::readOptions()
{
  QColor blue (0,0,128);
  QColor yellow (255,255,0);
  QColor white (255,255,255);

  KConfigGroup cgWM( config, QStringLiteral("WM"));
  setTitleColors (
      cgWM.readEntry("inactiveBackground", blue),
      cgWM.readEntry("inactiveForeground", white),
      cgWM.readEntry("inactiveTitleBtnBg", yellow));

  KConfigGroup cg(config, QStringLiteral("General Options"));

  QSize defSize(460,390);
  QSize size=cg.readEntry("Geometry", defSize);
  if(!size.isEmpty())
  {
    resize(size);
  }

  // set zoom - defaults to 2x
  unsigned int zoomIndex = cg.readEntry("ZoomIndex", 4);
  setZoomIndex(zoomIndex);
  Q_EMIT updateZoomIndex(m_zoomIndex);

  unsigned int rotationIndex = cg.readEntry("RotationIndex", 0);
  setRotationIndex(rotationIndex);
  Q_EMIT updateRotationIndex(m_rotationIndex);

  unsigned int fpsIndex = cg.readEntry("FPSIndex", 2);
  setFPSIndex(fpsIndex);
  Q_EMIT updateFPSIndex(m_fpsIndex);

  unsigned int colorIndex = cg.readEntry("ColorIndex", 0);
  setColorIndex(colorIndex);
  Q_EMIT updateColorIndex(colorIndex);

  QString mode = cg.readEntry("Mode", "followmouse");
  if (mode == QLatin1String( "wholescreen" )) {
    slotModeWholeScreen();
  } else if (mode == QLatin1String( "selectionwindow" )) {
    slotModeSelWin();
  } else if (mode == QLatin1String( "followfocus" )) {
    m_modeFollowFocus->setChecked(true);
    slotModeChanged();
  } else {
    m_modeFollowMouse->setChecked(true);
    slotModeChanged();
  }

  QRect defaultRect(0,0,211,164);
  m_zoomView->setSelRectPos(cg.readEntry("SelRect", defaultRect));

  m_mouseCursorType = cg.readEntry("ShowMouse", m_defaultMouseCursorType);
  m_zoomView->showMouse(m_mouseCursorType);
  m_hideCursor->setChecked(!m_mouseCursorType);

  m_staysOnTop->setChecked(cg.readEntry("StaysOnTop", true));
  slotStaysOnTop();

  // XMLGui has already read and set up the menuBar for us
  m_pShowMenu->setChecked(menuBar()->isVisible());

  if (m_dockController) {
    const int edgeInt = cg.readEntry("DockEdge", static_cast<int>(DockingManager::Edge::Top));
    const DockingManager::Edge edge =
        (edgeInt >= 0 && edgeInt <= 3) ? static_cast<DockingManager::Edge>(edgeInt)
                                       : DockingManager::Edge::Top;
    const int thickness = cg.readEntry("DockThickness", 300);
    const QString screenName = cg.readEntry("DockScreen", QString());
    m_dockController->configure(edge, thickness, screenName);
    setDockEdgeChecked(edge);

    if (cg.readEntry("Docked", false) && m_dockController->isAvailable()) {
      // Defer until the event loop is running so screens/geometry are settled.
      QMetaObject::invokeMethod(this, [this] {
        QSignalBlocker block(m_dockMode);
        m_dockMode->setChecked(true);
        m_dockController->enterDock();
      }, Qt::QueuedConnection);
    }
  }
}

bool KmagApp::queryClose()
{
  // Persist the live docked state first so a docked session is restored on the
  // next launch; then tear the dock down so the strip top-level isn't orphaned
  // and the reserved strut is released.
  saveOptions();
  if (m_dockController && m_dockController->isDocked()) {
    // Tear the dock down WITHOUT reparenting (that corrupts the native surface
    // and double-frees at teardown). While docked the zoom view is an orphan
    // top-level, so destroy it ourselves; the dock control panel is the main
    // window's central widget and is destroyed automatically with the window.
    m_dockController->prepareForClose();
    m_zoomView->deleteLater();
    m_zoomView = nullptr;
  }
  return (true);
}

/**
 * Called when mouse is clicked inside the window
 *
 * @param e
 */
void KmagApp::contextMenuEvent ( QContextMenuEvent * e )
{
 // show popup
 KXMLGUIFactory *factory = this->factory();
 QMenu *popup = (QMenu *)factory->container(QStringLiteral( "mainPopUp" ),this);
 if (popup != nullptr)
   popup->popup(e->globalPos());
 e->accept();
}


/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

/**
 * Shows/hides the mouse cursor
 */
void KmagApp::showMouseCursor(bool show)
{
  if(show) {
    if(m_mouseCursorType == 0)
      m_mouseCursorType = m_defaultMouseCursorType;
    m_zoomView->showMouse(m_mouseCursorType);
  } else {
    m_zoomView->showMouse(0);
  }
}

/**
 * Zoom in.
 */
void KmagApp::zoomIn()
{
  // set the new index .. checking will done inside setZoom
  setZoomIndex(m_zoomIndex+1);
  // signal change in zoom index
  Q_EMIT updateZoomIndex((int)m_zoomIndex);
}

/**
 * Zoom out.
 */
void KmagApp::zoomOut()
{
  // set the new index .. checking will done inside setZoom
  setZoomIndex(m_zoomIndex-1);
  // signal change in zoom index
  Q_EMIT updateZoomIndex((int)m_zoomIndex);
}
/**
 * Sets the zoom index to index
 */
void KmagApp::setZoomIndex(int index)
{
  if(index < 0 || index >= (int)zoomArray.size()) {
    // the index is invalid
    qDebug() << "Invalid index!" ;
    return;
  } else if((int)m_zoomIndex == index) {
    // do nothing!
    return;
  } else {
    m_zoomIndex = index;
  }

  if(m_zoomIndex == 0) {
    // meaning that no more zooming-out is possible
    // -> disable zoom-out icon
    m_pZoomOut->setEnabled(false);
  } else { // enable the icon
    m_pZoomOut->setEnabled(true);
  }

  if((int)m_zoomIndex == zoomArray.size()-1) {
    // meaning that no more zooming-in is possible
    // -> disable zoom-in icon
    m_pZoomIn->setEnabled(false);
  } else { // enable the icon
    m_pZoomIn->setEnabled(true);
  }

  // signal change in zoom value
  Q_EMIT updateZoomValue(zoomArray.at(m_zoomIndex));
}

/**
 * Sets the rotation index to index
 */
void KmagApp::setRotationIndex(int index)
{
  if(index < 0 || index >= (int)rotationArray.size()) {
    // the index is invalid
    qDebug() << "Invalid index!" ;
    return;
  } else if((int)m_rotationIndex == index) {
    // do nothing!
    return;
  } else {
    m_rotationIndex = index;
  }

  // signal change in zoom value
  Q_EMIT updateRotationValue(rotationArray.at(m_rotationIndex));
}

/**
 * Sets the fps index to index
 */
void KmagApp::setFPSIndex(int index)
{
  if(index < 0 || index >= (int)fpsArray.size()) {
    // the index is invalid
    qDebug() << "Invalid index!" ;
    return;
  } else if((int)m_fpsIndex == index) {
    // do nothing!
    return;
  } else {
    m_fpsIndex = index;
  }

  // signal change in fps value
  Q_EMIT updateFPSValue(fpsArray.at(m_fpsIndex));
}

/**
 * Sets the color index to index
 */
void KmagApp::setColorIndex(int index)
{
  if(index < 0 || index >= (int)colorArray.size()) {
    // the index is invalid
    qDebug() << "Invalid index!" ;
    return;
  } else if((int)m_colorIndex == index) {
    // do nothing!
    return;
  } else {
    m_colorIndex = index;
  }

  // signal change in fps value
  Q_EMIT updateColorValue(colorArray.at(m_colorIndex));
}

/**
 * Save the zoomed image
 */
void KmagApp::saveZoomPixmap()
{
  bool toggled(false);

  // stop refresh temporarily
  if (m_zoomView->getRefreshStatus()) {
    slotToggleRefresh();
    toggled = true;
  }

  QStringList mimeTypes;
  const QList<QByteArray> supported = QImageWriter::supportedMimeTypes();
  for (const QByteArray& mimeType : supported) {
    mimeTypes.append(QString::fromLatin1(mimeType));
  }

  QFileDialog fileDialog(this);
  fileDialog.setWindowTitle(i18n("Save Snapshot As"));
  fileDialog.setAcceptMode(QFileDialog::AcceptSave);
  fileDialog.setDefaultSuffix(QStringLiteral("png"));
  fileDialog.setMimeTypeFilters(mimeTypes);
  QUrl url;
  if (fileDialog.exec() && !fileDialog.selectedUrls().isEmpty()) {
      url = fileDialog.selectedUrls().at(0);
  }

  if(!url.fileName().isEmpty()) {
    if(!url.isLocalFile()) {
      // create a temp file.. save image to it.. copy over the n/w and then delete the temp file.
      QTemporaryFile tempFile;
      if(!tempFile.open() || !m_zoomView->getImage().save(tempFile.fileName())) {
        KMessageBox::error(nullptr, i18n("Unable to save temporary file (before uploading to the network file you specified)."),
                          i18n("Error Writing File"));
      } else {
        KIO::FileCopyJob *job = KIO::file_copy(QUrl::fromLocalFile(tempFile.fileName()), url);
        if(!job) {
          KMessageBox::error(nullptr, i18n("Unable to upload file over the network."),
                            i18n("Error Writing File"));
        } else {
          KMessageBox::information(nullptr, i18n("Current zoomed image saved to\n%1", url.toDisplayString()),
                              i18n("Information"), QStringLiteral( "save_confirm" ));
        }
      }

    } else {
      if(!m_zoomView->getImage().save(url.path())) {
        KMessageBox::error(nullptr, i18n("Unable to save file. Please check if you have permission to write to the directory."),
                            i18n("Error Writing File"));
      } else {
        KMessageBox::information(nullptr, i18n("Current zoomed image saved to\n%1", url.toDisplayString()),
                                i18n("Information"), QStringLiteral( "save_confirm" ));
      }
    }
  }
  if(toggled) {
    slotToggleRefresh();
  }
}


void KmagApp::slotToggleRefresh()
{
  m_zoomView->toggleRefresh();
  if(m_zoomView->getRefreshStatus()) {
    refreshSwitch->setIcon(QIcon::fromTheme(QStringLiteral( "process-stop" )));
    refreshSwitch->setText(i18n("Stop"));
    refreshSwitch->setToolTip(i18n("Click to stop window update"));
  } else {
    refreshSwitch->setIcon(QIcon::fromTheme(QStringLiteral( "system-run" )));
    refreshSwitch->setText(i18nc("Start updating the window", "Start"));
    refreshSwitch->setToolTip(i18n("Click to start window update"));
  }
}


void KmagApp::slotModeWholeScreen()
{
  if (m_dockController && m_dockController->isDocked()) {
    QSignalBlocker block(m_dockMode);
    m_dockMode->setChecked(false);
    m_dockController->exitDock();
  }
  m_zoomView->followMouse(false);
  m_zoomView->setSelRectPos(screen()->virtualGeometry());
  m_zoomView->showSelRect(false);
  m_zoomView->setFitToWindow (false);
  m_modeFollowMouse->setChecked(false);
#if HAVE_QACCESSIBILITYCLIENT
  m_zoomView->followBoth(false);
  m_zoomView->followFocus(false);
  m_modeFollowFocus->setChecked(false);
#endif
  m_modeWholeScreen->setChecked(true);
  m_modeSelWin->setChecked(false);
}


void KmagApp::slotModeSelWin()
{
  m_zoomView->followMouse(false);
  m_zoomView->showSelRect(true);
  m_zoomView->setFitToWindow (false);
  m_modeFollowMouse->setChecked(false);
#if HAVE_QACCESSIBILITYCLIENT
  m_zoomView->followBoth(false);
  m_zoomView->followFocus(false);
  m_modeFollowFocus->setChecked(false);
#endif
  m_modeWholeScreen->setChecked(false);
  m_modeSelWin->setChecked(true);
}

void KmagApp::slotToggleDock(bool checked)
{
  if (!m_dockController || !m_dockController->isAvailable()) {
    QSignalBlocker block(m_dockMode);
    m_dockMode->setChecked(false);
    return;
  }

  if (checked) {
    // Docking is incompatible with Whole Screen: fall back to Follow Mouse first.
    if (m_modeWholeScreen->isChecked()) {
      m_modeFollowMouse->setChecked(true);
      slotModeChanged();
    }
    m_dockController->enterDock();
  } else {
    m_dockController->exitDock();
  }
}

void KmagApp::slotDockEdgeChanged()
{
  if (!m_dockController) {
    return;
  }
  DockingManager::Edge edge = DockingManager::Edge::Top;
  if (m_dockEdgeBottom->isChecked())      edge = DockingManager::Edge::Bottom;
  else if (m_dockEdgeLeft->isChecked())   edge = DockingManager::Edge::Left;
  else if (m_dockEdgeRight->isChecked())  edge = DockingManager::Edge::Right;
  m_dockController->setEdge(edge);
}

void KmagApp::setDockToggleChecked(bool checked)
{
  QSignalBlocker block(m_dockMode);
  m_dockMode->setChecked(checked);
}

void KmagApp::setDockEdgeChecked(DockingManager::Edge e)
{
  QSignalBlocker block(m_dockEdgeGroup);
  switch (e) {
  case DockingManager::Edge::Bottom: m_dockEdgeBottom->setChecked(true); break;
  case DockingManager::Edge::Left:   m_dockEdgeLeft->setChecked(true);   break;
  case DockingManager::Edge::Right:  m_dockEdgeRight->setChecked(true);  break;
  default:                           m_dockEdgeTop->setChecked(true);    break;
  }
}

void KmagApp::setDockEdgeActionsEnabled(bool enabled)
{
  m_dockEdgeTop->setEnabled(enabled);
  m_dockEdgeBottom->setEnabled(enabled);
  m_dockEdgeLeft->setEnabled(enabled);
  m_dockEdgeRight->setEnabled(enabled);
}

void KmagApp::popupDockContextMenu(const QPoint &globalPos)
{
  KXMLGUIFactory *factory = this->factory();
  QMenu *popup = static_cast<QMenu *>(factory->container(QStringLiteral("mainPopUp"), this));
  if (popup) {
    popup->popup(globalPos);
  }
}

void KmagApp::addStripActions(QWidget *strip)
{
  strip->addAction(m_dockMode);
}

void KmagApp::removeStripActions(QWidget *strip)
{
  strip->removeAction(m_dockMode);
}

void KmagApp::slotModeChanged()
{
#if HAVE_QACCESSIBILITYCLIENT
  if(m_modeFollowMouse->isChecked() && m_modeFollowFocus->isChecked()) {

    //BOTH MODE
    m_zoomView->followMouse(false);
    m_zoomView->followBoth(true);
    m_zoomView->followFocus(false);
    m_zoomView->showSelRect(false);
    m_zoomView->setFitToWindow (true);
    m_modeWholeScreen->setChecked(false);
    m_modeSelWin->setChecked(false);
    return;
  }
#endif

  if (m_modeFollowMouse->isChecked()) {

    //MOUSE MODE
    m_zoomView->followMouse(true);
    m_zoomView->showSelRect(false);
    m_zoomView->setFitToWindow (true);
    m_modeFollowMouse->setChecked(true);
#if HAVE_QACCESSIBILITYCLIENT
    m_zoomView->followBoth(false);
    m_zoomView->followFocus(false);
    m_modeFollowFocus->setChecked(false);
#endif
    m_modeWholeScreen->setChecked(false);
    m_modeSelWin->setChecked(false);

#if HAVE_QACCESSIBILITYCLIENT
  } else if (m_modeFollowFocus->isChecked()) {

    //FOCUS MODE
    m_zoomView->followBoth(false);
    m_zoomView->followMouse(false);
    m_zoomView->followFocus(true);
    m_zoomView->showSelRect(false);
    m_zoomView->setFitToWindow (true);
    m_modeFollowMouse->setChecked(false);
    m_modeFollowFocus->setChecked(true);
    m_modeWholeScreen->setChecked(false);
    m_modeSelWin->setChecked(false);
#endif

  }
}

void KmagApp::slotToggleHideCursor()
{
  showMouseCursor(!m_hideCursor->isChecked());
}


void KmagApp::slotStaysOnTop()
{
  if (m_staysOnTop->isChecked())
      setWindowFlags( windowFlags() | Qt::WindowStaysOnTopHint );
  else
      setWindowFlags( windowFlags() & ~Qt::WindowStaysOnTopHint );
  show();
}


void KmagApp::slotFileNewWindow()
{
  KmagApp *new_window= new KmagApp();
  new_window->show();
}


void KmagApp::slotFilePrint()
{
#ifndef QT_NO_PRINTER

  bool toggled(false);

  if (m_printer == nullptr) {
    m_printer = new QPrinter();
  }

  // stop refresh temporarily
  if (m_zoomView->getRefreshStatus()) {
    slotToggleRefresh();
    toggled = true;
  }

  const QImage pixmap(m_zoomView->getImage());

  // use some AI to get the best orientation
  if(pixmap.width() > pixmap.height()) {
    m_printer->setPageOrientation(QPageLayout::Landscape);
  } else {
    m_printer->setPageOrientation(QPageLayout::Portrait);
  }

  QPrintDialog *printDialog = new QPrintDialog(m_printer, this);

  if (printDialog->exec()) {
    QPainter paint;

    if(!paint.begin(m_printer))
    {
      delete printDialog;
      return;
    }
    // draw the pixmap
    paint.drawImage(0, 0, pixmap);
    // end the painting
    paint.end();
  }

  if(toggled) {
    slotToggleRefresh();
  }
  delete printDialog;
#endif // QT_NO_PRINTER
}

void KmagApp::slotFileQuit()
{
  saveOptions();
  // close the first window, the list makes the next one the first again.
  // This ensures that queryClose() is called on each window to ask for closing
  KMainWindow* w;
  if (!memberList().isEmpty())
  {
	for (int i = 0; i < memberList().size(); ++i)
    {
	  w = memberList().at(i);
      // only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
      // the window and the application stay open.
      if(!w->close())
         break;
#ifdef __GNUC__
#warning "kde4: now memberList() is constant => we can't remove some element!"
#endif
	//memberList()->removeRef(w);
    }
  }
}

void KmagApp::copyToClipBoard()
{
  QClipboard *cb=QApplication::clipboard();
  cb->setPixmap(QPixmap::fromImage(m_zoomView->getImage()));
}

void KmagApp::slotShowMenu()
{
  ///////////////////////////////////////////////////////////////////
  // turn Menu on or off
  if(!m_pShowMenu->isChecked())
  {
    menuBar()->hide();
  }
  else
  {
    menuBar()->show();
  }
}

void KmagApp::slotConfKeys()
{
  KShortcutsDialog::showDialog(actionCollection(), KShortcutsEditor::LetterShortcutsAllowed, this);
}

void KmagApp::slotEditToolbars()
{
  KConfigGroup cg( KSharedConfig::openConfig(), QStringLiteral("MainWindow") );
  saveMainWindowSettings( cg );
  QPointer<KEditToolBar> dlg = new KEditToolBar( actionCollection() );
  connect(dlg.data(), &KEditToolBar::newToolBarConfig, this, &KmagApp::slotNewToolbarConfig );
  if ( dlg->exec() )
    createGUI();
  delete dlg;
}


void KmagApp::slotNewToolbarConfig()
{
  applyMainWindowSettings( KSharedConfig::openConfig()->group( QStringLiteral("MainWindow") ) );
  createGUI();
}

#include "moc_kmag.cpp"
