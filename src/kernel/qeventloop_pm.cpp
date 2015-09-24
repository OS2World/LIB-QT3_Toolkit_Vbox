/****************************************************************************
** $Id: qeventloop_pm.cpp 180 2008-03-01 15:42:34Z dmik $
**
** Implementation of OS/2 startup routines and event handling
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
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

#include "qeventloop_p.h"
#include "qeventloop.h"
#include "qapplication.h"
#include "qptrlist.h"
#include "qvaluelist.h"

#include <sys/socket.h>

#if defined(QT_THREAD_SUPPORT)
#  include "qmutex.h"
#  include "qthread.h"
#  include "qsemaphore.h"
#endif // QT_THREAD_SUPPORT

// entry point: PMWIN.305
extern "C" HMQ APIENTRY WinQueueFromID( HAB hab, PID pid, PID tid );

extern uint qGlobalPostedEventsCount();
extern bool qt_pmEventFilter( QMSG* msg, MRESULT &result );

#if !defined (QT_NO_SESSIONMANAGER)
extern bool qt_app_canQuit(); // defined in qapplication_pm.cpp
#endif

static HAB qt_gui_hab = 0;
static HMQ qt_gui_queue = 0;

#ifdef QT_PM_NO_DOSTIMERS
static bool	dispatchTimer( uint, QMSG * );
#endif
static void	activateZeroTimers();

static void	initTimers();
static void	cleanupTimers();

static int	 numZeroTimers	= 0;		// number of full-speed timers

// a flag to disable the warning when the console process dynamically
// switches istelf to PM. Currently used by the UIC tool.
bool qt_suppress_morph_warning = false;

// Check that the current process is in PM mode. This is called by QEventLoop
// and QApplication initialization routines and by initTimers() to ensure
// that we are in PM mode and therefore it is possible to create the message
// queue. "Morphing" to PM leaves the console attahed which can be used for
// debugging.
void qt_ensure_pm()
{
    PPIB ppib;
    DosGetInfoBlocks( NULL, &ppib );
    if( ppib->pib_ultype != 3 ) {
#if defined(QT_CHECK_STATE)
        if( !qt_suppress_morph_warning )
            qWarning( "Qt: the program has not been linked as the Presentation "
                      "Manager application\n"
                      "but it uses GUI capabilities. Switching to PM mode "
                      "dynamically." );
#endif
        ppib->pib_ultype = 3;
    }
}

enum
{
    // socket select notification (highest priority)
    WM_U_SEM_SELECT = WM_SEM1,
    // zero timer notification (lowest priority)
    WM_U_SEM_ZEROTIMER = WM_SEM4,
};

/*****************************************************************************
  Auxiliary object window class for dedicated message processing.
  Declared in qwindowdefs_pm.h
 *****************************************************************************/

/*!
  \class QPMObjectWindow qwindowdefs.h

  The QPMObjectWindow class is an auxiliary class for dedicated message
  processing. Its functionality is based on PM object windows. Once an instance
  of this class is created, PM window messages can be sent or posted to it using
  send() or post() methods. Subclasses should override the message() method to
  process sent or posted messages. The hwnd() method is used whenever a PM
  window handle of this object window is necessary to be passed as a HWND
  argument to other calls and/or messages.

  Instances of this class must be created on the main GUI thread only or on a
  thread that has created a PM message queue and runs the PM message loop
  \b itself. If you create an instance on a thread other than main, make sure
  you destroy it before destroying the thread's message queue.

  \note WM_CREATE and WM_DESTROY messages are processed internally and not
  delivered do the message() method. Instead, you can use the constructor and
  the destructor of the subclasses, respectively.

  \note This class is non-portable!
*/

// returns HMQ of the current thread or NULL if no event queue has been created
static HMQ qt_WinQueryQueue( HAB hab )
{
    PTIB ptib;
    PPIB ppib;
    DosGetInfoBlocks( &ptib, &ppib );
    return WinQueueFromID( hab, ppib->pib_ulpid, ptib->tib_ptib2->tib2_ultid );
}

static QPtrList<HWND> qt_objWindows;

/*!
  Constructs a new object window for the current thread.
  If \a deferred is FALSE, this method calls create() to create a PM object
  window. Otherwise, you must call create() yourself before this object
  window is able to process messages.
*/
QPMObjectWindow::QPMObjectWindow( bool deferred /* = FALSE */ ) :
    w( NULLHANDLE )
{
    if ( !deferred )
        create();
}

/*!
  Destroys this object window.
  This method calls destroy() to free the PM object window.
*/
QPMObjectWindow::~QPMObjectWindow()
{
    destroy();
}

/*!
  Creates a PM object window.
  Returns \c TRUE on success or \c FALSE otherwise.
  The method does nothing but returns \c FALSE  if the window has been
  already created. The handle of the successfully created object window can
  be obtained using the hwnd() method.

  \note Must be called on the same thread that cosnstructed this instance.
*/
bool QPMObjectWindow::create()
{
    if ( w != NULLHANDLE )
        return FALSE;

    static const char *ClassName = "QPMObjectWindow";
    static bool classRegistered = FALSE;

    if ( !classRegistered ) {
        WinRegisterClass( 0, ClassName, windowProc, 0, QWL_USER + sizeof(PVOID) );
        classRegistered = TRUE;
    }

    w = WinCreateWindow( HWND_OBJECT, ClassName,
                         NULL, 0, 0, 0, 0, 0, NULL,
                         HWND_BOTTOM, 0, this, NULL );
#ifndef QT_NO_DEBUG
    if ( w == NULLHANDLE )
        qSystemWarning( "QPMObjectWindow: Failed to create object window" );
#endif

    if ( w != NULLHANDLE && qt_gui_queue &&
         qt_WinQueryQueue( 0 ) == qt_gui_queue ) {
            // store the handle in the list for clean destruction
            qt_objWindows.append( &w );
    }

    return w != NULLHANDLE;
}

/*!
  Destroys the PM object window.
  Returns \c TRUE on success or \c FALSE otherwise.
  The method does nothing but returns \c FALSE  if the window has been
  already destroyed (or never created).

  \note Must be called on the same thread that cosnstructed this instance.
*/
bool QPMObjectWindow::destroy()
{
    if ( w == NULLHANDLE )
        return FALSE;

    if ( qt_gui_queue && qt_WinQueryQueue( 0 ) == qt_gui_queue ) {
        // remove the handle from the 'clean destruction' list
        qt_objWindows.removeRef( &w );
    }

    HWND h = w;
    w = 0; // tell windowProc() we're unsafe
    WinDestroyWindow( h );

    return TRUE;
}

