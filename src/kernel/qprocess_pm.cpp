/****************************************************************************
** $Id: qprocess_pm.cpp 188 2008-11-21 03:36:19Z dmik $
**
** Implementation of QProcess class for OS/2
**
** Copyright (C) 1992-2001 Trolltech AS.  All rights reserved.
** Copyright (C) 2005 netlabs.org.  OS/2 Version.
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

#include "qplatformdefs.h"
#include "qprocess.h"

#ifndef QT_NO_PROCESS

#include "qapplication.h"
#include "qptrqueue.h"
#include "qtimer.h"
#include "qregexp.h"
#include "qdir.h"
#include "qthread.h"
#include "qintdict.h"
#include "qmutex.h"
#include "qfile.h"
#include "private/qinternal_p.h"
#include "qt_os2.h"

// When QT_QPROCESS_USE_DOSEXECPGM is not defined, we use spawnvpe() instead of
// DosExecPgm() to let LIBC file (pipe, socket) descriptors be properly
// inherited if the child uses the same LIBC version.
//#define QT_QPROCESS_USE_DOSEXECPGM

#include <string.h>

#if !defined(QT_QPROCESS_USE_DOSEXECPGM)
#include <process.h>
#include <sys/wait.h>
#endif

//#define QT_QPROCESS_DEBUG

#define HF_STDIN        HFILE( 0 )
#define HF_STDOUT       HFILE( 1 )
#define HF_STDERR       HFILE( 2 )
#define HF_NULL         HFILE( ~0 ) 

#define HP_NULL         HPIPE( ~0 ) 
#define KEY_NULL        USHORT( ~0 )

#define PID_NULL        PID( ~0 ) 

enum
{
    PIPE_SIZE_STDIN = 65520, // max
    PIPE_SIZE_STDOUT = 65520, // max
    PIPE_SIZE_STDERR = 4096,

    POLL_TIMER = 100,

    // new pipe data notification
    WM_U_PIPE_RDATA = WM_USER + 0,
    WM_U_PIPE_CLOSE = WM_USER + 1,
};

#if defined(QT_QPROCESS_DEBUG)
#include <stdarg.h>
static HFILE StdErrHandle = HF_NULL;
QtMsgHandler OldMsgHandler = NULL;
static void StdErrMsgHandler( QtMsgType type, const char *msg )
{
    if ( OldMsgHandler == NULL ) {
       size_t len = strlen( msg );
       ULONG written = 0;
       DosWrite( StdErrHandle, msg, len, &written );
       const char *EOL = "\n\r";
       DosWrite( StdErrHandle, EOL, 2, &written );
    } else {
        OldMsgHandler( type, msg );
    }
}
#define InstallQtMsgHandler() \
    do { \
        DosDupHandle( HF_STDERR, &StdErrHandle ); \
        qInstallMsgHandler( StdErrMsgHandler ); \
    } while (0)
#define UninstallQtMsgHandler() \
    do { \
        qInstallMsgHandler( OldMsgHandler ); \
        DosClose( StdErrHandle ); \
    } while (0)
#else
#define InstallQtMsgHandler() do {} while (0)
#define UninstallQtMsgHandler() do {} while (0)
#endif

/***********************************************************************
 *
 * QProcessPrivate
 *
 **********************************************************************/
class QProcessPrivate
{
public:
    struct Pipe
    {
        Pipe () : pipe( HP_NULL ), key( KEY_NULL ), pending( 0 )
                , closed( false ) {}
        HPIPE pipe;
        USHORT key;
        QMembuf buf;
        // note: when QProcess is watched by QProcessMonitor, the below fields
        // must be accessed from under the QProcessMonitor lock
        uint pending;
        bool closed : 1;
    };

    QProcessPrivate( QProcess *proc )
    {
        stdinBufRead = 0;
        pipeStdin = HP_NULL;
        pid = PID_NULL;
        exitValuesCalculated = FALSE;

        lookup = new QTimer( proc );
        qApp->connect( lookup, SIGNAL(timeout()), proc, SLOT(timeout()) );
    }

    ~QProcessPrivate()
    {
        reset();
    }

    void reset()
    {
        while ( !stdinBuf.isEmpty() ) {
            delete stdinBuf.dequeue();
        }
        stdinBufRead = 0;
        closeHandles();
        stdout.buf.clear();
        stderr.buf.clear();
        pid = PID_NULL;
        exitValuesCalculated = FALSE;
    }

    Pipe *findPipe( USHORT key )
    {
        if ( stdout.key == key ) return &stdout;
        if ( stderr.key == key ) return &stderr;
        return NULL;
    }
    
    void closePipe( Pipe *pipe )
    {
        if ( pipe->pipe != HP_NULL ) {
            Q_ASSERT( pipe->key == KEY_NULL );
            DosDisConnectNPipe( pipe->pipe );
            DosClose( pipe->pipe );
            pipe->pipe = HP_NULL;
            pipe->pending = 0;
            pipe->closed = FALSE;
        }
    }
    
    bool readPipe( Pipe *pipe );
    
    void closeHandles()
    {
        if( pipeStdin != HP_NULL ) {
            DosDisConnectNPipe( pipeStdin );
            DosClose( pipeStdin );
            pipeStdin = HP_NULL;
        }
        closePipe( &stdout );
        closePipe( &stderr );
    }

    QPtrQueue <QByteArray> stdinBuf;
    uint stdinBufRead;

    HPIPE pipeStdin;
    
    Pipe stdout;
    Pipe stderr;

    PID pid;
    
    QTimer *lookup;

    bool exitValuesCalculated : 1;
};

class QProcessMonitor : public QPMObjectWindow
{
public:
    class Thread : public QThread
    {
    public:
        Thread( QProcessMonitor *m ) : mon( m ) {}
        void run() { mon->monitor(); }
        QProcessMonitor *mon;
    };

    QProcessMonitor();
    ~QProcessMonitor();

    QMutex &mlock() { return lock; }
    
    bool isOk() const { return pipeSem != 0 && thread != NULL; }

    bool addProcess( QProcess *proc );
    void removeProcess( QProcessPrivate *d,
                        QProcessPrivate::Pipe *pipe = NULL,
                        bool inMsgHandler = false);

    void monitor();
    MRESULT message( ULONG msg, MPARAM mp1, MPARAM mp2 );
    
private:

    struct PipeStates
    {
        PipeStates() {
            size = 4;
            arr = new PIPESEMSTATE[ size ]; 
        }
        ~PipeStates() {
            delete[] arr;
        }
        void ensure( size_t sz ) {
            // best size for sz pipes is sz * 2 (to be able to store both
            // NPSS_RDATA & NPSS_CLOSE for every pipe) + one for NPSS_EOI
            size_t newSize = sz * 2 + 1; 
            newSize = ((newSize + 5 - 1) / 5) * 5;
            if ( newSize != size ) {
                delete[] arr;
                size = newSize;
                arr = new PIPESEMSTATE[ size ];
            }
        }
        size_t dataSize() { return size * sizeof(PIPESEMSTATE); }
        PIPESEMSTATE *data() { return arr; }
        PIPESEMSTATE &operator[]( size_t i ) {
            Q_ASSERT( i < size );
            return arr[i];
        }
    private:
        PIPESEMSTATE *arr;
        size_t size;
    };

