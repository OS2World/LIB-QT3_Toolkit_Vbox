/****************************************************************************
** $Id: qapplication_pm.cpp 195 2011-06-18 05:23:01Z rudi $
**
** Implementation of OS/2 startup routines and event handling
**
** Copyright (C) 1992-2003 Trolltech AS.  All rights reserved.
** Copyright (C) 2004 Norman ASA.  Initial OS/2 Port.
** Copyright (C) 2005-2006 netlabs.org.  Further OS/2 Development.
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

// GCC headers do not define this when INCL_BASE is defined,
// but we need it for IOCTL_KEYBOARD etc. constants
#ifndef INCL_DOSDEVIOCTL
#define INCL_DOSDEVIOCTL
#endif

#include "qapplication.h"
#include "private/qapplication_p.h"
#include "qwidget.h"
#include "qwidgetlist.h"
#include "qwidgetintdict.h"
#include "qobjectlist.h"
#include "qpainter.h"
#include "qpixmapcache.h"
#include "qdatetime.h"
#include "qsessionmanager.h"
#include "qmime.h"
#include "qguardedptr.h"
#include "qclipboard.h"
#include "qthread.h"
#include "qwhatsthis.h" // ######## dependency
#include "qthread.h"
#include "qlibrary.h"
#include "qt_os2.h"
#include "qcursor.h"
#include "private/qinternal_p.h"
#include "private/qcriticalsection_p.h"
/// @todo (dmik) remove?
//#include "private/qinputcontext_p.h"
#include "qstyle.h"
#include "qmetaobject.h"

#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

extern void qt_ensure_pm();

/// @todo (dmik) hmm, extended keys under OS/2?
//// support for multi-media-keys on ME/2000/XP
//#ifndef WM_APPCOMMAND
//#define WM_APPCOMMAND                   0x0319
//
//#define FAPPCOMMAND_MOUSE 0x8000
//#define FAPPCOMMAND_KEY   0
//#define FAPPCOMMAND_OEM   0x1000
//#define FAPPCOMMAND_MASK  0xF000
//#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
//#define GET_DEVICE_LPARAM(lParam)     ((WORD)(HIWORD(lParam) & FAPPCOMMAND_MASK))
//#define GET_MOUSEORKEY_LPARAM         GET_DEVICE_LPARAM
//#define GET_FLAGS_LPARAM(lParam)      (LOWORD(lParam))
//#define GET_KEYSTATE_LPARAM(lParam)   GET_FLAGS_LPARAM(lParam)
//
//#define APPCOMMAND_BROWSER_BACKWARD       1
//#define APPCOMMAND_BROWSER_FORWARD        2
//#define APPCOMMAND_BROWSER_REFRESH        3
//#define APPCOMMAND_BROWSER_STOP           4
//#define APPCOMMAND_BROWSER_SEARCH         5
//#define APPCOMMAND_BROWSER_FAVORITES      6
//#define APPCOMMAND_BROWSER_HOME           7
//#define APPCOMMAND_VOLUME_MUTE            8
//#define APPCOMMAND_VOLUME_DOWN            9
//#define APPCOMMAND_VOLUME_UP              10
//#define APPCOMMAND_MEDIA_NEXTTRACK        11
//#define APPCOMMAND_MEDIA_PREVIOUSTRACK    12
//#define APPCOMMAND_MEDIA_STOP             13
//#define APPCOMMAND_MEDIA_PLAY_PAUSE       14
//#define APPCOMMAND_LAUNCH_MAIL            15
//#define APPCOMMAND_LAUNCH_MEDIA_SELECT    16
//#define APPCOMMAND_LAUNCH_APP1            17
//#define APPCOMMAND_LAUNCH_APP2            18
//#define APPCOMMAND_BASS_DOWN              19
//#define APPCOMMAND_BASS_BOOST             20
//#define APPCOMMAND_BASS_UP                21
//#define APPCOMMAND_TREBLE_DOWN            22
//#define APPCOMMAND_TREBLE_UP              23
//#endif

extern void qt_dispatchEnterLeave( QWidget*, QWidget* ); // qapplication.cpp

extern void qt_erase_background(
        HPS hps, int x, int y, int w, int h,
	const QColor &bg_color,
	const QPixmap *bg_pixmap, int off_x, int off_y, int devh
);

/*
  Internal functions.
*/

Q_EXPORT
void qt_draw_tiled_pixmap( HPS, int, int, int, int,
			   const QPixmap *, int, int, int );

Q_EXPORT
QRgb qt_sysclr2qrgb( LONG sysClr )
{
    // QRgb has the same RGB format (0xaarrggbb) as OS/2 uses (ignoring the
    // highest alpha byte) so we just cast OS/2 LONG RGB value to qRgb
    // which is an unsigned int actually.
    return ((QRgb) WinQuerySysColor( HWND_DESKTOP, sysClr, 0 )) & RGB_MASK;
}

static
QFont qt_sysfont2qfont( const PSZ scope )
{
    static const PSZ app = "PM_SystemFonts";
    static const PSZ def = "10.System Proportional";
    ULONG keyLen = 0;
    QFont f( "System Proportional", 10 );

    if ( PrfQueryProfileSize( HINI_USERPROFILE, app, scope, &keyLen ) && keyLen ) {
        keyLen ++; // reserve space for a dot
        char *buf = new char [keyLen];
        ULONG realLen =
            PrfQueryProfileString( HINI_USERPROFILE, app, scope, def, buf, keyLen );
        realLen --; // excude zero terminator

        // parse the font definition
        int height = 0;
        char *dot = strchr( buf, '.' ), *dot2 = 0;
        if ( dot ) {
            *dot = 0;
            height = strtoul( buf, NULL, 10 );
            dot2 = strchr( ++ dot, '.' );
            if ( dot2 ) {
                // process simulated styles
                buf[realLen] = '.';
                buf[realLen+1] = 0;
                strupr( dot2 );
/// @todo (dmik) currently, simulated bold and italic font styles are not
//  supported by Qt/OS2 because Qt doesn't support style simulation
//  explicitly. the code below is commented out to prevent selecting
//  true fonts when simulated ones are actually requested.
//                if ( strstr( dot2, ".BOLD." ) ) f.setBold( TRUE );
//                if ( strstr( dot2, ".ITALIC.") ) f.setItalic( TRUE );
                if ( strstr( dot2, ".UNDERSCORE.") ) f.setUnderline( TRUE );
                if ( strstr( dot2, ".UNDERLINED.") ) f.setUnderline( TRUE );
                if ( strstr( dot2, ".STRIKEOUT.") ) f.setStrikeOut( TRUE );
                *dot2 = 0;
            }
            // query non-simulated styles
            FONTMETRICS fm;
            LONG cnt = 1; // use the first match
            GpiQueryFonts(
                qt_display_ps(), QF_PUBLIC, dot, &cnt, sizeof(FONTMETRICS), &fm );
            if ( cnt ) {
                // the code below is mostly from QFontDatabasePrivate::reload()
                if ( fm.fsSelection & FM_SEL_ITALIC ) f.setItalic( TRUE );
                if ( fm.fsType & FM_TYPE_FIXED ) f.setFixedPitch( TRUE );
                USHORT weight = fm.usWeightClass;
                USHORT width = fm.usWidthClass;
                if ( weight < 4 ) f.setWeight( QFont::Light );
                else if ( weight < 6 ) f.setWeight( QFont::Normal );
                else if ( weight < 7 ) f.setWeight( QFont::DemiBold );
                else if ( weight < 8 ) f.setWeight( QFont::Bold );
                else f.setWeight( QFont::Black );
                switch ( width ) {
                    case 1: f.setStretch( QFont::UltraCondensed ); break;
                    case 2: f.setStretch( QFont::ExtraCondensed ); break;
                    case 3: f.setStretch( QFont::Condensed ); break;
                    case 4: f.setStretch( QFont::SemiCondensed ); break;
                    case 5: f.setStretch( QFont::Unstretched ); break;
                    case 6: f.setStretch( QFont::SemiExpanded ); break;
                    case 7: f.setStretch( QFont::Expanded ); break;
                    case 8: f.setStretch( QFont::ExtraExpanded ); break;
                    case 9: f.setStretch( QFont::UltraExpanded ); break;
                    default: f.setStretch( QFont::Unstretched ); break;
                }
                f.setFamily( fm.szFamilyname );
                f.setPointSize( height );
            }
        }
        delete[] buf;
    }
    return f;
}

/*****************************************************************************
  Internal variables and functions
 *****************************************************************************/

static HWND	 curWin		= 0;		// current window
static HPS	 displayPS	= 0;		// display presentation space

#if !defined (QT_NO_SESSIONMANAGER)

// Session management
static bool	sm_blockUserInput    = FALSE;
static bool	sm_smActive          = FALSE;
extern QSessionManager* qt_session_manager_self;
static bool	sm_cancel            = FALSE;
static bool	sm_gracefulShutdown  = FALSE;
static bool	sm_quitSkipped       = FALSE;
bool qt_about_to_destroy_wnd     = FALSE;

//#define DEBUG_SESSIONMANAGER

#endif

static bool replayPopupMouseEvent = FALSE; // replay handling when popups close

static bool ignoreNextMouseReleaseEvent = FALSE; // ignore the next release event if
						 // return from a modal widget
#if defined(QT_DEBUG)
static bool	appNoGrab	= FALSE;	// mouse/keyboard grabbing
#endif

static bool	app_do_modal	   = FALSE;	// modal mode
extern QWidgetList *qt_modal_stack;
extern QDesktopWidget *qt_desktopWidget;
static QWidget *popupButtonFocus   = 0;
static bool	popupCloseDownMode = FALSE;
static bool	qt_try_modal( QWidget *, QMSG *, int& ret );

QWidget	       *qt_button_down = 0;		// widget got last button-down

extern bool qt_tryAccelEvent( QWidget*, QKeyEvent* ); // def in qaccel.cpp

static HWND	autoCaptureWnd = 0;
static bool     autoCaptureReleased = FALSE;
static void	setAutoCapture( HWND );		// automatic capture
static void	releaseAutoCapture();

//@@TODO (dmik):
//  do we really need this filter stuff or qApp->pmEventFilter()
//  is enough?
typedef int (*QPMEventFilter) (QMSG*);
static QPMEventFilter qt_pm_event_filter = 0;

Q_EXPORT QPMEventFilter qt_set_pm_event_filter( QPMEventFilter filter )
{
    QPMEventFilter old_filter = qt_pm_event_filter;
    qt_pm_event_filter = filter;
    return old_filter;
}

typedef bool (*QPMEventFilterEx) (QMSG*,MRESULT&);
static QPMEventFilterEx qt_pm_event_filter_ex = 0;

Q_EXPORT QPMEventFilterEx qt_set_pm_event_filter_ex( QPMEventFilterEx filter )
{
    QPMEventFilterEx old = qt_pm_event_filter_ex;
    qt_pm_event_filter_ex = filter;
    return old;
}

bool qt_pmEventFilter( QMSG* msg, MRESULT &result )
{
    result = NULL;
    if ( qt_pm_event_filter && qt_pm_event_filter( msg )  )
	return TRUE;
    if ( qt_pm_event_filter_ex && qt_pm_event_filter_ex( msg, result ) )
        return TRUE;
    return qApp->pmEventFilter( msg );
}

static void     unregWinClasses();

extern QCursor *qt_grab_cursor();

extern "C" MRESULT EXPENTRY QtWndProc( HWND, ULONG, MPARAM, MPARAM );

class QETWidget : public QWidget		// event translator widget
{
public:
    void	setWFlags( WFlags f )	{ QWidget::setWFlags(f); }
    void	clearWFlags( WFlags f ) { QWidget::clearWFlags(f); }
    void	setWState( WState f )	{ QWidget::setWState(f); }
    void	clearWState( WState f ) { QWidget::clearWState(f); }
    QWExtra    *xtra()			{ return QWidget::extraData(); }
    QTLWExtra   *top()		        { return QWidget::topData(); }
    bool	pmEvent( QMSG *m )	{ return QWidget::pmEvent(m); }
#if defined(Q_CC_GNU)
    void	markFrameStrutDirty()	{ fstrut_dirty = 1; }
#else
    void	markFrameStrutDirty()	{ QWidget::fstrut_dirty = 1; }
#endif
    void	updateFrameStrut()	{ QWidget::updateFrameStrut(); }
    bool	translateMouseEvent( const QMSG &qmsg );
    bool	translateKeyEvent( const QMSG &qmsg, bool grab );
#ifndef QT_NO_WHEELEVENT
    bool	translateWheelEvent( const QMSG &qmsg );
#endif
    bool	sendKeyEvent( QEvent::Type type, int code, int ascii,
			      int state, bool grab, const QString& text,
			      bool autor = FALSE );
    bool	translatePaintEvent( const QMSG &qmsg );
    bool	translateConfigEvent( const QMSG &qmsg );
    bool	translateCloseEvent( const QMSG &qmsg );
//@@TODO (dmik): later
//    void	repolishStyle( QStyle &style ) { styleChange( style ); }
//@@TODO (dmik): it seems we don't need this
//    void	reparentWorkaround();
    void	showChildren(bool spontaneous) { QWidget::showChildren(spontaneous); }
    void	hideChildren(bool spontaneous) { QWidget::hideChildren(spontaneous); }
};

//@@TODO (dmik): it seems we don't need this
//void QETWidget::reparentWorkaround()
//{
//    ((QWidgetIntDict*)QWidget::wmapper())->remove((long)winid);
//    clearWState(WState_Created | WState_Visible | WState_ForceHide);
//    winid = 0;
//    QRect geom = geometry();
//    create(0, FALSE, FALSE);
//    setGeometry(geom);
//    QWidget *p = parentWidget();
//    while (p) {
//	if (!p->isVisible())
//	    return;
//	p = p->parentWidget();
//    }
//    show();
//}


static bool qt_show_system_menu( QWidget* tlw )
{
    HWND fId = tlw->winFId();
    HWND sysMenu = WinWindowFromID( fId, FID_SYSMENU );
    if ( !sysMenu )
	return FALSE; // no menu for this window
    WinPostMsg( sysMenu, MM_STARTMENUMODE, MPFROM2SHORT(TRUE, FALSE), 0 );
    return TRUE;
}

// Palette handling
extern QPalette *qt_std_pal;
extern void qt_create_std_palette();

static void qt_set_pm_resources()
{
    // Do the font settings

#ifndef QT_PM_NO_DEFAULTFONT_OVERRIDE
    QFont windowFont = qt_sysfont2qfont( "WindowText" );
#else
    QFont windowFont = qt_sysfont2qfont( "DefaultFont" );
#endif
    QFont menuFont = qt_sysfont2qfont( "Menus" );
    QFont iconFont = qt_sysfont2qfont( "IconText" );
    QFont titleFont = qt_sysfont2qfont( "WindowTitles" );

    QApplication::setFont( windowFont, TRUE );
    QApplication::setFont( menuFont, TRUE, "QPopupMenu" );
    QApplication::setFont( menuFont, TRUE, "QMenuBar" );
    QApplication::setFont( iconFont, TRUE, "QTipLabel" );
    QApplication::setFont( iconFont, TRUE, "QStatusBar" );
    QApplication::setFont( titleFont, TRUE, "QTitleBar" );
    QApplication::setFont( titleFont, TRUE, "QDockWindowTitleBar" );

    if ( qt_std_pal && *qt_std_pal != QApplication::palette() )
	return;

    // Do the color settings

    // active colors
    QColorGroup acg;
    acg.setColor( QColorGroup::Foreground,
                  QColor( qt_sysclr2qrgb( SYSCLR_WINDOWTEXT ) ) );
    acg.setColor( QColorGroup::Background,
                  QColor( qt_sysclr2qrgb( SYSCLR_DIALOGBACKGROUND ) ) );
    acg.setColor( QColorGroup::ButtonText,
                  QColor( qt_sysclr2qrgb( SYSCLR_MENUTEXT ) ) );
    acg.setColor( QColorGroup::Button,
                  QColor( qt_sysclr2qrgb( SYSCLR_BUTTONMIDDLE ) ) );
    acg.setColor( QColorGroup::Light,
                  QColor( qt_sysclr2qrgb( SYSCLR_BUTTONLIGHT ) ) );
    acg.setColor( QColorGroup::Dark,
                  QColor( qt_sysclr2qrgb( SYSCLR_BUTTONDARK ) ) );
    acg.setColor( QColorGroup::Midlight,
                  QColor( (acg.light().red()   + acg.button().red())   / 2,
                          (acg.light().green() + acg.button().green()) / 2,
		          (acg.light().blue()  + acg.button().blue())  / 2 ) );
    acg.setColor( QColorGroup::Mid,
                  QColor( (acg.dark().red()    + acg.button().red())   / 2,
                          (acg.dark().green()  + acg.button().green()) / 2,
		          (acg.dark().blue()   + acg.button().blue())  / 2 ) );
    acg.setColor( QColorGroup::Text,
                  QColor( qt_sysclr2qrgb( SYSCLR_MENUTEXT ) ) );
    acg.setColor( QColorGroup::Base,
                  QColor( qt_sysclr2qrgb( SYSCLR_ENTRYFIELD ) ) );
    acg.setColor( QColorGroup::BrightText,
                  QColor( qt_sysclr2qrgb( SYSCLR_BUTTONLIGHT ) ) );
    acg.setColor( QColorGroup::Shadow,
                  Qt::black );
    acg.setColor( QColorGroup::Highlight,
                  QColor( qt_sysclr2qrgb( SYSCLR_HILITEBACKGROUND ) ) );
    acg.setColor( QColorGroup::HighlightedText,
                  QColor( qt_sysclr2qrgb( SYSCLR_HILITEFOREGROUND ) ) );

    // these colors are not present in the PM system palette
    acg.setColor( QColorGroup::Link, Qt::blue );
    acg.setColor( QColorGroup::LinkVisited, Qt::magenta );

    // disabled colors
    QColorGroup dcg = acg;
    dcg.setColor( QColorGroup::Foreground,
                  QColor( qt_sysclr2qrgb( SYSCLR_MENUDISABLEDTEXT ) ) );
    dcg.setColor( QColorGroup::Text,
                  QColor( qt_sysclr2qrgb( SYSCLR_MENUDISABLEDTEXT ) ) );

    // inactive colors are currently the same as active
    QColorGroup icg = acg;

    QPalette pal( acg, dcg, icg );
    QApplication::setPalette( pal, TRUE );
    *qt_std_pal = pal;

    // special palettes
    QColorGroup scg;

    // menus
    scg = acg;
    scg.setColor( QColorGroup::Background,
                  QColor( qt_sysclr2qrgb( SYSCLR_MENU ) ) );
    scg.setColor( QColorGroup::Highlight,
                  QColor( qt_sysclr2qrgb( SYSCLR_MENUHILITEBGND ) ) );
    scg.setColor( QColorGroup::HighlightedText,
                  QColor( qt_sysclr2qrgb( SYSCLR_MENUHILITE ) ) );
    pal.setActive( scg );
    pal.setInactive( scg );
    QApplication::setPalette( pal, TRUE, "QPopupMenu" );
    QApplication::setPalette( pal, TRUE, "QMenuBar" );

    // static widget texts
    scg = acg;
    QColor staticTextCol( qt_sysclr2qrgb( SYSCLR_WINDOWSTATICTEXT ) );
    scg.setColor( QColorGroup::Foreground, staticTextCol );
    scg.setColor( QColorGroup::Text, staticTextCol );
    pal.setActive( scg );
    pal.setInactive( scg );
    QApplication::setPalette( pal, TRUE, "QLabel" );
    QApplication::setPalette( pal, TRUE, "QGroupBox" );
}

