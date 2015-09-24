/****************************************************************************
** $Id: qthread_pm.cpp 139 2006-10-20 21:44:52Z dmik $
**
** QThread class for OS/2
**
** Copyright (C) 1992-2002 Trolltech AS.  All rights reserved.
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

#if defined(QT_THREAD_SUPPORT)

#include "qt_os2.h"

#include "qthread.h"
#include <private/qthreadinstance_p.h>
#include <private/qmutexpool_p.h>
#include "qthreadstorage.h"

#include <private/qcriticalsection_p.h>

#include <stdlib.h>

static QThreadInstance main_instance = {
    0, { 0, &main_instance }, 0, 0, 1, 0, 0, 0
};


static QMutexPool *qt_thread_mutexpool = 0;

#define LocalStorage (*_threadstore())

enum
{
    // OS/2 doesn't support specifying a maximum time to wait for a thread
    // to end other than infininty, so we use a loop containing of wait
    // intervals until the thread has ended or the maximum time specified
    // by the caller has expired (in ms)
    THREAD_WAIT_INTERVAL = 1000,
    // default thread stack size (in bytes)
    THREAD_DEF_STACK_SIZE = 0x200000, // 2 MB
};

/**************************************************************************
 ** QThreadInstance
 *************************************************************************/

QThreadInstance *QThreadInstance::current()
{
    QThreadInstance *ret = (QThreadInstance *) LocalStorage;
    if ( ! ret ) return &main_instance;
    return ret;
}

void QThreadInstance::init(unsigned int stackSize)
{
    stacksize = stackSize;
    args[0] = args[1] = 0;
    thread_storage = 0;
    finished = FALSE;
    running = FALSE;
    orphan = FALSE;

    thread_id = 0;
    waiters = 0;

    // threads have not been initialized yet, do it now
    if ( ! qt_thread_mutexpool ) QThread::initialize();
}

void QThreadInstance::deinit()
{
}

void _System QThreadInstance::start( void * _arg )
{
    void **arg = (void **) _arg;

    LocalStorage = arg [1];

#if !defined(QT_OS2_NO_SYSEXCEPTIONS)
    // don't set the exception handler if not able to do it for the main thread
    bool setException = QtOS2SysXcptMainHandler::installed;
    EXCEPTIONREGISTRATIONRECORD excRegRec =
        { 0, QtOS2SysXcptMainHandler::handler };
    if ( setException )
        DosSetExceptionHandler( &excRegRec );
#endif

    ( (QThread *) arg[0] )->run();

    finish( (QThreadInstance *) arg[1] );

#if !defined(QT_OS2_NO_SYSEXCEPTIONS)
    if ( setException )
        DosUnsetExceptionHandler( &excRegRec );
#endif
    
    return;
}

void QThreadInstance::finish( QThreadInstance *d )
{
    if ( ! d ) {
#ifdef QT_CHECK_STATE
	qWarning( "QThread: internal error: zero data for running thread." );
#endif // QT_CHECK_STATE
	return;
    }

    QMutexLocker locker( d->mutex() );
    d->running = FALSE;
    d->finished = TRUE;
    d->args[0] = d->args[1] = 0;

    QThreadStorageData::finish( d->thread_storage );
    d->thread_storage = 0;
    d->thread_id = 0;

    if ( d->orphan ) {
        d->deinit();
	delete d;
    }
}

QMutex *QThreadInstance::mutex() const
{
    return qt_thread_mutexpool ? qt_thread_mutexpool->get( (void *) this ) : 0;
}

void QThreadInstance::terminate()
{
    PTIB ptib;
    DosGetInfoBlocks( &ptib, NULL );
    if ( ptib->tib_ptib2->tib2_ultid == thread_id ) {
#ifdef QT_CHECK_STATE
	qWarning( "QThread: internal error: thread cannot terminate itself." );
#endif // QT_CHECK_STATE
        return;
    }

    /*
      delete the thread storage *after* the thread has been
      terminated.  we could end up deleting the thread's data while it
      is accessing it (but before the thread stops), which results in
      a crash.
    */
    void **storage = thread_storage;
    thread_storage = 0;

    running = FALSE;
    finished = TRUE;
    args[0] = args[1] = 0;

    if ( orphan ) {
        deinit();
	delete this;
    }

    DosKillThread( thread_id );
    thread_id = 0;

    QThreadStorageData::finish( storage );
}


/**************************************************************************
 ** QThread
 *************************************************************************/

Qt::HANDLE QThread::currentThread()
{
    PTIB ptib;
    DosGetInfoBlocks( &ptib, NULL );
    return (Qt::HANDLE) ptib->tib_ptib2->tib2_ultid;
}

void QThread::initialize()
{
    if ( ! qt_global_mutexpool )
	qt_global_mutexpool = new QMutexPool( TRUE, 73 );
    if ( ! qt_thread_mutexpool )
	qt_thread_mutexpool = new QMutexPool( FALSE, 127 );
}

void QThread::cleanup()
{
    delete qt_global_mutexpool;
    delete qt_thread_mutexpool;
    qt_global_mutexpool = 0;
    qt_thread_mutexpool = 0;

    QThreadInstance::finish(&main_instance);
}