    bool addPipe( QProcess *proc, QProcessPrivate::Pipe *pipe );
    void removePipe( QProcessPrivate::Pipe *pipe );

    HEV pipeSem;
    Thread *thread;
    bool threadRunning;
    QIntDict<QProcess> pipeKeys;
    PipeStates pipeStates;
    QMutex lock;
};

QProcessMonitor::QProcessMonitor()
    : pipeSem( 0 ), thread( NULL ), threadRunning( FALSE )
{
    APIRET rc = DosCreateEventSem( NULL, &pipeSem, DC_SEM_SHARED, 0 );
    if ( rc != NO_ERROR ) {
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
        qSystemWarning( "Failed to create a semaphore!", rc );
#endif
        return;
    }
    
    thread = new Thread( this );
    Q_ASSERT( thread != NULL );
}

QProcessMonitor::~QProcessMonitor()
{
    if ( thread ) {
        lock.lock();
        if ( threadRunning ) {
            threadRunning = FALSE;
            DosPostEventSem( pipeSem );
        }
        lock.unlock();
        thread->wait();
        delete thread;
    }
    
    if ( pipeSem )
        DosCloseEventSem( pipeSem );
}

bool QProcessMonitor::addPipe( QProcess *proc, QProcessPrivate::Pipe *pipe )
{
    // Note: we use HPIPE as pipe keys in DosSetNPipeSem. However, HPIPE
    // is 32-bit (ulong), while usKey in PIPESEMSTATE is 16-bit (USHORT)
    // We unreasonably assume that the system will never assign HPIPE >= 0xFFFF,
    // and just cast HPIPE to USHORT. There is an assertion checking this
    // condition, so this method should simply return FALSE once our assumption
    // breaks.

    Q_ASSERT( pipe->pipe < HPIPE( KEY_NULL ) );
    if ( pipe->pipe >= HPIPE( KEY_NULL ) )
        return FALSE;

    Q_ASSERT( pipe->pipe != HP_NULL && pipe->key == KEY_NULL );
    
    pipe->key = USHORT( pipe->pipe );
    APIRET rc = DosSetNPipeSem( pipe->pipe, (HSEM) pipeSem, pipe->key );
    if ( rc != NO_ERROR ) {
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
        qSystemWarning( "Failed to set the pipe semaphore!", rc );
#endif        
        return FALSE;
    }

    pipeKeys.insert( pipe->key, proc );
    return TRUE;
}

void QProcessMonitor::removePipe( QProcessPrivate::Pipe *pipe )
{
    Q_ASSERT( pipe->pipe != HP_NULL && pipe->key != KEY_NULL );

    /// @todo (r=dmik) is this really necessary to detach pipeSem?
    DosSetNPipeSem( pipe->pipe, 0, 0 );
    bool ok = pipeKeys.remove( pipe->key );
    pipe->key = KEY_NULL;
    Q_ASSERT( ok );
    Q_UNUSED( ok );
}

bool QProcessMonitor::addProcess( QProcess *proc )
{
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcessMonitor::addProcess(): proc=%p d=%p", proc, proc->d );
#endif

    QProcessPrivate *d = proc->d; 
    
    // check if we need to monitor this process before entering the lock
    if ( d->stdout.pipe == HP_NULL && d->stderr.pipe == HP_NULL )
        return TRUE;
    
    uint newPipes = 0;

    QMutexLocker locker( &lock ); 
    
    if ( d->stdout.pipe != HP_NULL ) {
        if ( !addPipe( proc, &d->stdout ) )
            return FALSE;
        ++ newPipes;
    }

    if ( d->stderr.pipe != HP_NULL ) {
        if ( !addPipe( proc, &d->stderr ) ) {
            removePipe( &d->stderr );
            return FALSE;
        }
        ++ newPipes;
    }
    
    Q_ASSERT( newPipes > 0 );

    pipeStates.ensure( pipeKeys.count() );
    
    // start the monitor thread if necessary
    if ( pipeKeys.count() == newPipes ) {
        threadRunning = TRUE;
        thread->start();
    }
    
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcessMonitor::addProcess(): added %d pipes", newPipes );
#endif        
    return TRUE;
}

void QProcessMonitor::removeProcess( QProcessPrivate *d,
                                     QProcessPrivate::Pipe *pipe /* = NULL */,
                                     bool inMsgHandler /* = false */)
{
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcessMonitor::removeProcess(): d=%p pipe=%p key=%04hX",
            d, pipe, pipe ? pipe->key : KEY_NULL );
#endif
    
    // check if we monitor this process before entering the lock
    if ( d->stdout.pipe == HP_NULL && d->stderr.pipe == HP_NULL )
        return;

    lock.lock(); 

    if ( pipeKeys.count() == 0 ) {
        // Nothing to do. This is an outdated general removeProcess (d, NULL)
        // call from the QProcess destructor or from isRunning(). Just return.
        lock.unlock();
        return;
    }

    if ( !inMsgHandler ) {
        // process all messages in the message queue related to QProcessMonitor.
        // A failure to do so may lead to a situation when another QProcess
        // started before these messages are processed but after our ends of
        // pipes are closed (for example, by QProcessPrivate::reset()) gets the
        // same pipe handles from the system. Since pipe handles are also used
        // as keys, the delayed messages will then refer to the new QProcess
        // object they were not intended for. The fix is to remove "our"
        // messages from the queue now, before closing pipes (which will usually
        // be done right after this method).
    
        QMSG qmsg;
        while (WinPeekMsg (0, &qmsg, hwnd(), WM_U_PIPE_RDATA, WM_U_PIPE_CLOSE,
                           PM_REMOVE)) {
            qDebug( "msg=%08x", qmsg.msg );
            if ( qmsg.msg == WM_U_PIPE_CLOSE ) {
                QProcess *proc = (QProcess *) PVOIDFROMMP( qmsg.mp1 );
                USHORT key = SHORT1FROMMP( qmsg.mp2 );
                if ( proc == pipeKeys.find( key ) && proc->d == d ) {
                    // skip the close message for ourselves as we will do all
                    // the necessary stuff
                    continue;
                }
            }
            WinDispatchMsg( 0, &qmsg );
        }
    }

    if ( pipe ) {
        removePipe( pipe );
    } else {
        if ( d->stdout.pipe != HP_NULL && d->stdout.key != KEY_NULL )
            removePipe( &d->stdout );
        if ( d->stderr.pipe != HP_NULL && d->stderr.key != KEY_NULL )
            removePipe( &d->stderr );
    }

    pipeStates.ensure( pipeKeys.count() );
    
    // stop the monitor thread if no more necessary
    if ( pipeKeys.count() == 0 ) {
        Q_ASSERT( threadRunning );
        if ( threadRunning ) {
            threadRunning = FALSE;
            DosPostEventSem( pipeSem );
        }
        lock.unlock();
        thread->wait();
    } else {
        lock.unlock();
    }
}