/*****************************************************************************
  qt_init() - initializes Qt for OS/2
 *****************************************************************************/

// wrapper for QtOS2SysXcptMainHandler to provide local access to private data
class QtOS2SysXcptMainHandlerInternal : public QtOS2SysXcptMainHandler
{
public:
    static bool &installed() { return QtOS2SysXcptMainHandler::installed; }
    static ERR &libcHandler() { return QtOS2SysXcptMainHandler::libcHandler; }
    static ERR handler() { return QtOS2SysXcptMainHandler::handler; }
};

#define XcptMainHandler QtOS2SysXcptMainHandlerInternal

// need to get default font?
extern bool qt_app_has_font;

void qt_init( int *argcptr, char **argv, QApplication::Type )
{
#if !defined(QT_OS2_NO_SYSEXCEPTIONS)
    // Even if the user didn't install the exception handler for the main
    // thread using QtOS2SysXcptMainHandler prior to constructing a QApplication
    // instance, we still want it to be installed to catch system traps. But
    // since the exception registration record can only be located on the stack,
    // we can enable our exception handling on the main thread only if the LIBC
    // library of the compiler provides an exception handler (already allocated
    // on the stack before main() is called). In this case, we replace the LIBC
    // exception handler's pointer with our own (not nice but what to do?) and
    // call the former (to let it do signal processing and the stuff) after we
    // generate the trap file. Note that in this tricky case, the user has no
    // possibility to install the exception handler callback (since it's only
    // possible using QtOS2SysXcptMainHandler).

    PTIB ptib = NULL;
    DosGetInfoBlocks( &ptib, NULL );
    Q_ASSERT( ptib && ptib->tib_ptib2 );

    if ( ptib && ptib->tib_ptib2 ) {
        // must be called only on the main (first) thread
        Q_ASSERT( ptib->tib_ptib2->tib2_ultid == 1 );
        // also make sure that QtOS2SysXcptMainHandler was not already
        // instantiated on the main thread
        if ( ptib->tib_ptib2->tib2_ultid == 1 &&
             XcptMainHandler::installed() == FALSE )
        {
            if ( ptib->tib_pexchain != END_OF_CHAIN ) {
                PEXCEPTIONREGISTRATIONRECORD r =
                    (PEXCEPTIONREGISTRATIONRECORD) ptib->tib_pexchain;
                XcptMainHandler::libcHandler() = r->ExceptionHandler;
                r->ExceptionHandler = XcptMainHandler::handler();
                XcptMainHandler::installed() = TRUE;
            } else {
                Q_ASSERT( 0 /* no exception handler for the main thread */ );
            }
        }
    }
#endif

#if defined(QT_DEBUG)
    int argc = *argcptr;
    int i, j;

    // Get command line params

    j = argc ? 1 : 0;
    for ( i=1; i<argc; i++ ) {
	if ( argv[i] && *argv[i] != '-' ) {
	    argv[j++] = argv[i];
	    continue;
	}
	QCString arg = argv[i];
	if ( arg == "-nograb" )
	    appNoGrab = !appNoGrab;
	else
	    argv[j++] = argv[i];
    }
    *argcptr = j;
#else
    Q_UNUSED( argcptr );
    Q_UNUSED( argv );
#endif // QT_DEBUG

    // Ensure we are in PM mode
    qt_ensure_pm();
    // Force creation of the main event queue to make it possible to
    // create windows before the application invokes exec()
    QApplication::eventLoop();

    // Misc. initialization
    QPMMime::initialize();
    QColor::initialize();
    QFont::initialize();
    QCursor::initialize();
    QPainter::initialize();
#if defined(QT_THREAD_SUPPORT)
    QThread::initialize();
#endif
    qApp->setName( qAppName() );

    // default font
    if ( !qt_app_has_font )
        QApplication::setFont( QFont( "System Proportional", 10 ) );

    // QFont::locale_init();  ### Uncomment when it does something on OS/2

    if ( !qt_std_pal )
	qt_create_std_palette();

    if ( QApplication::desktopSettingsAware() )
	qt_set_pm_resources();

/// @todo (dmik) remove?
//    QInputContext::init();
}

/*****************************************************************************
  qt_cleanup() - cleans up when the application is finished
 *****************************************************************************/

void qt_cleanup()
{
    unregWinClasses();
    QPixmapCache::clear();
    QPainter::cleanup();
    QCursor::cleanup();
    QFont::cleanup();
    QColor::cleanup();
    QSharedDoubleBuffer::cleanup();
#if defined(QT_THREAD_SUPPORT)
    QThread::cleanup();
#endif
    if ( displayPS ) {
	WinReleasePS( displayPS );
	displayPS = 0;
    }

//@@TODO (dmik): remove?
//    QInputContext::shutdown();

#if !defined(QT_OS2_NO_SYSEXCEPTIONS)
    // remove the exception handler if it was installed in qt_init()
    PTIB ptib = NULL;
    DosGetInfoBlocks( &ptib, NULL );
    Q_ASSERT( ptib && ptib->tib_ptib2 );

    if ( ptib && ptib->tib_ptib2 ) {
        // must be called only on the main (first) thread
        Q_ASSERT( ptib->tib_ptib2->tib2_ultid == 1 );
        // also make sure the handler was really installed by qt_init()
        if ( ptib->tib_ptib2->tib2_ultid == 1 &&
             XcptMainHandler::installed() == TRUE &&
             XcptMainHandler::libcHandler() != NULL )
        {
            Q_ASSERT( ptib->tib_pexchain != END_OF_CHAIN );
            if ( ptib->tib_pexchain != END_OF_CHAIN ) {
                PEXCEPTIONREGISTRATIONRECORD r =
                    (PEXCEPTIONREGISTRATIONRECORD) ptib->tib_pexchain;
                Q_ASSERT( r->ExceptionHandler == XcptMainHandler::handler() );
                if ( r->ExceptionHandler == XcptMainHandler::handler() ) {
                    r->ExceptionHandler = XcptMainHandler::libcHandler();
                    XcptMainHandler::libcHandler() = NULL;
                    XcptMainHandler::installed() = FALSE;
                }
            }
        }
    }
#endif
}


/*****************************************************************************
  Platform specific global and internal functions
 *****************************************************************************/

Q_EXPORT const char *qAppFileName()		// get application file name
{
    static char appFileName[CCHMAXPATH] = "\0";
    if ( !appFileName[0] ) {
        PPIB ppib;
        DosGetInfoBlocks( NULL, &ppib );
        DosQueryModuleName( ppib->pib_hmte, sizeof(appFileName), appFileName );
    }
    return appFileName;
}

Q_EXPORT const char *qAppName()			// get application name
{
    static char appName[CCHMAXPATH] = "\0";
    if ( !appName[0] ) {
	const char *p = strrchr( qAppFileName(), '\\' );    // skip path
	if ( p )
	    memcpy( appName, p+1, qstrlen(p) );
	int l = qstrlen( appName );
	if ( (l > 4) && !qstricmp( appName + l - 4, ".exe" ) )
	    appName[l-4] = '\0';		// drop .exe extension
    }
    return appName;
}

Q_EXPORT HPS qt_display_ps()			// get display PS
{
    if ( !displayPS )
	displayPS = WinGetScreenPS( HWND_DESKTOP );
    return displayPS;
}

bool qt_nograb()				// application no-grab option
{
#if defined(QT_DEBUG)
    return appNoGrab;
#else
    return FALSE;
#endif
}


static QAsciiDict<int> *winclassNames = 0;

const QString qt_reg_winclass( int flags )	// register window class
{
    if ( flags & Qt::WType_Desktop )
        return QString();

    if ( !winclassNames ) {
	winclassNames = new QAsciiDict<int>;
	Q_CHECK_PTR( winclassNames );
    }

    ULONG style = 0;
    QString cname;
    if ( flags & Qt::WType_TopLevel ) {
        if ( flags & Qt::WType_Popup ) {
            cname = "QPopup";
        } else {
            // this class is for frame window clients when WC_FRAME is used.
            cname = "QClient";
            style |= CS_MOVENOTIFY;
        }
    } else {
	cname = "QWidget";
    }

    if ( winclassNames->find(cname.latin1()) )		// already registered
	return cname;

    // QT_EXTRAWINDATASIZE is defined in qwindowdefs_pm.h.
    WinRegisterClass( 0, cname.latin1(), QtWndProc, style, QT_EXTRAWINDATASIZE );

    winclassNames->insert( cname.latin1(), (int*)1 );
    return cname;
}

static void unregWinClasses()
{
    if ( !winclassNames )
	return;
    // there is no need to unregister private window classes -- it is done
    // automatically upon process termination.
    delete winclassNames;
    winclassNames = 0;
}


/*****************************************************************************
  Platform specific QApplication members
 *****************************************************************************/

void QApplication::setMainWidget( QWidget *mainWidget )
{
    main_widget = mainWidget;
}

//@@TODO (dmik): later (os/2 version?)
//Qt::WindowsVersion QApplication::winVersion()
//{
//    return qt_winver;
//}

#ifndef QT_NO_CURSOR

/*****************************************************************************
  QApplication cursor stack
 *****************************************************************************/

typedef QPtrList<QCursor> QCursorList;

static QCursorList *cursorStack = 0;

void QApplication::setOverrideCursor( const QCursor &cursor, bool replace )
{
    if ( !cursorStack ) {
	cursorStack = new QCursorList;
	Q_CHECK_PTR( cursorStack );
	cursorStack->setAutoDelete( TRUE );
    }
    app_cursor = new QCursor( cursor );
    Q_CHECK_PTR( app_cursor );
    if ( replace )
	cursorStack->removeLast();
    cursorStack->append( app_cursor );
    WinSetPointer( HWND_DESKTOP, app_cursor->handle() );
}

void QApplication::restoreOverrideCursor()
{
    if ( !cursorStack )				// no cursor stack
	return;
    cursorStack->removeLast();
    app_cursor = cursorStack->last();
    if ( app_cursor ) {
	WinSetPointer( HWND_DESKTOP, app_cursor->handle() );
    } else {
	delete cursorStack;
	cursorStack = 0;
	QWidget *w = QWidget::find( curWin );
	if ( w )
	    WinSetPointer( HWND_DESKTOP, w->cursor().handle() );
	else
	    WinSetPointer( HWND_DESKTOP, arrowCursor.handle() );
    }
}

#endif

/*
  Internal function called from QWidget::setCursor()
*/

void qt_set_cursor( QWidget *w, const QCursor& /* c */)
{
    if ( !curWin )
	return;
    QWidget* cW = QWidget::find( curWin );
    if ( !cW || cW->topLevelWidget() != w->topLevelWidget() ||
	 !cW->isVisible() || !cW->hasMouse() || cursorStack )
	return;

    WinSetPointer( HWND_DESKTOP, cW->cursor().handle() );
}


void QApplication::setGlobalMouseTracking( bool enable )
{
    bool tellAllWidgets;
    if ( enable ) {
	tellAllWidgets = (++app_tracking == 1);
    } else {
	tellAllWidgets = (--app_tracking == 0);
    }
    if ( tellAllWidgets ) {
	QWidgetIntDictIt it( *((QWidgetIntDict*)QWidget::mapper) );
	register QWidget *w;
	while ( (w=it.current()) ) {
	    if ( app_tracking > 0 ) {		// switch on
		if ( !w->testWState(WState_MouseTracking) ) {
		    w->setMouseTracking( TRUE );
		}
	    } else {				// switch off
		if ( w->testWState(WState_MouseTracking) ) {
		    w->setMouseTracking( FALSE );
		}
	    }
	    ++it;
	}
    }
}


/*****************************************************************************
  Routines to find a Qt widget from a screen position
 *****************************************************************************/

static QWidget *findChildWidget( const QWidget *p, const QPoint &pos )
{
    if ( p->children() ) {
	QWidget *w;
	QObjectListIt it( *p->children() );
	it.toLast();
	while ( it.current() ) {
	    if ( it.current()->isWidgetType() ) {
		w = (QWidget*)it.current();
		if ( w->isVisible() && w->geometry().contains(pos) ) {
		    QWidget *c = findChildWidget( w, w->mapFromParent(pos) );
		    return c ? c : w;
		}
	    }
	    --it;
	}
    }
    return 0;
}

QWidget *QApplication::widgetAt( int x, int y, bool child )
{
    // flip y coordinate
    y = desktop()->height() - (y + 1);
    POINTL ptl = { x, y };
    HWND win = WinWindowFromPoint( HWND_DESKTOP, &ptl, child );
    if ( !win )
	return 0;

    QWidget *w = QWidget::find( win );
    if( !w && !child )
        return 0;
//@@TODO (dmik): can it ever happen that a child window is a not Qt window
//  but its (grand) parent is?
    while (
        !w && (win = WinQueryWindow( win, QW_PARENT )) != desktop()->winId()
    ) {
	w = QWidget::find( win );
    }
    return w;
}

void QApplication::beep()
{
    WinAlarm( HWND_DESKTOP, WA_WARNING );
}

/*****************************************************************************
  OS/2-specific drawing used here
 *****************************************************************************/

