/****************************************************************************
** $Id: qt_os2.h 189 2011-06-16 16:08:06Z abwillis $
**
** Includes OS/2 system header files.
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

#ifndef QT_OS2_H
#define QT_OS2_H

#include "qwindowdefs.h"

#define INCL_BASE
#define INCL_PM
#include <os2.h>

// wrappers for each Win* and Gpi* call that restore the FPU Control Word 
#include <API_FPU_CW_Wrappers.h>

// OS/2 system exception handler callback interface

#if !defined(QT_OS2_NO_SYSEXCEPTIONS)

enum QtOS2SysXcptReq
{
    QtOS2SysXcptReq_AppName = 0,
    QtOS2SysXcptReq_AppVer = 1,
    QtOS2SysXcptReq_ReportTo = 2,
    QtOS2SysXcptReq_ReportSubj = 3,
};

typedef void (*QtOS2SysXcptWriter)( const char *str );
typedef int (*QtOS2SysXcptCallback)( QtOS2SysXcptReq req,
                                     QtOS2SysXcptWriter writer,
                                     int reserved );

class Q_EXPORT QtOS2SysXcptMainHandler
{
public:
    QtOS2SysXcptMainHandler( QtOS2SysXcptCallback cb = NULL );
    ~QtOS2SysXcptMainHandler();

    class Private;
    
private:
    EXCEPTIONREGISTRATIONRECORD rec;
    
    static bool installed;
    static QtOS2SysXcptCallback callback;
    static ERR libcHandler;

    // @todo (r=dmik) Inntotek GCC/kLIBC v0.6.1 and earlier have a problem with
    // __attribute__((__system__)) (see http://svn.netlabs.org/libc/ticket/129)
    // so we have to temporarily comment out APIENTRY below, based on the fact
    // that the OS/2 _System calling convention corresponds to cdecl in terms
    // of argument order and stack cleanup. Once it's fixed, this should be
    // guarded by #if __INNOTEK_LIBC__<=0x006...
    static ULONG /* APIENTRY */ handler( PEXCEPTIONREPORTRECORD pReportRec,
                                         PEXCEPTIONREGISTRATIONRECORD pRegRec,
                                         PCONTEXTRECORD pContextRec,
                                         PVOID pv );

    friend class QtOS2SysXcptMainHandlerInternal;
    friend class QThreadInstance;

    // these are private to allow stack-based instances only    
    QtOS2SysXcptMainHandler( QtOS2SysXcptMainHandler &/*that*/ ) {}
    QtOS2SysXcptMainHandler &operator =( QtOS2SysXcptMainHandler &/*that*/) {
        return *this;
    }
    static void *operator new( size_t /*size*/ ) throw() { return NULL; }
    static void operator delete( void */*memory*/ ) {}    
};

#endif // !defined(QT_OS2_NO_SYSEXCEPTIONS)

#endif // QT_OS2_H
