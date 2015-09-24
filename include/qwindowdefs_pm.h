/****************************************************************************
** $Id: qwindowdefs_pm.h 139 2006-10-20 21:44:52Z dmik $
**
** Definition of OS/2 functions, types and constants for the PM
** window system
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

#ifndef QWINDOWDEFS_PM_H
#ifndef QT_H
#endif // QT_H
#define QWINDOWDEFS_PM_H

#ifndef QT_H
#endif // QT_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(Q_CC_GNU) && !defined(USE_OS2_TOOLKIT_HEADERS)

#define OS2EMX_PLAIN_CHAR
#define INCL_BASE
#define INCL_PM
#include <os2.h>

// Innotek GCC lacks some API functions in its version of OS/2 Toolkit headers

#define QCRGN_ERROR                 0
#define QCRGN_OK                    1
#define QCRGN_NO_CLIP_REGION        2

LONG APIENTRY WinQueryClipRegion( HWND hwnd, HRGN hrgnClip );
BOOL APIENTRY WinSetClipRegion( HWND hwnd, HRGN hrgnClip );

#else

#ifndef _Seg16
#define _Seg16
#endif
#include <os2def.h>

#endif

#ifdef __cplusplus
}
#endif

/*
 *  Undoc'd DC_PREPAREITEM, see
 *  http://lxr.mozilla.org/seamonkey/source/widget/src/os2/nsDragService.cpp
 */
#if !defined (DC_PREPAREITEM)
#define DC_PREPAREITEM 0x40
#endif

typedef HWND WId;

Q_EXPORT HPS	   qt_display_ps();

// special LCID values for font and bitmap handling
const LONG LCID_QTPixmapBrush   = 1;
const LONG LCID_QTFont          = 2;

// constants to address extra window data
const LONG QWL_QTCLIPRGN        = QWL_USER;
const LONG QWL_QTMODAL          = QWL_USER + sizeof(LONG);
const ULONG QT_EXTRAWINDATASIZE = sizeof(LONG) * 2;

class Q_EXPORT QPMObjectWindow
{
public:
    QPMObjectWindow( bool deferred = FALSE );
    virtual ~QPMObjectWindow();
    
    bool create();
    bool destroy();
    HWND hwnd() const { return w; }

    MRESULT send( ULONG msg, MPARAM mp1, MPARAM mp2 ) const {
        return WinSendMsg( w, msg, mp1, mp2 );
    }
    bool post( ULONG msg, MPARAM mp1, MPARAM mp2 ) const {
        return WinPostMsg( w, msg, mp1, mp2 );
    }
    
    virtual MRESULT message( ULONG msg, MPARAM mp1, MPARAM mp2 ) = 0;
    
private:
    static MRESULT EXPENTRY windowProc( HWND, ULONG, MPARAM, MPARAM );

    HWND w;
};

#endif
