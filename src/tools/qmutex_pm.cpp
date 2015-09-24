/****************************************************************************
** $Id: qmutex_pm.cpp 8 2005-11-16 19:36:46Z dmik $
**
** Implementation of QMutex class for OS/2
**
** Copyright (C) 2000-2001 Trolltech AS.  All rights reserved.
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

#if defined(QT_THREAD_SUPPORT)

#include "qt_os2.h"
#include "qmutex.h"
#include "qnamespace.h"

#define Q_MUTEX_T HMTX
#include <private/qmutex_p.h>

QMutexPrivate::~QMutexPrivate()
{
    APIRET rc = DosCloseMutexSem( handle );
    if ( rc ) {
#ifdef QT_CHECK_RANGE
	qSystemWarning( "Mutex destroy failure", rc );
#endif
    }
}

/*
  QRecursiveMutexPrivate - implements a recursive mutex
*/

class QRecursiveMutexPrivate : public QMutexPrivate
{
public:
    QRecursiveMutexPrivate();

    void lock();
    void unlock();
    bool locked();
    bool trylock();
    int type() const { return Q_MUTEX_RECURSIVE; }
};

QRecursiveMutexPrivate::QRecursiveMutexPrivate()
{
    APIRET rc = DosCreateMutexSem( NULL, &handle, 0, FALSE );
    if ( rc ) {
        handle = 0;
#ifdef QT_CHECK_RANGE
	qSystemWarning( "Mutex init failure", rc );
#endif
    }
}

void QRecursiveMutexPrivate::lock()
{
    APIRET rc = DosRequestMutexSem( handle, SEM_INDEFINITE_WAIT );
    switch ( rc ) {
        case NO_ERROR:
            break;
        case ERROR_SEM_OWNER_DIED:
#ifdef QT_CHECK_RANGE
            qWarning( "Thread terminated while locking mutex!" );
#endif
            break;
        default:
#ifdef QT_CHECK_RANGE
            qSystemWarning( "Mutex lock failure", rc );
#endif
            break;
    }
}

void QRecursiveMutexPrivate::unlock()
{
    APIRET rc = DosReleaseMutexSem( handle );
    if ( rc ) {
#ifdef QT_CHECK_RANGE
	qSystemWarning( "Mutex unlock failure", rc );
#endif
    }
}

bool QRecursiveMutexPrivate::locked()
{
    PID pid = 0;
    TID tid = 0;
    ULONG count = 0;
    APIRET rc = DosQueryMutexSem( handle, &pid, &tid, &count );
    if ( rc ) {
#ifdef QT_CHECK_RANGE
        if ( rc == ERROR_SEM_OWNER_DIED )
            qWarning( "Thread terminated while locking mutex!" );
        else
            qSystemWarning( "Mutex locktest failure", rc );
#endif
        return FALSE;
    }
    return count != 0;
}

bool QRecursiveMutexPrivate::trylock()
{
    APIRET rc = DosRequestMutexSem( handle, SEM_IMMEDIATE_RETURN );
    switch ( rc ) {
        case NO_ERROR:
            return TRUE;
        case ERROR_TIMEOUT:
            break;
        case ERROR_SEM_OWNER_DIED:
#ifdef QT_CHECK_RANGE
            qWarning( "Thread terminated while locking mutex!" );
#endif
            break;
        default:
#ifdef QT_CHECK_RANGE
            qSystemWarning( "Mutex lock failure", rc );
#endif
            break;
    }
    return FALSE;
}

/*
  QNonRecursiveMutexPrivate - implements a non-recursive mutex
*/

class QNonRecursiveMutexPrivate : public QRecursiveMutexPrivate
{
public:
    QNonRecursiveMutexPrivate();

    void lock();
    bool trylock();
    int type() const { return Q_MUTEX_NORMAL; };
};

QNonRecursiveMutexPrivate::QNonRecursiveMutexPrivate()
    : QRecursiveMutexPrivate()
{
}

void QNonRecursiveMutexPrivate::lock()
{
    PTIB ptib;
    DosGetInfoBlocks( &ptib, NULL );

    PID pid = 0;
    TID tid = 0;
    ULONG count = 0;
    APIRET rc = DosQueryMutexSem( handle, &pid, &tid, &count );
    if ( rc ) {
#ifdef QT_CHECK_RANGE
        if ( rc == ERROR_SEM_OWNER_DIED )
            qWarning( "Thread terminated while locking mutex!" );
        else
            qSystemWarning( "Mutex locktest failure", rc );
#endif
        return;
    }

    if ( count != 0 && tid == ptib->tib_ptib2->tib2_ultid ) {
#ifdef QT_CHECK_RANGE
	qWarning( "Non-recursive mutex already locked by this thread" );
#endif
    } else {
        QRecursiveMutexPrivate::lock();
    }
}

bool QNonRecursiveMutexPrivate::trylock()
{
    PTIB ptib;
    DosGetInfoBlocks( &ptib, NULL );

    PID pid = 0;
    TID tid = 0;
    ULONG count = 0;
    APIRET rc = DosQueryMutexSem( handle, &pid, &tid, &count );
    if ( rc ) {
#ifdef QT_CHECK_RANGE
        if ( rc == ERROR_SEM_OWNER_DIED )
            qWarning( "Thread terminated while locking mutex!" );
        else
            qSystemWarning( "Mutex locktest failure", rc );
#endif
        return FALSE;
    }

    if ( count != 0 && tid == ptib->tib_ptib2->tib2_ultid ) {
	// locked by this thread already, return FALSE
        return FALSE;
    }
    return QRecursiveMutexPrivate::trylock();
}

/*
  QMutex implementation
*/

QMutex::QMutex(bool recursive)
{
    if ( recursive )
	d = new QRecursiveMutexPrivate();
    else
	d = new QNonRecursiveMutexPrivate();
}

QMutex::~QMutex()
{
    delete d;
}

void QMutex::lock()
{
    d->lock();
}

void QMutex::unlock()
{
    d->unlock();
}

bool QMutex::locked()
{
    return d->locked();
}

bool QMutex::tryLock()
{
    return d->trylock();
}

#endif