static void drawTile( HPS hps, int x, int y, int w, int h,
		      const QPixmap *pixmap, int xOffset, int yOffset, int devh )
{
    int yPos, xPos, drawH, drawW, yOff, xOff;
    yPos = y;
    yOff = yOffset;
    while( yPos < y + h ) {
	drawH = pixmap->height() - yOff;	// Cropping first row
	if ( yPos + drawH > y + h )		// Cropping last row
	    drawH = y + h - yPos;
	xPos = x;
	xOff = xOffset;
	while( xPos < x + w ) {
	    drawW = pixmap->width() - xOff;	// Cropping first column
	    if ( xPos + drawW > x + w )		// Cropping last column
		drawW = x + w - xPos;
//@@TODO (dmik): optimize: flip y only once and change loops accodringly
            // flip y coordinates
            POINTL ptls[] = {
                { xPos, yPos }, { xPos + drawW, drawH },
                { xOff, pixmap->height() - (yOff + drawH) }
            };
            if ( devh ) ptls[0].y = devh - (ptls[0].y + drawH);
            ptls[1].y += ptls[0].y;
            GpiBitBlt( hps, pixmap->handle(), 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
	    xPos += drawW;
	    xOff = 0;
	}
	yPos += drawH;
	yOff = 0;
    }
}

//@@TODO (dmik): implement as fill pattern?
//  masks and transforms are not used here.
Q_EXPORT
void qt_fill_tile( QPixmap *tile, const QPixmap &pixmap )
{
    copyBlt( tile, 0, 0, &pixmap, 0, 0, -1, -1);
    int x = pixmap.width();
    while ( x < tile->width() ) {
	copyBlt( tile, x, 0, tile, 0, 0, x, pixmap.height() );
	x *= 2;
    }
    int y = pixmap.height();
    while ( y < tile->height() ) {
	copyBlt( tile, 0, y, tile, 0, 0, tile->width(), y );
	y *= 2;
    }
}

//@@TODO (dmik): implement as fill pattern?
//  masks and transforms are not used here.
Q_EXPORT
void qt_draw_tiled_pixmap( HPS hps, int x, int y, int w, int h,
			   const QPixmap *bg_pixmap,
			   int off_x, int off_y, int devh )
{
    QPixmap *tile = 0;
    QPixmap *pm;
    int  sw = bg_pixmap->width(), sh = bg_pixmap->height();
    if ( sw*sh < 8192 && sw*sh < 16*w*h ) {
	int tw = sw, th = sh;
	while ( tw*th < 32678 && tw < w/2 )
	    tw *= 2;
	while ( tw*th < 32678 && th < h/2 )
	    th *= 2;
	tile = new QPixmap( tw, th, bg_pixmap->depth(),
			    QPixmap::NormalOptim );
	qt_fill_tile( tile, *bg_pixmap );
	pm = tile;
    } else {
	pm = (QPixmap*)bg_pixmap;
    }
    drawTile( hps, x, y, w, h, pm, off_x, off_y, devh );
    if ( tile )
	delete tile;
}



/*****************************************************************************
  Main event loop
 *****************************************************************************/

extern uint qGlobalPostedEventsCount();

#ifndef QT_NO_DRAGANDDROP
extern MRESULT qt_dispatchDragAndDrop( QWidget *, const QMSG & ); // qdnd_pm.cpp
#endif

/*!
    The message procedure calls this function for every message
    received. Reimplement this function if you want to process window
    messages \e msg that are not processed by Qt. If you don't want
    the event to be processed by Qt, then return TRUE; otherwise
    return FALSE.
*/
bool QApplication::pmEventFilter( QMSG * /*msg*/ )	// OS/2 PM event filter
{
    return FALSE;
}

/*!
    If \a gotFocus is TRUE, \a widget will become the active window.
    Otherwise the active window is reset to NULL.
*/
void QApplication::pmFocus( QWidget *widget, bool gotFocus )
{
    if ( gotFocus ) {
	setActiveWindow( widget );
	if ( active_window && active_window->testWFlags( WType_Dialog ) ) {
	    // raise the entire application, not just the dialog
	    QWidget* mw = active_window;
	    while( mw->parentWidget() && mw->testWFlags( WType_Dialog) )
		mw = mw->parentWidget()->topLevelWidget();
	    if ( mw != active_window )
                WinSetWindowPos( mw->winFId(), HWND_TOP, 0, 0, 0, 0, SWP_ZORDER );
	}
    } else {
	setActiveWindow( 0 );
    }
}

//
// QtWndProc() receives all messages from the main event loop
//

enum {
    // some undocumented messages (they have the WM_U_ prefix for clarity)

    // sent to hwnd that has been entered to by a mouse pointer.
    // FID_CLIENT also receives enter messages of its WC_FRAME.
    // mp1 = hwnd that is entered, mp2 = hwnd that is left
    WM_U_MOUSEENTER = 0x41E,
    // sent to hwnd that has been left by a mouse pointer.
    // FID_CLIENT also receives leave messages of its WC_FRAME.
    // mp1 = hwnd that is left, mp2 = hwnd that is entered
    WM_U_MOUSELEAVE = 0x41F,

    // some undocumented system values
    
    SV_WORKAREA_YTOP = 51,
    SV_WORKAREA_YBOTTOM = 52,
    SV_WORKAREA_XRIGHT = 53,
    SV_WORKAREA_XLEFT = 54,
};

static bool inLoop = FALSE;

#define RETURN(x) { inLoop = FALSE; return (MRESULT) (x); }

inline bool qt_sendSpontaneousEvent( QObject *receiver, QEvent *event )
{
    return QApplication::sendSpontaneousEvent( receiver, event );
}

extern "C" MRESULT EXPENTRY QtWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    // message handling indicators: if result is true at the end of message
    // processing, no default window proc is called but rc is returned.
    bool result = TRUE;
    MRESULT rc = (MRESULT) 0;

    QEvent::Type evt_type = QEvent::None;
    QETWidget *widget = 0;
    bool isMouseEvent = FALSE;

    if ( !qApp )				// unstable app state
	goto do_default;

    // make sure we show widgets (e.g. scrollbars) when the user resizes
    if ( inLoop && qApp->loopLevel() )
	qApp->sendPostedEvents( 0, QEvent::ShowWindowRequest );

    inLoop = TRUE;

    isMouseEvent =
        (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) ||
        (msg >= WM_EXTMOUSEFIRST && msg <= WM_EXTMOUSELAST);
//@@TODO (dmik): later (extra buttons)
//	    (message >= WM_XBUTTONDOWN && message <= WM_XBUTTONDBLCLK)

    QMSG qmsg;                                  // create QMSG structure
    qmsg.hwnd = hwnd;
    qmsg.msg = msg;
    qmsg.mp1 = mp1;
    qmsg.mp2 = mp2;
    qmsg.time = WinQueryMsgTime( 0 );
    if ( isMouseEvent || msg == WM_CONTEXTMENU ) {
        qmsg.ptl.x = (short)SHORT1FROMMP(mp1);
        qmsg.ptl.y = (short)SHORT2FROMMP(mp1);
        WinMapWindowPoints( qmsg.hwnd, HWND_DESKTOP, &qmsg.ptl, 1 );
    } else {
        WinQueryMsgPos( 0, &qmsg.ptl );
    }
    // flip y coordinate
    qmsg.ptl.y = QApplication::desktop()->height() - (qmsg.ptl.y + 1);

    if ( qt_pmEventFilter( &qmsg, rc ) )	// send through app filter
	RETURN( rc );

    switch ( msg ) {
#if !defined (QT_NO_SESSIONMANAGER)
        case WM_SAVEAPPLICATION: {
#if defined (DEBUG_SESSIONMANAGER)
            qDebug( "WM_SAVEAPPLICATION: sm_gracefulShutdown=%d "
                    "qt_about_to_destroy_wnd=%d (mp1=%p mp2=%p)",
                    sm_gracefulShutdown, qt_about_to_destroy_wnd,
                    mp1, mp2 );
#endif
            // PM seems to post this message to all top-level windows on system
            // shutdown, so react only to the first one. Also, this message is
            // always sent by WinDestroyWindow(), where it must be also ignored.
            if ( !qt_about_to_destroy_wnd && !sm_smActive &&
                 !sm_gracefulShutdown ) {
                sm_smActive = TRUE;
                sm_gracefulShutdown = TRUE;
                sm_blockUserInput = TRUE; // prevent user-interaction outside interaction windows
                sm_cancel = FALSE;
                sm_quitSkipped = FALSE;
                if ( qt_session_manager_self )
                    qApp->commitData( *qt_session_manager_self );
                sm_smActive = FALSE; // session management has been finished
                if ( sm_cancel ) {
                    // Here we try to cancel the Extended XWorkplace shutdown.
                    // If it's XWorkplace who sent us WM_SAVEAPPLICATION, then
                    // it probably passed us non-NULL parameters, so that
                    // mp1 = it's window handle and mp2 = WM_COMMAND code to
                    // cancel the shutdown procedure.
                    HWND shutdownHwnd = HWNDFROMMP(mp1);
                    if ( WinIsWindow( 0, shutdownHwnd ) ) {
                        WinPostMsg( shutdownHwnd, WM_COMMAND, mp2, 0 );
                        // Ensure we will get WM_QUIT anyway, even if xwp was
                        // not that fast to post it yet (we need it to correctly
                        // finish the graceful shutdown procedure)
                        sm_quitSkipped = TRUE;
                    }
                }
                // repost WM_QUIT to ourselves because we might have ignored
                // it in qt_app_canQuit(), so will not get one anymore
                if ( sm_quitSkipped )
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
            }
            // PMREF recommends to pass it to WinDefWindowProc()
            rc = WinDefWindowProc( hwnd, msg, mp1, mp2 );
            RETURN( rc );
        }
#endif

        case WM_SYSVALUECHANGED: {
            // This message is sent to all top-level widgets, handle only once
            QWidgetList *list = QApplication::topLevelWidgets();
            bool firstWidget = list->first()->winId() == hwnd;
            delete list;
            if ( !firstWidget )
                break;
            LONG from = (LONG) mp1;
            LONG to = (LONG) mp2;
            #define _IS_SV(sv) (from >= (sv) && to <= (sv))
            if ( _IS_SV( SV_WORKAREA_XLEFT ) || _IS_SV( SV_WORKAREA_XRIGHT ) ||
                 _IS_SV( SV_WORKAREA_YBOTTOM ) || _IS_SV( SV_WORKAREA_YTOP ) ) {
                 // send a special invalid resize event to QDesktopWidget
                 QResizeEvent re( QSize( -1, -1 ), QSize( -1, -1 ) );
                 QApplication::sendEvent( qt_desktopWidget, &re );
                 /// @todo (dmik) enumerate all top-level widgets and
                 //  remaximize those that are maximized
            } else {
                 /// @todo (dmik) call qt_set_pm_resources() in the way it is
                 //  done in WM_SYSCOLORCHANGE for relevant SV_ values.
            }
            #undef _IS_SV
            break;
        }

        case WM_SYSCOLORCHANGE: {
            // This message is sent to all top-level widgets, handle only once
            QWidgetList *list = QApplication::topLevelWidgets();
            bool firstWidget = list->first()->winId() == hwnd;
            delete list;
            if ( !firstWidget )
                break;
            if ( qApp->type() == QApplication::Tty )
                break;
            if ( QApplication::desktopSettingsAware() ) {
                widget = (QETWidget*)QWidget::find( hwnd );
                if ( widget && !widget->parentWidget() )
                    qt_set_pm_resources();
            }
            break;
        }

        case WM_BUTTON1DOWN:
        case WM_BUTTON2DOWN:
        case WM_BUTTON3DOWN:
            if ( ignoreNextMouseReleaseEvent )
                ignoreNextMouseReleaseEvent = FALSE;
            break;
        case WM_BUTTON1UP:
        case WM_BUTTON2UP:
        case WM_BUTTON3UP:
            if ( ignoreNextMouseReleaseEvent ) {
                ignoreNextMouseReleaseEvent = FALSE;
                if ( qt_button_down && qt_button_down->winId() == autoCaptureWnd ) {
                    releaseAutoCapture();
                    qt_button_down = 0;
                }
                RETURN( TRUE );
            }
            break;

        default:
            break;
    }

    if ( !widget )
	widget = (QETWidget*)QWidget::find( hwnd );
    if ( !widget )				// don't know this widget
	goto do_default;

    if ( app_do_modal )	{			// modal event handling
	int ret = 0;
	if ( !qt_try_modal( widget, &qmsg, ret ) )
	    return MRESULT( ret );
    }

    // send through widget filter
    // (WM_CHAR will be handled later)
    if ( msg != WM_CHAR )
        if ( widget->pmEvent( &qmsg ) )
            RETURN( TRUE );

    if ( isMouseEvent ) {                       // mouse events
        if ( qApp->activePopupWidget() != 0 ) { // in popup mode
            QWidget* w = QApplication::widgetAt( qmsg.ptl.x, qmsg.ptl.y, TRUE );
            if ( w ) {
                POINTL ptl = { SHORT1FROMMP(qmsg.mp1), SHORT2FROMMP(qmsg.mp1) };
                WinMapWindowPoints( qmsg.hwnd, w->winId(), &ptl, 1 );
                qmsg.mp1 = MPFROM2SHORT( ptl.x, ptl.y );
                widget = (QETWidget*)w;
            }
        }
        result = widget->translateMouseEvent( qmsg );
        rc = (MRESULT) result;
#ifndef QT_NO_WHEELEVENT
    } else if ( msg == WM_VSCROLL || msg == WM_HSCROLL ) {
        result = widget->translateWheelEvent( qmsg );
        rc = (MRESULT) result;
#endif
#ifndef QT_NO_DRAGANDDROP
    } else if ( msg >= WM_DRAGFIRST && msg <= WM_DRAGLAST ) {
        RETURN( qt_dispatchDragAndDrop( widget, qmsg ) );
#endif
    } else {
        switch ( msg ) {
        
        case WM_TRANSLATEACCEL:
            if ( widget->isTopLevel() ) {
                rc = WinDefWindowProc( hwnd, msg, mp1, mp2 );
                if ( rc ) {
                    QMSG &qmsg = *(QMSG*) mp1;
                    if (
                        qmsg.msg == WM_SYSCOMMAND &&
                        WinWindowFromID( widget->winFId(), FID_SYSMENU )
                    ) {
                        switch ( SHORT1FROMMP(qmsg.mp1) ) {
                            case SC_CLOSE:
                            case SC_TASKMANAGER:
                                RETURN( TRUE );
                            default:
                                break;
                        }
                    }
                }
            }
            // return FALSE in all other cases to let Qt process keystrokes
            // that are in the system-wide frame accelerator table.
            RETURN( FALSE );

        case WM_CHAR: {			// keyboard event
#if 0
            qDebug( "WM_CHAR:  [%s]", widget->name() );
#endif
            QWidget *g = QWidget::keyboardGrabber();
            if ( g )
                widget = (QETWidget*)g;
            else if ( qApp->focusWidget() )
                widget = (QETWidget*)qApp->focusWidget();
            else if ( !widget )
/// @todo (dmik) currently we don't use WinSetFocus(). what for? Qt seems
//  to completely handle focus traversal itself.
//                    || widget->winId() == WinQueryFocus( HWND_DESKTOP ) ) // We faked the message to go to exactly that widget.
                widget = (QETWidget*)widget->topLevelWidget();

            if ( widget->pmEvent( &qmsg ) )
                RETURN( TRUE );

            if ( widget->isEnabled() ) {
/// @todo (dmik) we should not pass WM_CHAR to the default window proc,
//  otherwise it will come to us again through the widget parent (owner in PM)
//  if the widget is not top-level, and will be treated by translateKeyEvent()
//  as a key repeat (in case of key down) or a lost key (in case of key up).
//  NOTE: currently we don't use WinSetFocus(), so the active top-level window
//  will always have the focus, so it doesn't matter wheither we pass WM_CHAR
//  to the default window proc or not.
//                    result =  widget->translateKeyEvent( qmsg, g != 0 );
//                    rc = (MRESULT) result;
                RETURN( widget->translateKeyEvent( qmsg, g != 0 ) );
            }
            break;
        }

        case WM_QUERYCONVERTPOS :
        {
            // Ooops, how to get caret pos ?
            /// @todo (r=dmik) we should make usage of the QIMEvent
            //  to query widgets about IME data (see how/where this event
            //  is used across the sources)

            PRECTL pcp = ( PRECTL )mp1;
            CURSORINFO ci;

            WinQueryCursorInfo( HWND_DESKTOP, &ci );

            memset( pcp, 0xFF, sizeof( RECTL ));

            pcp->xLeft = ci.x;
            pcp->yBottom = ci.y;
            WinMapWindowPoints( ci.hwnd, hwnd, ( PPOINTL )pcp, 2 );

            RETURN( QCP_CONVERT );
        }

/// @todo (dmik) later
//	case WM_APPCOMMAND:
//	    {
//		uint cmd = GET_APPCOMMAND_LPARAM(lParam);
//		uint uDevice = GET_DEVICE_LPARAM(lParam);
//		uint dwKeys = GET_KEYSTATE_LPARAM(lParam);
//
//		int state = translateButtonState( dwKeys, QEvent::KeyPress, 0 );
//
//		switch ( uDevice ) {
//		case FAPPCOMMAND_KEY:
//		    {
//			int key = 0;
//
//			switch( cmd ) {
//			case APPCOMMAND_BASS_BOOST:
//			    key = Qt::Key_BassBoost;
//			    break;
//			case APPCOMMAND_BASS_DOWN:
//			    key = Qt::Key_BassDown;
//			    break;
//			case APPCOMMAND_BASS_UP:
//			    key = Qt::Key_BassUp;
//			    break;
//			case APPCOMMAND_BROWSER_BACKWARD:
//			    key = Qt::Key_Back;
//			    break;
//			case APPCOMMAND_BROWSER_FAVORITES:
//			    key = Qt::Key_Favorites;
//			    break;
//			case APPCOMMAND_BROWSER_FORWARD:
//			    key = Qt::Key_Forward;
//			    break;
//			case APPCOMMAND_BROWSER_HOME:
//			    key = Qt::Key_HomePage;
//			    break;
//			case APPCOMMAND_BROWSER_REFRESH:
//			    key = Qt::Key_Refresh;
//			    break;
//			case APPCOMMAND_BROWSER_SEARCH:
//			    key = Qt::Key_Search;
//			    break;
//			case APPCOMMAND_BROWSER_STOP:
//			    key = Qt::Key_Stop;
//			    break;
//			case APPCOMMAND_LAUNCH_APP1:
//			    key = Qt::Key_Launch0;
//			    break;
//			case APPCOMMAND_LAUNCH_APP2:
//			    key = Qt::Key_Launch1;
//			    break;
//			case APPCOMMAND_LAUNCH_MAIL:
//			    key = Qt::Key_LaunchMail;
//			    break;
//			case APPCOMMAND_LAUNCH_MEDIA_SELECT:
//			    key = Qt::Key_LaunchMedia;
//			    break;
//			case APPCOMMAND_MEDIA_NEXTTRACK:
//			    key = Qt::Key_MediaNext;
//			    break;
//			case APPCOMMAND_MEDIA_PLAY_PAUSE:
//			    key = Qt::Key_MediaPlay;
//			    break;
//			case APPCOMMAND_MEDIA_PREVIOUSTRACK:
//			    key = Qt::Key_MediaPrev;
//			    break;
//			case APPCOMMAND_MEDIA_STOP:
//			    key = Qt::Key_MediaStop;
//			    break;
//			case APPCOMMAND_TREBLE_DOWN:
//			    key = Qt::Key_TrebleDown;
//			    break;
//			case APPCOMMAND_TREBLE_UP:
//			    key = Qt::Key_TrebleUp;
//			    break;
//			case APPCOMMAND_VOLUME_DOWN:
//			    key = Qt::Key_VolumeDown;
//			    break;
//			case APPCOMMAND_VOLUME_MUTE:
//			    key = Qt::Key_VolumeMute;
//			    break;
//			case APPCOMMAND_VOLUME_UP:
//			    key = Qt::Key_VolumeUp;
//			    break;
//			default:
//			    break;
//			}
//			if ( key ) {
//			    bool res = FALSE;
//			    QWidget *g = QWidget::keyboardGrabber();
//			    if ( g )
//				widget = (QETWidget*)g;
//			    else if ( qApp->focusWidget() )
//				widget = (QETWidget*)qApp->focusWidget();
//			    else
//				widget = (QETWidget*)widget->topLevelWidget();
//			    if ( widget->isEnabled() )
//				res = ((QETWidget*)widget)->sendKeyEvent( QEvent::KeyPress, key, 0, state, FALSE, QString::null, g != 0 );
//			    if ( res )
//				return TRUE;
//			}
//		    }
//		    break;
//
//		default:
//		    break;
//		}
//
//		result = FALSE;
//	    }
//	    break;

//@@TODO (dmik):
//  we can cause WC_FRAME to send equivalents of WM_NCMOUSEMOVE,
//  but is it really necesary? We already do some similar stuff in WM_HITTEST
//#ifndef Q_OS_TEMP
//	case WM_NCMOUSEMOVE:
//	    {
//		// span the application wide cursor over the
//		// non-client area.
//		QCursor *c = qt_grab_cursor();
//		if ( !c )
//		    c = QApplication::overrideCursor();
//		if ( c )	// application cursor defined
//		    SetCursor( c->handle() );
//		else
//		    result = FALSE;
//		// generate leave event also when the caret enters
//		// the non-client area.
//		qt_dispatchEnterLeave( 0, QWidget::find(curWin) );
//		curWin = 0;
//	    }
//	    break;
//#endif

/// @todo (dmik) later (WM_KBDLAYERCHANGED (0xBD4)?)
//	case WM_SETTINGCHANGE:
//	    if ( !msg.wParam ) {
//		QString area = QT_WA_INLINE( QString::fromUcs2( (unsigned short *)msg.lParam ),
//					     QString::fromLocal8Bit( (char*)msg.lParam ) );
//		if ( area == "intl" )
//		    QApplication::postEvent( widget, new QEvent( QEvent::LocaleChange ) );
//	    }
//	    break;
//
	case WM_PAINT:				// paint event
	    result = widget->translatePaintEvent( qmsg );
	    break;
	case WM_ERASEBACKGROUND:		// erase window background
            // flush WM_PAINT messages here to update window contents
            // instantly while tracking the resize frame (normally these
            // messages are delivered after the user has stopped resizing
            // for some time). this slows down resizing slightly but gives a
            // better look (no invalid window contents can be seen during
            // resize). the alternative could be to erase the background only,
            // but we need to do it for every non-toplevel window, which can
            // also be time-consuming (WM_ERASEBACKGROUND is sent to WC_FRAME
            // clients only, so we would have to do all calculations ourselves).
            WinUpdateWindow( widget->winId() );
            RETURN( FALSE );
	    break;
	case WM_CALCVALIDRECTS:
            // we must always return this value here to cause PM to reposition
            // our children accordingly (othwerwise we would have to do it
            // ourselves to keep them top-left aligned).
            RETURN( CVR_ALIGNLEFT | CVR_ALIGNTOP );
            break;

	case WM_MOVE:				// move window
	case WM_SIZE:				// resize window
	    result = widget->translateConfigEvent( qmsg );
	    break;

        case WM_ACTIVATE:
#if 0
            qDebug( "WM_ACTIVATE: [%s] %d", widget->name(), SHORT1FROMMP(mp1) );
#endif
	    qApp->pmFocus( widget, SHORT1FROMMP(mp1) );
	    break;

        case WM_SETFOCUS:
#if 0
            qDebug( "WM_SETFOCUS: [%s] %s [%s]", widget->name(),
                    SHORT1FROMMP(mp2) ? "<=" : "=>",
                    QWidget::find( (HWND)mp1 ) ? QWidget::find( (HWND)mp1 )->name()
                                               : "{foreign}" );
#endif
            result = FALSE;
            if ( !SHORT1FROMMP(mp2) ) {
                // we're losing focus
                if ( !QWidget::find( (HWND)mp1 ) ) {
                    if ( QApplication::activePopupWidget() ) {
                        // Another application was activated while our popups are open,
                        // then close all popups.  In case some popup refuses to close,
                        // we give up after 1024 attempts (to avoid an infinite loop).
                        int maxiter = 1024;
                        QWidget *popup;
                        while ( (popup=QApplication::activePopupWidget()) && maxiter-- )
                            popup->close();
                    }
                    if (
                        // non-Qt ownees of our WC_FRAME window (such as
                        // FID_SYSMENU) should not cause the focus to be lost.
                        WinQueryWindow( (HWND)mp1, QW_OWNER ) ==
                            widget->topLevelWidget()->winFId()
                    )
                        break;
                    if ( !widget->isTopLevel() )
                        qApp->pmFocus( widget, SHORT1FROMMP(mp2) );
                }
            }
            break;

/// @todo (dmik) remove?
//#ifndef Q_OS_TEMP
//	    case WM_MOUSEACTIVATE:
//		{
//		    const QWidget *tlw = widget->topLevelWidget();
//		    // Do not change activation if the clicked widget is inside a floating dock window
//		    if ( tlw->inherits( "QDockWindow" ) && qApp->activeWindow()
//			 && !qApp->activeWindow()->inherits("QDockWindow") )
//			RETURN(MA_NOACTIVATE);
//		}
//		result = FALSE;
//		break;
//#endif

        case WM_SHOW:
            if ( !SHORT1FROMMP(mp1) && autoCaptureWnd == widget->winId() )
                releaseAutoCapture();
            result = FALSE;
            break;
/// @todo (dmik) remove later
//	    case WM_SHOWWINDOW:
//#ifndef Q_OS_TEMP
//		if ( lParam == SW_PARENTOPENING ) {
//		    if ( widget->testWState(Qt::WState_ForceHide) )
//			RETURN(0);
//		}
//#endif
//		if  (!wParam && autoCaptureWnd == widget->winId())
//		    releaseAutoCapture();
//		result = FALSE;
//		break;

	case WM_REALIZEPALETTE: 		// realize own palette
	    if ( QColor::hPal() ) {
		HPS hps = WinGetPS( widget->winId() );
                GpiSelectPalette( hps, QColor::hPal() );
                ULONG cclr;
                WinRealizePalette( widget->winId(), hps, &cclr );
                WinReleasePS( hps );
                // on OS/2, the value returned by WinRealizePalette() does not
                // necessarily reflect the number of colors that have been
                // remapped. therefore, we cannot rely on it and must always
                // invalidate the window.
                WinInvalidateRect( widget->winId(), NULL, TRUE );
                RETURN( 0 );
	    }
	    break;

	case WM_CLOSE:				// close window
	    widget->translateCloseEvent( qmsg );
	    RETURN(0);				// always handled

/// @todo (dmik) it seems we don't need this
//	case WM_DESTROY:			// destroy window
//	    if ( hwnd == curWin ) {
//		QEvent leave( QEvent::Leave );
//		QApplication::sendEvent( widget, &leave );
//		curWin = 0;
//	    }
//	    // We are blown away when our parent reparents, so we have to
//	    // recreate the handle
//	    if (widget->testWState(Qt::WState_Created))
//		((QETWidget*)widget)->reparentWorkaround();
//	    if ( widget == popupButtonFocus )
//		popupButtonFocus = 0;
//	    result = FALSE;
//	    break;

        case WM_CONTEXTMENU:
            if ( SHORT2FROMMP(mp2) ) {
                // keyboard event
                QWidget *fw = qApp->focusWidget();
                if ( fw ) {
                    QContextMenuEvent e(
                        QContextMenuEvent::Keyboard,
                        QPoint( 5, 5 ),
                        fw->mapToGlobal( QPoint( 5, 5 ) ),
                        0
                    );
                    result = qt_sendSpontaneousEvent( fw, &e );
                    rc = (MRESULT) result;
                }
            } else {
                // mouse event
                result = widget->translateMouseEvent( qmsg );
                rc = (MRESULT) result;
            }
            break;

/// @todo (dmik) remove?
//	case WM_IME_STARTCOMPOSITION:
//    	    result = QInputContext::startComposition();
//	    break;
//	case WM_IME_ENDCOMPOSITION:
//	    result = QInputContext::endComposition();
//	    break;
//	case WM_IME_COMPOSITION:
//	    result = QInputContext::composition( lParam );
//	    break;

#ifndef QT_NO_CLIPBOARD
	case WM_DRAWCLIPBOARD:
	case WM_RENDERFMT:
	case WM_RENDERALLFMTS:
	case WM_DESTROYCLIPBOARD:
	    if ( qt_clipboard ) {
		QCustomEvent e( QEvent::Clipboard, &qmsg );
		qt_sendSpontaneousEvent( qt_clipboard, &e );
		RETURN(0);
	    }
	    result = FALSE;
	    break;
#endif

/// @todo (dmik) remove? functionality is implemented in WM_SETFOCUS above.
//	case WM_KILLFOCUS:
//	    if ( !QWidget::find( (HWND)wParam ) ) { // we don't get focus, so unset it now
//		if ( !widget->hasFocus() ) // work around Windows bug after minimizing/restoring
//		    widget = (QETWidget*)qApp->focusWidget();
//		HWND focus = ::GetFocus();
//		if ( !widget || (focus && ::IsChild( widget->winId(), focus )) ) {
//		    result = FALSE;
//		} else {
//		    widget->clearFocus();
//		    result = TRUE;
//		}
//	    } else {
//		result = FALSE;
//	    }
//	    break;

//@@TODO (dmik): later
//	case WM_THEMECHANGED:
//	    if ( widget->testWFlags( Qt::WType_Desktop ) || !qApp || qApp->closingDown()
//							 || qApp->type() == QApplication::Tty )
//		break;
//
//	    if ( widget->testWState(Qt::WState_Polished) )
//		qApp->style().unPolish(widget);
//
//	    if ( widget->testWState(Qt::WState_Polished) )
//		qApp->style().polish(widget);
//	    widget->repolishStyle( qApp->style() );
//	    if ( widget->isVisible() )
//		widget->update();
//	    break;
//
//	case WM_COMMAND:
//	    result = (wParam == 0x1);
//	    if ( result )
//		QApplication::postEvent( widget, new QEvent( QEvent::OkRequest ) );
//	    break;
//	case WM_HELP:
//	    QApplication::postEvent( widget, new QEvent( QEvent::HelpRequest ) );
//	    result = TRUE;
//	    break;
//#endif

	case WM_U_MOUSELEAVE:
	    // We receive a mouse leave for curWin, meaning
	    // the mouse was moved outside our widgets
	    if ( widget->winId() == curWin && (HWND) mp1 == curWin ) {
		bool dispatch = !widget->hasMouse();
		// hasMouse is updated when dispatching enter/leave,
		// so test if it is actually up-to-date
		if ( !dispatch ) {
		    QRect geom = widget->geometry();
		    if ( widget->parentWidget() && !widget->isTopLevel() ) {
			QPoint gp = widget->parentWidget()->mapToGlobal( widget->pos() );
			geom.setX( gp.x() );
			geom.setY( gp.y() );
		    }
		    QPoint cpos = QCursor::pos();
		    dispatch = !geom.contains( cpos );
		}
		if ( dispatch ) {
		    qt_dispatchEnterLeave( 0, QWidget::find( (WId)curWin ) );
		    curWin = 0;
		}
	    }
	    break;

/// @todo (dmik) remove? functionality is implemented in WM_SETFOCUS above.
//	case WM_CANCELMODE:
//	    if ( qApp->focusWidget() ) {
//		QFocusEvent::setReason( QFocusEvent::ActiveWindow );
//		QFocusEvent e( QEvent::FocusOut );
//		QApplication::sendEvent( qApp->focusWidget(), &e );
//		QFocusEvent::resetReason();
//	    }
//	    break;

	default:
	    result = FALSE;			// event was not processed
	    break;
	}
    }

    if ( evt_type != QEvent::None ) {		// simple event
	QEvent e( evt_type );
	result = qt_sendSpontaneousEvent( widget, &e );
    }
    if ( result )
	RETURN( rc );

do_default:
    RETURN( WinDefWindowProc( hwnd, msg, mp1, mp2 ) );
/// @todo (dmik) remove?
//    RETURN( QInputContext::DefWindowProc(hwnd,message,wParam,lParam) )
}