/** Monitor thread function */
void QProcessMonitor::monitor()
{
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcessMonitor::monitor() ENTER" );
#endif

    lock.lock();
    while( 1 ) {
        lock.unlock();
        APIRET rc = DosWaitEventSem( pipeSem, SEM_INDEFINITE_WAIT );
        lock.lock();
        if ( rc != NO_ERROR ) {
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
            qSystemWarning( "Failed to wait on event semaphore!", rc );
#endif            
            break;
        }

        ULONG posts = 0;
        DosResetEventSem( pipeSem, &posts );
#if defined(QT_QPROCESS_DEBUG)        
        qDebug( "QProcessMonitor::monitor(): got semaphore (posts=%ld)", posts );
#endif

        if ( !threadRunning )
            break;

        rc = DosQueryNPipeSemState( (HSEM) pipeSem, pipeStates.data(),
                                    pipeStates.dataSize() );
        if ( rc != NO_ERROR ) {
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
            qSystemWarning( "Failed to query pipe semaphore state!", rc );
#endif
            continue;
        }

        // In the returned information array, CLOSE and READ records for the
        // same pipe key may be mixed. We need CLOSE messages to be posted after
        // READ messages, so we do two passes.

        int pass = 0;
        for ( int i = 0; pass < 2; ++ i ) {
            if ( pipeStates[i].fStatus == NPSS_EOI ) {
                ++ pass;
                i = -1;
                continue;
            }
            if ( pass == 0 && pipeStates[i].fStatus != NPSS_RDATA )
                continue;
            if ( pass == 1 && pipeStates[i].fStatus != NPSS_CLOSE )
                continue;

#if defined(QT_QPROCESS_DEBUG)
            qDebug( " %d/%d: fStatus=%u fFlag=%02X usKey=%04hX usAvail=%hu",
                    pass, i, (uint) pipeStates[i].fStatus,
                    (uint) pipeStates[i].fFlag, pipeStates[i].usKey,
                    pipeStates[i].usAvail );
#endif
            QProcess *proc = pipeKeys.find( pipeStates[i].usKey );
            Q_ASSERT( proc );
            if ( !proc )
                continue;
#if defined(QT_QPROCESS_DEBUG)
            qDebug( "  proc=%p (%s/%s) d=%p",
                       proc, proc->name(), proc->className(), proc->d );
#endif
            QProcessPrivate::Pipe *pipe = proc->d->findPipe( pipeStates[i].usKey );
            Q_ASSERT( pipe );
            if ( !pipe )
                continue;

            if ( pipeStates[i].fStatus == NPSS_RDATA ) {
                bool wasPending = pipe->pending > 0;    
                pipe->pending = pipeStates[i].usAvail;
                // inform the GUI thread that there is new data
                if ( !wasPending )
                    WinPostMsg( hwnd(), WM_U_PIPE_RDATA, MPFROMP( proc ),
                                MPFROMSHORT( pipe->key ) );
            } else if ( pipeStates[i].fStatus == NPSS_CLOSE ) {
                // there may be repeated CLOSE records for the same pipe
                // in subsequent DosQueryNPipeSemState() calls
                if ( pipe->closed == FALSE ) {
                    pipe->closed = TRUE;
                    // inform the GUI thread that the client's pipe end
                    // was closed
                    WinPostMsg( hwnd(), WM_U_PIPE_CLOSE, MPFROMP( proc ),
                                MPFROMSHORT( pipe->key ) );
                }
            }
        }
    }
    lock.unlock();

#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcessMonitor::monitor() LEAVE" );
#endif            
}

MRESULT QProcessMonitor::message( ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch ( msg ) {
        case WM_U_PIPE_RDATA: {
            QProcess *proc = (QProcess *) PVOIDFROMMP( mp1 );
            USHORT key = SHORT1FROMMP( mp2 );
            // check parameter validity (we can safely do it outside the lock
            // because the GUI thread is the only that can modify pipeKeys)
            if ( proc == pipeKeys.find( key ) ) {
#if defined(QT_QPROCESS_DEBUG)
                qDebug( "QProcessMonitor::WM_U_PIPE_RDATA: proc=%p (%s/%s) d=%p "
                        "key=%04hX",
                        proc, proc->name(), proc->className(), proc->d, key );
#endif
                QProcessPrivate *d = proc->d;
                if ( d->stdout.key == key ) {
                    emit proc->readyReadStdout();
                } else {
                    Q_ASSERT( d->stderr.key == key );
                    emit proc->readyReadStderr();
                }
            }
#if defined(QT_QPROCESS_DEBUG)
            else {
                qDebug( "QProcessMonitor::WM_U_PIPE_RDATA: proc=%p (invalid)",
                        proc );
            }
#endif				
            break;
        }
        case WM_U_PIPE_CLOSE: {
            QProcess *proc = (QProcess *) PVOIDFROMMP( mp1 );
            USHORT key = SHORT1FROMMP( mp2 );
            // check parameter validity (we can safely do it outside the lock
            // because the GUI thread is the only that can modify pipeKeys)
            if ( proc == pipeKeys.find( key ) ) {
#if defined(QT_QPROCESS_DEBUG)
                qDebug( "QProcessMonitor::WM_U_PIPE_CLOSE: proc=%p (%s/%s) d=%p "
                        "key=%04hX",
                        proc, proc->name(), proc->className(), proc->d, key );
#endif
                QProcessPrivate *d = proc->d;
                QProcessPrivate::Pipe *pipe = d->findPipe( key );
                Q_ASSERT( pipe );
                if ( pipe ) {
                    Q_ASSERT( pipe->closed );
                    // remove the single pipe from watching
                    removeProcess( d, pipe, TRUE /* inMsgHandler */ ); 
                    // close the pipe since no more necessary
                    // (pipe is not watched anymore, no need to lock access)
                    if ( pipe->pending == 0 )
                        d->closePipe( pipe );
                }
            }
#if defined(QT_QPROCESS_DEBUG)
            else {
                qDebug( "QProcessMonitor::WM_U_PIPE_CLOSE: proc=%p (invalid)",
                        proc );
            }
#endif				
            break;
        }
        default:
            break;
    }
    
    return FALSE;
}

static QProcessMonitor *processMonitor = NULL;

void QProcessMonitor_cleanup()
{
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcessMonitor_cleanup()" );
#endif
    delete processMonitor;
    processMonitor = NULL;
}

