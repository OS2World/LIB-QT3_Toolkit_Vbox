/****************************************************************************
** $Id: qdesktopwidget_pm.cpp 110 2006-07-30 17:11:51Z dmik $
**
** Implementation of QDesktopWidget class for OS/2
**
** Copyright (C) 1992-2001 Trolltech AS.  All rights reserved.
** Copyright (C) 2004 Norman ASA.  Initial OS/2 Port.
** Copyright (C) 2005 netlabs.org.  Further OS/2 Development.
**
** This file is part of the kernel module of the Qt GUI Toolkit.
**
** This file may be distributed under the terms of the Q Public License
** as defined by Trolltech AS of Norway and appearing in the file
** LICENSE.QPL included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses may use this file in accordance with the Qt Commercial License
** Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/qpl/ for QPL licensing information.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

#include "qdesktopwidget.h"

/*
  \omit
  Function is commented out in header
  \fn void *QDesktopWidget::handle( int screen ) const

  Returns the window system handle of the display device with the
  index \a screen, for low-level access.  Using this function is not
  portable.

  The return type varies with platform; see qwindowdefs.h for details.

  \sa x11Display(), QPaintDevice::handle()
  \endomit
*/

// just stolen from xWorkplace sources 
static
APIRET qt_DosQueryProcAddr( PCSZ pcszModuleName, ULONG ulOrdinal, PFN *ppfn )
{
    HMODULE hmod = NULL;
    APIRET rc = 0;
    if ( !(rc = DosQueryModuleHandle( (PSZ)pcszModuleName, &hmod )) ) {
        if ( (rc = DosQueryProcAddr( hmod, ulOrdinal, NULL, ppfn )) ) {
            // the CP programming guide and reference says use
            // DosLoadModule if DosQueryProcAddr fails with this error
            if ( rc == ERROR_INVALID_HANDLE ) {
                if ( !(rc = DosLoadModule( NULL, 0, (PSZ) pcszModuleName,
                                            &hmod )) ) {
                    rc = DosQueryProcAddr( hmod, ulOrdinal, NULL, ppfn );
                }
            }
        }
    }
    return rc;
}

class QDesktopWidgetPrivate
{
public:

    QRect workArea;
};

/*!
    \class QDesktopWidget qdesktopwidget.h
    \brief The QDesktopWidget class provides access to screen information on multi-head systems.

    \ingroup advanced
    \ingroup environment

    Systems with more than one graphics card and monitor can manage the
    physical screen space available either as multiple desktops, or as a
    large virtual desktop, which usually has the size of the bounding
    rectangle of all the screens (see isVirtualDesktop()). For an
    application, one of the available screens is the primary screen, i.e.
    the screen where the main widget resides (see primaryScreen()). All
    windows opened in the context of the application must be
    constrained to the boundaries of the primary screen; for example,
    it would be inconvenient if a dialog box popped up on a different
    screen, or split over two screens.

    The QDesktopWidget provides information about the geometry of the
    available screens with screenGeometry(). The number of screens
    available is returned by numScreens(). The screen number that a
    particular point or widget is located in is returned by
    screenNumber().

    Widgets provided by Qt use this class, for example, to place
    tooltips, menus and dialog boxes according to the parent or
    application widget.

    Applications can use this class to save window positions, or to place
    child widgets on one screen.

    \img qdesktopwidget.png Managing Multiple Screens

    In the illustration above, Application One's primary screen is
    screen 0, and App Two's primary screen is screen 1.


*/

/*!
    Creates the desktop widget.

    If the system supports a virtual desktop, this widget will have
    the size of the virtual desktop; otherwise this widget will have
    the size of the primary screen.

    Instead of using QDesktopWidget directly, use
    QAppliation::desktop().
*/
QDesktopWidget::QDesktopWidget()
: QWidget( 0, "desktop", WType_Desktop )
{
    d = new QDesktopWidgetPrivate();
}

/*!
    Destroy the object and free allocated resources.
*/
QDesktopWidget::~QDesktopWidget()
{
    delete d;
}

/*!
    Returns TRUE if the system manages the available screens in a
    virtual desktop; otherwise returns FALSE.

    For virtual desktops, screen() will always return the same widget.
    The size of the virtual desktop is the size of this desktop
    widget.
*/
bool QDesktopWidget::isVirtualDesktop() const
{
    return FALSE;
}

/*!
    Returns the index of the primary screen.

    \sa numScreens()
*/
int QDesktopWidget::primaryScreen() const
{
    return 0;
}