PFNWP QtOldFrameProc = 0;

extern "C" MRESULT EXPENTRY QtFrameProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    // message handling indicators: if result is true at the end of message
    // processing, no default window proc is called but rc is returned.
    bool result = FALSE;
    MRESULT rc = (MRESULT) FALSE;
    QETWidget *widget = 0;
    HWND hwndC = 0;

    if ( !qApp )				// unstable app state
	goto do_default;

    // make sure we show widgets (e.g. scrollbars) when the user resizes
    if ( inLoop && qApp->loopLevel() )
	qApp->sendPostedEvents( 0, QEvent::ShowWindowRequest );

    inLoop = TRUE;

    hwndC = WinWindowFromID( hwnd, FID_CLIENT );
    widget = (QETWidget*)QWidget::find( hwndC );
    if ( !widget )				// don't know this widget
	goto do_default;

    switch ( msg ) {
        case WM_HITTEST: {
            if ( !WinIsWindowEnabled( hwnd ) ) {
                if (
                    qApp->activePopupWidget() &&
                    (WinQueryQueueStatus( HWND_DESKTOP ) & QS_MOUSEBUTTON)
                ) {
                    // the user has clicked over the Qt window that is disabled
                    // by some modal widget, therefore we close all popups. In
                    // case some popup refuses to close, we give up after 1024
                    // attempts (to avoid an infinite loop).
                    int maxiter = 1024;
                    QWidget *popup;
                    while ( (popup=QApplication::activePopupWidget()) && maxiter-- )
                        popup->close();
                }
#ifndef QT_NO_CURSOR
                else {
                    QCursor *c = qt_grab_cursor();
                    if ( !c )
                        c = QApplication::overrideCursor();
                    if ( c )				// application cursor defined
                        WinSetPointer( HWND_DESKTOP, c->handle() );
                    else
                        WinSetPointer( HWND_DESKTOP, Qt::arrowCursor.handle() );
                }
#endif
            }
            break;
        }

        case WM_ADJUSTWINDOWPOS: {
            SWP &swp = *(PSWP) mp1;
            if ( swp.fl & SWP_MAXIMIZE ) {
                QWExtra *x = widget->xtra();
                if ( x ) {
                    result = TRUE;
                    rc = QtOldFrameProc( hwnd, msg, mp1, mp2 );
                    int maxw = QWIDGETSIZE_MAX, maxh = QWIDGETSIZE_MAX;
                    QTLWExtra *top = widget->top();
                    if ( x->maxw < QWIDGETSIZE_MAX )
                        maxw = x->maxw + top->fleft + top->fright;
                    if ( x->maxh < QWIDGETSIZE_MAX )
                        maxh  = x->maxh + top->ftop + top->fbottom;
                    if ( maxw < QWIDGETSIZE_MAX ) swp.cx = maxw;
                    if ( maxh < QWIDGETSIZE_MAX ) {
                        swp.y = swp.y + swp.cy - maxh;
                        swp.cy = maxh;
                    }
                }
            }
            if ( (swp.fl & SWP_RESTORE) &&
                 !(swp.fl & (SWP_MOVE | SWP_SIZE)) ) {
                QRect r = widget->top()->normalGeometry;
                if ( r.isValid() ) {
                    // store normal geometry in window words
                    USHORT x = r.x();
                    USHORT y = r.y();
                    USHORT w = r.width();
                    USHORT h = r.height();
                    // flip y coordinate
                    y = QApplication::desktop()->height() - (y + h);
                    WinSetWindowUShort( hwnd, QWS_XRESTORE, x );
                    WinSetWindowUShort( hwnd, QWS_YRESTORE, y );
                    WinSetWindowUShort( hwnd, QWS_CXRESTORE, w );
                    WinSetWindowUShort( hwnd, QWS_CYRESTORE, h );
                    widget->top()->normalGeometry.setWidth( 0 );
                }
            }
            if ( swp.fl & (SWP_ACTIVATE | SWP_ZORDER) ) {
                // get the modal widget that made this window blocked
                QWidget *m =
                    (QWidget*) WinQueryWindowPtr( widget->winId(), QWL_QTMODAL );
                if( m ) {
                    if ( swp.fl & SWP_ACTIVATE ) {
                        QWidget *a = qt_modal_stack->first();
                        if ( !a->isActiveWindow() )
                            a->setActiveWindow();
                        swp.fl &= ~SWP_ACTIVATE;
                    }
                    if ( swp.fl & SWP_ZORDER ) {
                        QWidget *mp = m->parentWidget();
                        if ( mp ) {
                            mp = mp->topLevelWidget();
                            if ( !mp->isDesktop() && mp != widget )
                                m = mp;
                        }
                        HWND hm = m->winFId();
                        if ( swp.hwndInsertBehind != hm ) {
                            swp.hwndInsertBehind = hm;
                        }
                    }
                }
            }
            break;
        }

        case WM_QUERYTRACKINFO: {
            QWExtra *x = widget->xtra();
            if ( x ) {
                result = TRUE;
                rc = QtOldFrameProc( hwnd, msg, mp1, mp2 );
                PTRACKINFO pti = (PTRACKINFO) mp2;
                int minw = 0, minh = 0;
                int maxw = QWIDGETSIZE_MAX, maxh = QWIDGETSIZE_MAX;
                QTLWExtra *top = widget->top();
                if ( x->minw > 0 )
                    minw = x->minw + top->fleft + top->fright;
                if ( x->minh > 0 )
                    minh = x->minh + top->ftop + top->fbottom;
                if ( x->maxw < QWIDGETSIZE_MAX )
                    maxw = x->maxw + top->fleft + top->fright;
                if ( x->maxh < QWIDGETSIZE_MAX )
                    maxh  = x->maxh + top->ftop + top->fbottom;
                // obey system recommended minimum size (to emulate Qt/Win32)
                pti->ptlMinTrackSize.x = QMAX( minw, pti->ptlMinTrackSize.x );
                pti->ptlMinTrackSize.y = QMAX( minh, pti->ptlMinTrackSize.y );
                pti->ptlMaxTrackSize.x = maxw;
                pti->ptlMaxTrackSize.y = maxh;
            }
            break;
        }

        case WM_TRACKFRAME: {
            if ( QApplication::activePopupWidget() ) {
                // The user starts to size/move the frame window, therefore
                // we close all popups.  In case some popup refuses to close,
                // we give up after 1024 attempts (to avoid an infinite loop).
                int maxiter = 1024;
                QWidget *popup;
                while ( (popup=QApplication::activePopupWidget()) && maxiter-- )
                    popup->close();
            }
            break;
        }

        case WM_WINDOWPOSCHANGED: {
            // We detect spontaneous min/max/restore events here instead of
            // WM_MINMAXFRAME, because WM_MINMAXFRAME is a pre-process message
            // (i.e. no actual changes have been made) We need actual changes
            // in order to update the frame strut and send WindowStateChange.
            result = TRUE;
            rc = QtOldFrameProc( hwnd, msg, mp1, mp2 );
            ULONG awp = LONGFROMMP( mp2 );
            bool window_state_change = FALSE;
            if ( awp & AWP_MAXIMIZED ) {
                window_state_change = TRUE;
                widget->setWState( Qt::WState_Maximized );
                widget->clearWState( Qt::WState_FullScreen );
                if ( widget->isMinimized() ) {
                    widget->clearWState( Qt::WState_Minimized );
                    widget->showChildren( TRUE );
                    QShowEvent e;
                    qt_sendSpontaneousEvent( widget, &e );
                }
            } else if ( awp & AWP_MINIMIZED ) {
                window_state_change = TRUE;
                widget->setWState( Qt::WState_Minimized );
                widget->clearWState( Qt::WState_Maximized );
                widget->clearWState( Qt::WState_FullScreen );
                if ( widget->isVisible() ) {
                    QHideEvent e;
                    qt_sendSpontaneousEvent( widget, &e );
                    widget->hideChildren( TRUE );
                }
            } else if ( awp & AWP_RESTORED ) {
                window_state_change = TRUE;
                if ( widget->isMinimized() ) {
                    widget->showChildren( TRUE );
                    QShowEvent e;
                    qt_sendSpontaneousEvent( widget, &e );
                }
                widget->clearWState( Qt::WState_Minimized );
                widget->clearWState( Qt::WState_Maximized );
            }
            if ( window_state_change ) {
                widget->updateFrameStrut();
                if ( !widget->top()->in_sendWindowState ) {
                    // send WindowStateChange event if this message is NOT
                    // originated from QWidget::setWindowState().
                    QEvent e( QEvent::WindowStateChange );
                    qt_sendSpontaneousEvent( widget, &e );
                }
            }
            break;
        }

        default:
            break;
    }

    if ( result )
	RETURN( rc );