/** Returns true if some data was successfully read */
bool QProcessPrivate::readPipe( Pipe *pipe )
{
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcessPrivate::readPipe(): this=%p key=%04hX", this, pipe->key );
#endif

    if ( pipe->pipe == HP_NULL )
        return false;

    // Pipe::pending and Pipe::closed need the lock
    QMutex *lock = NULL;
    
    // Notes:
    // 1. If procesMonitor is NULL, it which means we're somewhere inside
    //    the QApplication destruction procedure.
    // 2. If pipe->key is KEY_NULL, it means GUI thread has already processed
    //    the WM_U_PIPE_CLOSE message and we're free to access fields w/o a
    //    lock. pipe->key is assigned only on the GUI thread, so it's safe
    //    to check it here w/o the lock.

    if ( processMonitor && pipe->key != KEY_NULL )
        lock = &processMonitor->mlock();
    
    QByteArray *ba = NULL;
    bool close = FALSE;
    
    {
        QMutexLocker locker( lock );
        
        if ( pipe->pending == 0 )
            return false;
        
        ba = new QByteArray( pipe->pending );
            
        ULONG read = 0;
        APIRET rc = DosRead( pipe->pipe, ba->data(), pipe->pending, &read );
        if ( rc != NO_ERROR ) {
            delete ba;
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
            qSystemWarning( "Failed to read from the pipe!", rc ); 
#endif
            return false;
        }
    
        Q_ASSERT( read == pipe->pending );
        if ( read < pipe->pending )
            ba->resize( read );
        
        pipe->pending -= read;
        
        close = pipe->pending == 0 && pipe->closed;
    }
    
    if ( close ) {
        // the client's end of pipe has been closed, so close our end as well
        if ( processMonitor && pipe->key != KEY_NULL ) {
            // WM_U_PIPE_CLOSE has been posted but not yet processed
            // (the following call make sure it will be processed)
            processMonitor->removeProcess( this, pipe );
        }
        else
            closePipe( pipe );
    }
        
    pipe->buf.append( ba );
    return true;
}

/***********************************************************************
 *
 * QProcess
 *
 **********************************************************************/
 
void QProcess::init()
{
    d = new QProcessPrivate( this );
    exitStat = 0;
    exitNormal = FALSE;
    
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcess::init(): d=%p", d );
#endif
}

void QProcess::reset()
{
    // remove monitoring if the monitor is there (note that it's done before
    // resetting QProcessPrivate which has fields protected by the
    // QProcessMonitor lock) 
    if ( processMonitor )
        processMonitor->removeProcess( d );
    
    d->reset();
    exitStat = 0;
    exitNormal = FALSE;
}

QMembuf* QProcess::membufStdout()
{
    if( d->stdout.pipe != 0 )
        d->readPipe( &d->stdout );
    return &d->stdout.buf;
}

QMembuf* QProcess::membufStderr()
{
    if( d->stderr.pipe != 0 )
        d->readPipe( &d->stderr );
    return &d->stderr.buf;
}

QProcess::~QProcess()
{
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "~QProcess::QProcess(): d=%p", d );
#endif

    // remove monitoring if the monitor is there (note that it's done before
    // resetting QProcessPrivate which has fields protected by the
    // QProcessMonitor lock) 
    if ( processMonitor )
        processMonitor->removeProcess( d );

    delete d;
}

bool QProcess::start( QStringList *env )
{
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcess::start()" );
#endif

    reset();

    if ( _arguments.isEmpty() )
        return FALSE;

#if defined(QT_QPROCESS_USE_DOSEXECPGM)

    // construct the arguments for DosExecPgm()

    QCString appName, appArgs;
    {
        QString args;
        for ( QStringList::ConstIterator it = _arguments.begin();
              it != _arguments.end(); ++ it
        ) {
            if ( it == _arguments.begin() ) {
                appName = (*it).local8Bit();
            } else {
                QString s = *it;
                // quote the argument when it contains spaces
                if ( s.contains( ' ' ) )
                    s = '"' + s + '"';
                if ( args.isNull() )
                    args = s;
                else
                    args += ' ' + s;
            }
        }
        appArgs = args.local8Bit();

#if defined(QT_QPROCESS_DEBUG)
        qDebug( "QProcess::start(): app [%s], args [%s]",
                appName.data(), appArgs.data() );
#endif
    }

    QByteArray envlist;
    if ( env != 0 ) {
        uint pos = 0;
        // inherit PATH if missing (for convenience)
        // (note that BEGINLIBPATH and ENDLIBPATH, if any, are automatically
        // inherited, while LIBPATH is always a global setting)
        {
            const char *path = getenv( "PATH" );
            if ( env->grep( QRegExp( "^PATH=", FALSE ) ).empty() && path ) {
                uint sz = strlen( path ) + 5 /* PATH= */ + 1;
                envlist.resize( envlist.size() + sz );
                sprintf( envlist.data() + pos, "PATH=%s", path ); 
                pos += sz;
            }
        }
        // inherit COMSPEC if missing (to let the child start .cmd and .bat)
        {
            const char *comspec = getenv( "COMSPEC" );
            if ( env->grep( QRegExp( "^COMSPEC=", FALSE ) ).empty() && comspec ) {
                uint sz = strlen( comspec ) + 8 /* COMSPEC= */ + 1;
                envlist.resize( envlist.size() + sz );
                sprintf( envlist.data() + pos, "COMSPEC=%s", comspec ); 
                pos += sz;
            }
        }
        // add the user environment
        for ( QStringList::ConstIterator it = env->begin();
              it != env->end(); ++ it
        ) {
            QCString var = (*it).local8Bit();
            uint sz = var.length() + 1;
            envlist.resize( envlist.size() + sz );
            memcpy( envlist.data() + pos, var.data(), sz );
            pos += sz;
        }
        // add the terminating 0
        envlist.resize( envlist.size() + 1 );
        envlist[ pos ++ ] = 0;
    } else {
        // copy the entire environment (to let all variables added using
        // putenv() appear in the child process)
        uint sz = 1;
        for( const char * const *envp = environ; *envp != NULL; ++ envp )
            sz += strlen( *envp ) + 1;
        envlist.resize( sz );
        uint pos = 0;
        for( const char * const *envp = environ; *envp != NULL; ++ envp ) {
            sz = strlen( *envp ) + 1;
            memcpy( envlist.data() + pos, *envp, sz );
            pos += sz;
        }
        // add the terminating 0
        envlist[ pos ++ ] = 0;
    }
    
    // search for the app executable

    const uint appNameLen = appName.length();
    const uint appArgsLen = appArgs.length();
    
    bool hasPath = appName[ strcspn( appName.data(), "/\\" ) ] != '\0';
    bool hasSpaces = strchr( appName.data(), ' ' ) != NULL;
    
    // list of executable file extensions, in order of CMD.EXE's precedence
    // (the first runs directly, the rest requires CMD.EXE) 
    const char *exeExts[] = { ".exe", ".cmd", ".bat" };
    
    // detect which exe extension do we have, if any
    int ext = -1;
    if ( appNameLen >= 4 ) {
        for ( uint i = 0; ext < 0 && i < sizeof(exeExts) / sizeof(exeExts[0]); ++ i )
            if ( stricmp( appName.data() + appNameLen - 4, exeExts[i] ) == 0 )
                ext = i;
    }

    QByteArray buf;
    QCString appNameFull;
    
    APIRET rc = 0;
    char pathBuf[ CCHMAXPATH ];
    
    // run through all possible exe extension combinations (+ no extension case)
    for ( uint i = 0; i <= sizeof(exeExts) / sizeof(exeExts[ 0 ]); ++ i ) {
        if ( i == 0 ) {
            // try the unmodified app name first if it contains a path spec
            // or has one of predefined exe extensions 
            if ( hasPath || ext >= 0 ) {
                if ( buf.size() < appNameLen + 1 )
                    buf.resize ( appNameLen + 1 );
                strcpy( buf.data(), appName.data() );
            } else
                continue;
        } else {
            ext = i - 1;
            const uint extLen = strlen( exeExts[ ext ] );
            uint sz = appNameLen + extLen + 1;
            if ( buf.size() < sz )
                buf.resize( sz );
            strcpy( buf.data(), appName.data() );
            strcpy( buf.data() + appNameLen, exeExts[ ext ] );
        }

#if defined(QT_QPROCESS_DEBUG)
        qDebug( "QProcess::start(): trying to find [%s]", buf.data() );
#endif
        if ( hasPath ) {
            uint lastSep = strlen( buf.data() );
            while ( lastSep-- && buf[ lastSep ] != '/' && buf[ lastSep ] != '\\' )
                ;
            buf[ lastSep ] = '\0';
            rc = DosSearchPath( 0, buf.data(), buf.data() + lastSep + 1,
                                pathBuf, sizeof(pathBuf) );
        } else {
            rc = DosSearchPath( SEARCH_IGNORENETERRS | SEARCH_ENVIRONMENT |
                                SEARCH_CUR_DIRECTORY, "PATH",
                                buf.data(), pathBuf, sizeof(pathBuf) );
        }
        
        if ( rc == NO_ERROR ) {
#if defined(QT_QPROCESS_DEBUG)
            qDebug( "QProcess::start(): found [%s]", pathBuf ); 
#endif
            appNameFull = pathBuf;
            break;
        }

        if ( rc != ERROR_FILE_NOT_FOUND && rc != ERROR_PATH_NOT_FOUND ) {
#if defined(QT_QPROCESS_DEBUG)
            qDebug( "QProcess::start(): found [%s], but got an error:", pathBuf );
            qSystemWarning( "", rc );
#endif
            break;
        }
    }
    
    if ( appNameFull.isNull() ) {
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
#if defined(QT_DEBUG)
        qWarning( "[%s]:", appName.data() ); 
#endif
        qSystemWarning( "Failed to find the executable!", rc );
#endif
        return FALSE;
    }

    // final preparation of arguments
    
    if ( ext <= 0 ) {
        // run directly (args = <appName>\0\<appArgs>\0\0)
        uint sz = appNameLen + 1 + appArgsLen + 2;
        if ( buf.size() < sz )
             buf.resize( sz );
        strcpy( buf.data(), appName.data() );
        strcpy( buf.data() + appNameLen + 1, appArgs.data() );
        buf[ sz - 1 ] = '\0';
    } else {
        // run via shell (args = <shell>\0"/c "<appName>" "<appArgs>\0\0)
        appNameFull = getenv( "COMSPEC" );
        if ( appNameFull.isNull() ) {
            ULONG bootDrv = 0;
            rc = DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                             &bootDrv, sizeof(bootDrv) );
            if ( rc == NO_ERROR && bootDrv >= 1 ) {
                appNameFull = "?:\\OS2\\CMD.EXE";
                appNameFull[ 0 ] = char( bootDrv ) + 'A' - 1;
            }
        }
        if ( appNameFull.isNull() ) {
#if defined(QT_QPROCESS_DEBUG)
            qDebug( "QProcess::start(): OS/2 command line interpreter is not found!" );
#endif        
            return FALSE;
        }
        const uint shellLen = strlen( appNameFull );
        uint sz = shellLen + 1 + 3 + appNameLen + 1 + appArgsLen + 2;
        if ( hasSpaces )
            sz += 2; // for quotes
        if ( !appArgsLen )
            sz -= 1; // no need for space
        if ( buf.size() < sz )
             buf.resize( sz );
        strcpy( buf.data(), appNameFull );
        char *argsPtr = buf.data() + shellLen + 1;
        sprintf( argsPtr, hasSpaces ? "/c \"%s\"" : "/c %s", appName.data() );
        if ( appArgsLen ) {
			strcat( argsPtr, " " );
            strcat( argsPtr, appArgs.data() );
        }
        buf[ sz - 1 ] = '\0';
    }