//static
MRESULT EXPENTRY QPMObjectWindow::windowProc( HWND hwnd, ULONG msg,
                                              MPARAM mp1, MPARAM mp2 )
{
    if ( msg == WM_CREATE ) {
        QPMObjectWindow *that = static_cast< QPMObjectWindow *>( mp1 );
        if ( !that )
            return (MRESULT) TRUE;
        WinSetWindowPtr( hwnd, QWL_USER, that );
        return (MRESULT) FALSE;
    }

    QPMObjectWindow *that =
        static_cast< QPMObjectWindow * >( WinQueryWindowPtr( hwnd, QWL_USER ) );
    Q_ASSERT( that );

    // Note: before WinCreateWindow() returns to the constructor or after the
    // destructor has been called, w is 0. We use this to determine that the
    // object is in the unsafe state (VTBL is not yet initialized or has been
    // already uninitialized), so message() points to never-never land.
    if ( !that || !that->w )
        return (MRESULT) FALSE;

    return that->message( msg, mp1, mp2 );
}

/*!
  \fn QPMObjectWindow::send( ULONG msg, MPARAM mp1, MPARAM mp2 ) const

  Synchronously sends a message \a msg with the given parameters \a mp1 and
  \a mp2 to this window handle and returns a reply from the message() function.

  \note Must be called on the same thread that cosnstructed this instance.
*/

/*!
  \fn QPMObjectWindow::post( ULONG msg, MPARAM mp1, MPARAM mp2 ) const

  Asynchronously posts a message \a msg with the given parameters \a mp1 and
  \a mp2 to this window handle. Returns \c TRUE on success and \c FALSE
  otherwise.

  \note Can be called on any thread.
*/

// Auxiliary object window to process WM_U_SEM_SELECT and WM_TIMER messages.
// We need a dedicated window along with processing these messages directly in
// QEventLoop::processEvents() to make sure they are processed even if the
// current message loop is not run by Qt. This happens when a native modal
// dialog is shown or when a top-level Qt widget is being moved or resized
// using the mouse, or when a Qt-based DLL plugin is used by a non-Qt
// application.

class QPMAuxEventQueueWin : public QPMObjectWindow
{
public:
    QPMAuxEventQueueWin() : QPMObjectWindow( TRUE ), eventLoop( NULL ) {}
    void setEventLoop( QEventLoop *el ) { eventLoop = el; }
    MRESULT message( ULONG msg, MPARAM mp1, MPARAM mp2 );
private:
    QEventLoop *eventLoop;
};

MRESULT QPMAuxEventQueueWin::message( ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch ( msg ) {
        case WM_U_SEM_SELECT: {
            if ( eventLoop )
                eventLoop->activateSocketNotifiers();
            break;
        }
        case WM_U_SEM_ZEROTIMER: {
            if ( numZeroTimers ) {
                activateZeroTimers();
                // repost the message if there are still zero timers left
                if ( numZeroTimers )
                    WinPostMsg( hwnd(), WM_U_SEM_ZEROTIMER, 0, 0 );
            }
            break;
        }
        case WM_TIMER: {
#ifndef QT_PM_NO_DOSTIMERS
            QApplication::sendPostedEvents( NULL, QEvent::Timer );
            break;
#else // ifndef QT_PM_NO_DOSTIMERS
            USHORT id = SHORT1FROMMP( mp1 );
            QMSG qmsg = { hwnd(), msg, mp1, mp2 };
            dispatchTimer( (uint) id, &qmsg );
            break;
#endif // ifndef QT_PM_NO_DOSTIMERS
        }
    }

    return FALSE;
}

static QPMAuxEventQueueWin qt_aux_win;


/*****************************************************************************
  Safe configuration (move,resize,setGeometry) mechanism to avoid
  recursion when processing messages.
 *****************************************************************************/

#include "qptrqueue.h"

struct QPMConfigRequest {
    WId	 id;					// widget to be configured
    int	 req;					// 0=move, 1=resize, 2=setGeo
    int	 x, y, w, h;				// request parameters
};

static QPtrQueue<QPMConfigRequest> *configRequests = 0;

void qPMRequestConfig( WId id, int req, int x, int y, int w, int h )
{
    if ( !configRequests )			// create queue
	configRequests = new QPtrQueue<QPMConfigRequest>;
    QPMConfigRequest *r = new QPMConfigRequest;
    r->id = id;					// create new request
    r->req = req;
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
    configRequests->enqueue( r );		// store request in queue
}

Q_EXPORT void qPMProcessConfigRequests()		// perform requests in queue
{
    if ( !configRequests )
	return;
    QPMConfigRequest *r;
    for ( ;; ) {
	if ( configRequests->isEmpty() )
	    break;
	r = configRequests->dequeue();
	QWidget *w = QWidget::find( r->id );
	if ( w ) {				// widget exists
	    if ( w->testWState(Qt::WState_ConfigPending) )
		return;				// biting our tail
	    if ( r->req == 0 )
		w->move( r->x, r->y );
	    else if ( r->req == 1 )
		w->resize( r->w, r->h );
	    else
		w->setGeometry( r->x, r->y, r->w, r->h );
	}
	delete r;
    }
    delete configRequests;
    configRequests = 0;
}

/*****************************************************************************
  Timer handling; Our routines depend on OS/2 PM timer functions, but we
  need some extra handling to activate objects at timeout.

  There are two implemetations of Qt timers. When QT_PM_NO_DOSTIMERS is defined
  (NOT by default), Qt uses PM timers. In this case, there are two types of
  timer identifiers. PM timer ids (internal use) are stored in TimerInfo. Qt
  timer ids are indexes (+1) into the timerVec vector. Note that this PM timer
  based implementation is error-prone due to stupid PM limitations: there are
  only several dozens of PM timers per the whole system and this number is
  shared across all processes (e.g. if you create too many timers, other
  applications that also need them will suck).

  When QT_PM_NO_DOSTIMERS is not defined (by default), Qt uses its own timer
  implementation based on a signle DosAsyncTimer() and a dedicated timer thread.
  This implementation allows virtually unlimited number of Qt timers. In this
  case, there is only one type of type identifiers: Qt timer identifiers are
  indexes (+1) into the timeDict dictionary.

  NOTE: These functions are for internal use. QObject::startTimer() and
	QObject::killTimer() are for public use.
	The QTimer class provides a high-level interface which translates
	timer events into signals.

  qStartTimer( interval, obj )
	Starts a timer which will run until it is killed with qKillTimer()
	Arguments:
	    int interval	timer interval in milliseconds
	    QObject *obj	where to send the timer event
	Returns:
	    int			timer identifier, or zero if not successful

  qKillTimer( timerId )
	Stops a timer specified by a timer identifier.
	Arguments:
	    int timerId		timer identifier
	Returns:
	    bool		TRUE if successful

  qKillTimer( obj )
	Stops all timers that are sent to the specified object.
	Arguments:
	    QObject *obj	object receiving timer events
	Returns:
	    bool		TRUE if successful
 *****************************************************************************/