do_default:
    RETURN( QtOldFrameProc( hwnd, msg, mp1, mp2 ) );
}

/*****************************************************************************
  Modal widgets; We have implemented our own modal widget mechanism
  to get total control.
  A modal widget without a parent becomes application-modal.
  A modal widget with a parent becomes modal to its parent and grandparents..

//@@TODO (dmik): the above comment is not correct (outdated?): for example,
//  in accordance with the current Qt logic, a modal widget with a parent
//  becomes modal to its (grand)parents upto the first of them that has the
//  WGroupLeader flag, not necessarily to all...

  qt_enter_modal()
	Enters modal state
	Arguments:
	    QWidget *widget	A modal widget

  qt_leave_modal()
	Leaves modal state for a widget
	Arguments:
	    QWidget *widget	A modal widget
 *****************************************************************************/

Q_EXPORT bool qt_modal_state()
{
    return app_do_modal;
}

// helper for qt_dispatchBlocked().
// sends block events to the given widget and its children.
void qt_sendBlocked( QObject *obj, QWidget *modal, QEvent *e, bool override )
{
    if ( obj == modal ) {
        // don't touch modal itself and its children
        return;
    }
    bool blocked = e->type() == QEvent::WindowBlocked;

    if ( obj->isWidgetType() ) {
        QWidget *w = (QWidget*) obj;
        if ( w->isTopLevel() ) {
            if ( w->testWFlags( Qt::WGroupLeader ) && !override ) {
                // stop sending on group leaders
                return;
            }
            QWidget *blockedBy =
                (QWidget*) WinQueryWindowPtr( w->winId(), QWL_QTMODAL );
            if ( blocked ) {
                // stop sending on alreay blocked widgets
                if ( blockedBy )
                    return;
            } else {
                // stop sending on widgets blocked by another modal
                if ( blockedBy != modal )
                    return;
            }
            WinSetWindowPtr( w->winId(), QWL_QTMODAL, blocked ? modal : NULL );
            WinEnableWindow( w->winFId(), !blocked );
        }
    }
    QApplication::sendEvent( obj, e );

    // now send blocked to children
    if ( obj->children() ) {
        QObjectListIt it( *obj->children() );
        QObject *o;
        while( ( o = it.current() ) != 0 ) {
            ++it;
            qt_sendBlocked( o, modal, e, blocked ? FALSE : TRUE );
        }
    }
}

// sends blocked/unblocked events to top-level widgets depending on the
// modal widget given and the WGroupLeader flag presence.
static void qt_dispatchBlocked( QWidget *modal, bool blocked )
{
    // we process those top-level windows that must be blocked
    // by the given modal -- this should correlate with the
    // qt_tryModalHelper() logic that is widget-centric (i.e. a try
    // to block a particular widget given the current set of
    // modals) rather than modal-centric (a try to block the current set
    // of widgets given a particular modal); currently it blocks events
    // for a top-level widget if the widget doesn't have (a parent with
    // with) the WGroupLeader flag or it has but (this parent) has a child
    // among the current set of modals. So, the modal-centric logic is
    // to block any top-level widget unless it has (a parent with) the
    // WGroupLeader flag and (this parent) is not (a child of) the modal's
    // group leader.

    QEvent e( blocked ? QEvent::WindowBlocked : QEvent::WindowUnblocked );

    if ( !blocked ) {
        // For unblocking, we go through all top-level widgets and unblock
        // those that have been blocked by the given modal widget. It means that
        // we don't select widgets to unblock based on widget flags (as we do
        // below when blocking) because these flags might have been changed
        // so that we could skip widgets that we would have to unblock. Although
        // normally widget flags should never be changed after widget creation
        // (except that by QWidget::reparentSys() that takes blocking into
        // account and corrects it as needed), nothing stops one from doing so,
        // leading to a case when the QWL_QTMODAL word of some widget still
        // points to a widget that has been deleted (so that any attempt to
        // access it results into a nasty segfault).
        QWidgetList *list = QApplication::topLevelWidgets();
        for( QWidget *w = list->first(); w; w = list->next() ) {
            if ( WinQueryWindowPtr( w->winId(), QWL_QTMODAL ) == modal )
                qt_sendBlocked( w, modal, &e, TRUE );
        }
        delete list;
        return;
    }

    // find the modal's group leader
    QWidget *mgl = modal->parentWidget();
    while ( mgl && !mgl->testWFlags( Qt::WGroupLeader ) )
        mgl = mgl->parentWidget();
    if ( mgl ) {
        mgl = mgl->topLevelWidget();
        if ( mgl->isDesktop() )
            mgl = 0;
    }

    QWidgetList *list = QApplication::topLevelWidgets();
    for( QWidget *w = list->first(); w; w = list->next() ) {
        if (
            !w->isDesktop() && !w->isPopup() &&
            !w->testWFlags( Qt::WGroupLeader ) &&
            (!w->parentWidget() || w->parentWidget()->isDesktop())
        ) {
            qt_sendBlocked( w, modal, &e, FALSE );
        }
    }
    delete list;

    if ( mgl ) {
        // send blocked to modal's group leader
        qt_sendBlocked( mgl, modal, &e, TRUE );
    }
    // qt_tryModalHelper() also assumes that the toppest modal widget blocks
    // other modals, regardless of WGroupLeader flags in parents. do the same.
    // Note: the given modal is not yet at the stack here.
    if ( qt_modal_stack ) {
        QWidget *m = qt_modal_stack->first();
        while ( m ) {
            qt_sendBlocked( m, modal, &e, TRUE );
            m = qt_modal_stack->next();
        }
    }
}

static inline bool isChildOf( QWidget * child, QWidget * parent )
{
    if ( !parent || !child )
	return FALSE;
    QWidget * w = child;
    while( w && w != parent )
	w = w->parentWidget();
    return w != 0;
}

Q_EXPORT void qt_enter_modal( QWidget *widget )
{
    if ( !qt_modal_stack ) {			// create modal stack
	qt_modal_stack = new QWidgetList;
	Q_CHECK_PTR( qt_modal_stack );
    }

    if ( qt_modal_stack->containsRef( widget ) )
        return; // already modal

    QWidget *m = qt_modal_stack->first();
    while ( m ) {
        if ( isChildOf( m, widget ) )
            return; // child is already modal (prevent endless recursion)
        m = qt_modal_stack->next();
    }

//@@TODO (dmik): Qt/Win32 sends WindowBlocked/WindowUnblocked events only
//  to the direct parent of the modal widget. Why? We use qt_dispatchBlocked()
//  to send them to all windows that do not get input when the given widget
//  is modal; also we disable/enable that windows there, which is essential
//  for modality support in Qt/OS2 (code in QtOldFrameProc() and qt_try_modal()
//  functionality depend on it).
//
//    if (widget->parentWidget()) {
//	QEvent e(QEvent::WindowBlocked);
//	QApplication::sendEvent(widget->parentWidget(), &e);
//        WinEnableWindow( widget->parentWidget()->winFId(), FALSE );
//    }
    qt_dispatchBlocked( widget, TRUE );

    releaseAutoCapture();
    qt_dispatchEnterLeave( 0, QWidget::find((WId)curWin));
    qt_modal_stack->insert( 0, widget );
    app_do_modal = TRUE;
    curWin = 0;
    qt_button_down = 0;
    ignoreNextMouseReleaseEvent = FALSE;
}


Q_EXPORT void qt_leave_modal( QWidget *widget )
{
    if ( qt_modal_stack && qt_modal_stack->removeRef(widget) ) {
	if ( qt_modal_stack->isEmpty() ) {
	    delete qt_modal_stack;
	    qt_modal_stack = 0;
	    QPoint p( QCursor::pos() );
	    app_do_modal = FALSE; // necessary, we may get recursively into qt_try_modal below
	    QWidget* w = QApplication::widgetAt( p.x(), p.y(), TRUE );
	    qt_dispatchEnterLeave( w, QWidget::find( curWin ) ); // send synthetic enter event
	    curWin = w? w->winId() : 0;
	}
	ignoreNextMouseReleaseEvent = TRUE;

        qt_dispatchBlocked( widget, FALSE );
    }
    app_do_modal = qt_modal_stack != 0;

//@@TODO (dmik): see the comments inside qt_enter_modal()
//
//    if (widget->parentWidget()) {
//        WinEnableWindow( widget->parentWidget()->winFId(), TRUE );
//	QEvent e(QEvent::WindowUnblocked);
//	QApplication::sendEvent(widget->parentWidget(), &e);
//    }
}

static bool qt_blocked_modal( QWidget *widget )
{
    if ( !app_do_modal )
	return FALSE;
    if ( qApp->activePopupWidget() )
	return FALSE;
    if ( widget->testWFlags(Qt::WStyle_Tool) )	// allow tool windows
	return FALSE;

    QWidget *modal=0, *top=qt_modal_stack->getFirst();

    widget = widget->topLevelWidget();
    if ( widget->testWFlags(Qt::WShowModal) )	// widget is modal
	modal = widget;
    if ( !top || modal == top )				// don't block event
	return FALSE;
    return TRUE;
}

static bool qt_try_modal( QWidget *widget, QMSG *qmsg, int &ret )
{
    QWidget * top = 0;

    if ( qt_tryModalHelper( widget, &top ) )
	return TRUE;

    bool block_event = FALSE;
    ULONG type  = qmsg->msg;

    if ( type == WM_CLOSE ) {
	block_event = TRUE;
    }

    return !block_event;
}


/*****************************************************************************
  Popup widget mechanism

  openPopup()
	Adds a widget to the list of popup widgets
	Arguments:
	    QWidget *widget	The popup widget to be added

  closePopup()
	Removes a widget from the list of popup widgets
	Arguments:
	    QWidget *widget	The popup widget to be removed
 *****************************************************************************/

void QApplication::openPopup( QWidget *popup )
{
    if ( !popupWidgets ) {			// create list
	popupWidgets = new QWidgetList;
	Q_CHECK_PTR( popupWidgets );
    }
    popupWidgets->append( popup );		// add to end of list
    if ( !popup->isEnabled() )
	return;

    if ( popupWidgets->count() == 1 && !qt_nograb() )
	setAutoCapture( popup->winId() );	// grab mouse/keyboard
    // Popups are not focus-handled by the window system (the first
    // popup grabbed the keyboard), so we have to do that manually: A
    // new popup gets the focus
    QFocusEvent::setReason( QFocusEvent::Popup );
    if ( popup->focusWidget())
	popup->focusWidget()->setFocus();
    else
	popup->setFocus();
    QFocusEvent::resetReason();
}

void QApplication::closePopup( QWidget *popup )
{
    if ( !popupWidgets )
	return;
    popupWidgets->removeRef( popup );
    POINTL curPos;
    WinQueryPointerPos( HWND_DESKTOP, &curPos );
    // flip y coordinate
    curPos.y = desktop()->height() - (curPos.y + 1);
    replayPopupMouseEvent = !popup->geometry().contains( QPoint(curPos.x, curPos.y) );
    if ( popupWidgets->count() == 0 ) {		// this was the last popup
	popupCloseDownMode = TRUE;		// control mouse events
	delete popupWidgets;
	popupWidgets = 0;
	if ( !popup->isEnabled() )
	    return;
	if ( !qt_nograb() )			// grabbing not disabled
	    releaseAutoCapture();
	if ( active_window ) {
	    QFocusEvent::setReason( QFocusEvent::Popup );
	    if ( active_window->focusWidget() )
		active_window->focusWidget()->setFocus();
	    else
		active_window->setFocus();
	    QFocusEvent::resetReason();
	}
    } else {
	// Popups are not focus-handled by the window system (the
	// first popup grabbed the keyboard), so we have to do that
	// manually: A popup was closed, so the previous popup gets
	// the focus.
	QFocusEvent::setReason( QFocusEvent::Popup );
	QWidget* aw = popupWidgets->getLast();
	if ( popupWidgets->count() == 1 )
	    setAutoCapture( aw->winId() );
	if (aw->focusWidget())
	    aw->focusWidget()->setFocus();
	else
	    aw->setFocus();
	QFocusEvent::resetReason();
    }
}



/*****************************************************************************
  Event translation; translates OS/2 PM events to Qt events
 *****************************************************************************/

// State holder for LWIN/RWIN and ALTGr keys
// (ALTGr is also necessary since OS/2 doesn't report ALTGr as KC_ALT)
static int qt_extraKeyState = 0;

static int mouseButtonState()
{
    int state = 0;

    if ( WinGetKeyState( HWND_DESKTOP, VK_BUTTON1 ) & 0x8000 )
        state |= Qt::LeftButton;
    if ( WinGetKeyState( HWND_DESKTOP, VK_BUTTON2 ) & 0x8000 )
        state |= Qt::RightButton;
    if ( WinGetKeyState( HWND_DESKTOP, VK_BUTTON3 ) & 0x8000 )
        state |= Qt::MidButton;

    return state;
}

//
// Auto-capturing for mouse press and mouse release
//

static void setAutoCapture( HWND h )
{
    if ( autoCaptureWnd )
	releaseAutoCapture();
    autoCaptureWnd = h;

    if ( !mouseButtonState() ) {
        // all buttons released, we don't actually capture the mouse
        // (see QWidget::translateMouseEvent())
        autoCaptureReleased = TRUE;
    } else {
        autoCaptureReleased = FALSE;
        WinSetCapture( HWND_DESKTOP, h );
    }
}

static void releaseAutoCapture()
{
    if ( autoCaptureWnd ) {
        if ( !autoCaptureReleased ) {
            WinSetCapture( HWND_DESKTOP, 0 );
            autoCaptureReleased = TRUE;
        }
	autoCaptureWnd = 0;
    }
}


//
// Mouse event translation
//

