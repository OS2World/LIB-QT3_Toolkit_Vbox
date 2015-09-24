/****************************************************************************
** $Id: qcriticalsection_p.cpp 79 2006-04-05 19:18:34Z dmik $
**
** Implementation of QCriticalSection class
**
** Copyright (C) 2001 Trolltech AS.  All rights reserved.
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

#include <private/qcriticalsection_p.h>

#if defined(Q_WS_WIN)
#   include "qt_windows.h"
#elif defined(Q_OS_OS2)
#   include "qt_os2.h"
#endif

class QCriticalSectionPrivate 
{
public:
    QCriticalSectionPrivate() {}

#if defined(Q_WS_WIN)
    CRITICAL_SECTION section;
#elif defined(Q_OS_OS2)
    HMTX mutex;
#endif
};

QCriticalSection::QCriticalSection()
{
    d = new QCriticalSectionPrivate;
#if defined(Q_WS_WIN)
    InitializeCriticalSection( &d->section );
#elif defined(Q_OS_OS2)
    DosCreateMutexSem( NULL, &d->mutex, 0, FALSE );
#endif
}

QCriticalSection::~QCriticalSection()
{
#if defined(Q_WS_WIN)
    DeleteCriticalSection( &d->section );
#elif defined(Q_OS_OS2)
    DosCloseMutexSem( d->mutex );
#endif
    delete d;
}

void QCriticalSection::enter()
{
#if defined(Q_WS_WIN)
    EnterCriticalSection( &d->section );
#elif defined(Q_OS_OS2)
    DosRequestMutexSem( d->mutex, SEM_INDEFINITE_WAIT );
#endif
}

void QCriticalSection::leave()
{
#if defined(Q_WS_WIN)
    LeaveCriticalSection( &d->section );
#elif defined(Q_OS_OS2)
    DosReleaseMutexSem( d->mutex );
#endif
}

#endif