#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcess::start(): will start [%s] as [%s] [%s]",
            appNameFull.data(), buf.data(), buf.data() + strlen( buf.data() ) + 1 ); 
#endif

#else // QT_QPROCESS_USE_DOSEXECPGM

    // construct the arguments for spawnvpe()

    QCString appName;
    QStrList appArgs;

    for ( QStringList::ConstIterator it = _arguments.begin();
          it != _arguments.end(); ++ it
    ) {
        if ( it == _arguments.begin() ) {
            appName = QFile::encodeName( *it );
        } else {
            appArgs.append( QFile::encodeName( *it ) );
        }
    }

#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcess::start(): [%s]",
            QFile::encodeName( _arguments.join( "] [" ) ).data() );
#endif

    // prepare the environment

    QStrList envList;
    if ( env != 0 ) {
        // add the user environment
        bool seenPATH = FALSE;
        bool seenCOMSPEC = FALSE;
        for ( QStringList::ConstIterator it = env->begin();
              it != env->end(); ++ it
        ) {
            envList.append( QFile::encodeName( *it ) );
            if ( !seenPATH )
                seenPATH = !strncmp( envList.current(), "PATH=", 4 );
            if ( !seenCOMSPEC )
                seenCOMSPEC = !strncmp( envList.current(), "COMSPEC=", 8 );
        }
        if ( !seenPATH ) {
            // inherit PATH if missing (for convenience)
            // (note that BEGINLIBPATH and ENDLIBPATH, if any, are automatically
            // inherited, while LIBPATH is always a global setting)
            const char *path = getenv( "PATH" );
            QCString str( 5 /* PATH= */ + strlen( path ) + 1 /* \0 */ );
            strcpy( str.data(), "PATH=" );
            strcat( str.data() + 5, path );
            envList.append( str );
        }
        // inherit COMSPEC if missing (to let the child start .cmd and .bat)
        if ( !seenCOMSPEC ) {
            const char *comspec = getenv( "COMSPEC" );
            QCString str( 8 /* COMSPEC= */ + strlen( comspec ) + 1 /* \0 */ );
            strcpy( str.data(), "COMSPEC=" );
            strcat( str.data() + 5, comspec );
            envList.append( str );
        }
    }

    APIRET rc = 0;
    char pathBuf[ CCHMAXPATH ];