static ushort mouseTbl[] = {
    WM_MOUSEMOVE,	QEvent::MouseMove,		0,
    WM_BUTTON1DOWN,	QEvent::MouseButtonPress,	Qt::LeftButton,
    WM_BUTTON1UP,	QEvent::MouseButtonRelease,	Qt::LeftButton,
    WM_BUTTON1DBLCLK,	QEvent::MouseButtonDblClick,	Qt::LeftButton,
    WM_BUTTON2DOWN,	QEvent::MouseButtonPress,	Qt::RightButton,
    WM_BUTTON2UP,	QEvent::MouseButtonRelease,	Qt::RightButton,
    WM_BUTTON2DBLCLK,	QEvent::MouseButtonDblClick,	Qt::RightButton,
    WM_BUTTON3DOWN,	QEvent::MouseButtonPress,	Qt::MidButton,
    WM_BUTTON3UP,	QEvent::MouseButtonRelease,	Qt::MidButton,
    WM_BUTTON3DBLCLK,	QEvent::MouseButtonDblClick,	Qt::MidButton,
//@@TODO (dmik): later (extra buttons)
//    WM_XBUTTONDOWN,	QEvent::MouseButtonPress,	Qt::MidButton*2, //### Qt::XButton1/2
//    WM_XBUTTONUP,	QEvent::MouseButtonRelease,	Qt::MidButton*2,
//    WM_XBUTTONDBLCLK,	QEvent::MouseButtonDblClick,	Qt::MidButton*2,
    WM_CONTEXTMENU,     QEvent::ContextMenu,            0,
    0,			0,				0
};

static int translateButtonState( USHORT s, int type, int button )
{
    int bst = mouseButtonState();

    if ( type == QEvent::ContextMenu ) {
        if ( WinGetKeyState( HWND_DESKTOP, VK_SHIFT ) & 0x8000 )
            bst |= Qt::ShiftButton;
        if ( WinGetKeyState( HWND_DESKTOP, VK_ALT ) & 0x8000 )
            bst |= Qt::AltButton;
        if ( WinGetKeyState( HWND_DESKTOP, VK_CTRL ) & 0x8000 )
            bst |= Qt::ControlButton;
    } else {
        if ( s & KC_SHIFT )
            bst |= Qt::ShiftButton;
        if ( (s & KC_ALT) )
            bst |= Qt::AltButton;
        if ( s & KC_CTRL )
            bst |= Qt::ControlButton;
    }
    if ( (qt_extraKeyState & Qt::AltButton) )
        bst |= Qt::AltButton;
    if ( qt_extraKeyState & Qt::MetaButton )
        bst |= Qt::MetaButton;

    // Translate from OS/2-style "state after event"
    // to X-style "state before event"
    if ( type == QEvent::MouseButtonPress ||
	 type == QEvent::MouseButtonDblClick )
	bst &= ~button;
    else if ( type == QEvent::MouseButtonRelease )
	bst |= button;

    return bst;
}

/*! \internal
  In DnD, the mouse release event never appears, so the
  mouse button state machine must be manually reset
*/
void qt_pmMouseButtonUp()
{
    // release any stored mouse capture
    qt_button_down = 0;
    autoCaptureReleased = TRUE;
    releaseAutoCapture();
}

bool QETWidget::translateMouseEvent( const QMSG &qmsg )
{
#if 0
    static const char *msgNames[] = { // 11 items
        "WM_MOUSEMOVE",
        "WM_BUTTON1DOWN", "WM_BUTTON1UP", "WM_BUTTON1DBLCLK",
        "WM_BUTTON2DOWN", "WM_BUTTON2UP", "WM_BUTTON2DBLCLK",
        "WM_BUTTON3DOWN", "WM_BUTTON3UP", "WM_BUTTON3DBLCLK",
        "WM_???"
    };
    int msgIdx = qmsg.msg - WM_MOUSEMOVE;
    if (msgIdx < 0 || msgIdx > 9)
        msgIdx = 10;
    qDebug( "%s (%04lX): [%08lX/%p:%s/%s] %04hd,%04hd hit=%04hX fl=%04hX",
            msgNames[msgIdx], qmsg.msg, qmsg.hwnd, this, name(), className(),
            SHORT1FROMMP(qmsg.mp1), SHORT2FROMMP(qmsg.mp1),
            SHORT1FROMMP(qmsg.mp2), SHORT2FROMMP(qmsg.mp2) );
#endif

    static QPoint pos;                          // window pos (y flipped)
    static POINTL gpos = { -1, -1 };            // global pos (y flipped)
    QEvent::Type type;				// event parameters
    int	   button;
    int	   state;
    int	   i;

    // candidate for a double click event
    static HWND dblClickCandidateWin = 0;

    if ( sm_blockUserInput ) //block user interaction during session management
	return TRUE;

    // Compress mouse move events
    if ( qmsg.msg == WM_MOUSEMOVE ) {
	QMSG mouseMsg;
        mouseMsg.msg = WM_NULL;
	while (
            WinPeekMsg( 0, &mouseMsg, qmsg.hwnd, WM_MOUSEMOVE,
                WM_MOUSEMOVE, PM_NOREMOVE )
        ) {
            if ( mouseMsg.mp2 != qmsg.mp2 )
                break; // leave the message in the queue because
                       // the key state has changed
            // Remove the mouse move message
            WinPeekMsg( 0, &mouseMsg, qmsg.hwnd, WM_MOUSEMOVE,
                WM_MOUSEMOVE, PM_REMOVE );
	}
        // Update the passed in QMSG structure with the
        // most recent one.
        if ( mouseMsg.msg != WM_NULL ) {
            PQMSG pqmsg = (PQMSG)&qmsg;
            pqmsg->mp1 = mouseMsg.mp1;
            pqmsg->mp2 = mouseMsg.mp2;
            pqmsg->time = mouseMsg.time;
            pqmsg->ptl.x = (short)SHORT1FROMMP(mouseMsg.mp1);
            pqmsg->ptl.y = (short)SHORT2FROMMP(mouseMsg.mp1);
            WinMapWindowPoints( pqmsg->hwnd, HWND_DESKTOP, &pqmsg->ptl, 1 );
            // flip y coordinate
            pqmsg->ptl.y = QApplication::desktop()->height() - (pqmsg->ptl.y + 1);
        }
    }

    for ( i = 0; mouseTbl[i] && (ULONG)mouseTbl[i] != qmsg.msg; i += 3 )
        ;
    if ( !mouseTbl[i] )
	return FALSE;

    type   = (QEvent::Type)mouseTbl[++i];	// event type
    button = mouseTbl[++i];			// which button
    state  = translateButtonState( SHORT2FROMMP(qmsg.mp2), type, button ); // button state

    // It seems, that PM remembers only the WM_BUTTONxDOWN message (instead of
    // the WM_BUTTONxDOWN + WM_BUTTONxUP pair) to detect whether the next button
    // press should be converted to WM_BUTTONxDBLCLK or not. As a result, the
    // window gets WM_BUTTONxDBLCLK even if it didn't receive the preceeding
    // WM_BUTTONxUP (this happens if we issue WinSetCapture() on the first
    // WM_BUTTONxDOWN), which is obviously wrong and makes problems for QWorkspace
    // and QTitleBar system menu handlers that don't expect a double click after
    // they opened a popup menu. dblClickCandidateWin is reset to 0 (see a ***
    // remmark below) when WinSetCapture is issued that directs messages
    // to a window other than one received the first WM_BUTTONxDOWN,
    // so we can fix it here.  Note that if there is more than one popup window,
    // WinSetCapture is issued only for the first of them, so this code doesn't
    // prevent MouseButtonDblClick from being delivered to a popup when another
    // popup gets closed on the first WM_BUTTONxDOWN (Qt/Win32 behaves in the
    // same way, so it's left for compatibility).
    if ( type == QEvent::MouseButtonPress ) {
        dblClickCandidateWin = qmsg.hwnd;
    } else if ( type == QEvent::MouseButtonDblClick ) {
        if ( dblClickCandidateWin != qmsg.hwnd )
            type = QEvent::MouseButtonPress;
        dblClickCandidateWin = 0;
    }

    if ( type == QEvent::ContextMenu ) {
	QPoint g = QPoint( qmsg.ptl.x, qmsg.ptl.y );
        QContextMenuEvent e( QContextMenuEvent::Mouse, mapFromGlobal( g ), g, state );
        QApplication::sendSpontaneousEvent( this, &e );
        return TRUE;
    }

    if ( type == QEvent::MouseMove ) {
	if ( !(state & MouseButtonMask) )
	    qt_button_down = 0;
#ifndef QT_NO_CURSOR
	QCursor *c = qt_grab_cursor();
	if ( !c )
	    c = QApplication::overrideCursor();
	if ( c )				// application cursor defined
	    WinSetPointer( HWND_DESKTOP, c->handle() );
	else if ( isEnabled() )		// use widget cursor if widget is enabled
	    WinSetPointer( HWND_DESKTOP, cursor().handle() );
	else {
	    QWidget *parent = parentWidget();
	    while ( parent && !parent->isEnabled() )
		parent = parent->parentWidget();
	    if ( parent )
		WinSetPointer( HWND_DESKTOP, parent->cursor().handle() );
	}
#else
        // pass the msg to the default proc to let it change the pointer shape
        WinDefWindowProc( qmsg.hwnd, qmsg.msg, qmsg.mp1, qmsg.mp2 );
#endif
	if ( curWin != winId() ) {		// new current window
/// @todo (dmik)
//  add CS_HITTEST to our window classes and handle WM_HITTEST,
//  otherwise disabled windows will not get mouse events?
	    qt_dispatchEnterLeave( this, QWidget::find(curWin) );
	    curWin = winId();
	}

        // *** PM posts a dummy WM_MOUSEMOVE message (with the same, uncahnged
        // pointer coordinates) after every WinSetCapture that actually changes
        // the capture target. I.e., if the argument of WinSetCapture is
        // NULLHANDLE, a window under the mouse pointer gets this message,
        // otherwise the specified window gets it unless it is already under the
        // pointer. We use this info to check whether the window can be a double
        // click candidate (see above).
        if ( qmsg.ptl.x == gpos.x && qmsg.ptl.y == gpos.y ) {
            if ( dblClickCandidateWin != qmsg.hwnd )
                dblClickCandidateWin = 0;
            return TRUE;
        }

	gpos = qmsg.ptl;

	if ( state == 0 && autoCaptureWnd == 0 && !hasMouseTracking() &&
	     !QApplication::hasGlobalMouseTracking() )
	    return TRUE;			// no button

	pos = mapFromGlobal( QPoint(gpos.x, gpos.y) );
    } else {
        if ( type == QEvent::MouseButtonPress && !isActiveWindow() )
            setActiveWindow();

	gpos = qmsg.ptl;
	pos = mapFromGlobal( QPoint(gpos.x, gpos.y) );

	if ( type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick ) {	// mouse button pressed
	    // Magic for masked widgets
	    qt_button_down = findChildWidget( this, pos );
	    if ( !qt_button_down || !qt_button_down->testWFlags(WMouseNoMask) )
		qt_button_down = this;
	}
    }

    // detect special button states
    enum { Other, SinglePressed, AllReleased } btnState = Other;
    int bs = state & MouseButtonMask;
    if ( (type == QEvent::MouseButtonPress ||
          type == QEvent::MouseButtonDblClick) && bs == 0
    ) {
        btnState = SinglePressed;
    } else if ( type == QEvent::MouseButtonRelease && bs == button ) {
        btnState = AllReleased;
    }

    if ( qApp->inPopupMode() ) {			// in popup mode
        if ( !autoCaptureReleased && btnState == AllReleased ) {
            // in order to give non-Qt windows the opportunity to see mouse
            // messages while our popups are active we need to release the
            // mouse capture which is absolute in OS/2. we do it directly
            // (not through releaseAutoCapture()) in order to keep
            // autoCaptureWnd nonzero to keep forwarding mouse move events
            // (actually sent to one of Qt widgets) to the active popup.
            autoCaptureReleased = TRUE;
            WinSetCapture( HWND_DESKTOP, 0 );
        } else if ( autoCaptureReleased && btnState == SinglePressed ) {
            // set the mouse capture back if a button is pressed.
            if ( autoCaptureWnd ) {
                autoCaptureReleased = FALSE;
                WinSetCapture( HWND_DESKTOP, autoCaptureWnd );
            }
        }

	replayPopupMouseEvent = FALSE;
	QWidget* activePopupWidget = qApp->activePopupWidget();
	QWidget *popup = activePopupWidget;

	if ( popup != this ) {
	    if ( testWFlags(WType_Popup) && rect().contains(pos) )
		popup = this;
	    else				// send to last popup
		pos = popup->mapFromGlobal( QPoint(gpos.x, gpos.y) );
	}
	QWidget *popupChild = findChildWidget( popup, pos );
	bool releaseAfter = FALSE;
	switch ( type ) {
	    case QEvent::MouseButtonPress:
	    case QEvent::MouseButtonDblClick:
		popupButtonFocus = popupChild;
		break;
	    case QEvent::MouseButtonRelease:
		releaseAfter = TRUE;
		break;
	    default:
		break;				// nothing for mouse move
	}

	if ( popupButtonFocus ) {
	    QMouseEvent e( type,
		popupButtonFocus->mapFromGlobal(QPoint(gpos.x,gpos.y)),
		QPoint(gpos.x,gpos.y), button, state );
	    QApplication::sendSpontaneousEvent( popupButtonFocus, &e );
	    if ( releaseAfter ) {
		popupButtonFocus = 0;
	    }
	} else if ( popupChild ) {
	    QMouseEvent e( type,
		popupChild->mapFromGlobal(QPoint(gpos.x,gpos.y)),
		QPoint(gpos.x,gpos.y), button, state );
	    QApplication::sendSpontaneousEvent( popupChild, &e );
	} else {
	    QMouseEvent e( type, pos, QPoint(gpos.x,gpos.y), button, state );
	    QApplication::sendSpontaneousEvent( popup, &e );
	}

	if ( releaseAfter )
	    qt_button_down = 0;

	if ( type == QEvent::MouseButtonPress
	     && qApp->activePopupWidget() != activePopupWidget
	     && replayPopupMouseEvent ) {
	    // the popup dissappeared. Replay the event
	    QWidget* w = QApplication::widgetAt( gpos.x, gpos.y, TRUE );
	    if ( w && !qt_blocked_modal( w ) ) {
                QPoint wpos = w->mapFromGlobal(QPoint(gpos.x, gpos.y));
                // flip y coordinate
                wpos.ry() = w->height() - (wpos.y() + 1);
                // Note: it's important to post this message rather than send:
                // QPushButton::popupPressed() depends on this so that
                // popup->exec() must return before this message is delivered
                // as QWidget::keyPressEvent().
                WinPostMsg( w->winId(), qmsg.msg,
                            MPFROM2SHORT( wpos.x(), wpos.y() ), qmsg.mp2 );
	    }
	}
    } else {					// not popup mode
	if ( btnState == SinglePressed && QWidget::mouseGrabber() == 0 )
            setAutoCapture( winId() );
	else if ( btnState == AllReleased && QWidget::mouseGrabber() == 0 )
            releaseAutoCapture();

	QWidget *widget = this;
	QWidget *w = QWidget::mouseGrabber();
	if ( !w )
	    w = qt_button_down;
	if ( w && w != this ) {
	    widget = w;
	    pos = w->mapFromGlobal(QPoint(gpos.x, gpos.y));
	}

	if ( type == QEvent::MouseButtonRelease &&
	     (state & (~button) & ( MouseButtonMask )) == 0 ) {
	    qt_button_down = 0;
	}

	QMouseEvent e( type, pos, QPoint(gpos.x,gpos.y), button, state );
	QApplication::sendSpontaneousEvent( widget, &e );

	if ( type != QEvent::MouseMove )
	    pos.rx() = pos.ry() = -9999;	// init for move compression
    }
    return TRUE;
}


//
// Keyboard event translation
//

static const ushort KeyTbl[] = {		// keyboard mapping table
    VK_ESC,		Qt::Key_Escape,		// misc keys
    VK_TAB,		Qt::Key_Tab,
    VK_BACKTAB,		Qt::Key_Backtab,
    VK_BACKSPACE,	Qt::Key_Backspace,
    VK_ENTER,		Qt::Key_Enter,
    VK_NEWLINE,		Qt::Key_Return,
    VK_INSERT,		Qt::Key_Insert,
    VK_DELETE,		Qt::Key_Delete,
    VK_CLEAR,		Qt::Key_Clear,
    VK_PAUSE,		Qt::Key_Pause,
    VK_PRINTSCRN,	Qt::Key_Print,
    VK_SPACE,           Qt::Key_Space,
    VK_HOME,		Qt::Key_Home,		// cursor movement
    VK_END,		Qt::Key_End,
    VK_LEFT,		Qt::Key_Left,
    VK_UP,		Qt::Key_Up,
    VK_RIGHT,		Qt::Key_Right,
    VK_DOWN,		Qt::Key_Down,
    VK_PAGEUP,		Qt::Key_Prior,
    VK_PAGEDOWN,	Qt::Key_Next,
    VK_SHIFT,		Qt::Key_Shift,		// modifiers
    VK_CTRL,		Qt::Key_Control,
    VK_ALT,		Qt::Key_Alt,
    VK_CAPSLOCK,	Qt::Key_CapsLock,
    VK_NUMLOCK,		Qt::Key_NumLock,
    VK_SCRLLOCK,	Qt::Key_ScrollLock,
    0,			0
};

// when the compatibility mode is FALSE Qt/OS2 uses the following rule
// to calculate QKeyEvent::key() codes when an alpha-numeric key is
// pressed: key code is the ASCII (Latin 1) character code of that key as if
// there were no any keyboard modifiers (CTRL, SHIFT, ALT) pressed, with the
// exception that alpha characters are uppercased.
// when the compatibility mode is TRUE Qt/OS2 behaves mostly like Qt/Win32.
Q_EXPORT bool qt_kbd_compatibility = TRUE;