void QThread::sleep( unsigned long secs )
{
    DosSleep( secs * 1000 );
}

void QThread::msleep( unsigned long msecs )
{
    DosSleep( msecs );
}

void QThread::usleep( unsigned long usecs )
{
    DosSleep( ( usecs / 1000 ) + 1 );
}


void QThread::start(Priority priority)
{
    QMutexLocker locker( d->mutex() );

    if ( d->running && !d->finished ) {
#ifdef QT_CHECK_RANGE
	qWarning( "Thread is already running" );
#endif
	wait();
    }

    d->running = TRUE;
    d->finished = FALSE;
    d->args[0] = this;
    d->args[1] = d;

    /*
      NOTE: we create the thread in the suspended state, set the
      priority and then resume the thread.

      since threads are created with normal priority by default, we
      could get into a case where a thread (with priority less than
      NormalPriority) tries to create a new thread (also with priority
      less than NormalPriority), but the newly created thread preempts
      its 'parent' and runs at normal priority.
    */
    int stacksize = d->stacksize ? d->stacksize : THREAD_DEF_STACK_SIZE;
    int tid = _beginthread( QThreadInstance::start, NULL, stacksize, d->args );
    d->thread_id = tid != -1 ? (TID) tid : 0;

    if ( !d->thread_id ) {
#ifdef QT_CHECK_STATE
	qWarning( "Failed to create thread\n\tError code %d - %s", errno, strerror( errno ) );
#endif
	d->running = FALSE;
	d->finished = TRUE;
	d->args[0] = d->args[1] = 0;
	return;
    }

    ULONG prioClass = 0;
    LONG prioDelta = 0;
    switch (priority) {
    case IdlePriority:
	prioClass = PRTYC_IDLETIME;
        prioDelta = PRTYD_MINIMUM;
	break;

    case LowestPriority:
	prioClass = PRTYC_IDLETIME;
	break;

    case LowPriority:
	prioClass = PRTYC_IDLETIME;
        prioDelta = PRTYD_MAXIMUM;
	break;

    case NormalPriority:
	prioClass = PRTYC_REGULAR;
	break;

    case HighPriority:
	prioClass = PRTYC_REGULAR;
        prioDelta = PRTYD_MAXIMUM;
	break;

    case HighestPriority:
	prioClass = PRTYC_TIMECRITICAL;
	break;

    case TimeCriticalPriority:
	prioClass = PRTYC_TIMECRITICAL;
        prioDelta = PRTYD_MAXIMUM;
	break;

    case InheritPriority:
    default:
        PTIB ptib;
        DosGetInfoBlocks( &ptib, NULL );
        prioClass = (ptib->tib_ptib2->tib2_ulpri >> 8) & 0xFF;
        prioDelta = (ptib->tib_ptib2->tib2_ulpri) & 0xFF;
	break;
    }

    APIRET rc = DosSetPriority( PRTYS_THREAD, prioClass, prioDelta, d->thread_id );
    if ( rc ) {
#ifdef QT_CHECK_STATE
	qSystemWarning( "Failed to set thread priority", rc );
#endif
    }
}

void QThread::start()
{
    start(InheritPriority);
}

bool QThread::wait( unsigned long time )
{
    PTIB ptib;
    DosGetInfoBlocks( &ptib, NULL );

    QMutexLocker locker(d->mutex());

    if ( d->thread_id == ptib->tib_ptib2->tib2_ultid ) {
#ifdef QT_CHECK_RANGE
	qWarning( "Thread tried to wait on itself" );
#endif
	return FALSE;
    }
    if ( d->finished || !d->running )
	return TRUE;

    ++d->waiters;
    locker.mutex()->unlock();

    bool ret = TRUE;
    TID tid = d->thread_id;
    APIRET rc = 0;
    if (time == ULONG_MAX ) {
        rc = DosWaitThread( &tid, DCWW_WAIT );
    } else {
        ULONG sleep;
        do {
            sleep = time > THREAD_WAIT_INTERVAL ? THREAD_WAIT_INTERVAL : time;
            DosSleep( sleep );
            rc = DosWaitThread( &tid, DCWW_NOWAIT );
            if ( rc != ERROR_THREAD_NOT_TERMINATED )
                break;
            if ( d->finished || !d->running ) {
                rc = 0;
                break;
            }
            time -= sleep;
        } while ( time );
    }
    if ( rc && rc != ERROR_INVALID_THREADID ) {
#ifdef QT_CHECK_RANGE
	qSystemWarning( "Thread wait failure", rc );
#endif
        ret = FALSE;
    }

    locker.mutex()->lock();
    --d->waiters;

    return ret;
}

void QThread::exit()
{
    QThreadInstance *d = QThreadInstance::current();

    if ( ! d ) {
#ifdef QT_CHECK_STATE
	qWarning( "QThread::exit() called without a QThread instance." );
#endif // QT_CHECK_STATE

	_endthread();
	return;
    }

    QThreadInstance::finish(d);
    _endthread();
}


#endif // QT_THREAD_SUPPORT