//
// Internal data structure for timers
//

//
// Template to track availability of free (unused) integer values within some
// interval. Note that take() and give() maintain spans sorted in ascending
// order.
//
template <typename T>
class QFreeValueList
{
public:
    QFreeValueList( T min, T max ) : intMin( min ), intMax( max ) {
        freeValues.push_front( Span( min, max ) );
    }
    T take() {
        Q_ASSERT( !isEmpty() );
        Span &free = freeValues.first();
        T freeValue = free.min;
        if ( free.min == free.max ) freeValues.pop_front();
        else free.min ++;
        return freeValue;
    }
    void give( T val ) {
        Q_ASSERT( val >= intMin && val <= intMax );
        // look for the span to join
        typename FreeList::iterator it = freeValues.begin();
        for ( ; it != freeValues.end(); ++ it ) {
            Span &span = (*it);
            if ( val == span.min - 1 ) {
                // join the less end of span
                span.min --;
                typename FreeList::iterator it2 = it;
                if ( it2 != freeValues.begin() && (*--it2).max + 1 == span.min ) {
                    span.min = (*it2).min;
                    freeValues.erase( it2 );
                }
                return;
            } else if ( val == span.max + 1 ) {
                // join the greater end of span
                span.max ++;
                typename FreeList::iterator it2 = it;
                if ( ++it2 != freeValues.end() && (*it2).min - 1 == span.max ) {
                    span.max = (*it2).max;
                    freeValues.erase( it2 );
                }
                return;
            } else if ( val < span.min ) {
                // all the following spans are too "far" to join
                break;
            }
            // span must not include val (contradicts take())
            Q_ASSERT( val > span.max );
        }
        if ( it == freeValues.end() ) freeValues.push_back( Span( val, val ) );
        else freeValues.insert( it, Span( val, val ) );
    }
    bool isEmpty() { return freeValues.isEmpty(); }
    T min() const { return intMin; }
    T max() const { return intMax; }
private:
    struct Span {
        Span() : min( 0 ), max( 0 ) {}
        Span( T mn, T mx ) : min( mn ), max( mx ) {}
        T min;
        T max;
    };
    typedef QValueList<Span> FreeList;
    FreeList freeValues;
    T intMin;
    T intMax;
};

#ifndef QT_PM_NO_DOSTIMERS

#include "qintdict.h"

struct TimerInfo {              // internal timer info
    uint     ind;               // - Qt timer identifier - 1 (= index in vector)
    ULONG    interval;          // - timeout interval, milliseconds
    ULONG    last;              // - last shot timestamp
    QObject *obj;               // - object to receive events
};

typedef QIntDict<TimerInfo> TimerDict;          // dict of TimerInfo structs
static TimerDict *timerDict = NULL;

static QMutex timerMutex;                       // mutex protecting timerDict

typedef QFreeValueList<int> FreeQtTIDList;      // list of free Qt timer IDs
static FreeQtTIDList *freeQtTIDs = NULL;

//
// Auxiliary timer thread to wait for Dos timer events and post Qt timer events.
//
static class QTimerThread : public QThread
{
public:
    QTimerThread();
    virtual ~QTimerThread();
    void run();
    void signalQuit();
    void ensureShot( int ival );
private:
    HEV hev;                        // OS/2 timer event semaphore
    HTIMER htimer;                  // OS/2 timer
    ULONG interval;                 // OS/2 timer interval
    bool quit : 1;                  // quit flag
} *timerThread = NULL;

QTimerThread::QTimerThread()
    : hev( NULLHANDLE ), htimer( NULLHANDLE )
    , interval( 0 ), quit( false )
{
    APIRET rc;
    rc = DosCreateEventSem( NULL, &hev, DC_SEM_SHARED, 0 /* reset */);
    Q_ASSERT( rc == NO_ERROR );
}

QTimerThread::~QTimerThread()
{
    if ( hev != NULLHANDLE ) {
        DosCloseEventSem( hev );
        hev = NULLHANDLE;
    }
}

void QTimerThread::run()
{
    APIRET rc;
    ULONG now = 0;

    do {
        rc = DosWaitEventSem( hev, SEM_INDEFINITE_WAIT );
        Q_ASSERT( rc == NO_ERROR );
        if ( quit )
            break;

        // get current time again and calculate the interval
        DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &now, sizeof(now) );

        // go through all timers and see which were shot

        QMutexLocker lock( &timerMutex );

        ULONG minLeft = ULONG_MAX;

        QIntDictIterator<TimerInfo> it( *timerDict );
        for ( register TimerInfo *t; (t = it.current()); ++it ) {
            // skip zero timers
            if ( t->interval == 0 )
                continue;
            ULONG spent = now - t->last;
            if ( spent >= t->interval ) {
                // the timer iterval has expired, post the timer event
                QTimerEvent *e = new QTimerEvent( t->ind + 1 );
                QApplication::postEvent( t->obj, e );
                // set the new last stamp
                t->last += t->interval * (spent / t->interval);
            }
            // calculate minimal time to the next shot
            minLeft = QMIN( minLeft, t->interval - (spent % t->interval) );
        }

        if ( timerDict->count() > 0 ) {
            // post a spare WM_TIMER message to make sure timers run even when
            // the message loop is not controlled by QEventLoop::processEvents()
            // (e.g. when moving or resizing a window using the mouse)
            WinPostMsg( qt_aux_win.hwnd(), WM_TIMER, 0, 0 );
            // restart the OS/2 timer
            interval = minLeft;
            ULONG postCnt;
            DosResetEventSem( hev, &postCnt );
            // check for quit (cancelQuit() could post the semaphore just before
            // we reset it above)
            if ( quit )
                break;
            rc = DosAsyncTimer( minLeft, (HSEM) hev, &htimer );
            Q_ASSERT( rc == NO_ERROR );
        } else {
            htimer = NULLHANDLE;
            interval = 0;
        }

    } while (1);
}

