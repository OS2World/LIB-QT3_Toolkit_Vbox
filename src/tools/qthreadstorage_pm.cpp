/****************************************************************************
** $Id: qthreadstorage_pm.cpp 8 2005-11-16 19:36:46Z dmik $
**
** ...
**
** Copyright (C) 2003 Trolltech AS.  All rights reserved.
** Copyright (C) 2004 Norman ASA.  Initial OS/2 Port.
** Copyright (C) 2005 netlabs.org.  Further OS/2 Development.
**
** This file is part of the tools module of the Qt GUI Toolkit.
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

#ifdef QT_THREAD_SUPPORT

#include "qt_os2.h"

#include "qthreadstorage.h"
#include <private/qthreadinstance_p.h>
#include <private/qcriticalsection_p.h>

#include <string.h>

// #define QTHREADSTORAGE_DEBUG


// keep this in sync with the implementation in qthreadstorage.cpp
static const int MAX_THREAD_STORAGE = 257; // 256 maximum + 1 used in QRegExp

static bool thread_storage_init = FALSE;
static struct {
    bool used;
    void (*func)( void * );
} thread_storage_usage[MAX_THREAD_STORAGE];

static void thread_storage_id( int &id, void (*func)( void * ), bool alloc )
{
    static QCriticalSection cs;
    cs.enter();

    if ( alloc ) {
	// make sure things are initialized
	if ( ! thread_storage_init )
	    memset( thread_storage_usage, 0, sizeof( thread_storage_usage ) );
	thread_storage_init = TRUE;

	for ( ; id < MAX_THREAD_STORAGE; ++id ) {
	    if ( !thread_storage_usage[id].used )
		break;
	}

	Q_ASSERT( id >= 0 && id < MAX_THREAD_STORAGE );
	thread_storage_usage[id].used = TRUE;
	thread_storage_usage[id].func = func;

#ifdef QTHREADSTORAGE_DEBUG
	qDebug( "QThreadStorageData: allocated id %d", id );
#endif // QTHREADSTORAGE_DEBUG
    } else {
	thread_storage_usage[id].used = FALSE;
	thread_storage_usage[id].func = 0;

#ifdef QTHREADSTORAGE_DEBUG
	qDebug( "QThreadStorageData: released id %d", id );
#endif // QTHREADSTORAGE_DEBUG
    }

    cs.leave();
}


QThreadStorageData::QThreadStorageData( void (*func)( void * ) )
    : id( 0 )
{
    thread_storage_id( id, func, TRUE );
}

QThreadStorageData::~QThreadStorageData()
{
    thread_storage_id( id, 0, FALSE );
}

void **QThreadStorageData::get() const
{
    QThreadInstance *d = QThreadInstance::current();
    QMutexLocker locker( d->mutex() );
    return d->thread_storage && d->thread_storage[id] ? &d->thread_storage[id] : 0;
}

void **QThreadStorageData::set( void *p )
{
    QThreadInstance *d = QThreadInstance::current();
    QMutexLocker locker( d->mutex() );
    if ( !d->thread_storage ) {
#ifdef QTHREADSTORAGE_DEBUG
        PTIB ptib;
        DosGetInfoBlocks( &ptib, NULL );
	qDebug( "QThreadStorageData: allocating storage for thread %lx",
		(unsigned long) ptib->tib_ptib2->tib2_ultid );
#endif // QTHREADSTORAGE_DEBUG

	d->thread_storage = new void*[MAX_THREAD_STORAGE];
	memset( d->thread_storage, 0, sizeof( void* ) * MAX_THREAD_STORAGE );
    }

    // delete any previous data
    if ( d->thread_storage[id] )
	thread_storage_usage[id].func( d->thread_storage[id] );

    // store new data
    d->thread_storage[id] = p;
    return &d->thread_storage[id];
}

void QThreadStorageData::finish( void **thread_storage )
{
    if ( ! thread_storage ) return; // nothing to do

#if defined(QTHREADSTORAGE_DEBUG) || defined(QT_CHECK_STATE)
    PTIB ptib;
    DosGetInfoBlocks( &ptib, NULL );
#endif
#ifdef QTHREADSTORAGE_DEBUG
    // Note: This method can be called (by QThread::terminate()) from a
    // thread other than one we are destroying storage for, in which case
    // we will have the TID of the terminating thread here.
    qDebug( "QThreadStorageData: destroying storage for thread %lx",
	    (unsigned long) ptib->tib_ptib2->tib2_ultid );
#endif // QTHREADSTORAGE_DEBUG

    for ( int i = 0; i < MAX_THREAD_STORAGE; ++i ) {
	if ( ! thread_storage[i] ) continue;
	if ( ! thread_storage_usage[i].used ) {
#ifdef QT_CHECK_STATE
	    qWarning( "QThreadStorage: thread %lx exited after QThreadStorage destroyed",
		      (unsigned long) ptib->tib_ptib2->tib2_ultid );
#endif // QT_CHECK_STATE
	    continue;
	}

	thread_storage_usage[i].func( thread_storage[i] );
    }

    delete [] thread_storage;
}

#endif // QT_THREAD_SUPPORT