#endif // QT_QPROCESS_USE_DOSEXECPGM

    // create STDIN/OUT/ERR redirection pipes
    
    if ( comms & (Stdout | Stderr) ) {
        // lazily create the process monitor
        if ( !processMonitor ) {
            processMonitor = new QProcessMonitor();
            Q_ASSERT( processMonitor );
            if ( !processMonitor )
                return FALSE;
            if ( !processMonitor->isOk() ) {
                QProcessMonitor_cleanup();
                return FALSE;
            }
            qAddPostRoutine( QProcessMonitor_cleanup ); 
        }
    }
    
    HFILE tmpStdin = HF_NULL, tmpStdout = HF_NULL, tmpStderr = HF_NULL;
    HFILE realStdin = HF_STDIN, realStdout = HF_STDOUT, realStderr = HF_STDERR;

    // we need the process identifier to guarantee pipe name unicity
    PPIB ppib = NULL;
    DosGetInfoBlocks( NULL, &ppib );

    // use the custom Qt message handler to print errors/wranings
    // to the console when the real STDERR handle is redirected
    InstallQtMsgHandler();

    if ( comms & Stdin ) {
        HFILE clientStdin = HF_NULL;
        do {
            // create a Stdin pipe
            sprintf( pathBuf, "\\pipe\\Qt\\%08lX\\QProcess\\%p\\Stdin",
                     ppib->pib_ulpid, this );
            rc = DosCreateNPipe( pathBuf, &d->pipeStdin,
                                 NP_ACCESS_OUTBOUND, NP_NOWAIT | NP_TYPE_BYTE | 1,
                                 PIPE_SIZE_STDIN, 0, 0 );
            if ( rc != NO_ERROR )
                break;
            DosConnectNPipe( d->pipeStdin );
            // open a client end of the Stdout pipe
            ULONG action = 0;
            rc = DosOpen( pathBuf, &clientStdin, &action, 0, FILE_NORMAL, FILE_OPEN,
                          OPEN_ACCESS_READONLY | OPEN_SHARE_DENYREADWRITE | OPEN_FLAGS_NOINHERIT,
                          (PEAOP2) NULL);
            if ( rc != NO_ERROR )
                break;
            // save the real STDIN handle
            if ( (rc = DosDupHandle( HF_STDIN, &tmpStdin)) != NO_ERROR )
                break;
            // redirect the real STDIN handle to the client end of the pipe
            if ( (rc = DosDupHandle( clientStdin, &realStdin)) != NO_ERROR )
                break;
        } while( 0 );
        // close the client end anyway (no more necessary)
        if ( clientStdin != HF_NULL )
            DosClose ( clientStdin );
        // close all opened handles on error
        if ( rc  != NO_ERROR ) {
#if defined(QT_QPROCESS_DEBUG)
            qDebug( "Failed to create a STDIN redirection (%s): SYS%04ld",
                    pathBuf, rc ); 
#endif
            if ( tmpStdin != HF_NULL )
                DosClose ( tmpStdin );
            d->closeHandles();
            UninstallQtMsgHandler();
            return FALSE;
        }
    }
    if ( comms & Stdout ) {
        HFILE clientStdout = HF_NULL;
        do {
            // create a Stdout pipe
            sprintf( pathBuf, "\\pipe\\Qt\\%08lX\\QProcess\\%p\\Stdout",
                     ppib->pib_ulpid, this );
            rc = DosCreateNPipe( pathBuf, &d->stdout.pipe,
                                 NP_ACCESS_INBOUND, NP_NOWAIT | NP_TYPE_BYTE | 1,
                                 0, PIPE_SIZE_STDOUT, 0 );
            if ( rc != NO_ERROR )
                break;
            DosConnectNPipe( d->stdout.pipe );
            // open a client end of the Stdout pipe
            ULONG action = 0;
            rc = DosOpen( pathBuf, &clientStdout, &action, 0, FILE_NORMAL, FILE_OPEN,
                          OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYREADWRITE | OPEN_FLAGS_NOINHERIT,
                          (PEAOP2) NULL);
            if ( rc != NO_ERROR )
                break;
            // save the real STDOUT handle
            if ( (rc = DosDupHandle( HF_STDOUT, &tmpStdout )) != NO_ERROR )
                break;
            // redirect the real STDOUT handle to the client end of the pipe
            if ( (rc = DosDupHandle( clientStdout, &realStdout )) != NO_ERROR )
                break;
        } while( 0 );
        // close the client end anyway (no more necessary)
        if ( clientStdout != HF_NULL )
            DosClose ( clientStdout );
        // close all opened handles on error
        if ( rc != NO_ERROR ) {
#if defined(QT_QPROCESS_DEBUG)
            qDebug( "Failed to create a STDOUT redirection (%s): SYS%04ld", 
                    pathBuf, rc ); 
#endif
            if ( tmpStdin != HF_NULL ) {
                DosDupHandle( tmpStdin, &realStdin );
                DosClose( tmpStdin );
            }
            if ( tmpStdout != HF_NULL )
                DosClose ( tmpStdout );
            d->closeHandles();
            UninstallQtMsgHandler();
            return FALSE;
        }
    }
    if ( (comms & Stderr) && !(comms & DupStderr) ) {
        HFILE clientStderr = HF_NULL;
        do {
            // create a Stderr pipe
            sprintf( pathBuf, "\\pipe\\Qt\\%08lX\\QProcess\\%p\\Stderr",
                     ppib->pib_ulpid, this );
            rc = DosCreateNPipe( pathBuf, &d->stderr.pipe,
                                 NP_ACCESS_INBOUND, NP_NOWAIT | NP_TYPE_BYTE | 1,
                                 0, PIPE_SIZE_STDERR, 0 );
            if ( rc != NO_ERROR )
                break;
            DosConnectNPipe( d->stderr.pipe );
            // open a client end of the Stderr pipe
            ULONG action = 0;
            rc = DosOpen( pathBuf, &clientStderr, &action, 0, FILE_NORMAL, FILE_OPEN,
                          OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYREADWRITE | OPEN_FLAGS_NOINHERIT,
                          (PEAOP2) NULL);
            if ( rc != NO_ERROR )
                break;
            // save the real STDERR handle
            if ( (rc = DosDupHandle( HF_STDERR, &tmpStderr )) != NO_ERROR )
                break;
            // redirect the real STDERR handle to the client end of the pipe
            if ( (rc = DosDupHandle( clientStderr, &realStderr )) != NO_ERROR )
                break;
        } while( 0 );
        // close the client end anyway (no more necessary)
        if ( clientStderr != HF_NULL )
            DosClose ( clientStderr );
        // close all opened handles on error
        if ( rc != NO_ERROR ) {
#if defined(QT_QPROCESS_DEBUG)
            qDebug( "Failed to create a STDERR redirection (%s): SYS%04ld", 
                    pathBuf, rc ); 
#endif
            if ( tmpStdin != HF_NULL ) {
                DosDupHandle( tmpStdin, &realStdin );
                DosClose( tmpStdin );
            }
            if ( tmpStdout != HF_NULL ) {
                DosDupHandle( tmpStdout, &realStdout );
                DosClose( tmpStdout );
            }
            if ( tmpStderr != HF_NULL )
                DosClose ( tmpStderr );
            d->closeHandles();
            UninstallQtMsgHandler();
            return FALSE;
        }
    }
    if ( comms & DupStderr ) {
        // leave d->stderr.pipe equal to HP_NULL
        do {
            // save the real STDERR handle
            if ( (rc = DosDupHandle( HF_STDERR, &tmpStderr )) != NO_ERROR )
                break;
            // redirect STDERR to STDOUT
            if ( (rc = DosDupHandle( HF_STDOUT, &realStderr )) != NO_ERROR )
                break;
        } while( 0 );
        // close all opened handles on error
        if ( rc != NO_ERROR ) {
#if defined(QT_QPROCESS_DEBUG)
            qDebug( "Failed to redirect STDERR to STDOUT: SYS%04ld", 
                    rc ); 
#endif
            if ( tmpStdin != HF_NULL ) {
                DosDupHandle( tmpStdin, &realStdin );
                DosClose( tmpStdin );
            }
            if ( tmpStdout != HF_NULL ) {
                DosDupHandle( tmpStdout, &realStdout );
                DosClose( tmpStdout );
            }
            if ( tmpStderr != HF_NULL )
                DosClose ( tmpStderr );
            d->closeHandles();
            UninstallQtMsgHandler();
            return FALSE;
        }
    }

    // add this process to the monitor
    if ( !processMonitor->addProcess( this ) ) {
        if ( tmpStdin != HF_NULL ) {
            DosDupHandle( tmpStdin, &realStdin );
            DosClose( tmpStdin );
        }
        if ( tmpStdout != HF_NULL ) {
            DosDupHandle( tmpStdout, &realStdout );
            DosClose( tmpStdout );
        }
        if ( tmpStderr != HF_NULL ) {
            DosDupHandle( tmpStderr, &realStderr );
            DosClose ( tmpStderr );
        }
        d->closeHandles();
        UninstallQtMsgHandler();
        return FALSE;
    }
    
    // set the working directory
    QString curDir = QDir::currentDirPath();
    QDir::setCurrent( workingDir.path() );