void QTimerThread::signalQuit()
{
    quit = true;
    DosPostEventSem( hev );
    wait();
}

// Note: must be called from under timerMutex!
void QTimerThread::ensureShot( int ival )
{
    Q_ASSERT( timerMutex.locked() );
    Q_ASSERT( ival > 0 );
    if ( interval == 0 || interval > (ULONG) ival ) {
        // start another timer to make sure the new Qt timer is fired off in time
        interval = (ULONG) ival;
        APIRET rc = DosAsyncTimer( interval, (HSEM) hev, &htimer );
        Q_ASSERT( rc == NO_ERROR );
    }
}

#else // ifndef QT_PM_NO_DOSTIMERS

#include "qptrvector.h"
#include "qintdict.h"

struct TimerInfo {				// internal timer info
    uint     ind;				// - Qt timer identifier - 1
    ULONG    id;				// - PM timer identifier
    bool     zero;				// - zero timing
    QObject *obj;				// - object to receive events
};
typedef QPtrVector<TimerInfo>  TimerVec;	// vector of TimerInfo structs
typedef QIntDict<TimerInfo> TimerDict;		// fast dict of timers

static TimerVec  *timerVec  = 0;		// timer vector
static TimerDict *timerDict = 0;		// timer dict

typedef QFreeValueList<int> FreeQtTIDList;      // list of free Qt timer IDs
static FreeQtTIDList *freeQtTIDs = 0;
typedef QFreeValueList<ULONG> FreePMTIDList;    // list of free PM timer IDs
static FreePMTIDList *freePMTIDs = 0;

//
// Timer activation (called from the event loop when WM_TIMER arrives)
//
static bool dispatchTimer( uint timerId, QMSG *msg )
{
    MRESULT res = NULL;
    if ( !msg || !qApp || !qt_pmEventFilter(msg,res) )
    {
        if ( !timerVec )            // should never happen
            return FALSE;
        register TimerInfo *t = timerDict->find( timerId );
        if ( !t )                   // no such timer id
            return FALSE;
        QTimerEvent e( t->ind + 1 );
        QApplication::sendEvent( t->obj, &e );  // send event
        return TRUE;                // timer event was processed
    }
    return TRUE;
}

#endif // ifndef QT_PM_NO_DOSTIMERS

//
// activate full-speed timers
//
static void activateZeroTimers()
{
#ifndef QT_PM_NO_DOSTIMERS
    if ( !timerDict )
        return;
    QIntDictIterator<TimerInfo> it( *timerDict );
    register TimerInfo *t = 0;
    int n = numZeroTimers;
    while ( n-- ) {
        for ( ;; ) {
            t = it();
            Q_ASSERT( t );
            if ( !t )
                return;
            if ( t->interval == 0 )
                break;
        }
        QTimerEvent e( t->ind + 1 );
        QApplication::sendEvent( t->obj, &e );
    }
#else
    if ( !timerVec )
        return;
    uint i=0;
    register TimerInfo *t = 0;
    int n = numZeroTimers;
    while ( n-- ) {
        for ( ;; ) {
            t = timerVec->at(i++);
            if ( t && t->zero )
                break;
            else if ( i == timerVec->size() )		// should not happen
                return;
        }
        QTimerEvent e( t->ind + 1 );
        QApplication::sendEvent( t->obj, &e );
    }
#endif
}


//
// Timer initialization and cleanup routines
//

static void initTimers()			// initialize timers
{
#ifndef QT_PM_NO_DOSTIMERS
    timerDict = new TimerDict( 29 );
    Q_CHECK_PTR( timerDict );
    timerDict->setAutoDelete( TRUE );
    freeQtTIDs = new FreeQtTIDList( 0, 1023 );  // resonable max amount of timers
    Q_CHECK_PTR( freeQtTIDs );
    timerThread = new QTimerThread();
    Q_CHECK_PTR( timerThread );
    timerThread->start();
#else // ifndef QT_PM_NO_DOSTIMERS
    timerVec = new TimerVec( 128 );
    Q_CHECK_PTR( timerVec );
    timerVec->setAutoDelete( TRUE );
    timerDict = new TimerDict( 29 );
    Q_CHECK_PTR( timerDict );
    freeQtTIDs = new FreeQtTIDList( 0, 1023 );  // resonable max amount of timers
    Q_CHECK_PTR( freeQtTIDs );
    freePMTIDs = new FreePMTIDList( 1, TID_USERMAX - 1 );
    Q_CHECK_PTR( freePMTIDs );
#endif // ifndef QT_PM_NO_DOSTIMERS
}

/// @todo (dmik) cleanupTimers() is only called by QEventLoop::cleanup() so it
//  won't be called if there is no QEventLoop instance created (for example,
//  when a Qt-based plugin DLL is used by a non-Qt application). Use atexit()?
//  Btw, the same relates to the QThread::cleanup() vs QApplication pair.
//  And to qt_aux_win (the DLL may be unloaded after the application has
//  destroyed the main event queue, so the static destructor will not be able
//  to properly destroy the window).

static bool timersCleanedUp = false;

static void cleanupTimers()			// remove pending timers
{
    timersCleanedUp = true;

#ifndef QT_PM_NO_DOSTIMERS
    if ( !timerDict )               // no timers were used
        return;
    timerThread->signalQuit();
    delete timerThread;
    timerThread = NULL;
    delete freeQtTIDs;
    freeQtTIDs = NULL;
    delete timerDict;
    timerDict = NULL;
#else // ifndef QT_PM_NO_DOSTIMERS
    register TimerInfo *t;
    if ( !timerVec )				// no timers were used
	return;
    for ( uint i=0; i<timerVec->size(); i++ ) {		// kill all pending timers
	t = timerVec->at( i );
	if ( t && !t->zero )
	    WinStopTimer( 0, qt_aux_win.hwnd(), t->id );
    }
    delete freePMTIDs;
    freePMTIDs = 0;
    delete freeQtTIDs;
    freeQtTIDs = 0;
    delete timerDict;
    timerDict = 0;
    delete timerVec;
    timerVec = 0;
#endif // ifndef QT_PM_NO_DOSTIMERS

    numZeroTimers = 0;
}


//
// Main timer functions for starting and killing timers
//