/*!
    Returns the number of available screens.

    \sa primaryScreen()
*/
int QDesktopWidget::numScreens() const
{
    return 1;
}

/*!
    Returns a widget that represents the screen with index \a screen.
    This widget can be used to draw directly on the desktop, using an
    unclipped painter like this:

    \code
    QPainter paint( QApplication::desktop()->screen( 0 ), TRUE );
    paint.draw...
    ...
    paint.end();
    \endcode

    If the system uses a virtual desktop, the returned widget will
    have the geometry of the entire virtual desktop i.e. bounding
    every \a screen.

    \sa primaryScreen(), numScreens(), isVirtualDesktop()
*/
QWidget *QDesktopWidget::screen( int /*screen*/ )
{
    // It seems that a WType_Desktop cannot be moved?
    return this;
}

/*!
  Returns the available geometry of the screen with index \a screen. What
  is available will be subrect of screenGeometry() based on what the
  platform decides is available (for example excludes the Dock and Menubar
  on Mac OS X, or the taskbar on Windows).

  \sa screenNumber(), screenGeometry()
*/
const QRect& QDesktopWidget::availableGeometry( int /*screen*/ ) const
{
    typedef    
    BOOL (APIENTRY *WinQueryDesktopWorkArea_T) (HWND hwndDesktop,
                                                PRECTL pwrcWorkArea);
    static WinQueryDesktopWorkArea_T WinQueryDesktopWorkArea =
        (WinQueryDesktopWorkArea_T) ~0;
    
    if ( (ULONG) WinQueryDesktopWorkArea == (ULONG) ~0 ) {
        if ( qt_DosQueryProcAddr( "PMMERGE", 5469,
                                  (PFN *) &WinQueryDesktopWorkArea ) )
            WinQueryDesktopWorkArea = NULL;
    }

    if ( WinQueryDesktopWorkArea ) {
        RECTL rcl;
        if ( WinQueryDesktopWorkArea( HWND_DESKTOP, &rcl ) ) {
            // flip y coordinates
            d->workArea.setCoords( rcl.xLeft, height() - rcl.yTop,
                                   rcl.xRight - 1,
                                   height() - (rcl.yBottom + 1) );
            return d->workArea;
        }
    }
    
    return geometry();
}

/*!
    \overload const QRect &QDesktopWidget::availableGeometry( QWidget *widget ) const

    Returns the available geometry of the screen which contains \a widget.

    \sa screenGeometry()
*/

/*!
    \overload const QRect &QDesktopWidget::availableGeometry( const QPoint &p ) const

    Returns the available geometry of the screen which contains \a p.

    \sa screenGeometry()
*/


/*!
    Returns the geometry of the screen with index \a screen.

    \sa screenNumber()
*/
const QRect& QDesktopWidget::screenGeometry( int /*screen*/ ) const
{
    return geometry();
}

/*!
    \overload const QRect &QDesktopWidget::screenGeometry( QWidget *widget ) const

    Returns the geometry of the screen which contains \a widget.
*/

/*!
    \overload const QRect &QDesktopWidget::screenGeometry( const QPoint &p ) const

    Returns the geometry of the screen which contains \a p.
*/


/*!
    Returns the index of the screen that contains the largest
    part of \a widget, or -1 if the widget not on a screen.

    \sa primaryScreen()
*/
int QDesktopWidget::screenNumber( QWidget */*widget*/ ) const
{
    return 0;
}

/*!
    \overload
    Returns the index of the screen that contains \a point, or -1 if
    no screen contains the point.

    \sa primaryScreen()
*/
int QDesktopWidget::screenNumber( const QPoint &/*point*/ ) const
{
    return 0;
}

/*!
    \reimp
*/
void QDesktopWidget::resizeEvent( QResizeEvent *e )
{
    if ( e && !e->size().isValid() && !e->oldSize().isValid() ) {
        // This is a Work Area Changed notification, see WM_SYSVALUECHANGED
        // in qapplication_pm.cpp
        emit workAreaResized( 0 );
        return;
    }
    
    // nothing to do, the desktop cannot be dynamically resized in OS/2
}

/*! \fn void QDesktopWidget::insertChild( QObject *child )
    \reimp
*/

/*! \fn void QDesktopWidget::resized( int screen )
    This signal is emitted when the size of \a screen changes.
*/

/*! \fn void QDesktopWidget::workAreaResized( int screen )
  \internal
    This signal is emitted when the work area available on \a screen changes.
*/
