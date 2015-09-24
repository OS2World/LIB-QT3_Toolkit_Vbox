/****************************************************************************
** $Id: qlibrary_pm.cpp 156 2006-11-15 01:45:23Z dmik $
**
** Implementation of QLibraryPrivate class for Win32
**
** Copyright (C) 2000-2002 Trolltech AS.  All rights reserved.
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

#include <qmap.h>
#include <private/qlibrary_p.h>

#ifdef QT_THREAD_SUPPORT
#  include <private/qmutexpool_p.h>
#endif // QT_THREAD_SUPPORT

#ifndef QT_H
#include "qfile.h"
#include "qdir.h"
#endif // QT_H

#ifndef QT_NO_LIBRARY

struct LibInstance {
    LibInstance() { module = 0; refCount = 0; }
    HMODULE module;
    int refCount;
};

typedef QMap<QCString, LibInstance*> LibInstanceMap;

static LibInstanceMap map;
/*
  The platform dependent implementations of
  - loadLibrary
  - freeLibrary
  - resolveSymbol

  It's not too hard to guess what the functions do.
*/

#include "qt_os2.h"

bool QLibraryPrivate::loadLibrary()
{
    if ( pHnd )
        return TRUE;

#ifdef QT_THREAD_SUPPORT
    // protect map creation/access
    QMutexLocker locker( qt_global_mutexpool ?
                         qt_global_mutexpool->get( &map ) : 0 );
#endif // QT_THREAD_SUPPORT

    QCString filename =
        QFile::encodeName( QDir::convertSeparators ( library->library() ) );
    if ( map.find(filename) != map.end() ) {
        LibInstance *lib = map[filename];
        lib->refCount++;
        pHnd = lib->module;
    } else {
        APIRET rc = NO_ERROR;
        char errModule [CCHMAXPATH];
        if ( (rc = DosLoadModule( errModule, sizeof(errModule),
                                  filename.data(), &pHnd
        )) != NO_ERROR ) {
            pHnd = 0;
#if defined(QT_DEBUG) || defined(QT_DEBUG_COMPONENT)
            qSystemWarning( QString("Failed to load library %1 (reason: %2)!")
                .arg( filename.data() ).arg( errModule ), rc
            );
#endif
        }
        if ( pHnd ) {
            LibInstance *lib = new LibInstance;
            lib->module = pHnd;
            lib->refCount++;
            map.insert( filename, lib );
        }
    }
    return pHnd != 0;
}

bool QLibraryPrivate::freeLibrary()
{
    return derefLib( TRUE );
}

void* QLibraryPrivate::resolveSymbol( const char* f )
{
    if ( !pHnd )
        return 0;

    void *address = 0;
    APIRET rc = NO_ERROR;
    if ( (rc = DosQueryProcAddr( pHnd, 0, f, (PFN*) &address)) != NO_ERROR )
        address = 0;
#if defined(QT_DEBUG_COMPONENT)
    if ( !address )
        qSystemWarning( QString("Couldn't resolve symbol \"%1\"").arg( f ), rc );
#endif

    return address;
}

bool QLibraryPrivate::derefLib( bool doFree )
{
    if ( !pHnd )
        return TRUE;

#ifdef QT_THREAD_SUPPORT
    // protect map access
    QMutexLocker locker( qt_global_mutexpool ?
                         qt_global_mutexpool->get( &map ) : 0 );
#endif // QT_THREAD_SUPPORT

    APIRET rc = ERROR_INVALID_HANDLE;
    LibInstanceMap::iterator it;
    for ( it = map.begin(); it != map.end(); ++it ) {
        LibInstance *lib = *it;
        if ( lib->module == pHnd ) {
            lib->refCount--;
            if ( lib->refCount == 0 ) {
                if ( doFree )
                    rc = DosFreeModule( pHnd );
                else
                    rc = NO_ERROR;
                if ( rc == NO_ERROR ) {
                    map.remove( it );
                    delete lib;
                } else
                    lib->refCount++;                
            } else
                rc = NO_ERROR;
            break;
        }
    }
    if ( rc == NO_ERROR )
        pHnd = 0;
#if defined(QT_DEBUG) || defined(QT_DEBUG_COMPONENT)
    else
        qSystemWarning( "Failed to unload library!", rc );
#endif
    return rc == NO_ERROR;
}

#endif //QT_NO_LIBRARY
