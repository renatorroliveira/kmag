//$Id$
/***************************************************************************
                          kmag.h  -  description
                             -------------------
    begin                : Mon Feb 12 23:45:41 EST 2001
    copyright            : (C) 2001 by Sarang Lakare
    email                : sarang@users.sourceforge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef KMAG_H
#define KMAG_H
 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <vector>

// include files for Qt
#include <qstringlist.h>

// include files for KDE 
#include <kapp.h>
#include <kmainwindow.h>
#include <kdockwidget.h>
#include <kaccel.h>
#include <kaction.h>
#include <knuminput.h>

// forward declaration of the Kmag classes
class KMagZoomView;
class QButtonGroup;
class	QCheckBox;

/**
  * The base class for Kmag application windows. It sets up the main
  * window and reads the config file as well as providing a menubar, toolbar
  * and statusbar. An instance of KmagView creates your center view, which is connected
  * to the window's Doc object.
  * KmagApp reimplements the methods that KMainWindow provides for main window handling and supports
  * full session management as well as using KActions.
  * @see KMainWindow
  * @see KApplication
  * @see KConfig
  *
  * @author Source Framework Automatically Generated by KDevelop, (c) The KDevelop Team.
  * @version KDevelop version 1.2 code generation
  */
class KmagApp : public KMainWindow
{
  Q_OBJECT

  public:
    /** construtor of KmagApp, calls all init functions to create the application.
     */
    KmagApp(QWidget* parent=0, const char* name=0);
    ~KmagApp();
    /** opens a file specified by commandline option
     */
//    void openDocumentFile(const KURL& url=0);
    /** returns a pointer to the current document connected to the KTMainWindow instance and is used by
     * the View class to access the document object's methods
     */	
//    KmagDoc *getDocument() const; 	


		
  protected:
    /** save general Options like all bar positions and status as well as the geometry and the recent file list to the configuration
     * file
     */ 	
    void saveOptions();
    /** read general Options again and initialize all variables like the recent file list
     */
    void readOptions();
    /** initializes the KActions of the application */
    void initActions();

    /** initializes the document object of the main window that is connected to the view in initView().
     * @see initView();
     */
//    void initDocument();

    /** creates the centerwidget of the KTMainWindow instance and sets it as the view
     */
    void initView();

    /** queryClose is called by KTMainWindow on each closeEvent of a window. Against the
     * default implementation (only returns true), this calles saveModified() on the document object to ask if the document shall
     * be saved if Modified; on cancel the closeEvent is rejected.
     * @see KTMainWindow#queryClose
     * @see KTMainWindow#closeEvent
     */

		/// Initialize all connections
		void initConnections();

    virtual bool queryClose();

    /** queryExit is called by KTMainWindow when the last window of the application is going to be closed during the closeEvent().
     * Against the default implementation that just returns true, this calls saveOptions() to save the settings of the last window's	
     * properties.
     * @see KTMainWindow#queryExit
     * @see KTMainWindow#closeEvent
     */
    virtual bool queryExit();

    /** saves the window properties for each open window during session end to the session config file, including saving the currently
     * opened file by a temporary filename provided by KApplication.
     * @see KTMainWindow#saveProperties
     */
    virtual void saveProperties(KConfig *_cfg);

    /** reads the session config file and restores the application's state including the last opened files and documents by reading the
     * temporary files saved by saveProperties()
     * @see KTMainWindow#readProperties
     */
    virtual void readProperties(KConfig *_cfg);

  public slots:
    /** open a new application window by creating a new instance of KmagApp */
    void slotFileNewWindow();

    /** print the actual file */
    void slotFilePrint();

    /** closes all open windows by calling close() on each memberList item until the list is empty, then quits the application.
     * If queryClose() returns false because the user canceled the saveModified() dialog, the closing breaks.
     */
    void slotFileQuit();

    /** put the marked text/object into the clipboard
     */
    void slotEditCopy();

    /** paste the clipboard into the document
     */
    void slotViewToolBar();

    /// Toggle the refreshing of the window
    void slotToggleRefresh();

		/// Zooms in
    void zoomIn();

		/// Zooms out
    void zoomOut();

    /// Sets the zoom index to index
    void setZoomIndex(int index=4);

	signals:
		/// This signal is raised whenever the index into the zoom array is changed
		void updateZoomIndex(int);
		
		/// This signal is raised whenever the zoom value changes
		void updateZoomValue(float);

  private:
    /** the configuration object of the application */
    KConfig *config;
    /** doc represents your actual document and is created only once. It keeps
     * information such as filename and does the serialization of your files.
     */
//    KmagDoc *doc;

    // KAction pointers to enable/disable actions
    KAction* fileNewWindow;
		KAction *m_pZoomIn;
		KAction *m_pZoomOut;
    KAction* filePrint;
    KAction* fileQuit;
    KAction* editCopy;
    KAction *refreshSwitch;
    KToggleAction* viewToolBar;
		KSelectAction *m_pZoomBox;

		/// zoom slider
		KIntNumInput *m_zoomSlider;

		/// Current index into the zoomArray
		unsigned int m_zoomIndex;

		QStringList zoomArrayString;
		vector<float> zoomArray;

	KMagZoomView* m_zoomView;
  QButtonGroup *m_settingsGroup;
	QCheckBox *m_followMouseButton;
};


#endif // KMAG_H