int qStartTimer( int interval, QObject *obj )
{
    // ignore start timer requests after application termination has started
    Q_ASSERT( !timersCleanedUp );
    if ( timersCleanedUp )
        return 0;

    Q_ASSERT( obj );
    if ( !obj || interval < 0 )
        return 0;

    // lazily create the auxiliary window to process WM_TIMER and
    // WM_U_SEM_ZEROTIMER messages
    if ( !qt_aux_win.hwnd() )
        if ( !qt_aux_win.create() )
            return 0;

#ifndef QT_PM_NO_DOSTIMERS
    if ( !timerDict )				// initialize timer data
        initTimers();
#else  // ifndef QT_PM_NO_DOSTIMERS
    if ( !timerVec )				// initialize timer data
        initTimers();
#endif // ifndef QT_PM_NO_DOSTIMERS

    if ( freeQtTIDs->isEmpty() ) {
#if defined(QT_CHECK_STATE)
        qWarning( "qStartTimer: Maximum number of timers (%d) is reached.",
                  freeQtTIDs->max() - freeQtTIDs->min() + 1 );
#endif
        return 0;
    }

#ifdef QT_PM_NO_DOSTIMERS
    if ( freePMTIDs->isEmpty() ) {
#if defined(QT_CHECK_STATE)
        qWarning( "qStartTimer: Maximum number of non-zero timers (%ld) is reached.",
                  freePMTIDs->max() - freePMTIDs->min() + 1 );
#endif
        return 0;
    }
#endif

    int ind = freeQtTIDs->take();		        // get free timer

#ifndef QT_PM_NO_DOSTIMERS
    register TimerInfo *t = new TimerInfo;      // create timer entry
    Q_CHECK_PTR( t );
    t->ind  = ind;
    t->obj  = obj;

    t->interval = interval;
    if ( t->interval == 0 ) {                   // add zero timer
        numZeroTimers++;
        // indicate there is a new zero timer
        WinPostMsg( qt_aux_win.hwnd(), WM_U_SEM_ZEROTIMER, 0, 0 );
    } else {
        // set the initial last shot timestamp value to now
        DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &t->last, sizeof(t->last) );
    }

    QMutexLocker lock( &timerMutex );
    timerDict->insert( ind, t );                // store in timer dict
    if ( t->interval != 0 )
        timerThread->ensureShot( t->interval );
#else // ifndef QT_PM_NO_DOSTIMERS
    if ( (uint) ind >= timerVec->size() ) {
        uint newSize = timerVec->size() * 4;	// increase the size
        if ( newSize <= (uint) ind )
            newSize = (uint) ind + 1;
        timerVec->resize( newSize );
    }

    register TimerInfo *t = new TimerInfo;      // create timer entry
    Q_CHECK_PTR( t );
    t->ind  = ind;
    t->obj  = obj;

    t->zero = interval == 0;
    if ( t->zero ) {				// add zero timer
        t->id = 0;
        numZeroTimers++;
        // indicate there is a new zero timer
        WinPostMsg( qt_aux_win.hwnd(), WM_U_SEM_ZEROTIMER, 0, 0 );
    } else {
        ULONG freeId = freePMTIDs->take();      // get free timer ID
        t->id = WinStartTimer( 0, qt_aux_win.hwnd(), freeId, (ULONG) interval );
        if ( t->id == 0 ) {
#if defined(QT_CHECK_STATE)
            qSystemWarning( "qStartTimer: Failed to create a timer." );
#endif
            freePMTIDs->give( freeId );         // could not start timer
            freeQtTIDs->give( ind );
            delete t;
            return 0;
        }
    }
    timerVec->insert( ind, t );         // store in timer vector
    if ( !t->zero )
        timerDict->insert( t->id, t );  // store in dict
#endif // ifndef QT_PM_NO_DOSTIMERS

    return ind + 1;                     // return index in vector
}

bool qKillTimer( int ind )
{
#ifndef QT_PM_NO_DOSTIMERS
    if ( !timerDict )
        return FALSE;
    register TimerInfo *t = timerDict->find( ind-1 );
    if ( !t )
        return FALSE;
    if ( t->interval == 0 ) {
        numZeroTimers--;
    }
    freeQtTIDs->give( t->ind );
    QMutexLocker lock( &timerMutex );
    timerDict->remove( ind-1 );
    return TRUE;
#else // ifndef QT_PM_NO_DOSTIMERS
    if ( !timerVec || ind <= 0 || (uint)ind > timerVec->size() )
        return FALSE;
    register TimerInfo *t = timerVec->at(ind-1);
    if ( !t )
        return FALSE;
    if ( t->zero ) {
        numZeroTimers--;
    } else {
        WinStopTimer( 0, qt_aux_win.hwnd(), t->id );
        freePMTIDs->give( t->id );
        timerDict->remove( t->id );
    }
    freeQtTIDs->give( t->ind );
    timerVec->remove( ind-1 );
    return TRUE;
#endif // ifndef QT_PM_NO_DOSTIMERS
}

bool qKillTimer( QObject *obj )
{
#ifndef QT_PM_NO_DOSTIMERS
    if ( !timerDict )
        return FALSE;
    QIntDictIterator<TimerInfo> it ( *timerDict );
    for ( register TimerInfo *t; (t = it.current()); ) {
        if ( t->obj == obj ) {		// object found
            if ( t->interval == 0 ) {
                numZeroTimers--;
            }
            freeQtTIDs->give( t->ind );
            QMutexLocker lock( &timerMutex );
            timerDict->remove( it.currentKey() );
        } else {
            ++it;
        }
    }
    return TRUE;
#else // ifndef QT_PM_NO_DOSTIMERS
    if ( !timerVec )
        return FALSE;
    register TimerInfo *t;
    for ( uint i=0; i<timerVec->size(); i++ ) {
        t = timerVec->at( i );
        if ( t && t->obj == obj ) {		// object found
            if ( t->zero ) {
                numZeroTimers--;
            } else {
                WinStopTimer( 0, qt_aux_win.hwnd(), t->id );
                freePMTIDs->give( t->id );
                timerDict->remove( t->id );
            }
            freeQtTIDs->give( t->ind );
            timerVec->remove( i );
        }
    }
    return TRUE;
#endif // ifndef QT_PM_NO_DOSTIMERS
}

/*****************************************************************************
 Socket notifier type
 *****************************************************************************/

QSockNotType::QSockNotType()
    : list( 0 )
{
    FD_ZERO( &select_fds );
    FD_ZERO( &enabled_fds );
    FD_ZERO( &pending_fds );
}

