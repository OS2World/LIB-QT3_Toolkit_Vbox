/****************************************************************************
** $Id: qwaitcondition_pm.cpp 8 2005-11-16 19:36:46Z dmik $
**
** Implementation of QWaitCondition class for OS/2
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

#include "qwaitcondition.h"
#include "qnamespace.h"
#include "qmutex.h"
#include "qptrlist.h"
#include "qt_os2.h"

#define Q_MUTEX_T HMTX
#include <private/qmutex_p.h>
#include <private/qcriticalsection_p.h>

//***********************************************************************
// QWaitConditionPrivate
// **********************************************************************

class QWaitConditionEvent
{
public:
    inline QWaitConditionEvent() : priority(0)
    {
        event = 0;
        DosCreateEventSem( NULL, &event, 0, FALSE );
    }
    inline ~QWaitConditionEvent() { DosCloseEventSem( event ); }
    int priority;
    HEV event;
};

typedef QPtrList<QWaitConditionEvent> EventQueue;

class QWaitConditionPrivate
{
public:
    QCriticalSection cs;
    EventQueue queue;
    EventQueue freeQueue;

    bool wait(QMutex *mutex, unsigned long time);
};

bool QWaitConditionPrivate::wait(QMutex *mutex, unsigned long time)
{
    bool ret = FALSE;

    cs.enter();
    QWaitConditionEvent *wce = freeQueue.take();
    if (!wce)
	wce = new QWaitConditionEvent;

    PTIB ptib;
    DosGetInfoBlocks( &ptib, NULL );
    wce->priority = ptib->tib_ptib2->tib2_ulpri;

    // insert 'wce' into the queue (sorted by priority)
    QWaitConditionEvent *current = queue.first();
    int index = 0;
    while (current && current->priority >= wce->priority) {
	current = queue.next();
	++index;
    }
    queue.insert(index, wce);
    cs.leave();

    if (mutex) mutex->unlock();

    // wait for the event
    APIRET rc = DosWaitEventSem( wce->event, time );
    if ( !rc ) ret = TRUE;

    if (mutex) mutex->lock();

    cs.enter();
    // remove 'wce' from the queue
    queue.removeRef(wce);
    ULONG posts;
    DosResetEventSem( wce->event, &posts );
    freeQueue.append(wce);
    cs.leave();

    return ret;
}

//***********************************************************************
// QWaitCondition implementation
//***********************************************************************

QWaitCondition::QWaitCondition()
{
    d = new QWaitConditionPrivate;
    d->freeQueue.setAutoDelete(TRUE);
}

QWaitCondition::~QWaitCondition()
{
    Q_ASSERT(d->queue.isEmpty());
    delete d;
}

bool QWaitCondition::wait( unsigned long time )
{
    return d->wait(0, time);
}

bool QWaitCondition::wait( QMutex *mutex, unsigned long time)
{
    if ( !mutex )
	return FALSE;

    if ( mutex->d->type() == Q_MUTEX_RECURSIVE ) {
#ifdef QT_CHECK_RANGE
	qWarning("QWaitCondition::wait: Cannot wait on recursive mutexes.");
#endif
	return FALSE;
    }
    return d->wait(mutex, time);
}

void QWaitCondition::wakeOne()
{
    // wake up the first thread in the queue
    d->cs.enter();
    QWaitConditionEvent *first = d->queue.first();
    if (first)
        DosPostEventSem( first->event );
    d->cs.leave();
}

void QWaitCondition::wakeAll()
{
    // wake up the all threads in the queue
    d->cs.enter();
    QWaitConditionEvent *current = d->queue.first();
    while (current) {
        DosPostEventSem( current->event );
	current = d->queue.next();
    }
    d->cs.leave();
}

#endif // QT_THREAD_SUPPORT