#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcess::start(): curDir='%s', workingDir='%s'",
            curDir.local8Bit().data(),
            QDir::currentDirPath().local8Bit().data() ); 
#endif
    
#if defined(QT_QPROCESS_USE_DOSEXECPGM)

    // DosExecPgm()
    
    RESULTCODES resc = { 0 };
        
    rc = DosExecPgm( pathBuf, sizeof(pathBuf), EXEC_ASYNCRESULT,
                     buf.data(), envlist.data(), &resc, appNameFull.data() );

#else // defined(QT_QPROCESS_USE_DOSEXECPGM)
    
    // start the application
    
    int i = 0;

    char **argv = new char *[ appArgs.count() + 2 ];
    argv[ i++ ] = appName.data();
    for ( appArgs.first(); appArgs.current(); appArgs.next() )
        argv[ i++ ] = appArgs.current();
    argv[ i ] = NULL;

    char **envv = NULL;
    if ( envList.count() ) {
        i = 0;
        envv = new char *[ envList.count() + 1 ];
        for ( envList.first(); envList.current(); envList.next() )
            envv[ i++ ] = envList.current();
        envv[ i ] = NULL;
    } else {
        envv = environ;
    }

    int pid = spawnvpe( P_NOWAIT | P_DEFAULT, appName.data(), argv, envv );
    
    // if spawnvpe() couldn't find the executable, try .CMD and .BAT
    // extensions (in order of CMD.EXE precedence); spawnvpe() knows how to
    // locate and run them (i.e. using CMD.EXE /c)
    if ( pid == -1 && errno == ENOENT ) {
        appName += ".cmd";
        pid = spawnvpe( P_NOWAIT | P_DEFAULT, appName.data(), argv, envv );
        if ( pid == -1 && errno == ENOENT ) {
            strcpy( appName.data() + appName.length() - 4, ".bat" );
            pid = spawnvpe( P_NOWAIT | P_DEFAULT, appName.data(), argv, envv );
        }
    }
    
    if ( envv != environ )
        delete[] envv;
    delete[] argv;
    
#endif // defined(QT_QPROCESS_USE_DOSEXECPGM)
    
    // restore the current directory
    QDir::setCurrent( curDir );
    
    // cancel STDIN/OUT/ERR redirections
    if ( tmpStdin != HF_NULL ) {
        DosDupHandle( tmpStdin, &realStdin );
        DosClose( tmpStdin );
    }
    if ( tmpStdout != HF_NULL ) {
        DosDupHandle( tmpStdout, &realStdout );
        DosClose( tmpStdout );
    }
    if ( tmpStderr != HF_NULL ) {
        DosDupHandle( tmpStderr, &realStderr );
        DosClose( tmpStderr );
    }
    
    UninstallQtMsgHandler();
    
#if defined(QT_QPROCESS_USE_DOSEXECPGM)

    if ( rc != NO_ERROR ) {
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
#if defined(QT_DEBUG)
        qWarning( "[%s] [%s]\n(%s):",
                  buf.data(), buf.data() + strlen( buf.data() ) + 1,
                  pathBuf ); 
#endif
        qSystemWarning( "Failed to start a new process!", rc );
#endif
        processMonitor->removeProcess( d );
        d->closeHandles();
        return FALSE;
    }
    
    // memporize PID of the started process
    d->pid = resc.codeTerminate;
    
#else // defined(QT_QPROCESS_USE_DOSEXECPGM)

    if ( pid == -1 ) {
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
        qWarning( "Failed to start a new process [%s]\n"
                  "errno=%d (%s)",
                  QFile::encodeName( _arguments.join( "] [" ) ).data(),
                  errno, strerror( errno ) );
#endif
        processMonitor->removeProcess( d );
        d->closeHandles();
        return FALSE;
    }

    // memporize PID of the started process
    d->pid = pid;

#endif // defined(QT_QPROCESS_USE_DOSEXECPGM)

#if defined(QT_QPROCESS_DEBUG)
    qDebug( "QProcess::start(): started PID %lu (0x%08lX)", d->pid, d->pid ); 
#endif
    
    // timer is not necessary for ioRedirection (we use the monitor thread)
    if ( /* ioRedirection || */ notifyOnExit ) {
	d->lookup->start( POLL_TIMER );
    }

    return TRUE;
}

void QProcess::tryTerminate() const
{
    if ( d->pid == PID_NULL )
        return;

    HSWITCH hswitch = WinQuerySwitchHandle( NULL, d->pid );
    if ( hswitch != NULLHANDLE ) {
        SWCNTRL swcntrl = { 0 };
        APIRET rc = WinQuerySwitchEntry( hswitch,  &swcntrl );
        // WinQuerySwitchEntry will return a switch entry of the parent
        // process if the specfied one doesn't have a separate session
        // (running a plain CMD.EXE is an example); ignore this case. 
        if ( rc == NO_ERROR && swcntrl.idProcess == d->pid )
        {
            // first, ensure that the Close action is enabled in the main frame
            // window (otherwise WM_SYSCOMMAND/SC_CLOSE will be ignored)
            HWND hwndSysMenu = WinWindowFromID( swcntrl.hwnd, FID_SYSMENU );
            if (hwndSysMenu) {
                WinPostMsg( hwndSysMenu, MM_SETITEMATTR,
                            MPFROM2SHORT( SC_CLOSE, TRUE ),
                            MPFROM2SHORT( MIA_DISABLED, 0 ) );            
            }
            WinPostMsg( swcntrl.hwnd, WM_SYSCOMMAND,
                        MPFROM2SHORT( SC_CLOSE, CMDSRC_OTHER ),
                        MPFROMLONG( FALSE ) );
        }
    }
}