QSockNotType::~QSockNotType()
{
    if ( list )
	delete list;
    list = 0;
}

/*****************************************************************************
 socket select() thread
 *****************************************************************************/

#if defined(QT_THREAD_SUPPORT)

static class QSockSelectThread : public QThread
{
public:
    QSockSelectThread( QEventLoopPrivate *_d ) : d( _d ), exit( FALSE ) {};
    void run();
    void cancelSelectOrIdle( bool terminate = FALSE );
private:
    QEventLoopPrivate *d;
    bool exit;
} *ss_thread = 0;

// recursive mutex to serialize access to socket notifier data
static QMutex ss_mutex( TRUE );
// flag to indicate the presence of sockets to do select() (we use QSemaphore
// instead of QWaitCondition because we need the "level-triggered" semantics,
// the "edge-triggered" semantics can produce deadlocks)
static QSemaphore ss_flag( 1 );

// Note: must be called from under ss_mutex lock!
static inline void find_highest_fd( QEventLoopPrivate *d )
{
    d->sn_highest = -1;
    for ( int i = 0; i < 3; i ++ ) {
        QPtrList<QSockNot> *list = d->sn_vec[i].list;
        if ( list && !list->isEmpty() ) {
            QSockNot *sn = list->first();
            while ( sn && !FD_ISSET( sn->fd, sn->active ) )
                sn = list->next();
            if ( sn )
                d->sn_highest = QMAX( d->sn_highest,  // list is fd-sorted
                                      sn->fd );
        }
    }
}

void QSockSelectThread::run()
{
    while ( !exit ) {
        ss_mutex.lock();
        if ( d->sn_highest >= 0 ) {		// has socket notifier(s)
            // read
            if ( d->sn_vec[0].list && ! d->sn_vec[0].list->isEmpty() )
                d->sn_vec[0].select_fds = d->sn_vec[0].enabled_fds;
            else
                FD_ZERO( &d->sn_vec[0].select_fds );
            // write
            if ( d->sn_vec[1].list && ! d->sn_vec[1].list->isEmpty() )
                d->sn_vec[1].select_fds = d->sn_vec[1].enabled_fds;
            else
                FD_ZERO( &d->sn_vec[1].select_fds );
            // except
            if ( d->sn_vec[2].list && ! d->sn_vec[2].list->isEmpty() )
                d->sn_vec[2].select_fds = d->sn_vec[2].enabled_fds;
            else
                FD_ZERO( &d->sn_vec[2].select_fds );
            // do select
            int nfds = d->sn_highest + 1;
            d->sn_cancel = d->sn_highest;
            ss_mutex.unlock();
            int nsel = ::select( nfds,
                                 &d->sn_vec[0].select_fds,
                                 &d->sn_vec[1].select_fds,
                                 &d->sn_vec[2].select_fds,
                                 NULL );
            if ( nsel > 0 ) {
                ss_mutex.lock();
                // if select says data is ready on any socket, then set
                // the socket notifier to pending
                bool activate = FALSE;
                for ( int i = 0; i < 3; i++ ) {
                    if ( ! d->sn_vec[i].list )
                        continue;
                    QPtrList<QSockNot> *list = d->sn_vec[i].list;
                    QSockNot *sn = list->first();
                    while ( sn ) {
                        if ( FD_ISSET( sn->fd, &d->sn_vec[i].select_fds ) ) {
                            // see comments in QEventLoop::setSocketNotifierPending()
                            if ( !FD_ISSET( sn->fd, sn->queue ) ) {
                                // queue the socket activation
                                d->sn_pending_list.insert(
                                    (rand() & 0xff) % (d->sn_pending_list.count()+1),
                                    sn );
                                FD_SET( sn->fd, sn->queue );
                                // remove from enabled sockets to prevent deadlocks
                                // on blocking sockets (and to reduce CPU load caused
                                // by frequent select() calls) when the event queue
                                // is not processed fast enough to handle activation
                                FD_CLR( sn->fd, sn->active );
                                if ( sn->fd == d->sn_highest )
                                    find_highest_fd( d );
                                activate = TRUE;
                            }
                        }
                        sn = list->next();
                    }
                }
                ss_mutex.unlock();
                // post a message to activate socket notifiers
                if ( activate )
                    qt_aux_win.post( WM_U_SEM_SELECT, 0, 0 );
            }
        } else {
            d->sn_cancel = -1;
            // no sockets to select(), go to the idle state
            ss_mutex.unlock();
            // wait for the ability to capture the flag
            ss_flag ++;
            // release the flag before termination
            if ( exit )
                ss_flag --;
        }
    }
}

void QSockSelectThread::cancelSelectOrIdle( bool terminate /* = FALSE */ )
{
    ss_mutex.lock();
    exit = terminate;
    if ( d->sn_cancel >= 0 ) {
        // terminate select() execution
        ::so_cancel( d->sn_cancel );
    } else {
        // terminate the idle state by releasing the flag
        if ( !ss_flag.available() )
            ss_flag --;
    }
    ss_mutex.unlock();
    // wait for this thread to end if the termination is requested
    if ( exit )
        wait();
}

static void ss_cleanup()
{
    ss_thread->cancelSelectOrIdle( TRUE );
    delete ss_thread;
    ss_thread = 0;
}

static void ss_init( QEventLoopPrivate *d )
{
    // capture the flag initially
    ss_flag ++;
    ss_thread = new QSockSelectThread( d );
    ss_thread->start();
}

#endif // QT_THREAD_SUPPORT

/*****************************************************************************
 QEventLoop Implementation for OS/2 PM
 *****************************************************************************/

void QEventLoop::init()
{
    d->sn_highest = -1;
    d->sn_cancel = -1;

    qt_ensure_pm();
    d->hab = WinInitialize( 0 );
    d->hmq = WinCreateMsgQueue( d->hab, 0 );
    qt_gui_hab = d->hab;
    qt_gui_queue = d->hmq;

    qt_aux_win.setEventLoop( this );

#if defined(QT_THREAD_SUPPORT)
    // there may only be one global QEventLoop instance that manages sockets
    Q_ASSERT( ss_thread == 0 );
#else
#if defined(QT_CHECK_STATE)
    qWarning( "QEventLoop::init: socket notifiers are not supported "
              "for a single-threaded version of Qt." );
#endif
#endif // QT_THREAD_SUPPORT
}