/// @todo (dmik) currentlly, qt_kbd_compatibility is TRUE because
//  qt_scan2Ascii function below is not well implemented yet (in particular,
//  it uses the 850 code page that may not be available on some systems...).
//  Once we find a way to translate scans to US ASCII regardless of the current
//  code page and/or NLS state (keyboard layout), qt_kbd_compatibility may be
//  set to FALSE. This, in particular, will enable more correct handling of
//  Alt+letter combinations when the keyboard is in the NLS state:
//  QKeyEvent::key() will return a non-null Qt::Key_XXX code corresponding to
//  the ASCII code of a pressed key, which in turn will let Qt process latin
//  Alt+letter shortcuts in the NLS keyboard mode together with Alt+NLS_letter
//  shortcuts  (nice feature imho). Note that Alt+NLS_letter shortcuts are
//  correctly processed in any case.

// cache table to store Qt::Key_... values for 256 hardware scancodes
// (even byte is for non-shifted keys, odd byte is for shifted ones).
// used to decrease the number of calls to KbdXlate.
static uchar ScanTbl[512] = { 0xFF };

static uchar qt_scan2Ascii( uchar scan, bool shift )
{
    uchar ascii = 0;
    HFILE kbd;
    ULONG dummy;
    DosOpen( "KBD$", &kbd, &dummy, 0,
        FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE, NULL );
    // parameter packet
    struct CPID {
      USHORT idCodePage;
      USHORT reserved;
    } cpid = { 850, 0 };
    ULONG szCpid = sizeof(CPID);
    // data packet
    KBDTRANS kt;
    ULONG szKt = sizeof(KBDTRANS);
    // reset all kbd states and modifiers
    memset( &kt, 0, szKt );
    // reflect Shift state to distinguish between [ and { etc.
    if ( qt_kbd_compatibility && shift )
        kt.fsState = KBDSTF_RIGHTSHIFT;
    kt.chScan = scan;
    DosDevIOCtl( kbd, IOCTL_KEYBOARD, KBD_XLATESCAN,
        &cpid, szCpid, &szCpid, &kt, szKt, &szKt );
    // store in cache
    uint idx = scan << 1;
    if ( qt_kbd_compatibility && shift )
        idx++;
    ascii = ScanTbl[idx] = toupper( kt.chChar );
    DosClose( kbd );
    return ascii;
}

// translates WM_CHAR to Qt::Key_..., ascii, state and text
static void translateKeyCode( CHRMSG &chm, int &code, int &ascii, int &state,
                              QString &text )
{
    if ( chm.fs & KC_SHIFT )
	state |= Qt::ShiftButton;
    if ( chm.fs & KC_CTRL )
	state |= Qt::ControlButton;
    if ( chm.fs & KC_ALT )
	state |= Qt::AltButton;
    if ( qt_extraKeyState & Qt::MetaButton )
        state |= Qt::MetaButton;

    unsigned char ch = chm.chr;

    if ( chm.fs & KC_VIRTUALKEY ) {
        if ( !chm.vkey ) {
            // The only known situation when KC_VIRTUALKEY is present but
            // vkey is zero is when Alt+Shift is pressed to switch the
            // keyboard layout state from latin to national and back.
            // It seems that this way the system informs applications about
            // layout changes: chm.chr is 0xF1 when the user switches
            // to the national layout (i.e. presses Alt + Left Shift)
            // and it is 0xF0 when he switches back (presses Alt + Right Shift).
            // We assume this and restore fs, vkey, scancode and chr accordingly.
            if ( chm.chr == 0xF0 || chm.chr == 0xF1 ) {
                chm.fs |= KC_ALT | KC_SHIFT;
                chm.vkey = VK_SHIFT;
                chm.scancode = chm.chr == 0xF1 ? 0x2A : 0x36;
                chm.chr = ch = 0;
                state |= Qt::AltButton | Qt::ShiftButton;
                // code will be assigned by the normal procedure below
            }
        }
        if ( chm.vkey >= VK_F1 && chm.vkey <= VK_F24 ) {
            // function keys
            code = Qt::Key_F1 + (chm.vkey - VK_F1);
        } else if ( chm.vkey == VK_ALTGRAF ) {
            code = Qt::Key_Alt;
            if ( !(chm.fs & KC_KEYUP) )
                qt_extraKeyState |= Qt::AltButton;
            else
                qt_extraKeyState &= ~Qt::AltButton;
        } else {
            // any other keys
            int i = 0;
            while ( KeyTbl[i] ) {
                if ( chm.vkey == (int)KeyTbl[i] ) {
                    code = KeyTbl[i+1];
                    break;
                }
                i += 2;
            }
        }
    } else {
        if ( qt_kbd_compatibility && ch && ch < 0x80 ) {
            code = toupper( ch );
        } else if ( !qt_kbd_compatibility && (chm.fs & KC_CHAR) && isalpha( ch ) ) {
            code = toupper( ch );
        } else {
            // detect some special keys that have a pseudo char code
            // in the high byte of chm.chr (probably this is less
            // device-dependent than scancode)
            switch ( chm.chr ) {
                case 0xEC00: // LWIN
                case 0xED00: // RWIN
                    code = Qt::Key_Meta;
                    if ( !(chm.fs & KC_KEYUP) )
                        qt_extraKeyState |= Qt::MetaButton;
                    else
                        qt_extraKeyState &= ~Qt::MetaButton;
                    break;
                case 0xEE00: // WINAPP (menu with arrow)
                    code = Qt::Key_Menu;
                    break;
                case 0x5600: // additional '\' (0x56 is actually its scancode)
                    ch = state & Qt::ShiftButton ? '|' : '\\';
                    if ( qt_kbd_compatibility ) code = ch;
                    else code = '\\';
                    break;
                default:
                    // break if qt_kbd_compatibility = TRUE to avoid using
                    // qt_scan2Ascii(), see comments to qt_kbd_compatibility
                    if ( qt_kbd_compatibility ) break;
                    // deduce Qt::Key... from scancode
                    if ( ScanTbl[0] == 0xFF )
                        memset( &ScanTbl, 0, sizeof(ScanTbl) );
                    uint idx = chm.scancode << 1;
                    if ( qt_kbd_compatibility && (state & Qt::ShiftButton) )
                        idx++;
                    code = ScanTbl[idx];
                    if ( !code )
                        // not found in cache
                        code = qt_scan2Ascii( chm.scancode, (state & Qt::ShiftButton) );
                    break;
            }
        }
    }
    // check extraState AFTER updating it
    if ( qt_extraKeyState & Qt::AltButton )
        state |= Qt::AltButton;

    // detect numeric keypad keys
    if ( chm.vkey == VK_ENTER || chm.vkey == VK_NUMLOCK ) {
        // these always come from the numpad
        state |= Qt::Keypad;
    } else if (
        ((chm.vkey >= VK_PAGEUP && chm.vkey <= VK_DOWN) ||
            chm.vkey == VK_INSERT || chm.vkey == VK_DELETE)
    ) {
        if ( ch != 0xE0 ) {
            state |= Qt::Keypad;
            if ( (state & Qt::AltButton) && chm.vkey != VK_DELETE ) {
                // reset both code and ch to "hide" them from Qt widgets and let
                // the standard OS/2 Alt+ddd shortcut (that composed chars from
                // typed in ASCII codes) work correctly. If we don't do that,
                // widgets will see both individual digits (or cursor movements
                // if NumLock is off) and the char composed by Alt+ddd.
                code = ch = 0;
            } else {
                if ( ch ) {
                    // override code to make it Qt::Key_0..Qt::Key_9 etc.
                    code = toupper( ch );
                }
            }
        } else {
            ch = 0;
        }
    }
    // detect other numpad keys. OS/2 doesn't assign virtual keys to them
    // so use scancodes (it can be device-dependent, is there a better way?)
    switch ( chm.scancode ) {
        case 0x4C: // 5
            state |= Qt::Keypad;
            if ( state & Qt::AltButton ) {
                // reset both code and ch to "hide" them from Qt (see above)
                code = ch = 0;
            } else {
                // scancode is zero if Numlock is set
                if ( !code ) code = Qt::Key_Clear;
            }
            break;
        case 0x37: // *
            // OS/2 assigns VK_PRINTSCRN to it when pressed with Shift, also
            // it sets chr to zero when it is released with Alt or Ctrl
            // leaving vkey as zero too, and does few other strange things --
            // override them all
            code = Qt::Key_Asterisk;
            state |= Qt::Keypad;
            break;
        case 0x5C: // /
            code = Qt::Key_Slash;
            // fall through
        case 0x4A: // -
        case 0x4E: // +
            // the code for the above two is obtained by KbdXlate above
            state |= Qt::Keypad;
            break;
    }

    if ( (state & Qt::ControlButton) && !(state & Qt::Keypad) ) {
        if ( !(state & Qt::AltButton) ) {
            unsigned char cch = toupper( ch ), newCh = 0;
            // Ctrl + A..Z etc. produce ascii from 0x01 to 0x1F
            if ( cch >= 0x41 && cch <= 0x5F ) newCh = cch - 0x40;
            // the below emulates OS/2 functionality. It differs from
            // Win32 one.
            else if ( cch == 0x36 && !(state & Qt::Keypad) ) newCh = 0x1E;
            else if ( cch == 0x2D ) newCh = 0x1F;
            else if ( cch >= 0x7B && cch <= 0x7D ) newCh = cch - 0x60;
            if ( newCh )
                ch = newCh;
        }
    }

    ascii = ch;
    if ( ascii > 0x7F ) ascii = 0;
    if ( ch ) {
        // Note: Ignore the KC_CHAR flag when generating text for a key to get
        // correct (non-null) text even for Alt+Letter combinations (that
        // don't have KC_CHAR set) because processing Alt+Letter shortcuts
        // for non-ASCII letters in widgets (e.g. QPushButton) depends on that.
        if ( (chm.fs & (KC_VIRTUALKEY | KC_CHAR)) == KC_CHAR && (chm.chr & 0xFF00) ) {
            // We assime we get a DBCS char if the above condition is met.
            // DBCS chars seem to have KC_CHAR set but not KC_VIRTUALKEY; we
            // use this to prevent keys like ESC (chm=0x011B) with the non-zero
            // high byte to be mistakenly recognized as DBCS.
            text = QString::fromLocal8Bit( (char *) &chm.chr, 2 );
        }
        else
            text = QString::fromLocal8Bit( (char*) &ch, 1 );
    }
}

struct KeyRec {
    KeyRec(unsigned char s, int c, int a, const QString& t) :
        scan(s), code(c), ascii(a), text(t) { }
    KeyRec() { }
    unsigned char scan;
    int code, ascii;
    QString text;
};

static const int maxrecs=64; // User has LOTS of fingers...
static KeyRec key_rec[maxrecs];
static int nrecs=0;

static KeyRec* find_key_rec( unsigned char scan, bool remove )
{
    KeyRec *result = 0;
    if( scan == 0 ) // DBCS chars or user-injected keys
        return result;

    for (int i=0; i<nrecs; i++) {
	if (key_rec[i].scan == scan) {
	    if (remove) {
		static KeyRec tmp;
		tmp = key_rec[i];
		while (i+1 < nrecs) {
		    key_rec[i] = key_rec[i+1];
		    i++;
		}
		nrecs--;
		result = &tmp;
	    } else {
		result = &key_rec[i];
	    }
	    break;
	}
    }
    return result;
}

static KeyRec* find_key_rec( int code, bool remove )
{
    KeyRec *result = 0;
    if( code == 0 ) // DBCS chars or user-injected keys
        return result;

    for (int i=0; i<nrecs; i++) {
	if (key_rec[i].code == code) {
	    if (remove) {
		static KeyRec tmp;
		tmp = key_rec[i];
		while (i+1 < nrecs) {
		    key_rec[i] = key_rec[i+1];
		    i++;
		}
		nrecs--;
		result = &tmp;
	    } else {
		result = &key_rec[i];
	    }
	    break;
	}
    }
    return result;
}

static void store_key_rec( unsigned char scan, int code, int ascii,
                           const QString& text )
{
    if( scan == 0 && code == 0 ) // DBCS chars or user-injected keys
        return;

    if ( nrecs == maxrecs ) {
#if defined(QT_CHECK_RANGE)
        qWarning( "Qt: Internal keyboard buffer overflow" );
#endif
        return;
    }

    key_rec[nrecs++] = KeyRec( scan, code, ascii, text );
}

bool QETWidget::translateKeyEvent( const QMSG &qmsg, bool grab )
{
    CHRMSG chm = *((PCHRMSG)(((char*)&qmsg) + sizeof(HWND) + sizeof(ULONG)));

#if 0
    qDebug( "WM_CHAR:  [%s]  fs: %04X  cRepeat: %03d  scancode: %02X  chr: %04X  vkey: %04X %s",
            name(), chm.fs, chm.cRepeat, chm.scancode, chm.chr, chm.vkey, (grab ? "{grab}" : "") );
#endif

    bool k0 = FALSE, k1 = FALSE;
    int code = 0, ascii = 0, state = 0;
    QString text;

    if ( sm_blockUserInput ) // block user interaction during session management
        return TRUE;

    translateKeyCode( chm, code, ascii, state, text );

    // Note: code and/or chm.scancode may be zero here. We cannot ignore such
    // events because, for example, all non-ASCII letters have zero virtual
    // codes, and DBCS characters entered via IME have both zero virtual codes
    // and zero scancodes. However, if both code and chm.scancode are zero
    // (as for DBCS), store_key_rec()/find_key_rec() will do nothing which
    // means that:
    //   1) QKeyEvents will not have the auto-repeat flag set when a key is
    //      being auto-repeated by the system;
    //   2) there will be no QEvent::KeyRelease event corresponding to the
    //      QEvent::KeyPress event.

    // Invert state logic
    if ( code == Key_Alt )
        state = state ^ AltButton;
    else if ( code == Key_Control )
        state = state ^ ControlButton;
    else if ( code == Key_Shift )
        state = state ^ ShiftButton;

    if ( !(chm.fs & KC_KEYUP) ) {
        // KEYDOWN
        KeyRec* rec = find_key_rec( chm.scancode, FALSE );

        if ( state == Qt::AltButton ) {
            // Special handling of global PM hotkeys
            switch ( code ) {
                case Qt::Key_Space:
                    if ( qt_show_system_menu( topLevelWidget() ) ) {
                        // remove the Key_Alt from the buffer (otherwise we will
                        // not get the next "Alt pressed" event because the
                        // "Alt depressed" event, that must preceed it, will be
                        // eaten by the system)
                        find_key_rec( Qt::Key_Alt, TRUE );
/// @todo (dmik) do the same for other global keys (ALT+TAB, ALT+ESC, CTRL+ESC)
//  by handling this situation when we obtain/loose focus)
/// @todo (dmik) update: I don't actually think the above should be done, because
//  it will not solve the problem of stuck modifier keys in general (there may be
//  other combinations stolen by the system or other apps). More over, I guess
//  that find_key_rec() above should also be removed to get identical behavior for
//  all stolen keys. This will allow to solve the problem on the Qt application
//  level if needed (and even in a platform-independent manner).
                    }
                    return TRUE;
                case Qt::Key_F4:
                    // we handle this key combination ourselves because not
                    // all top-level widgets have the system menu
                    WinPostMsg( topLevelWidget()->winFId(), WM_CLOSE, 0, 0 );
                    // see the comment above
                    find_key_rec( Qt::Key_Alt, TRUE );
                    return TRUE;
                default:
                    break;
            }
        }

        if ( rec ) {
            // it is already down (so it is auto-repeating)
            if ( rec->code < Key_Shift || rec->code > Key_ScrollLock ) {
                k0 = sendKeyEvent( QEvent::KeyRelease, rec->code, rec->ascii,
                                   state, grab, rec->text, TRUE );
                k1 = sendKeyEvent( QEvent::KeyPress, rec->code, rec->ascii,
                                   state, grab, rec->text, TRUE );
            }
        } else {
            // map shift+tab to shift+backtab, QAccel knows about it
            // and will handle it
            if ( code == Key_Tab && ( state & ShiftButton ) == ShiftButton )
                code = Key_BackTab;
            store_key_rec( chm.scancode, code, ascii, text );
            k0 = sendKeyEvent( QEvent::KeyPress, code, ascii,
                               state, grab, text );
        }
    } else {
        // KEYUP
        KeyRec* rec = find_key_rec( chm.scancode, TRUE );
        if ( !rec ) {
            // Someone ate the key down event
        } else {
            k0 = sendKeyEvent( QEvent::KeyRelease, rec->code, rec->ascii,
                                state, grab, rec->text );

            // keyboard context menu event
            if ( rec->code == Key_Menu && !state )
                WinPostMsg( qmsg.hwnd, WM_CONTEXTMENU, 0, MPFROM2SHORT( 0, 1 ) );
        }
    }

#if 0
    qDebug("WM_CHAR: RESULT = %d", (k0 || k1));
#endif
    return k0 || k1;
}