void QProcess::kill() const
{
    if ( d->pid != PID_NULL )
	DosKillProcess( DKP_PROCESS, d->pid );
}

bool QProcess::isRunning() const
{
    if ( d->pid == PID_NULL )
	return FALSE;

#if defined(QT_QPROCESS_USE_DOSEXECPGM)
    
    PID pidEnded = PID_NULL;
    RESULTCODES resc = { 0 };
    APIRET rc = DosWaitChild( DCWA_PROCESS, DCWW_NOWAIT, &resc,
                              &pidEnded, d->pid );        
    
    if ( rc == ERROR_CHILD_NOT_COMPLETE )
        return TRUE;

#else // defined(QT_QPROCESS_USE_DOSEXECPGM)

    int stat = 0;
    int pid = waitpid( d->pid, &stat, WNOHANG );
    if ( (pid == 0) ||
         ((PID) pid == d->pid &&
          !WIFEXITED( stat ) && !WIFSIGNALED( stat ) && !WIFSTOPPED( stat ) ) )
        return TRUE;

    if ( pid == -1 ) {
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
        qWarning( "Failed to wait for a child process\n"
                  "errno=%d (%s)", errno, strerror( errno ) );
#endif
    }

#endif // defined(QT_QPROCESS_USE_DOSEXECPGM)
    
    QProcess *that = (QProcess *) this;

    /// @todo the below comment is not relevant any more since removeProcess()
    //  will dispatch all related messages including WM_U_PIPE_RDATA. Therefore,
    //  this code should not be needed anymore.
#if 0
    // There might be data to read, but WM_U_PIPE_RDATA messages won't be
    // converted to signals after removeProcess() is called below. Therefore,
    // emit signals ourselved if necessary.

    if ( d->readPipe( &d->stdout ) )
        emit that->readyReadStdout();
    if ( d->readPipe( &d->stderr ) )
        emit that->readyReadStderr();
#endif
    
    // compute the exit values
    if ( !d->exitValuesCalculated ) {
#if defined(QT_QPROCESS_USE_DOSEXECPGM)
        that->exitNormal = resc.codeTerminate == TC_EXIT;
        that->exitStat = resc.codeResult;
#else // defined(QT_QPROCESS_USE_DOSEXECPGM)
        that->exitNormal = pid != -1 && WIFEXITED( stat );
        that->exitStat = WEXITSTATUS( stat );
#endif // defined(QT_QPROCESS_USE_DOSEXECPGM)
        d->exitValuesCalculated = TRUE;
    }

    processMonitor->removeProcess( d );
    
    d->pid = PID_NULL;
    d->closeHandles();
    return FALSE;
}

bool QProcess::canReadLineStdout() const
{
    if( d->stdout.pipe == HP_NULL )
	return d->stdout.buf.size() != 0;

    QProcess *that = (QProcess *) this;
    return that->membufStdout()->scanNewline( 0 );
}

bool QProcess::canReadLineStderr() const
{
    if( d->stderr.pipe == HP_NULL )
	return d->stderr.buf.size() != 0;

    QProcess *that = (QProcess *) this;
    return that->membufStderr()->scanNewline( 0 );
}

void QProcess::writeToStdin( const QByteArray& buf )
{
    d->stdinBuf.enqueue( new QByteArray(buf) );
    socketWrite( 0 );
}

void QProcess::closeStdin( )
{
    if ( d->pipeStdin != HP_NULL ) {
        DosDisConnectNPipe( d->pipeStdin );
        DosClose( d->pipeStdin );
        d->pipeStdin = HP_NULL;
    }
}

/* dummy, since MOC doesn't understand preprocessor defines */
void QProcess::socketRead( int fd ) 
{
}

void QProcess::socketWrite( int )
{
    if ( d->pipeStdin == HP_NULL )
        return;
    
    while ( !d->stdinBuf.isEmpty() && isRunning() ) {
        ULONG written = 0;
        APIRET rc = DosWrite( d->pipeStdin,
                              d->stdinBuf.head()->data() + d->stdinBufRead,
                              QMIN( PIPE_SIZE_STDIN,
                                    d->stdinBuf.head()->size() - d->stdinBufRead ),
                              &written );
        if ( rc != NO_ERROR ) {
#if defined(QT_CHECK_STATE) || defined(QT_QPROCESS_DEBUG)
            qSystemWarning( "Failed to write to the pipe!", rc ); 
#endif
	    d->lookup->start( POLL_TIMER );
	    return;
        }
        
	d->stdinBufRead += written;
	if ( d->stdinBufRead == d->stdinBuf.head()->size() ) {
	    d->stdinBufRead = 0;
	    delete d->stdinBuf.dequeue();
	    if ( wroteToStdinConnected && d->stdinBuf.isEmpty() )
		emit wroteToStdin();
	}
    }
}

void QProcess::flushStdin()
{
    socketWrite( 0 );
}

/*
  Use a timer for polling misc. stuff.
*/
void QProcess::timeout()
{
    // Disable the timer temporary since one of the slots that are connected to
    // the readyRead...(), etc. signals might trigger recursion if
    // processEvents() is called.
    d->lookup->stop();

    // try to write pending data to stdin
    if ( !d->stdinBuf.isEmpty() )
	socketWrite( 0 );

    if ( isRunning() ) {
	// enable timer again, if needed
        // timer is not necessary for ioRedirection (we use the monitor thread)
	if ( !d->stdinBuf.isEmpty() || /* ioRedirection || */ notifyOnExit )
	    d->lookup->start( POLL_TIMER );
    } else if ( notifyOnExit ) {
	emit processExited();
    }
}

/*
  Used by connectNotify() and disconnectNotify() to change the value of
  ioRedirection (and related behaviour)
*/
void QProcess::setIoRedirection( bool value )
{
    ioRedirection = value;
    // timer is not necessary for ioRedirection (we use the monitor thread)
#if 0
    if ( !ioRedirection && !notifyOnExit )
	d->lookup->stop();
    if ( ioRedirection ) {
	if ( isRunning() )
	    d->lookup->start( POLL_TIMER );
    }
#endif
}

/*
  Used by connectNotify() and disconnectNotify() to change the value of
  notifyOnExit (and related behaviour)
*/
void QProcess::setNotifyOnExit( bool value )
{
    notifyOnExit = value;
    // timer is not necessary for ioRedirection (we use the monitor thread)
    if ( /* !ioRedirection && */ !notifyOnExit )
	d->lookup->stop();
    if ( notifyOnExit ) {
	if ( isRunning() )
	    d->lookup->start( POLL_TIMER );
    }
}

/*
  Used by connectNotify() and disconnectNotify() to change the value of
  wroteToStdinConnected (and related behaviour)
*/
void QProcess::setWroteStdinConnected( bool value )
{
    wroteToStdinConnected = value;
}

QProcess::PID QProcess::processIdentifier()
{
    return d->pid;
}

#endif // QT_NO_PROCESS