void QEventLoop::cleanup()
{
    // timers should have been already uninitialized in appClosingDown()
#ifndef QT_PM_NO_DOSTIMERS
    Q_ASSERT( timerDict == 0 );
#else
    Q_ASSERT( timerVec == 0 );
#endif

    // ss_thread should have been already stopped in appClosingDown()
    Q_ASSERT( ss_thread == 0 );

    qt_aux_win.setEventLoop( 0 );

    // destroy all windows created by QPMObjectWindow instances
    for ( HWND *w = qt_objWindows.first(); w; w = qt_objWindows.next() ) {
        Q_ASSERT( *w );
        WinDestroyWindow( *w );
        // tell QPMObjectWindow the window has been destroyed
        *w = 0;
    }

    WinDestroyMsgQueue( d->hmq );
    WinTerminate( d->hab );
    qt_gui_queue = 0;
    qt_gui_hab = 0;
}

void QEventLoop::appStartingUp()
{
}

void QEventLoop::appClosingDown()
{
    // ensure the socket thread is terminated before QApplication calls
    // QThread::cleanup()
    if ( ss_thread )
        ss_cleanup();
    // the same applies to the timer thread
    cleanupTimers();
}

void QEventLoop::registerSocketNotifier( QSocketNotifier *notifier )
{
#if defined(QT_THREAD_SUPPORT)
    // there may only be one global QEventLoop instance that manages sockets
    Q_ASSERT( !qApp || qApp->eventloop == this );
    if ( qApp && qApp->eventloop != this )
        return;

    // lazily create the auxiliary window to process WM_U_SEM_SELECT messages
    if ( !qt_aux_win.hwnd() )
        if ( !qt_aux_win.create() )
            return;

    // lazily start the socket select thread
    if ( !ss_thread )
        ss_init( d );

    int sockfd = -1;
    int type = -1;
    if ( notifier ) {
        sockfd = notifier->socket();
        type = notifier->type();
    }
    if ( sockfd < 0 || sockfd >= FD_SETSIZE || type < 0 || type > 2 ) {
#if defined(QT_CHECK_RANGE)
	qWarning( "QSocketNotifier: Internal error" );
#endif
	return;
    }

    QMutexLocker locker( &ss_mutex );
    ss_thread->cancelSelectOrIdle();

    QPtrList<QSockNot>  *list = d->sn_vec[type].list;
    QSockNot *sn;

    if ( ! list ) {
	// create new list, the QSockNotType destructor will delete it for us
	list = new QPtrList<QSockNot>;
	Q_CHECK_PTR( list );
	list->setAutoDelete( TRUE );
	d->sn_vec[type].list = list;
    }

    sn = new QSockNot;
    Q_CHECK_PTR( sn );
    sn->obj = notifier;
    sn->fd = sockfd;
    sn->queue = &d->sn_vec[type].pending_fds;
    sn->active = &d->sn_vec[type].enabled_fds;

    if ( list->isEmpty() ) {
	list->insert( 0, sn );
    } else {				// sort list by fd, decreasing
	QSockNot *p = list->first();
	while ( p && p->fd > sockfd )
	    p = list->next();
#if defined(QT_CHECK_STATE)
	if ( p && p->fd == sockfd ) {
	    static const char *t[] = { "read", "write", "exception" };
	    qWarning( "QSocketNotifier: Multiple socket notifiers for "
		      "same socket %d and type %s", sockfd, t[type] );
	}
#endif
	if ( p )
	    list->insert( list->at(), sn );
	else
	    list->append( sn );
    }

    // enable the socket only if it's not already pending
    if ( !FD_ISSET( sockfd, sn->queue ) ) {
        FD_SET( sockfd, sn->active );
        d->sn_highest = QMAX( d->sn_highest, sockfd );
    }
#endif // QT_THREAD_SUPPORT
}

void QEventLoop::unregisterSocketNotifier( QSocketNotifier *notifier )
{
#if defined(QT_THREAD_SUPPORT)
    int sockfd = -1;
    int type = -1;
    if ( notifier ) {
        sockfd = notifier->socket();
        type = notifier->type();
    }
    if ( sockfd < 0 || type < 0 || type > 2 ) {
#if defined(QT_CHECK_RANGE)
	qWarning( "QSocketNotifier: Internal error" );
#endif
	return;
    }

    if ( !ss_thread )
        return; // definitely not found

    QMutexLocker locker( &ss_mutex );
    ss_thread->cancelSelectOrIdle();

    QPtrList<QSockNot> *list = d->sn_vec[type].list;
    QSockNot *sn;
    if ( ! list )
	return;
    sn = list->first();
    while ( sn && !(sn->obj == notifier && sn->fd == sockfd) )
	sn = list->next();
    if ( !sn ) // not found
	return;

    // touch fd bitmaps only if there are no other notifiers for the same socket
    // (QPtrList curiously lacks getNext()/getPrev(), so play tambourine)
    QSockNot *next = list->next();
    if ( next ) list->prev();
    else list->last();
    QSockNot *prev = list->prev();
    if ( prev ) list->next();
    else list->first();
    bool unique = (!next || next->fd != sockfd) && (!prev || prev->fd != sockfd);
    if ( unique) {
        FD_CLR( sockfd, sn->active );		// clear fd bit
        FD_CLR( sockfd, sn->queue );
    }
    d->sn_pending_list.removeRef( sn );		// remove from activation list
    list->remove();				// remove notifier found above

    if ( unique) {
        if ( d->sn_highest == sockfd )
            find_highest_fd( d );
    }
#endif // QT_THREAD_SUPPORT
}

void QEventLoop::setSocketNotifierPending( QSocketNotifier *notifier )
{
#if defined(QT_THREAD_SUPPORT)
    int sockfd = -1;
    int type = -1;
    if ( notifier ) {
        sockfd = notifier->socket();
        type = notifier->type();
    }
    if ( sockfd < 0 || type < 0 || type > 2 ) {
#if defined(QT_CHECK_RANGE)
	qWarning( "QSocketNotifier: Internal error" );
#endif
	return;
    }

    if ( !ss_thread )
        return; // definitely not found

    QMutexLocker locker( &ss_mutex );
    ss_thread->cancelSelectOrIdle();

    QPtrList<QSockNot> *list = d->sn_vec[type].list;
    QSockNot *sn;
    if ( ! list )
	return;
    sn = list->first();
    while ( sn && !(sn->obj == notifier && sn->fd == sockfd) )
	sn = list->next();
    if ( ! sn ) { // not found
	return;
    }

    // We choose a random activation order to be more fair under high load.
    // If a constant order is used and a peer early in the list can
    // saturate the IO, it might grab our attention completely.
    // Also, if we're using a straight list, the callback routines may
    // delete other entries from the list before those other entries are
    // processed.
    if ( !FD_ISSET( sn->fd, sn->queue ) ) {
	d->sn_pending_list.insert(
            (rand() & 0xff) % (d->sn_pending_list.count()+1),
            sn
        );
	FD_SET( sockfd, sn->queue );
        // remove from enabled sockets, see QSockSelectThread::run()
        FD_CLR( sockfd, sn->active );
        if ( d->sn_highest == sockfd )
            find_highest_fd( d );
    }
#endif // QT_THREAD_SUPPORT
}