#ifndef QT_NO_WHEELEVENT
bool QETWidget::translateWheelEvent( const QMSG &qmsg )
{
    enum { WHEEL_DELTA = 120 };

    if ( sm_blockUserInput ) // block user interaction during session management
	return TRUE;

    // consume duplicate wheel events sent by the AMouse driver to emulate
    // multiline scrolls. we need this since currently Qt (QScrollBar, for
    // instance) maintains the number of lines to scroll per wheel rotation
    // (including the special handling of CTRL and SHIFT modifiers) on its own
    // and doesn't have a setting to tell it to be aware of system settings
    // for the mouse wheel. if we had processed events as they are, we would
    // get a confusing behavior (too many lines scrolled etc.).
    {
        int devh = QApplication::desktop()->height();
        QMSG wheelMsg;
        while (
            WinPeekMsg( 0, &wheelMsg, qmsg.hwnd, qmsg.msg, qmsg.msg, PM_NOREMOVE )
        ) {
            // PM bug: ptl contains SHORT coordinates although fields are LONG
            wheelMsg.ptl.x = (short) wheelMsg.ptl.x;
            wheelMsg.ptl.y = (short) wheelMsg.ptl.y;
            // flip y coordinate
            wheelMsg.ptl.y = devh - (wheelMsg.ptl.y + 1);
            if (
                wheelMsg.mp1 != qmsg.mp1 ||
                wheelMsg.mp2 != qmsg.mp2 ||
                wheelMsg.ptl.x != qmsg.ptl.x ||
                wheelMsg.ptl.y != qmsg.ptl.y
            )
                break;
            WinPeekMsg( 0, &wheelMsg, qmsg.hwnd, qmsg.msg, qmsg.msg, PM_REMOVE );
        }
    }

    int delta;
    USHORT cmd = SHORT2FROMMP(qmsg.mp2);
    switch ( cmd ) {
        case SB_LINEUP:
        case SB_PAGEUP:
            delta = WHEEL_DELTA;
            break;
        case SB_LINEDOWN:
        case SB_PAGEDOWN:
            delta = -WHEEL_DELTA;
            break;
        default:
            return FALSE;
    }

    int  state = 0;
    if ( WinGetKeyState( HWND_DESKTOP, VK_SHIFT ) & 0x8000 )
	state |= ShiftButton;
    if ( WinGetKeyState( HWND_DESKTOP, VK_ALT ) & 0x8000 ||
        (qt_extraKeyState & Qt::AltButton)
    )
	state |= AltButton;
    if ( WinGetKeyState( HWND_DESKTOP, VK_CTRL ) & 0x8000 )
	state |= ControlButton;
    if ( qt_extraKeyState & Qt::MetaButton )
 	state |= MetaButton;

    Orientation orient;
    // Alt inverts scroll orientation (Qt/Win32 behavior)
    if ( state & AltButton )
        orient = qmsg.msg == WM_VSCROLL ? Horizontal : Vertical;
    else
        orient = qmsg.msg == WM_VSCROLL ? Vertical : Horizontal;

    QPoint globalPos (qmsg.ptl.x, qmsg.ptl.y);

    // if there is a widget under the mouse and it is not shadowed
    // by modality, we send the event to it first
    int ret = 0;
    QWidget* w = QApplication::widgetAt( globalPos, TRUE );
    if ( !w || !qt_try_modal( w, (QMSG*)&qmsg, ret ) )
	w = this;

    // send the event to the widget or its ancestors
    {
	QWidget* popup = qApp->activePopupWidget();
	if ( popup && w->topLevelWidget() != popup )
	    popup->close();
	QWheelEvent e( w->mapFromGlobal( globalPos ), globalPos, delta, state, orient );
	if ( QApplication::sendSpontaneousEvent( w, &e ) )
	    return TRUE;
    }

    // send the event to the widget that has the focus or its ancestors, if different
    if ( w != qApp->focusWidget() && ( w = qApp->focusWidget() ) ) {
	QWidget* popup = qApp->activePopupWidget();
	if ( popup && w->topLevelWidget() != popup )
	    popup->close();
	QWheelEvent e( w->mapFromGlobal( globalPos ), globalPos, delta, state, orient );
	if ( QApplication::sendSpontaneousEvent( w, &e ) )
	    return TRUE;
    }
    return FALSE;
}
#endif

static bool isModifierKey(int code)
{
    return code >= Qt::Key_Shift && code <= Qt::Key_ScrollLock;
}

bool QETWidget::sendKeyEvent( QEvent::Type type, int code, int ascii,
			      int state, bool grab, const QString& text,
			      bool autor )
{
    if ( type == QEvent::KeyPress && !grab ) {
	// send accel events if the keyboard is not grabbed
	QKeyEvent a( type, code, ascii, state, text, autor, QMAX(1, int(text.length())) );
	if ( qt_tryAccelEvent( this, &a ) )
	    return TRUE;
    }
    if ( !isEnabled() )
	return FALSE;
    QKeyEvent e( type, code, ascii, state, text, autor, QMAX(1, int(text.length())) );
    QApplication::sendSpontaneousEvent( this, &e );
    if ( !isModifierKey(code) && state == Qt::AltButton
	 && ((code>=Key_A && code<=Key_Z) || (code>=Key_0 && code<=Key_9))
	 && type == QEvent::KeyPress && !e.isAccepted() )
	QApplication::beep();  // emulate PM behavior
    return e.isAccepted();
}


//
// Paint event translation
//
bool QETWidget::translatePaintEvent( const QMSG & )
{
    HPS displayPS = qt_display_ps();

#if !defined (QT_PM_NO_WIDGETMASK)
    // Since we don't use WS_CLIPSIBLINGS and WS_CLIPCHILDREN bits (see
    // qwidget_pm.cpp), we have to validate areas that intersect with our
    // children and siblings, taking their clip regions into account.
    validateObstacles();
#endif

    HRGN hrgn = GpiCreateRegion( displayPS, 0, NULL );
    LONG rc = WinQueryUpdateRegion( winId(), hrgn );
    if ( rc == RGN_ERROR ) {
        GpiDestroyRegion( displayPS, hrgn );
	hps = 0;
	clearWState( WState_InPaintEvent );
	return FALSE;
    }

    setWState( WState_InPaintEvent );
    RECTL rcl;
    hps = WinBeginPaint( winId(), 0, &rcl );

    // convert to width and height
    rcl.xRight -= rcl.xLeft;
    rcl.yTop -= rcl.yBottom;

    // it's possible that the update rectangle is empty
    if ( !rcl.xRight || !rcl.yTop ) {
        WinEndPaint( hps );
        GpiDestroyRegion( displayPS, hrgn );
        hps = 0;
        clearWState( WState_InPaintEvent );
        return TRUE;
    }

#if !defined (QT_PM_NO_WIDGETMASK)
    if ( WinQueryClipRegion( winId(), 0 ) != QCRGN_NO_CLIP_REGION ) {
        // Correct the update region by intersecting it with the clip
        // region (PM doesn't do that itself). It is necessary
        // to have a correct QRegion in QPaintEvent.
        HRGN hcrgn = GpiCreateRegion( displayPS, 0, NULL );
        WinQueryClipRegion( winId(), hcrgn );
        GpiCombineRegion( displayPS, hrgn, hrgn, hcrgn, CRGN_AND );
        GpiDestroyRegion( displayPS, hcrgn );
    }
#endif

    // flip y coordinate
    rcl.yBottom = height() - (rcl.yBottom + rcl.yTop);

    // erase background
#if !defined (DEBUG_REPAINTRESIZE)
    bool erase = !( testWFlags( WRepaintNoErase ) );
#else
    // Some oldish Qt widgets think that if they specify WRepaintNoErase but
    // not WResizeNoErase, the background should still be erased for them
    // in *repaint* events. The code below is left to debug these widgets
    // (to ensure this is the exact cause of repaint problems)
    bool erase = testWFlags( WRepaintNoErase | WResizeNoErase ) != WRepaintNoErase | WResizeNoErase;
#endif
    if ( erase )
        this->erase( rcl.xLeft, rcl.yBottom,
                     rcl.xRight, rcl.yTop );

#if defined (DEBUG_REPAINTRESIZE)
    qDebug( "WM_PAINT: [%s/%s/%08X] %ld,%ld; %ld,%ld erase: %d",
            name(), className(), widget_flags,
            rcl.xLeft, rcl.yBottom, rcl.xRight, rcl.yTop, erase );
#endif

    // create a region that will take ownership of hrgn
    QRegion rgn( hrgn, height() );

    QPaintEvent e( rgn, QRect( rcl.xLeft, rcl.yBottom, rcl.xRight, rcl.yTop ),
                   erase );
    QApplication::sendSpontaneousEvent( this, (QEvent*) &e );

    WinEndPaint( hps );
    hps = 0;
    clearWState( WState_InPaintEvent );
    return TRUE;
}

//
// Window move and resize (configure) events
//

bool QETWidget::translateConfigEvent( const QMSG &qmsg )
{
    if ( !testWState(WState_Created) )		// in QWidget::create()
	return TRUE;

    WId fId  = winFId();
    ULONG fStyle = WinQueryWindowULong( fId, QWL_STYLE );
    if ( testWState( WState_ConfigPending ) ) {
        // it's possible that we're trying to set the frame size smaller
        // than it possible for WC_FRAME in QWidget::internalSetGeometry().
        // here we correct this (crect there is set before WinSetWindowPos()
        // that sends WM_SIZE).
	QSize newSize( SHORT1FROMMP(qmsg.mp2), SHORT2FROMMP(qmsg.mp2) );
        if ( qmsg.msg == WM_SIZE && size() != newSize ) {
            crect.setSize( newSize );
        }
	return TRUE;
    }
    setWState( WState_ConfigPending );		// set config flag
    QRect cr = geometry();
    if ( qmsg.msg == WM_SIZE ) {			// resize event
	QSize oldSize = size();
	QSize newSize( SHORT1FROMMP(qmsg.mp2), SHORT2FROMMP(qmsg.mp2) );
	cr.setSize( newSize );
        crect = cr;
	if ( isTopLevel() ) {			// update caption/icon text
	    createTLExtra();
	    QString txt;
	    if ( (fStyle & WS_MAXIMIZED) && !!iconText() )
		txt = iconText();
	    else
		if ( !caption().isNull() )
		txt = caption();

	    if ( !!txt ) {
                WinSetWindowText( fId, txt.local8Bit() );
	    }
	}
	if ( oldSize != newSize) {
#if !defined (QT_PM_NO_WIDGETMASK)
            // Spontaneous (external to Qt) WM_SIZE messages should occur only
            // on top-level widgets. If we get them for a non top-level widget,
            // the result will most likely be incorrect because widget masks will
            // not be properly processed (i.e. in the way it is done in
            // QWidget::internalSetGeometry() when the geometry is changed from
            // within Qt). So far, I see no need to support this (who will ever
            // need to move a non top-level window of a foreign process?).
            Q_ASSERT( isTopLevel() );
#endif
	    if ( isVisible() ) {
		QResizeEvent e( newSize, oldSize );
		QApplication::sendSpontaneousEvent( this, &e );
		if ( !testWFlags( WStaticContents ) )
		    repaint( testWFlags(WNoAutoErase) != WNoAutoErase );
	    } else {
		QResizeEvent *e = new QResizeEvent( newSize, oldSize );
		QApplication::postEvent( this, e );
	    }
	}
    } else if ( qmsg.msg == WM_MOVE ) {	// move event
	QPoint oldPos = geometry().topLeft();
        SWP swp;
        if ( isTopLevel() ) {
            WinQueryWindowPos( fId, &swp );
            // flip y coordinate
            swp.y = QApplication::desktop()->height() - ( swp.y + swp.cy );
            QTLWExtra *top = topData();
            swp.x += top->fleft;
            swp.y += top->ftop;
        } else {
            WinQueryWindowPos( winId(), &swp );
            // flip y coordinate
            swp.y = parentWidget()->height() - ( swp.y + swp.cy );
        }
        QPoint newCPos( swp.x, swp.y );
	if ( newCPos != oldPos ) {
	    cr.moveTopLeft( newCPos );
	    crect = cr;
	    if ( isVisible() ) {
		QMoveEvent e( newCPos, oldPos );  // cpos (client position)
		QApplication::sendSpontaneousEvent( this, &e );
	    } else {
		QMoveEvent * e = new QMoveEvent( newCPos, oldPos );
		QApplication::postEvent( this, e );
	    }
	}
    }
    clearWState( WState_ConfigPending );		// clear config flag
    return TRUE;
}


//
// Close window event translation.
//
// This class is a friend of QApplication because it needs to emit the
// lastWindowClosed() signal when the last top level widget is closed.
//

bool QETWidget::translateCloseEvent( const QMSG & )
{
    return close(FALSE);
}

void  QApplication::setCursorFlashTime( int msecs )
{
    WinSetSysValue( HWND_DESKTOP, SV_CURSORRATE, msecs / 2 );
    cursor_flash_time = msecs;
}

int QApplication::cursorFlashTime()
{
    int blink = (int) WinQuerySysValue( HWND_DESKTOP, SV_CURSORRATE );
    if ( !blink )
	return cursor_flash_time;
    if (blink > 0)
	return 2*blink;
    return 0;
}

void QApplication::setDoubleClickInterval( int ms )
{
    WinSetSysValue( HWND_DESKTOP, SV_DBLCLKTIME, ms );
    mouse_double_click_time = ms;
}


int QApplication::doubleClickInterval()
{
    int ms = (int) WinQuerySysValue( HWND_DESKTOP, SV_DBLCLKTIME );
    if ( ms != 0 )
	return ms;
    return mouse_double_click_time;
}

void QApplication::setWheelScrollLines( int n )
{
    wheel_scroll_lines = n;
}

int QApplication::wheelScrollLines()
{
    return wheel_scroll_lines;
}

void QApplication::setEffectEnabled( Qt::UIEffect effect, bool enable )
{
#ifndef QT_NO_EFFECTS
    switch (effect) {
        case UI_AnimateMenu:
            if ( enable ) fade_menu = FALSE;
            animate_menu = enable;
            break;
        case UI_FadeMenu:
            if ( enable )
                animate_menu = TRUE;
            fade_menu = enable;
            break;
        case UI_AnimateCombo:
            animate_combo = enable;
            break;
        case UI_AnimateTooltip:
            if ( enable ) fade_tooltip = FALSE;
            animate_tooltip = enable;
            break;
        case UI_FadeTooltip:
            if ( enable )
                animate_tooltip = TRUE;
            fade_tooltip = enable;
            break;
        case UI_AnimateToolBox:
            animate_toolbox = enable;
            break;
        default:
            animate_ui = enable;
            break;
    }
#endif
}

bool QApplication::isEffectEnabled( Qt::UIEffect effect )
{
#ifndef QT_NO_EFFECTS
    if ( QColor::numBitPlanes() < 16 || !animate_ui )
	return FALSE;

    switch( effect ) {
        case UI_AnimateMenu:
            return animate_menu;
        case UI_FadeMenu:
            return fade_menu;
        case UI_AnimateCombo:
            return animate_combo;
        case UI_AnimateTooltip:
            return animate_tooltip;
        case UI_FadeTooltip:
            return fade_tooltip;
        case UI_AnimateToolBox:
            return animate_toolbox;
        default:
            return animate_ui;
    }
#else
    return FALSE;
#endif
}

void QApplication::flush()
{
}

#if !defined (QT_NO_SESSIONMANAGER)

bool qt_app_canQuit()
{
#if defined (DEBUG_SESSIONMANAGER)
    qDebug( "qt_app_canQuit(): sm_smActive=%d qt_about_to_destroy_wnd=%d "
            "sm_gracefulShutdown=%d sm_cancel=%d",
            sm_smActive, qt_about_to_destroy_wnd,
            sm_gracefulShutdown, sm_cancel );
#endif

    BOOL answer = FALSE;

    // We can get multiple WM_QUIT messages while the "session termination
    // procedure" (i.e. the QApplication::commitData() call) is still in
    // progress. Ignore them.
    if ( !sm_smActive ) {
        if ( sm_gracefulShutdown ) {
            // this is WM_QUIT after WM_SAVEAPPLICATION (either posted by the OS
            // or by ourselves), confirm the quit depending on what the user wants
            sm_quitSkipped = FALSE;
            answer = !sm_cancel;
            if ( sm_cancel ) {
                // the shutdown has been canceled, reset the flag to let the
                // graceful shutdown happen again later
                sm_gracefulShutdown = FALSE;
            }
        } else {
            // sm_gracefulShutdown is FALSE, so allowsInteraction() and friends
            // will return FALSE during commitData() (assuming that WM_QUIT w/o
            // WM_SAVEAPPLICATION is an emergency termination)
            sm_smActive = TRUE;
            sm_blockUserInput = TRUE; // prevent user-interaction outside interaction windows
            sm_cancel = FALSE;
            if ( qt_session_manager_self )
                qApp->commitData( *qt_session_manager_self );
            sm_smActive = FALSE;
            answer = TRUE; // ignore sm_cancel
        }
    } else {
        // if this is a WM_QUIT received during WM_SAVEAPPLICATION handling,
        // remember we've skipped (refused) it
        if ( sm_gracefulShutdown )
            sm_quitSkipped = TRUE;
    }

#if defined (DEBUG_SESSIONMANAGER)
    qDebug( "qt_app_canQuit(): answer=%ld", answer );
#endif

    return answer;
}

bool QSessionManager::allowsInteraction()
{
    // Allow interation only when the system is being normally shutdown
    // and informs us using WM_SAVEAPPLICATION. When we receive WM_QUIT directly
    // (so sm_gracefulShutdown is FALSE), interaction is disallowed.
    if ( sm_smActive && sm_gracefulShutdown ) {
        sm_blockUserInput = FALSE;
        return TRUE;
    }

    return FALSE;
}

bool QSessionManager::allowsErrorInteraction()
{
    // Allow interation only when the system is being normally shutdown
    // and informs us using WM_SAVEAPPLICATION. When we receive WM_QUIT directly
    // (so sm_gracefulShutdown is FALSE), interaction is disallowed.
    if ( sm_smActive && sm_gracefulShutdown ) {
        sm_blockUserInput = FALSE;
        return TRUE;
    }

    return FALSE;
}

void QSessionManager::release()
{
    if ( sm_smActive && sm_gracefulShutdown )
        sm_blockUserInput = TRUE;
}

void QSessionManager::cancel()
{
    if ( sm_smActive && sm_gracefulShutdown )
        sm_cancel = TRUE;
}

#endif