bool QEventLoop::hasPendingEvents() const
{
    QMSG msg;
    return qGlobalPostedEventsCount() || WinPeekMsg( 0, &msg, NULL, 0, 0, PM_NOREMOVE );
}

bool QEventLoop::processEvents( ProcessEventsFlags flags )
{
    QMSG msg;

#if defined(QT_THREAD_SUPPORT)
    QMutexLocker locker( QApplication::qt_mutex );
#endif

    QApplication::sendPostedEvents();

#ifndef QT_PM_NO_DOSTIMERS
    // we've just processed all pending events above, including QEvent::Timer
    // events, so remove spare WM_TIMER messages posted to qt_aux_win for the
    // cases when the message loop is run bypassing this method
    if ( qt_aux_win.hwnd() )
        WinPeekMsg( 0, &msg, qt_aux_win.hwnd(), WM_TIMER, WM_TIMER, PM_REMOVE );
#endif // ifndef QT_PM_NO_DOSTIMERS

    if ( flags & ExcludeUserInput ) {
        while ( WinPeekMsg( 0, &msg, 0, 0, 0, PM_NOREMOVE ) ) {
            if ( msg.msg == WM_CHAR ||
                 (msg.msg >= WM_MOUSEFIRST &&
                     msg.msg <= WM_MOUSELAST) ||
                 (msg.msg >= WM_EXTMOUSEFIRST &&
                     msg.msg <= WM_EXTMOUSELAST) ||
                 msg.msg == WM_HSCROLL ||
                 msg.msg == WM_VSCROLL
            ) {
                WinPeekMsg( 0, &msg, 0, 0, 0, PM_REMOVE );
                continue;
            }
            break;
        }
    }

    bool canWait = d->exitloop || d->quitnow ? FALSE : (flags & WaitForMore);

    if ( canWait ) {
        // can wait if necessary
        if ( !WinPeekMsg( 0, &msg, 0, 0, 0, PM_REMOVE ) ) {
            emit aboutToBlock();
#ifdef QT_THREAD_SUPPORT
            locker.mutex()->unlock();
#endif
            WinGetMsg( 0, &msg, 0, 0, 0 );
#ifdef QT_THREAD_SUPPORT
            locker.mutex()->lock();
#endif
            emit awake();
            emit qApp->guiThreadAwake();
        }
    } else {
	if ( !WinPeekMsg( 0, &msg, 0, 0, 0, PM_REMOVE ) ) {
            // no pending events
	    return FALSE;
	}
    }

    if ( msg.msg == WM_QUIT ) {
        // process the quit request
#if !defined (QT_NO_SESSIONMANAGER)
        if ( qt_app_canQuit() ) {
#endif
            exit( 0 );
            return TRUE;
#if !defined (QT_NO_SESSIONMANAGER)
        } else {
            WinCancelShutdown( d->hmq, FALSE );
            return TRUE;
        }
#endif
    }

    bool handled = FALSE;
    MRESULT res = 0;

    if ( msg.msg == WM_U_SEM_SELECT && msg.hwnd == qt_aux_win.hwnd() ) {
        // socket select event received: prevent it from being handled by
        // qt_aux_win (to obey the ExcludeSocketNotifiers flag)
        handled = TRUE;
        // ignore socket notifier activation if processd by the Qt event hook
        if ( qt_pmEventFilter( &msg, res ) )
            flags |= ExcludeSocketNotifiers;
    }

    if ( !handled && msg.msg && (!msg.hwnd || !QWidget::find( msg.hwnd )) ) {
	handled = qt_pmEventFilter( &msg, res );
    }

    if ( !handled ) {
        // send to QtWndProc or to a native window procedure
	WinDispatchMsg( 0, &msg );
    }

    if ( !(flags & ExcludeSocketNotifiers) )
	activateSocketNotifiers();

    // any pending configs?
    if ( configRequests )
	qPMProcessConfigRequests();
    QApplication::sendPostedEvents();

    return TRUE;
}

void QEventLoop::wakeUp()
{
    PTIB ptib;
    DosGetInfoBlocks( &ptib, NULL );
    MQINFO mqinfo;
    WinQueryQueueInfo( qt_gui_queue, &mqinfo, sizeof(MQINFO) );
    if ( ptib->tib_ptib2->tib2_ultid != mqinfo.tid )
        WinPostQueueMsg( qt_gui_queue, WM_NULL, 0, 0 );
}

int QEventLoop::timeToWait() const
{
    return -1;
}

int QEventLoop::activateTimers()
{
    return 0;
}

int QEventLoop::activateSocketNotifiers()
{
#if defined(QT_THREAD_SUPPORT)
    if ( d->sn_pending_list.isEmpty() )
	return 0;

    if ( !ss_thread )
        return 0;

    // postpone activation if ss_thread is working with the list
    if ( !ss_mutex.tryLock() ) {
        qt_aux_win.post( WM_U_SEM_SELECT, 0, 0 );
        return 0;
    }

    ss_thread->cancelSelectOrIdle();

    // activate entries
    int n_act = 0;
    QEvent event( QEvent::SockAct );
    QPtrListIterator<QSockNot> it( d->sn_pending_list );
    QSockNot *sn;
    while ( (sn = it.current()) ) {
	++it;
	d->sn_pending_list.removeRef( sn );
	if ( FD_ISSET(sn->fd, sn->queue) ) {
	    FD_CLR( sn->fd, sn->queue );
            // reenable the socket to let it participate in the next select()
            FD_SET( sn->fd, sn->active );
            d->sn_highest = QMAX( d->sn_highest, sn->fd );
	    QApplication::sendEvent( sn->obj, &event );
	    n_act++;
	}
    }

    ss_mutex.unlock();

    return n_act;
#else
    return 0;
#endif // QT_THREAD_SUPPORT
}

