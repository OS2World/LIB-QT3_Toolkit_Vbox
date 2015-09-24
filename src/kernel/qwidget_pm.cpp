/****************************************************************************
** $Id: qwidget_pm.cpp 195 2011-06-18 05:23:01Z rudi $
**
** Implementation of QWidget and QWindow classes for OS/2
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

#include "qapplication.h"
#include "qapplication_p.h"
#include "qpainter.h"
#include "qbitmap.h"
#include "qwidgetlist.h"
#include "qwidgetintdict.h"
#include "qobjectlist.h"
#include "qaccel.h"
#include "qimage.h"
#include "qfocusdata.h"
#include "qlayout.h"
#include "qt_os2.h"
#include "qpaintdevicemetrics.h"
#include "qcursor.h"
#include <private/qapplication_p.h>
/// @todo (dmik) remove?
//#include <private/qinputcontext_p.h>

const QString qt_reg_winclass( int );		// defined in qapplication_pm.cpp
/// @todo (dmik) later?
//void	    qt_olednd_unregister( QWidget* widget, QOleDropTarget *dst ); // dnd_win
//QOleDropTarget* qt_olednd_register( QWidget* widget );


extern bool qt_nograb();

// defined in qapplication_pm.cpp
extern void qt_sendBlocked( QObject *obj, QWidget *modal, QEvent *e, bool override );
#if !defined (QT_NO_SESSIONMANAGER)
extern bool qt_about_to_destroy_wnd;
#endif

static QWidget *mouseGrb    = 0;
static QCursor *mouseGrbCur = 0;
static QWidget *keyboardGrb = 0;

extern "C" MRESULT EXPENTRY QtWndProc( HWND, ULONG, MPARAM, MPARAM );
extern PFNWP QtOldFrameProc;
extern "C" MRESULT EXPENTRY QtFrameProc( HWND, ULONG, MPARAM, MPARAM );

PFNWP QtOldSysMenuProc;
extern "C" MRESULT EXPENTRY QtSysMenuProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    if ( msg == WM_MENUEND ) {
        // the pull-down menu is closed, always dismiss the system menu itself
        WinPostMsg( hwnd, MM_ENDMENUMODE, MPFROMSHORT(TRUE), 0 );
    }
    return QtOldSysMenuProc( hwnd, msg, mp1, mp2 );
}

#if !defined (QT_PM_NO_WIDGETMASK)

//#define DEBUG_WIDGETMASK

/**
 *  \internal
 *  Extended version of WinQueryClipRegion(). If the given window has a
 *  clip region, the given region will receive a copy of the clip region
 *  clipped to the current window rectangle. If there is no clip region,
 *  the given region will contain only the window rectangle on return.
 */
static void qt_WinQueryClipRegionOrRect( HWND hwnd, HRGN hrgn )
{
    RECTL rcl;
    WinQueryWindowRect( hwnd, &rcl );

    HPS hps = qt_display_ps();
    GpiSetRegion( hps, hrgn, 1, &rcl );
    if ( WinQueryClipRegion( hwnd, 0 ) != QCRGN_NO_CLIP_REGION ) {
        HRGN hrgnTemp = GpiCreateRegion( hps, 0, NULL );
        WinQueryClipRegion( hwnd, hrgnTemp );
        GpiCombineRegion( hps, hrgn, hrgnTemp, hrgn, CRGN_AND );
        GpiDestroyRegion( hps, hrgnTemp );
    }    
}

/**
 *  \internal
 *  Extended version of WinInvalidateRegion(): invalidates the specified region
 *  of the given window and regions of children from \a hwndFrom to \a hwndTo
 *  if they intersect with the invalid region. If either of child window
 *  handles is NULLHANDLE, children are not invalidated at all. Also, HWND_TOP
 *  can be used as \a hwndFrom, HWND_BOTTOM \a can be used as \a hwndTo. 
 */
static BOOL qt_WinInvalidateRegionEx( HWND hwnd, HRGN hrgn,
                                      HWND hwndFrom, HWND hwndTo )
{
#if defined (DEBUG_WIDGETMASK)
    QWidget *w = QWidget::find( hwnd );
    qDebug( "qt_WinInvalidateRegionEx: hwnd=%08lX (%s/%s) "
            "hwndFrom=%08lX hwndTo=%08lX",
            hwnd, w ? w->name() : 0, w ? w->className() : 0, hwndFrom, hwndTo );
#endif
    
    if ( hwndFrom == HWND_TOP )
        hwndFrom = WinQueryWindow( hwnd, QW_TOP );
    if ( hwndTo == HWND_BOTTOM )
        hwndTo = WinQueryWindow( hwnd, QW_BOTTOM );

    if ( hwndFrom == 0 || hwndTo == 0 )
        return WinInvalidateRegion( hwnd, hrgn, FALSE );
    
    if ( WinQueryWindow( hwndFrom, QW_PARENT ) != hwnd ||
         WinQueryWindow( hwndTo, QW_PARENT ) != hwnd )
        return FALSE;
    
    HPS hps = qt_display_ps();
    
    SWP swp;
    HWND child = hwndFrom;
    HRGN hrgnChild = GpiCreateRegion( hps, 0, NULL );
    HRGN hrgnInv = GpiCreateRegion( hps, 0, NULL );
    GpiCombineRegion( hps, hrgnInv, hrgn, 0, CRGN_COPY ); 
    
    LONG cmplx = RGN_RECT;
    
    while ( child ) {
        WinQueryWindowPos( child, &swp );
#if defined (DEBUG_WIDGETMASK)
        w = QWidget::find( child );
        qDebug( " child=%08lX [fl=%08lX] (%s/%s)", child, swp.fl,
                w ? w->name() : 0, w ? w->className() : 0 );
#endif
        // proceed only if not hidden
        if ( swp.fl & SWP_SHOW ) {
            // get sibling's bounds (clip region or rect)
            qt_WinQueryClipRegionOrRect( child, hrgnChild );
            // translate the region to the parent's coordinate system
            POINTL ptl = { swp.x, swp.y };
            GpiOffsetRegion( hps, hrgnChild, &ptl );
            // intersect the child's region with the invalid one
            // and invalidate if not NULL
            cmplx = GpiCombineRegion( hps, hrgnChild, hrgnChild, hrgnInv,
                                      CRGN_AND );
            if ( cmplx != RGN_NULL ) {
                POINTL ptl2 = { -swp.x, -swp.y };
                GpiOffsetRegion( hps, hrgnChild, &ptl2 );
                WinInvalidateRegion( child, hrgnChild, TRUE );
                GpiOffsetRegion( hps, hrgnChild, &ptl );
                // substract the invalidated area from the widget's region
                // (no need to invalidate it any more)
                cmplx = GpiCombineRegion( hps, hrgnInv, hrgnInv, hrgnChild,
                                          CRGN_DIFF );
#if defined (DEBUG_WIDGETMASK)
                qDebug( "  processed" );
#endif
                // finish if nothing left
                if ( cmplx == RGN_NULL )
                    break;
            }
        }
        // iterate to the next window (below)
        if ( child == hwndTo )
            break;
        child = WinQueryWindow( child, QW_NEXT );
    }
    
    BOOL ok = (cmplx == RGN_NULL) || (child == hwndTo);
    
    if ( ok ) {
        // invalidate what's left invalid after substracting children
        WinInvalidateRegion( hwnd, hrgnInv, FALSE );
    }
    
    GpiDestroyRegion( hps, hrgnInv );
    GpiDestroyRegion( hps, hrgnChild );
    
    return ok;
}

/** \internal flags for qt_WinProcessWindowObstacles() */
enum {
    PWO_Children = 0x01,
    PWO_Sibings = 0x02,
    PWO_Ancestors = 0x04,
    PWO_Screen = 0x08,
    PWO_TopLevel = 0x80000000,
    // PWO_Default is suitable in most cases (for simple paint operations)
    PWO_Default = PWO_Children | PWO_Sibings | PWO_Ancestors | PWO_Screen, 
};

/**
 *  \internal
 *  Helper function to collect all relative windows intersecting with the
 *  given window and placed above it in z-order.
 *
 *  \param hwnd  window in question
 *  \param prcl  rectangle (in window coordinates) to limit processing to
 *               (if null, the whole window rectange is used)
 *  \param hrgn  region where to combine all obstacles
 *               (if 0, obstacles are directly validated instead of collecting)
 *  \param op    region operation perfomed when combining obstacles (CRGN_*)
 *  \param flags flags defining the scope (PWO_* ORed together)
 *
 *  \return complexity of the combined region (only when \a hrgn is not 0) 
 */
static LONG qt_WinProcessWindowObstacles( HWND hwnd, PRECTL prcl, HRGN hrgn,
                                          LONG op, LONG flags = PWO_Default )
{
    Q_ASSERT( hwnd );

    HPS displayPS = qt_display_ps();
    
#if defined (DEBUG_WIDGETMASK)
    QWidget *w = QWidget::find( hwnd );
    qDebug( "qt_WinProcessWindowObstacles: hwnd=%08lX (%s/%s), prcl=%p "
            "hrgn=%08lX, op=%ld flags=%08lX",
            hwnd, w->name(), w->className(), prcl, hrgn, op, flags );
#endif
 
    SWP swpSelf;
    WinQueryWindowPos( hwnd, &swpSelf ); 
    
    RECTL rclSelf = { 0, 0, swpSelf.cx, swpSelf.cy };
    if ( prcl )
        rclSelf = *prcl;

    HRGN whrgn = GpiCreateRegion( displayPS, 0, NULL );

    LONG cmplx = RGN_NULL;
    HWND relative;
    SWP swp;

    // first, process areas placed outside the screen bounds
    if ( flags & PWO_Screen ) {
        RECTL rclScr = { 0, 0, QApplication::desktop()->width(),
                               QApplication::desktop()->height() };
        WinMapWindowPoints( HWND_DESKTOP, hwnd, (PPOINTL) &rclScr, 2 );
        // rough check of whether some window part is outside bounds
        if ( rclSelf.xLeft < rclScr.xLeft ||
             rclSelf.yBottom < rclScr.yBottom ||
             rclSelf.xRight > rclScr.xRight ||
             rclSelf.yTop > rclScr.yTop ) {
            GpiSetRegion( displayPS, whrgn, 1, &rclSelf );
            HRGN hrgnScr = GpiCreateRegion( displayPS, 1, &rclScr );
            // substract the screen region from this window's region
            // to get parts placed outside
            GpiCombineRegion( displayPS, whrgn, whrgn, hrgnScr, CRGN_DIFF );
            GpiDestroyRegion( displayPS, hrgnScr ); 
            // process the region
            if ( hrgn != NULLHANDLE ) {
                cmplx = GpiCombineRegion( displayPS, hrgn, hrgn, whrgn, op );
            } else {
                WinValidateRegion( hwnd, whrgn, FALSE ); 
            }
#if defined (DEBUG_WIDGETMASK)
            qDebug( " collected areas outside screen bounds" );
#endif
         }
    }
    
    // next, go through all children (in z-order)
    if ( flags & PWO_Children ) {
        relative = WinQueryWindow( hwnd, QW_BOTTOM );
        if ( relative != NULLHANDLE ) {
            for ( ; relative != HWND_TOP; relative = swp.hwndInsertBehind ) {
                WinQueryWindowPos( relative, &swp );
#if defined (DEBUG_WIDGETMASK)
                w = QWidget::find( relative );
                qDebug( " child=%08lX [fl=%08lX] (%s/%s)", relative, swp.fl,
                        w ? w->name() : 0, w ? w->className() : 0 );
#endif
                // skip if hidden
                if ( !(swp.fl & SWP_SHOW) )
                    continue;
                // rough check for intersection
                if ( swp.x >= rclSelf.xRight || swp.y >= rclSelf.yTop ||
                     swp.x + swp.cx <= rclSelf.xLeft ||
                     swp.y + swp.cy <= rclSelf.yBottom )
                    continue;
                // get the bounds (clip region or rect)
                qt_WinQueryClipRegionOrRect( relative, whrgn );
                // translate the region to this window's coordinate system
                POINTL ptl = { swp.x, swp.y };
                GpiOffsetRegion( displayPS, whrgn, &ptl );
                // process the region
                if ( hrgn != NULLHANDLE ) {
                    cmplx = GpiCombineRegion( displayPS, hrgn, hrgn, whrgn, op );
                } else {
                    WinValidateRegion( hwnd, whrgn, FALSE ); 
                }
#if defined (DEBUG_WIDGETMASK)
                qDebug( "  collected" );
#endif
            }
        }
    }

    HWND desktop = WinQueryDesktopWindow( 0, 0 );
    HWND parent = WinQueryWindow( hwnd, QW_PARENT );
    
    // next, go through all siblings placed above (in z-order),
    // but only if they are not top-level windows (that cannot be
    // non-rectangular and thus are always correctly clipped by the system)
    if ( (flags & PWO_Sibings) && parent != desktop ) {
        for ( relative = swpSelf.hwndInsertBehind;
              relative != HWND_TOP; relative = swp.hwndInsertBehind ) {
            WinQueryWindowPos( relative, &swp );
#if defined (DEBUG_WIDGETMASK)
            w = QWidget::find( relative );
            qDebug( " sibling=%08lX [fl=%08lX] (%s/%s)", relative, swp.fl,
                    w ? w->name() : 0, w ? w->className() : 0 );
#endif
            // skip if hidden
            if ( !(swp.fl & SWP_SHOW) )
                continue;
            // rough check for intersection
            if ( swp.x >= swpSelf.x + rclSelf.xRight ||
                 swp.y >= swpSelf.y + rclSelf.yTop ||
                 swp.x + swp.cx <= swpSelf.x + rclSelf.xLeft ||
                 swp.y + swp.cy <= swpSelf.y + rclSelf.yBottom )
                continue;
            // get the bounds (clip region or rect)
            qt_WinQueryClipRegionOrRect( relative, whrgn );
            // translate the region to this window's coordinate system
            POINTL ptl = { swp.x - swpSelf.x, swp.y - swpSelf.y };
            GpiOffsetRegion( displayPS, whrgn, &ptl );
            // process the region
            if ( hrgn != NULLHANDLE ) {
                cmplx = GpiCombineRegion( displayPS, hrgn, hrgn, whrgn, op ); 
            } else {
                WinValidateRegion( hwnd, whrgn, FALSE ); 
            }
#if defined (DEBUG_WIDGETMASK)
            qDebug( "  collected" );
#endif
        }
    }
    
    // last, go through all siblings of our parent and its ancestors
    // placed above (in z-order)
    if ( flags & PWO_Ancestors ) {
        POINTL delta = { swpSelf.x, swpSelf.y };
        while ( parent != desktop ) {
            HWND grandParent = WinQueryWindow( parent, QW_PARENT );
            if ( !(flags & PWO_TopLevel) ) {
                // When PWO_TopLevel is not specified, top-level windows
                // (children of the desktop) are not processed. It makes sense
                // when qt_WinProcessWindowObstacles() is used to clip out
                // overlying windows during regular paint operations (WM_PAINT
                // processing or drawing in a window directly through
                // WinGetPS()): in this case, top-level windows are always
                // correctly clipped out by the system (because they cannot be
                // non-rectangular).
                if ( grandParent == desktop )
                    break;
            }
            
            WinQueryWindowPos( parent, &swp );
#if defined (DEBUG_WIDGETMASK)
            w = QWidget::find( parent );
            qDebug( " parent=%08lX [fl=%08lX] (%s/%s)", parent, swp.fl,
                    w ? w->name() : 0, w ? w->className() : 0 );
#endif
            delta.x += swp.x;
            delta.y += swp.y;
            for ( relative = swp.hwndInsertBehind;
                  relative != HWND_TOP; relative = swp.hwndInsertBehind ) {
                WinQueryWindowPos( relative, &swp );
#if defined (DEBUG_WIDGETMASK)
                w = QWidget::find( relative );
                qDebug( " ancestor=%08lX [fl=%08lX] (%s/%s)", relative, swp.fl,
                        w ? w->name() : 0, w ? w->className() : 0 );
#endif
                // skip if hidden
                if ( !(swp.fl & SWP_SHOW) )
                    continue;
                // rough check for intersection
                if ( swp.x - delta.x >= rclSelf.xRight ||
                     swp.y - delta.y >= rclSelf.yTop ||
                     swp.x - delta.x + swp.cx <= rclSelf.xLeft ||
                     swp.y - delta.y + swp.cy <= rclSelf.yBottom )
                    continue;
                // get the bounds (clip region or rect)
                qt_WinQueryClipRegionOrRect( relative, whrgn );
                // translate the region to this window's coordinate system
                POINTL ptl = { swp.x - delta.x, swp.y - delta.y };
                GpiOffsetRegion( displayPS, whrgn, &ptl );
                // process the region
                if ( hrgn != NULLHANDLE ) {
                    cmplx = GpiCombineRegion( displayPS, hrgn, hrgn, whrgn, op ); 
                } else {
                    WinValidateRegion( hwnd, whrgn, FALSE ); 
                }
#if defined (DEBUG_WIDGETMASK)
                qDebug( "  collected" );
#endif
            }
            parent = grandParent;
        }
    }

    GpiDestroyRegion( displayPS, whrgn );
    
    return cmplx;
}

/**
 *  \internal
 *  Partial reimplementation of the WinSetWindowPos() API that obeys window clip
 *  regions. Currently supported flags are SWP_ZORDER, SWP_SHOW and SWP_HIDE.
 *  Other flags should not be used. Note that if any other flag is specified
 *  (alone or in addition to the supported ones), or if the given window is a
 *  top-level window, this function acts exactly like the original
 *  WinSetWindowPos() function.
 */
static BOOL qt_WinSetWindowPos( HWND hwnd, HWND hwndInsertBehind,
                                LONG x, LONG y, LONG cx, LONG cy,
                                ULONG fl )
{
#if defined (DEBUG_WIDGETMASK)
    QWidget *w = QWidget::find( hwnd );
    qDebug( "qt_WinSetWindowPos: hwnd=%08lX (%s/%s) fl=%08lX",
            hwnd, w ? w->name() : 0, w ? w->className() : 0, fl );
#endif

    Q_ASSERT( (fl & ~(SWP_ZORDER | SWP_SHOW | SWP_HIDE)) == 0 );
    
    HWND desktop = WinQueryDesktopWindow( 0, 0 );
    if ( (fl & ~(SWP_ZORDER | SWP_SHOW | SWP_HIDE)) != 0 ||
         hwnd == desktop || WinQueryWindow( hwnd, QW_PARENT ) == desktop ) {
        return WinSetWindowPos( hwnd, hwndInsertBehind, x, y, cx, cy, fl );
    }
    
    SWP swpOld;
    WinQueryWindowPos( hwnd, &swpOld );

    // do some checks
    if ( (fl & SWP_ZORDER) && swpOld.hwndInsertBehind == hwndInsertBehind )
        fl &= ~SWP_ZORDER;
    if ( (fl & SWP_SHOW) && (swpOld.fl & SWP_SHOW) ) 
        fl &= ~SWP_SHOW;
    if ( (fl & SWP_HIDE) && (swpOld.fl & SWP_HIDE) ) 
        fl &= ~SWP_HIDE;
    if ( (fl & (SWP_SHOW | SWP_HIDE)) == (SWP_SHOW | SWP_HIDE) ) 
        fl &= ~SWP_HIDE;
    
    BOOL rc = WinSetWindowPos( hwnd, hwndInsertBehind, x, y, cx, cy,
                               fl | SWP_NOREDRAW );
    if ( rc == FALSE || (fl & SWP_NOREDRAW) )
        return rc;

    SWP swpNew;
    WinQueryWindowPos( hwnd, &swpNew );
    
    if ( swpOld.hwndInsertBehind == swpNew.hwndInsertBehind )
        fl &= ~SWP_ZORDER;
    
    if ( (fl & (SWP_ZORDER | SWP_SHOW | SWP_HIDE)) == 0 )
        return rc;

    HPS hps = qt_display_ps();
    HWND hwndParent = WinQueryWindow( hwnd, QW_PARENT );
    
    // get window bounds
    HRGN hrgnSelf = GpiCreateRegion( hps, 0, NULL );
    qt_WinQueryClipRegionOrRect( hwnd, hrgnSelf );
    
    if ( fl & SWP_SHOW ) {
        WinInvalidateRegion( hwnd, hrgnSelf, TRUE );
    } else if ( fl & SWP_HIDE ) {
        // translate the region to the parent coordinate system
        POINTL ptl = { swpNew.x, swpNew.y };
        GpiOffsetRegion( hps, hrgnSelf, &ptl );
        // invalidate the parent and children below this window
        qt_WinInvalidateRegionEx( hwndParent, hrgnSelf,
                                  WinQueryWindow( hwnd, QW_NEXT ), HWND_BOTTOM );
    } else { // fl & SWP_ZORDER
        // below we assume that WinSetWindowPos() returns FALSE if
        // an incorrect (unrelated) hwndInsertBehind is passed when SWP_ZORDER
        // is set
        
        // first, detect whether we are moving up or down
        BOOL up;
        HWND hwndFrom, hwndTo;
        if ( swpOld.hwndInsertBehind == HWND_TOP ) {
            up = FALSE;
            hwndFrom = WinQueryWindow( hwndParent, QW_TOP );
            hwndTo = swpNew.hwndInsertBehind;
        } else {
            up = TRUE;
            for ( HWND hwndAbove = hwnd;
                  (hwndAbove = WinQueryWindow( hwndAbove, QW_PREV )) != 0; ) {
                if ( hwndAbove == swpOld.hwndInsertBehind ) {
                    up = FALSE;
                    break;
                }
            }
            if ( up ) {
                hwndFrom = swpOld.hwndInsertBehind;
                hwndTo = WinQueryWindow( hwnd, QW_NEXT );
            } else {
                hwndFrom = WinQueryWindow( swpOld.hwndInsertBehind, QW_NEXT );
                hwndTo = swpNew.hwndInsertBehind;
            }
        }
#if defined (DEBUG_WIDGETMASK)
        qDebug( " moving up? %ld", up );
        w = QWidget::find( hwndFrom );
        qDebug( " hwndFrom=%08lX (%s/%s)", hwndFrom,
                    w ? w->name() : 0, w ? w->className() : 0 );
        w = QWidget::find( hwndTo );
        qDebug( " hwndTo=%08lX (%s/%s)", hwndTo,
                    w ? w->name() : 0, w ? w->className() : 0 );
#endif

        SWP swp;
        HWND sibling = hwndFrom;
        HRGN hrgn = GpiCreateRegion( hps, 0, NULL );
        HRGN hrgnUpd = GpiCreateRegion( hps, 0, NULL );
        
        if ( up ) {
            // go upwards in z-order
            while ( 1 ) {
                WinQueryWindowPos( sibling, &swp );
#if defined (DEBUG_WIDGETMASK)
                w = QWidget::find( sibling );
                qDebug( " sibling=%08lX [fl=%08lX] (%s/%s)", sibling, swp.fl,
                        w ? w->name() : 0, w ? w->className() : 0 );
#endif
                // proceed only if not hidden
                if ( swp.fl & SWP_SHOW ) {
                    // get sibling's bounds (clip region or rect)
                    qt_WinQueryClipRegionOrRect( sibling, hrgn );
                    // translate the region to this window's coordinate system
                    POINTL ptl = { swp.x - swpNew.x, swp.y - swpNew.y };
                    GpiOffsetRegion( hps, hrgn, &ptl );
                    // add to the region of siblings we're moving on top of
                    GpiCombineRegion( hps, hrgnUpd, hrgnUpd, hrgn, CRGN_OR );
#if defined (DEBUG_WIDGETMASK)
                    qDebug( "  processed" );
#endif
                }
                // iterate to the prev window (above)
                if ( sibling == hwndTo )
                    break;
                sibling = swp.hwndInsertBehind;
            }
            // intersect the resulting region with the widget region and
            // invalidate
            GpiCombineRegion( hps, hrgnUpd, hrgnSelf, hrgnUpd, CRGN_AND );
            WinInvalidateRegion( hwnd, hrgnUpd, TRUE );
        } else {
            // go downwards in reverse z-order
            POINTL ptl = { 0, 0 };
            while ( 1 ) {
                WinQueryWindowPos( sibling, &swp );
#if defined (DEBUG_WIDGETMASK)
                w = QWidget::find( sibling );
                qDebug( " sibling=%08lX [fl=%08lX] (%s/%s)", sibling, swp.fl,
                        w ? w->name() : 0, w ? w->className() : 0 );
#endif
                // proceed only if not hidden
                if ( swp.fl & SWP_SHOW ) {
                    // get sibling's bounds (clip region or rect)
                    qt_WinQueryClipRegionOrRect( sibling, hrgn );
                    // undo the previous translation and translate this window's
                    // region to the siblings's coordinate system
                    ptl.x += swpNew.x - swp.x;
                    ptl.y += swpNew.y - swp.y;
                    GpiOffsetRegion( hps, hrgnSelf, &ptl );
                    // intersect the sibling's region with the translated one
                    // and invalidate the sibling
                    GpiCombineRegion( hps, hrgnUpd, hrgnSelf, hrgn, CRGN_AND );
                    WinInvalidateRegion( sibling, hrgnUpd, TRUE );
                    // substract the invalidated area from the widget's region
                    // (no need to invalidate it any more)
                    GpiCombineRegion( hps, hrgnSelf, hrgnSelf, hrgnUpd, CRGN_DIFF );
                    // prepare the translation from the sibling's
                    // coordinates back to this window's coordinates
                    ptl.x = swp.x - swpNew.x;
                    ptl.y = swp.y - swpNew.y;
#if defined (DEBUG_WIDGETMASK)
                    qDebug( "  processed" );
#endif
                }
                // iterate to the next window (below)
                if ( sibling == hwndTo )
                    break;
                sibling = WinQueryWindow( sibling, QW_NEXT );
            }
        }

        GpiDestroyRegion( hps, hrgnUpd );
        GpiDestroyRegion( hps, hrgn );
    }
    
    GpiDestroyRegion( hps, hrgnSelf );
    
    return TRUE; 
}

#endif

/*!
 * \internal
 * For some unknown reason, PM sends WM_SAVEAPPLICATION to every window
 * being destroyed, which makes it indistinguishable from WM_SAVEAPPLICATION
 * sent to top level windows during system shutdown. We use our own version of
 * WinDestroyWindow() and a special flag (qt_about_to_destroy_wnd) to
 * distinguish it in qapplication_pm.cpp. 
 */
static BOOL qt_WinDestroyWindow( HWND hwnd )
{
#if !defined (QT_NO_SESSIONMANAGER)
    qt_about_to_destroy_wnd = TRUE;
#endif
    BOOL rc = WinDestroyWindow( hwnd );
#if !defined (QT_NO_SESSIONMANAGER)
    qt_about_to_destroy_wnd = FALSE;
#endif
    return rc;
}

static void removeSysMenuAccels( HWND frame )
{
    HWND sysMenu = WinWindowFromID( frame, FID_SYSMENU );
    if ( !sysMenu )
        return;

    SHORT subId = SHORT1FROMMR( WinSendMsg( sysMenu, MM_ITEMIDFROMPOSITION, 0, 0 ) );
    if ( subId != MIT_ERROR ) {
        MENUITEM item;
        WinSendMsg( sysMenu, MM_QUERYITEM, MPFROM2SHORT(subId, FALSE), MPFROMP(&item) );
        HWND subMenu = item.hwndSubMenu;
        if ( subMenu ) {
            USHORT cnt = SHORT1FROMMR( WinSendMsg( subMenu, MM_QUERYITEMCOUNT, 0, 0 ) );
            for ( int i = 0; i < cnt; i++ ) {
                USHORT id = SHORT1FROMMR(
                    WinSendMsg( subMenu, MM_ITEMIDFROMPOSITION, MPFROMSHORT(i), 0 ) );
                if ( id == SC_TASKMANAGER || id == SC_CLOSE ) {
                    // accels for these entries always work in Qt, skip them
                    continue;
                }
                USHORT len = SHORT1FROMMR(
                    WinSendMsg( subMenu, MM_QUERYITEMTEXTLENGTH, MPFROMSHORT(id), 0 ) );
                if ( len++ ) {
                    char *text = new char[len];
                    WinSendMsg( subMenu, MM_QUERYITEMTEXT,
                        MPFROM2SHORT(id, len), MPFROMP(text) );
                    char *tab = strrchr( text, '\t' );
                    if ( tab ) {
                        *tab = 0;
                        WinSendMsg( subMenu, MM_SETITEMTEXT,
                            MPFROMSHORT(id), MPFROMP(text) );
                    }
                    delete[] text;
                }
            }
            // sublclass the system menu to leave the menu mode completely
            // when the user presses the ESC key. by default, pressing ESC
            // while the pull-down menu is showing brings us to the menu bar,
            // which is confusing in the case of the system menu, because
            // there is only one item on the menu bar, and we cannot see
            // that it is active when the frame window has an icon.
            PFNWP oldProc = WinSubclassWindow( sysMenu, QtSysMenuProc );
            // set QtOldSysMenuProc only once: it must be the same for
            // all FID_SYSMENU windows.
            if ( !QtOldSysMenuProc )
                QtOldSysMenuProc = oldProc;
        }
    }
}

/*****************************************************************************
  QWidget member functions
 *****************************************************************************/

//#define QT_DEBUGWINCREATEDESTROY

void QWidget::create( WId window, bool initializeWindow, bool destroyOldWindow )
{
    // When window is not zero, it represents an existing (external) window
    // handle we should create a QWidget "wrapper" for to incorporate it to the
    // Qt widget hierarchy. But so far I have never seen this method called
    // with window != 0, so we ignore this argument (as well as the other two
    // that make sense only together with it) for now and will not implement
    // this functionality until there's a real need.
    
    Q_ASSERT( window == 0 );
    Q_UNUSED( initializeWindow );
    Q_UNUSED( destroyOldWindow );
    
    if ( testWState(WState_Created) && window == 0 )
	return;
    setWState( WState_Created );			// set created flag

    if ( !parentWidget() || parentWidget()->isDesktop() )
	setWFlags( WType_TopLevel );		// top-level widget

    static int sw = -1, sh = -1;

    bool topLevel = testWFlags(WType_TopLevel);
    bool popup = testWFlags(WType_Popup);
    bool dialog = testWFlags(WType_Dialog);
    bool desktop  = testWFlags(WType_Desktop);
    WId	 id = 0;

    if ( popup ) {
        /// @todo (dmik) WStyle_StaysOnTop is ignored for now (does nothing)
	setWFlags(WStyle_StaysOnTop); // a popup stays on top
    }

    if ( sw < 0 ) {				// get the screen size
        sw = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
        sh = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );
    }

    if ( dialog || popup || desktop ) {		// these are top-level, too
	topLevel = TRUE;
	setWFlags( WType_TopLevel );
    }

    if ( desktop ) {				// desktop widget
	popup = FALSE;				// force this flags off
        /// @todo (dmik)
        //  use WinGetMaxPosition () to take into account such things as XCenter?
        crect.setRect( 0, 0, sw, sh );
    }

    PCSZ title = NULL;
    ULONG style = 0;
    ULONG fId = 0, fStyle = 0, fcFlags = 0;

    if ( popup ) {
        style |= WS_SAVEBITS;
    } else if ( !topLevel ) {
	if ( !testWFlags(WStyle_Customize) )
	    setWFlags( WStyle_NormalBorder | WStyle_Title | WStyle_MinMax | WStyle_SysMenu  );
    } else if (!desktop ) {
	if ( !testWFlags(WStyle_Customize) ) {
            if ( testWFlags(WStyle_Tool) ) {
                // a special case for WStyle_Tool w/o WStyle_Customize.
                // it violates the Qt docs but it is used by QPopupMenu
                // to create torn-off menus.
                setWFlags( WStyle_Title );
            } else {
                if ( dialog )
                    setWFlags( WStyle_NormalBorder | WStyle_Title | WStyle_SysMenu );
                else
                    setWFlags( WStyle_NormalBorder | WStyle_Title | WStyle_MinMax | WStyle_SysMenu  );
            }
	}
    }
    if ( !desktop ) {
#if !defined (QT_PM_NO_WIDGETMASK)
        // We don't use WS_CLIPSIBLINGS and WS_CLIPCHILDREN, because when these
        // styles are set and a child/sibling window has a non-NULL clip region,
        // PM still continues to exclude the entire child's rectangle from the
        // parent window's update region, ignoring its clip region. As a result,
        // areas outside the clip region are left unpainted. Instead, we correct
        // the update region of the window ourselves, on every WM_PAINT event.
        // Note: for top-level widgets, we specify WS_CLIPSIBLINGS anyway to let
        // the system do correct clipping for us (qt_WinProcessWindowObstacles()
        // relies on this). It's ok because top-level widgets cannot be non-
        // rectangular and therefore don't require our magic clipping procedure. 
        if ( topLevel /* && !testWFlags( WPaintUnclipped ) */ )
            style |= WS_CLIPSIBLINGS;
#else
        /// @todo (dmik)
        //  this is temporarily commented out because QSplitter sets
        //  WPaintUnclipped which causes terrible flicker in QFileDialog's list
        //  box and list view. This needs to be investigated. Qt/Win32 does also
        //  comment this out...
        /* if ( !testWFlags( WPaintUnclipped ) ) */
	        style |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
#endif            
        // for all top-level windows except popups we create a WC_FRAME
        // as a parent and owner.
	if ( topLevel && !popup ) {
            if ( !testWFlags(WStyle_NoBorder) ) {
                if ( testWFlags(WStyle_NormalBorder) ) {
                    fcFlags |= FCF_SIZEBORDER;
                } else if ( testWFlags(WStyle_DialogBorder) ) {
                    fcFlags |= FCF_DLGBORDER;
                } else if ( testWFlags(WStyle_Tool)  ) {
                    fcFlags |= FCF_BORDER;
                }
            }
            if ( testWFlags(WStyle_Title) )
                fcFlags |= FCF_TITLEBAR;
            if ( testWFlags(WStyle_SysMenu) )
                fcFlags |= FCF_SYSMENU | FCF_CLOSEBUTTON;
            if ( testWFlags(WStyle_Minimize) )
                fcFlags |= FCF_MINBUTTON;
            if ( testWFlags(WStyle_Maximize) )
                fcFlags |= FCF_MAXBUTTON;
            fStyle |= FS_NOMOVEWITHOWNER | FS_NOBYTEALIGN;
            fStyle |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	}
    }
    if ( testWFlags(WStyle_Title) ) {
        title = topLevel ? qAppName() : name();
    }

    // The WState_Created flag is checked by translateConfigEvent() in
    // qapplication_pm.cpp. We switch it off temporarily to avoid move
    // and resize events during creation
    clearWState( WState_Created );

    QString className = qt_reg_winclass( getWFlags() );
    PCSZ pszClassName = className.latin1();

    if ( desktop ) {			// desktop widget
	id = WinQueryDesktopWindow( 0, 0 );
	QWidget *otherDesktop = find( id );	// is there another desktop?
	if ( otherDesktop && otherDesktop->testWFlags(WPaintDesktop) ) {
	    otherDesktop->setWinId( 0 );	// remove id from widget mapper
	    setWinId( id );			// make sure otherDesktop is
	    otherDesktop->setWinId( id );	//   found first
	} else {
	    setWinId( id );
	}
    } else if ( topLevel ) {
        // create top-level widget
        HWND ownerw = 0;
	if ( !popup ) {
            QWidget *p = parentWidget();
            if ( p && !p->isDesktop() )
                ownerw = p->topLevelWidget()->winFId();
        }

        if ( !popup ) {
            // create WC_FRAME
            FRAMECDATA fcData;
            fcData.cb = sizeof(FRAMECDATA);
            fcData.flCreateFlags = fcFlags;
            fcData.hmodResources = NULL;
            fcData.idResources = 0;
            // check whether a default icon is present in .EXE and use it if so
            ULONG sz = 0;
            if ( DosQueryResourceSize( NULL, RT_POINTER, 1, &sz ) == 0 ) {
                fcData.flCreateFlags |= FCF_ICON;
                fcData.idResources = 1;
            }
#if !defined(QT_NO_DEBUG) && defined(QT_DEBUGWINCREATEDESTROY)
            qDebug ( "|Creating top level window [%s/%s] (frame):\n"
                     "|  owner = %08lX\n"
                     "|  title = '%s'\n"
                     "|  style = %08lX\n"
                     "|  fcFlags = %08lX",
                     name(), this->className(), ownerw, title, fStyle, fcFlags );
#endif
            fId = WinCreateWindow(
                    HWND_DESKTOP, WC_FRAME, title, fStyle, 0, 0, 0, 0,
                    ownerw, HWND_TOP, 0, &fcData, NULL
            );
#ifndef QT_NO_DEBUG
#ifdef QT_DEBUGWINCREATEDESTROY
            qDebug( "|  hwnd = %08lX", fId );
#endif
            if ( fId == 0 )
                qSystemWarning( "QWidget: Failed to create frame window" );
#endif
            PFNWP oldProc = WinSubclassWindow( fId, QtFrameProc );
            // set QtOldFrameProc only once: it must be the same for
            // all WC_FRAME windows.
            if ( !QtOldFrameProc )
                QtOldFrameProc = oldProc;

            removeSysMenuAccels( fId );

            // create client
#if !defined(QT_NO_DEBUG) && defined(QT_DEBUGWINCREATEDESTROY)
            qDebug( "|Creating top level window [%s/%s] (client):\n"
                    "|  owner & parent = %08lX\n"
                    "|  class = '%s'\n"
                    "|  title = '%s'\n"
                    "|  style = %08lX",
                    name(), this->className(), fId, pszClassName, title, style );
#endif
            // note that we place the client on top (HWND_TOP) to exclude other
            // frame controls from being analyzed in qt_WinProcessWindowObstacles
            id = WinCreateWindow(
                    fId, pszClassName, title, style, 0, 0, 0, 0,
                    fId, HWND_TOP, FID_CLIENT, NULL, NULL
            );
        } else {
#if !defined(QT_NO_DEBUG) && defined(QT_DEBUGWINCREATEDESTROY)
            qDebug( "|Creating top level window [%s/%s]:\n"
                    "|  owner = %08lX\n"
                    "|  class = '%s'\n"
                    "|  title = '%s'\n"
                    "|  style = %08lX",
                    name(), this->className(), ownerw, pszClassName, title, style );
#endif
            id = WinCreateWindow(
                    HWND_DESKTOP, pszClassName, title, style, 0, 0, 0, 0,
                    ownerw, HWND_TOP, 0, NULL, NULL
            );
        }
#ifndef QT_NO_DEBUG
#ifdef QT_DEBUGWINCREATEDESTROY
        qDebug( "|  hwnd = %08lX", id );
#endif
	if ( id == 0 )
	    qSystemWarning( "QWidget: Failed to create window" );
#endif
	setWinId( id );
        /// @todo (dmik) WStyle_StaysOnTop is ignored for now (does nothing)
/*        
	if ( testWFlags( WStyle_StaysOnTop) )
	    SetWindowPos( id, HWND_TOPMOST, 0, 0, 100, 100, SWP_NOACTIVATE );
*/

        // strictly speaking, PM is not obliged to initialize window data
        // with zeroes (although seems to), so do it ourselves
        for ( LONG i = 0; i <= (LONG) (QT_EXTRAWINDATASIZE - 4); i += 4 )
            WinSetWindowULong( id, i, 0 );

        // When the FS_SHELLPOSITION flag is specified during WC_FRAME window
        // creation its size and position remains zero until it is shown
        // for the first time. So, we don't use FS_SHELLPOSITION but emulate
        // its functionality here.
        SWP swp;
        WinQueryTaskSizePos( 0, 0, &swp );
        WinSetWindowPos( fId, 0, swp.x, swp.y, swp.cx, swp.cy, SWP_SIZE | SWP_MOVE );
    } else {
        // create child widget
        HWND parentw = parentWidget()->winId();
#if !defined(QT_NO_DEBUG) && defined(QT_DEBUGWINCREATEDESTROY)
        qDebug( "|Creating child window [%s/%s]:\n"
                "|  owner & parent = %08lX\n"
                "|  class = '%s'\n"
                "|  title = '%s'\n"
                "|  style = %08lX",
                name(), this->className(), parentw, pszClassName, title, style );
#endif
        id = WinCreateWindow(
                parentw, pszClassName, title, style,
                0, parentWidget()->crect.height() - 30, 100, 30,
                parentw, HWND_TOP, 0, NULL, NULL
        );
#ifndef QT_NO_DEBUG
#ifdef QT_DEBUGWINCREATEDESTROY
        qDebug( "|  hwnd = %08lX", id );
#endif
	if ( id == 0 )
	    qSystemWarning( "QWidget: Failed to create window" );
#endif
	setWinId( id );
    }

    if ( desktop ) {
	setWState( WState_Visible );
    } else {
        SWP cswp;
        WinQueryWindowPos( id, &cswp );
	if ( topLevel ) {
	    QTLWExtra *top = topData();
            if ( fId ) {
                SWP fswp;
                WinQueryWindowPos( fId, &fswp );
                // flip y coordinates
                fswp.y = sh - (fswp.y + fswp.cy);
                cswp.y = fswp.cy - (cswp.y + cswp.cy);
                crect.setRect(
                    fswp.x + cswp.x, fswp.y + cswp.y,
                    cswp.cx, cswp.cy
                );

                top->fleft = cswp.x;
                top->ftop = cswp.y;
                top->fright = fswp.cx - cswp.x - cswp.cx;
                top->fbottom = fswp.cy - cswp.y - cswp.cy;
                top->fId = fId;
            } else {
                // flip y coordinate
                cswp.y = sh - (cswp.y + cswp.cy);
                crect.setRect( cswp.x, cswp.y, cswp.cx, cswp.cy );
            }
	    fstrut_dirty = FALSE;
 	} else {
            int cy = parentWidget()->crect.height();
            // flip y coordinate
            cswp.y = cy - (cswp.y + cswp.cy);
	    crect.setRect( cswp.x, cswp.y, cswp.cx, cswp.cy );
	}
    }

    setWState( WState_Created );		// accept move/resize events
    hps = 0;					// no presentation space

    setFontSys();

/// @todo (dmik) remove?
//    QInputContext::enable( this, im_enabled & !((bool)testWState(WState_Disabled)) );
}


void QWidget::destroy( bool destroyWindow, bool destroySubWindows )
{
    deactivateWidgetCleanup();
    if ( testWState(WState_Created) ) {
	clearWState( WState_Created );
	if ( children() ) {
	    QObjectListIt it(*children());
	    register QObject *obj;
	    while ( (obj=it.current()) ) {	// destroy all widget children
		++it;
		if ( obj->isWidgetType() )
		    ((QWidget*)obj)->destroy(destroySubWindows,
					     destroySubWindows);
	    }
	}
	if ( mouseGrb == this )
	    releaseMouse();
	if ( keyboardGrb == this )
	    releaseKeyboard();
	if ( testWFlags(WShowModal) )		// just be sure we leave modal
	    qt_leave_modal( this );
	else if ( testWFlags(WType_Popup) )
	    qApp->closePopup( this );
	if ( destroyWindow && !testWFlags(WType_Desktop) ) {
            HWND id = winId();
            if ( isTopLevel() && !testWFlags(WType_Popup) ) {
                // extra data including extra->topextra has been already
                // deleted at this point by deleteExtra() and therefore
                // calling winFId() useless -- it will always return the
                // client window handle. Use WinQueryWindow() instead.
                id = WinQueryWindow( id, QW_PARENT );
            }
#if !defined(QT_NO_DEBUG) && defined(QT_DEBUGWINCREATEDESTROY)
            qDebug( "|Destroying window [%s/%s]:\n"
                    "|  hwnd = %08lX",
                    name(), className(), id );
#endif
	    qt_WinDestroyWindow( id );
	}
	setWinId( 0 );
    }
}

void QWidget::reparentSys( QWidget *parent, WFlags f, const QPoint &p,
			   bool showIt )
{
    QWidget* oldtlw = topLevelWidget();
    WId old_winfid = winFId();

    // hide and reparent our own window away. Otherwise we might get
    // destroyed when emitting the child remove event below. See QWorkspace.
    // this is also necessary for modal windows to leave the modal state
    // correctly.
    if ( isVisible() ) {
        hide();
	WinSetParent( old_winfid, HWND_OBJECT, FALSE );
	WinSetOwner( old_winfid, 0 );
    }

    // unblock the widget if blocked
    QWidget *blockedBy = 0;
    if ( isTopLevel() )
        blockedBy = (QWidget*) WinQueryWindowPtr( winId(), QWL_QTMODAL );
    if ( !blockedBy && parentWidget() )
        blockedBy = (QWidget*) WinQueryWindowPtr(
            parentWidget()->topLevelWidget()->winId(), QWL_QTMODAL );
    if ( blockedBy ) {
        QEvent e( QEvent::WindowUnblocked );
        qt_sendBlocked( this, blockedBy, &e, FALSE );
    }

    if ( testWFlags(WType_Desktop) )
	old_winfid = 0;
    setWinId( 0 );
    if ( isTopLevel() ) {
        QTLWExtra *top = topData();
        if ( top->swEntry ) {
            WinRemoveSwitchEntry( top->swEntry );
            top->swEntry = 0;
        }
        top->fId = 0;
    }

    if ( parent != parentObj ) {
	if ( parentObj )				// remove from parent
	    parentObj->removeChild( this );
	if ( parent )					// insert into new parent
	    parent->insertChild( this );
    }

    bool enable     = isEnabled();		// remember status
    FocusPolicy fp  = focusPolicy();
    QSize s	    = size();
    QString capt    = caption();
    widget_flags    = f;
    clearWState( WState_Created | WState_Visible | WState_ForceHide );
    create();
    if ( isTopLevel() || (!parent || parent->isVisible() ) )
	setWState( WState_ForceHide );	// new widgets do not show up in already visible parents
    const QObjectList *chlist = children();
    if ( chlist ) {				// reparent children
        SWP swp;
	QObjectListIt it( *chlist );
	QObject *obj;
	while ( (obj=it.current()) ) {
	    if ( obj->isWidgetType() ) {
		QWidget *w = (QWidget *)obj;
		if ( w->isPopup() )
		    ;
		else if ( w->isTopLevel() ) {
		    w->reparent( this, w->getWFlags(), w->pos(), !w->isHidden() );
                } else {
		    WinSetParent( w->winId(), winId(), FALSE );
		    WinSetOwner( w->winId(), winId() );
                    // bring PM coords into accordance with Qt coords,
                    // otherwise setGeometry() below will wrongly position
                    // children if this widget manages their layout.
                    int hd = height() - s.height();
                    WinQueryWindowPos( w->winId(), &swp );
                    swp.y += hd;
                    WinSetWindowPos( w->winId(), 0, swp.x, swp.y, 0, 0, SWP_MOVE );
                }
	    }
	    ++it;
	}
    }

    // block the widget if it should be blocked
    blockedBy = 0;
    if ( parentWidget() )
        blockedBy = (QWidget*) WinQueryWindowPtr(
            parentWidget()->topLevelWidget()->winId(), QWL_QTMODAL );
    if ( blockedBy ) {
        if ( isTopLevel() && testWFlags( WGroupLeader ) )
            blockedBy = 0;
    } else {
        QWidget *topModal = qApp->activeModalWidget();
        if (
            topModal && this != topModal && parentWidget() != topModal &&
            isTopLevel() && !testWFlags( WGroupLeader ) &&
            !testWFlags( WShowModal ) // don't block if we're going to be modal
        )
            blockedBy = topModal;
    }
    if ( blockedBy ) {
        QEvent e( QEvent::WindowBlocked );
        qt_sendBlocked( this, blockedBy, &e, FALSE );
    }

    setGeometry( p.x(), p.y(), s.width(), s.height() );
    setEnabled( enable );
    setFocusPolicy( fp );
    if ( !capt.isNull() ) {
	extra->topextra->caption = QString::null;
	setCaption( capt );
    }
    if ( showIt )
	show();
    if ( old_winfid )
	qt_WinDestroyWindow( old_winfid );

    reparentFocusWidgets( oldtlw );		// fix focus chains
}

QPoint QWidget::mapToGlobal( const QPoint &pos ) const
{
    if ( !isVisible() || isMinimized() )
	return mapTo( topLevelWidget(), pos ) + topLevelWidget()->pos() +
	(topLevelWidget()->geometry().topLeft() - topLevelWidget()->frameGeometry().topLeft());
    POINTL ptl;
    ptl.x = pos.x();
    // flip y (local) coordinate
    ptl.y = height() - (pos.y() + 1);
    WinMapWindowPoints( winId(), HWND_DESKTOP, &ptl, 1 );
    // flip y (global) coordinate
    ptl.y = QApplication::desktop()->height() - (ptl.y + 1);
    return QPoint( ptl.x, ptl.y );
}

QPoint QWidget::mapFromGlobal( const QPoint &pos ) const
{
    if ( !isVisible() || isMinimized() )
	return mapFrom( topLevelWidget(), pos - topLevelWidget()->pos() );
    POINTL ptl;
    ptl.x = pos.x();
    // flip y (global) coordinate
    ptl.y = QApplication::desktop()->height() - (pos.y() + 1);
    WinMapWindowPoints( HWND_DESKTOP, winId(), &ptl, 1 );
    // flip y (local) coordinate
    ptl.y = height() - (ptl.y + 1);
    return QPoint( ptl.x, ptl.y );
}

void QWidget::setFontSys( QFont *f )
{
//@@TODO (dmik): should this do something on OS/2?
//@@TODO (dmik): remove?
//    QInputContext::setFont( this, (f ? *f : font()) );
}

void QWidget::setMicroFocusHint(int x, int y, int width, int height, bool text, QFont *f)
{
//@@TODO (dmik): do we need to create a caret (cursor) in OS/2 also?
//  currently, try to do so (which can be useful for 3rd-party apps
//  that are interested in the current cursor position, i.e. where the user
//  input is now taking place.
    // flip y coordinate
    int fy = crect.height() - (y + height);
    WinCreateCursor( winId(), x, fy, width, height, CURSOR_SOLID, NULL );

//@@TODO (dmik): remove?
//    if ( text )
//	QInputContext::setFocusHint( x, y, width, height, this );
    setFontSys( f );

    if ( QRect( x, y, width, height ) != microFocusHint() )
	extraData()->micro_focus_hint.setRect( x, y, width, height );
}

void QWidget::resetInputContext()
{
//@@TODO (dmik): remove?
//    QInputContext::accept();
}

void QWidget::setBackgroundColorDirect( const QColor &color )
{
    bg_col = color;
    if ( extra && extra->bg_pix ) {		// kill the background pixmap
	delete extra->bg_pix;
	extra->bg_pix = 0;
    }
}


static int allow_null_pixmaps = 0;


void QWidget::setBackgroundPixmapDirect( const QPixmap &pixmap )
{
    QPixmap old;
    if ( extra && extra->bg_pix )
	old = *extra->bg_pix;
    if ( !allow_null_pixmaps && pixmap.isNull() ) {
	if ( extra && extra->bg_pix ) {
	    delete extra->bg_pix;
	    extra->bg_pix = 0;
	}
    } else {
	if ( extra && extra->bg_pix )
	    delete extra->bg_pix;
	else
	    createExtra();
	extra->bg_pix = new QPixmap( pixmap );
    }
}


void QWidget::setBackgroundEmpty()
{
    allow_null_pixmaps++;
    setErasePixmap(QPixmap());
    allow_null_pixmaps--;
}

extern void qt_set_cursor( QWidget *, const QCursor & ); // qapplication_pm.cpp

void QWidget::setCursor( const QCursor &cursor )
{
    if ( cursor.handle() != arrowCursor.handle()
	 || (extra && extra->curs) ) {
	createExtra();
	delete extra->curs;
	extra->curs = new QCursor(cursor);
    }
    setWState( WState_OwnCursor );
    qt_set_cursor( this, QWidget::cursor() );
}

void QWidget::unsetCursor()
{
    if ( extra ) {
	delete extra->curs;
	extra->curs = 0;
    }
    if ( !isTopLevel() )
	clearWState( WState_OwnCursor );
    qt_set_cursor( this, cursor() );
}

void QWidget::setCaption( const QString &caption )
{
    if ( QWidget::caption() == caption )
	return; // for less flicker
    topData()->caption = caption;

    QCString cap = caption.local8Bit();
    WinSetWindowText( winFId(), cap );

    HSWITCH swEntry = topData()->swEntry;
    if ( swEntry ) {
        SWCNTRL swc;
        WinQuerySwitchEntry( swEntry, &swc );
        strncpy( swc.szSwtitle, cap, sizeof(swc.szSwtitle)-1 );
        swc.szSwtitle [sizeof(swc.szSwtitle)-1] = 0;
        WinChangeSwitchEntry( swEntry, &swc );
    }

    QEvent e( QEvent::CaptionChange );
    QApplication::sendEvent( this, &e );
}

void QWidget::setIcon( const QPixmap &pixmap )
{
    QTLWExtra* x = topData();
    delete x->icon;
    x->icon = 0;
    HPOINTER oldIcon = x->pmIcon;
    x->pmIcon = 0;
    
    if ( !pixmap.isNull() ) {			// valid icon
	x->icon = new QPixmap( pixmap );
        if ( isTopLevel() )
            x->pmIcon = QPixmap::createIcon( FALSE, 0, 0, &pixmap, &pixmap );
    }

    if ( isTopLevel() )
        WinSendMsg( x->fId, WM_SETICON, MPFROMLONG( x->pmIcon ), 0 );
    
    if ( oldIcon )
        WinDestroyPointer( oldIcon ); 
    
    QEvent e( QEvent::IconChange );
    QApplication::sendEvent( this, &e );
}


void QWidget::setIconText( const QString &iconText )
{
    topData()->iconText = iconText;
}

QCursor *qt_grab_cursor()
{
    return mouseGrbCur;
}

void QWidget::grabMouse()
{
    if ( !qt_nograb() ) {
	if ( mouseGrb )
	    mouseGrb->releaseMouse();
	WinSetCapture( HWND_DESKTOP, winFId() );
	mouseGrb = this;
    }
}

void QWidget::grabMouse( const QCursor &cursor )
{
    if ( !qt_nograb() ) {
	if ( mouseGrb )
	    mouseGrb->releaseMouse();
	WinSetCapture( HWND_DESKTOP, winFId() );
	mouseGrbCur = new QCursor( cursor );
	WinSetPointer( HWND_DESKTOP, mouseGrbCur->handle() );
	mouseGrb = this;
    }
}

void QWidget::releaseMouse()
{
    if ( !qt_nograb() && mouseGrb == this ) {
	WinSetCapture( HWND_DESKTOP, 0 );
	if ( mouseGrbCur ) {
	    delete mouseGrbCur;
	    mouseGrbCur = 0;
	}
	mouseGrb = 0;
    }
}

void QWidget::grabKeyboard()
{
    if ( !qt_nograb() ) {
	if ( keyboardGrb )
	    keyboardGrb->releaseKeyboard();
	keyboardGrb = this;
    }
}

void QWidget::releaseKeyboard()
{
    if ( !qt_nograb() && keyboardGrb == this )
	keyboardGrb = 0;
}


QWidget *QWidget::mouseGrabber()
{
    return mouseGrb;
}

QWidget *QWidget::keyboardGrabber()
{
    return keyboardGrb;
}

void QWidget::setActiveWindow()
{
    WinSetActiveWindow( HWND_DESKTOP, topLevelWidget()->winFId() );
}

void QWidget::update()
{
    update( 0, 0, -1, -1 );
}

void QWidget::update( int x, int y, int w, int h )
{
    if ( w && h &&
	 (widget_state & (WState_Visible|WState_BlockUpdates)) == WState_Visible ) {
	if ( w < 0 )
	    w = crect.width() - x;
	if ( h < 0 )
	    h = crect.height() - y;

        // flip y coordinate
        y = crect.height() - (y + h);
        RECTL rcl = { x, y, x + w, y + h };

#if !defined (QT_PM_NO_WIDGETMASK)
        WinInvalidateRect( winId(), &rcl, FALSE );
#else
        // WinInvalidateRect() has such a "feature" that children are not
        // actually extracted from the window's update region when it has
        // the WS_CLIPCHILDREN flag, meaning that every child will anyway
        // receive a WM_PAINT message if its visible region intersects with
        // the update region of its parent, with its update region set to
        // that intersection. This in turn means that if we invalidate the
        // window completely, all its visible children wil completely repaint
        // themselves, what is not we want. The solution is to manually
        // substract children from the region we want to be updated.
        ULONG wstyle = WinQueryWindowULong( winId(), QWL_STYLE );
        if ( (wstyle & WS_CLIPCHILDREN) && children() ) {
            HPS lhps = hps;
            if ( !lhps ) lhps = qt_display_ps();
            HRGN hrgn = GpiCreateRegion( lhps, 1, &rcl );
            HRGN whrgn = GpiCreateRegion( lhps, 0, NULL );
            int hgt = crect.height();

            QObjectListIt it(*children());
            register QObject *object;
            QWidget *w;
            while ( it ) {
                object = it.current();
                ++it;
                if ( object->isWidgetType() ) {
                    w = (QWidget*)object;
                    if ( !w->isTopLevel() && w->isShown() ) {
                        const QRect &r = w->crect;
                        rcl.xLeft = r.left();
                        rcl.yBottom = hgt - (r.bottom() + 1);
                        rcl.xRight = r.right() + 1;
                        rcl.yTop = hgt - r.top();
                        GpiSetRegion( lhps, whrgn, 1, &rcl );
                        GpiCombineRegion( lhps, hrgn, hrgn, whrgn, CRGN_DIFF );
                    }
                }
            }
            GpiDestroyRegion( lhps, whrgn );
            WinInvalidateRegion( winId(), hrgn, FALSE );
            GpiDestroyRegion( lhps, hrgn );
        } else {
            WinInvalidateRect( winId(), &rcl, FALSE );
        }
#endif            
    }
}

void QWidget::repaint( int x, int y, int w, int h, bool erase )
{
    if ( (widget_state & (WState_Visible|WState_BlockUpdates)) == WState_Visible ) {
#if defined (DEBUG_REPAINTRESIZE)
        qDebug( "repaint(): [%s/%s/%08X] %d,%d; %d,%d erase: %d resizeNoErase: %d",
                name(), className(), widget_flags, x, y, w, h, erase,
                (testWFlags( WResizeNoErase ) != 0) );
#endif
	if ( w < 0 )
	    w = crect.width()  - x;
	if ( h < 0 )
	    h = crect.height() - y;
	QRect r( x, y, w, h );
	if ( r.isEmpty() )
	    return; // nothing to do

        // flip y coordinate
        int fy = crect.height() - (y + h);
        RECTL rcl = { x, fy, x + w, fy + h };
        WinValidateRect( winId(), &rcl, FALSE );

        Q_ASSERT( !WinQueryWindowULong( winId(), QWL_QTCLIPRGN ) );
        QRegion reg;
        if ( r != rect() ) {
            reg = QRegion( r );
            WinSetWindowULong( winId(), QWL_QTCLIPRGN, (ULONG) &reg );
        }

	QPaintEvent e( r, erase );
	if ( erase )
	    this->erase( x, y, w, h );
	QApplication::sendEvent( this, &e );

        WinSetWindowULong( winId(), QWL_QTCLIPRGN, 0 );
    }
}

void QWidget::repaint( const QRegion& reg, bool erase )
{
    if ( (widget_state & (WState_Visible|WState_BlockUpdates)) == WState_Visible ) {
#if defined (DEBUG_REPAINTRESIZE)
        qDebug( "repaint(): [%s/%s/%08X] <region> erase: %d resizeNoErase: %d",
                name(), className(), widget_flags, erase,
                (testWFlags( WResizeNoErase ) != 0) );
#endif
        // convert region y coordinates from Qt to GPI (see qregion_pm.cpp)
        WinValidateRegion( winId(), reg.handle( height() ), FALSE );

        Q_ASSERT( !WinQueryWindowULong( winId(), QWL_QTCLIPRGN ) );
        WinSetWindowULong( winId(), QWL_QTCLIPRGN, (ULONG) &reg );

	QPaintEvent e( reg, erase );
	if ( erase )
	    this->erase( reg );
	QApplication::sendEvent( this, &e );

        WinSetWindowULong( winId(), QWL_QTCLIPRGN, 0 );
    }
}

void QWidget::setWindowState( uint newstate )
{
    uint oldstate = windowState();

    if ( newstate == oldstate )
        return;
    
    ULONG fl = (newstate & WindowActive) ? SWP_ACTIVATE : 0;

    /// @todo (dmik):
    //  Ugrh. I cannot see any good logic in those weird window state
    //  manipulations (see setWindowState() dox, original showMinimized(),
    //  showMaximized(). showFullScreen() and showNormal() implementations).
    //  So, treat all of three flags as exclusive for now (note: the below
    //  code relies on this).
    uint op = 0;
    if ( newstate & WindowFullScreen ) op = WindowFullScreen;
    else if ( newstate & WindowMinimized ) op = WindowMinimized;
    else if ( newstate & WindowMaximized ) op = WindowMaximized;

    if ( op == WindowMinimized )
        fl = SWP_DEACTIVATE;
    
    if ( isTopLevel() && isVisible() ) {
        WId id = winFId();
        QTLWExtra *top = topData();

        // set flag for the WM_WINDOWPOSCHANGED handler in QtFrameProc
        top->in_sendWindowState = 1;
        
	if ( op == WindowMinimized || op == WindowMaximized ) {
            fl |= op == WindowMinimized ? SWP_MINIMIZE : SWP_MAXIMIZE;
            WinSetWindowPos( id, 0, 0, 0, 0, 0, fl );
        } else if ( op == WindowFullScreen ) {
            if ( !(oldstate & (WindowMinimized | WindowMaximized)) ) {
                top->normalGeometry = frameGeometry();
            } else {
                // extract normal geometry from window words
                USHORT x = WinQueryWindowUShort( id, QWS_XRESTORE );
                USHORT y = WinQueryWindowUShort( id, QWS_YRESTORE );
                USHORT w = WinQueryWindowUShort( id, QWS_CXRESTORE );
                USHORT h = WinQueryWindowUShort( id, QWS_CYRESTORE );
                // restore first to update the frame strut
                if ( oldstate & WindowMinimized )
                    WinSetWindowPos( id, 0, 0, 0, 0, 0, SWP_RESTORE );
                // flip y coordinate 
                y = QApplication::desktop()->height() - (y + h);
                top->normalGeometry = QRect( x, y, w, h );
            }
            QRect r = QApplication::desktop()->screenGeometry( this );
            r.addCoords( -top->fleft, -top->fbottom, top->fright, top->ftop );
            WinSetWindowPos( id, 0, r.x(), r.y(), r.width(), r.height(),
                             fl | SWP_SIZE | SWP_MOVE );
        } else {
            if ( oldstate & WindowFullScreen ) {
                QRect r = top->normalGeometry;
                if ( r != frameGeometry() ) {
                    r.addCoords( top->fleft, top->ftop, -top->fright, -top->fbottom );
                    setGeometry( r );
                }
                top->normalGeometry.setWidth( 0 );
            } else {
                QRect r = top->normalGeometry;
                if ( r.isValid() ) {
                    // store normal geometry in window words
                    USHORT x = r.x();
                    USHORT y = r.y();
                    USHORT w = r.width();
                    USHORT h = r.height();
                    // flip y coordinate 
                    y = QApplication::desktop()->height() - (y + h);
                    WinSetWindowUShort( id, QWS_XRESTORE, x );
                    WinSetWindowUShort( id, QWS_YRESTORE, y );
                    WinSetWindowUShort( id, QWS_CXRESTORE, w );
                    WinSetWindowUShort( id, QWS_CYRESTORE, h );
                    top->normalGeometry.setWidth( 0 );
                }
                WinSetWindowPos( id, 0, 0, 0, 0, 0, fl | SWP_RESTORE );
            }
        }
        
        top->in_sendWindowState = 0;
    }

    widget_state &= ~(WState_Minimized | WState_Maximized | WState_FullScreen);
    if (newstate & WindowMinimized)
        widget_state |= WState_Minimized;
    if (newstate & WindowMaximized)
        widget_state |= WState_Maximized;
    if (newstate & WindowFullScreen)
        widget_state |= WState_FullScreen;
        
    QEvent e(QEvent::WindowStateChange);
    QApplication::sendEvent(this, &e);
}


/*
  \internal
  Platform-specific part of QWidget::hide().
*/

void QWidget::hideWindow()
{
    deactivateWidgetCleanup();
    HWND id = winFId();
    
#if defined (QT_PM_NO_WIDGETMASK)
    WinShowWindow( id, FALSE );
#else        
    qt_WinSetWindowPos( id, 0, 0, 0, 0, 0, SWP_HIDE );
#endif
    
    if ( isTopLevel() )
        WinSetWindowPos( id, 0, 0, 0, 0, 0, SWP_DEACTIVATE );

    HSWITCH swEntry = topData()->swEntry;
    if ( swEntry ) {
        SWCNTRL swc;
        WinQuerySwitchEntry( swEntry, &swc );
        swc.uchVisibility = SWL_INVISIBLE;
        WinChangeSwitchEntry( swEntry, &swc );
    }
}


/*
  \internal
  Platform-specific part of QWidget::show().
*/

void QWidget::showWindow()
{
#if defined (QT_PM_NO_WIDGETMASK)
    WinShowWindow( winFId(), TRUE );
#else        
    qt_WinSetWindowPos( winFId(), 0, 0, 0, 0, 0, SWP_SHOW );
#endif
    
    ULONG fl = 0;
    if ( isTopLevel() ) {
        // we do this check here because setWindowState() does not actually
        // change the window state when the window is not visible, assuming
        // it will be done here.
	if (testWState(WState_Minimized))
	    fl |= SWP_MINIMIZE;
	else if (testWState(WState_Maximized))
	    fl |= SWP_MAXIMIZE;
        else if (testWState(WState_FullScreen)) {
            QTLWExtra *top = topData();
            topData()->normalGeometry = QRect( pos(), size() );
            QRect r = QApplication::desktop()->screenGeometry( this );
            r.addCoords( -top->fleft, -top->fbottom, top->fright, top->ftop );
            WinSetWindowPos( winFId(), 0, r.x(), r.y(), r.width(), r.height(),
                             fl | SWP_SIZE | SWP_MOVE );
        }

        if (
            !testWFlags(WType_Popup | WStyle_Tool) &&
            (!testWFlags(WType_Dialog) || !parentWidget())
        ) {
            HSWITCH &swEntry = topData()->swEntry;
            if ( !swEntry ) {
                // lazily create a new window list entry
                HWND id = winFId(); // frame handle, if any
                PID pid;
                WinQueryWindowProcess( id, &pid, NULL );
                SWCNTRL swc;
                memset( &swc, 0, sizeof(SWCNTRL) );
                swc.hwnd = id;
                swc.idProcess = pid;
                swc.uchVisibility = SWL_VISIBLE;
                swc.fbJump = SWL_JUMPABLE;
                WinQueryWindowText( id, sizeof(swc.szSwtitle), swc.szSwtitle );
                swEntry = WinAddSwitchEntry( &swc );
            } else {
                SWCNTRL swc;
                WinQuerySwitchEntry( swEntry, &swc );
                swc.uchVisibility = SWL_VISIBLE;
                WinChangeSwitchEntry( swEntry, &swc );
            }
        }
    }
    if (!testWFlags(WStyle_Tool) && !isPopup())
        fl |= SWP_ACTIVATE;
    if ( fl )
        WinSetWindowPos( winFId(), 0, 0, 0, 0, 0, fl );
}

void QWidget::raise()
{
    QWidget *p = parentWidget();
    if ( p && p->childObjects && p->childObjects->findRef(this) >= 0 )
	p->childObjects->append( p->childObjects->take() );
//@@TODO (dmik): remove? SWP_ZORDER doesn't cause activation in OS/2...
//    uint f = ( isPopup() || testWFlags(WStyle_Tool) ) ? SWP_NOACTIVATE : 0;

#if defined (QT_PM_NO_WIDGETMASK)
    WinSetWindowPos( winFId(), HWND_TOP, 0, 0, 0, 0, SWP_ZORDER );
#else        
    qt_WinSetWindowPos( winFId(), HWND_TOP, 0, 0, 0, 0, SWP_ZORDER );
#endif
}

void QWidget::lower()
{
    QWidget *p = parentWidget();
    if ( p && p->childObjects && p->childObjects->findRef(this) >= 0 )
	p->childObjects->insert( 0, p->childObjects->take() );
//@@TODO (dmik): remove? SWP_ZORDER doesn't cause activation in OS/2...
//    uint f = ( isPopup() || testWFlags(WStyle_Tool) ) ? SWP_NOACTIVATE : 0;

#if defined (QT_PM_NO_WIDGETMASK)
    WinSetWindowPos( winFId(), HWND_BOTTOM, 0, 0, 0, 0, SWP_ZORDER );
#else        
    qt_WinSetWindowPos( winFId(), HWND_BOTTOM, 0, 0, 0, 0, SWP_ZORDER );
#endif
}

void QWidget::stackUnder( QWidget* w )
{
    QWidget *p = parentWidget();
    if ( !w || isTopLevel() || p != w->parentWidget() || this == w )
	return;
    if ( p && p->childObjects && p->childObjects->findRef(w) >= 0 && p->childObjects->findRef(this) >= 0 ) {
	p->childObjects->take();
	p->childObjects->insert( p->childObjects->findRef(w), this );
    }
#if defined (QT_PM_NO_WIDGETMASK)
    WinSetWindowPos( winId(), w->winId(), 0, 0, 0, 0, SWP_ZORDER );
#else        
    qt_WinSetWindowPos( winId(), w->winId(), 0, 0, 0, 0, SWP_ZORDER );
#endif
}

//
// The internal qPMRequestConfig, defined in qeventloop_pm.cpp, stores move,
// resize and setGeometry requests for a widget that is already
// processing a config event. The purpose is to avoid recursion.
//
void qPMRequestConfig( WId, int, int, int, int, int );

void QWidget::internalSetGeometry( int x, int y, int w, int h, bool isMove )
{
    if ( extra ) {				// any size restrictions?
	w = QMIN(w,extra->maxw);
	h = QMIN(h,extra->maxh);
	w = QMAX(w,extra->minw);
	h = QMAX(h,extra->minh);
    }
    if ( w < 1 )				// invalid size
	w = 1;
    if ( h < 1 )
	h = 1;
    QSize  oldSize( size() );
    QPoint oldPos( pos() );
    if ( !isTopLevel() )
	isMove = (crect.topLeft() != QPoint( x, y ));
    bool isResize = w != oldSize.width() || h != oldSize.height();
    if ( isMove == FALSE && isResize == FALSE )
	return;
    clearWState(WState_Maximized);
    clearWState(WState_FullScreen);
    if ( testWState(WState_ConfigPending) ) {	// processing config event
	qPMRequestConfig( winId(), isMove ? 2 : 1, x, y, w, h );
    } else {
	setWState( WState_ConfigPending );
	if ( isTopLevel() ) {
/// @todo (dmik) optimize: use extra->topextra directly (after updating fstrut
//  if necessary) instead of calling frameGeometry()
	    QRect fr( frameGeometry() );
            int fx = fr.left() + x - crect.left();
            int fy = fr.top() + y - crect.top();
            int fw = fr.width() + w - crect.width();
            int fh = fr.height() + h - crect.height();
            // flip y coordinate
            fy = QApplication::desktop()->height() - (fy + fh);
	    crect.setRect( x, y, w, h );
            WinSetWindowPos( winFId(), 0, fx, fy, fw, fh, SWP_MOVE | SWP_SIZE );
	} else {
#if defined (QT_PM_NO_WIDGETMASK)
            // flip y coordinate
            int fy = parentWidget()->height() - (y + h);
	    crect.setRect( x, y, w, h );
            WinSetWindowPos( winId(), 0, x, fy, w, h, SWP_MOVE | SWP_SIZE );
#else
            // When WS_CLIPCHILDREN and WS_CLIPSIBLINGS are not used,
            // WinSetWindowPos() does not correctly update involved windows.
            // The fix is to do it ourselves, taking clip regions into account.
            
            QWidget *parent = parentWidget();
            const int ph = parent->height();
            // flip y coordinate
            int fy = ph - (y + h);
            // set new and old rectangles
            const RECTL rcls [2] = {
                // new (y flipped, relative to parent)
                { x, fy, x + w, fy + h },    
                // old (y flipped, relative to parent)
                { crect.left(), ph - (crect.bottom() + 1),
                  crect.right() + 1, ph - crect.top() }
            };
            const RECTL &rclNew = rcls [0];
            const RECTL &rclOld = rcls [1];
            // delta to shift coordinate space from parent to this widget
            POINTL ptlToSelf = { -x, -fy };
            // update crect and move the widget w/o redrawing
	    crect.setRect( x, y, w, h );
            WinSetWindowPos( winId(), 0, x, fy, w, h,
                             SWP_MOVE | SWP_SIZE | SWP_NOREDRAW );
            // use parent PS for blitting  
            HPS hps = WinGetPS( parent->winId() );
            // get old and new clip regions (relative to parent)
            HRGN hrgnOld = GpiCreateRegion( hps, 1, &rclOld );
            HRGN hrgnNew = GpiCreateRegion( hps, 1, &rclNew );
            if ( WinQueryClipRegion( winId(), 0 ) != QCRGN_NO_CLIP_REGION ) {
                HRGN hrgnTemp = GpiCreateRegion( hps, 0, NULL );
                // old (clipped to the old rect)
                WinQueryClipRegion( winId(), hrgnTemp );
                GpiOffsetRegion( hps, hrgnTemp, (PPOINTL) &rclOld );
                GpiCombineRegion( hps, hrgnOld, hrgnTemp, hrgnOld, CRGN_AND ); 
                // new (clipped to the new rect)
                WinQueryClipRegion( winId(), hrgnTemp );
                if ( oldSize.height() != h ) {
                    // keep the clip region top-left aligned by adding the
                    // height delta (new size - old size)
                    POINTL ptl = { 0, h - oldSize.height() }; 
                    GpiOffsetRegion( hps, hrgnTemp, &ptl );
                    WinSetClipRegion( winId(), hrgnTemp );
                }
                GpiOffsetRegion( hps, hrgnTemp, (PPOINTL) &rclNew );
                GpiCombineRegion( hps, hrgnNew, hrgnTemp, hrgnNew, CRGN_AND );
                GpiDestroyRegion( hps, hrgnTemp );
            }
            // the rest is useful only when the widget is visible
            if ( isVisible() ) {
                // create affected region (old + new, relative to widget)
                HRGN hrgnAff = GpiCreateRegion( hps, 0, NULL );
                GpiCombineRegion( hps, hrgnAff, hrgnOld, hrgnNew, CRGN_OR );
                GpiOffsetRegion( hps, hrgnAff, &ptlToSelf );
                // get bounding rectangle of affected region
                RECTL rclAff;
                GpiQueryRegionBox( hps, hrgnAff, &rclAff );
                // get region of obstacles limited to affected rectangle
                HRGN hrgnObst = GpiCreateRegion( hps, 0, NULL );
                qt_WinProcessWindowObstacles( winId(), &rclAff, hrgnObst, CRGN_OR,
                                              PWO_Sibings | PWO_Ancestors |
                                              PWO_Screen | PWO_TopLevel );
                // shift region of obstacles and affected region back to
                // parent coords
                GpiOffsetRegion( hps, hrgnObst, (PPOINTL) &rclNew );
                GpiOffsetRegion( hps, hrgnAff, (PPOINTL) &rclNew );
                // get parent bounds (clip region or rect)
                HRGN hrgnUpd = GpiCreateRegion( hps, 0, NULL );
                qt_WinQueryClipRegionOrRect( parent->winId(), hrgnUpd );
                // add parts of old region beyond parent bounds to
                // region of obstacles
                GpiCombineRegion( hps, hrgnOld, hrgnOld, hrgnUpd, CRGN_DIFF );
                GpiCombineRegion( hps, hrgnObst, hrgnObst, hrgnOld, CRGN_OR );
                // substract region of obstacles from affected region
                GpiCombineRegion( hps, hrgnAff, hrgnAff, hrgnObst, CRGN_DIFF );
                // remember it as parent update region (need later)
                GpiCombineRegion( hps, hrgnUpd, hrgnAff, 0, CRGN_COPY );
                // copy region of obstacles to delta region and shift it by
                // delta (note: movement is considered to be top-left aligned)
                HRGN hrgnDelta = GpiCreateRegion( hps, 0, NULL );
                GpiCombineRegion( hps, hrgnDelta, hrgnObst, 0, CRGN_COPY );
                POINTL ptlDelta = { rclNew.xLeft - rclOld.xLeft,
                                    rclNew.yTop - rclOld.yTop };
                GpiOffsetRegion( hps, hrgnDelta, &ptlDelta );
                // substract region of obstacles from delta region to get
                // pure delta
                GpiCombineRegion( hps, hrgnDelta, hrgnDelta, hrgnObst, CRGN_DIFF );
                // calculate minimal rectangle to blit (top-left aligned)
                int minw = QMIN( oldSize.width(), w );
                int minh = QMIN( oldSize.height(), h );
                POINTL blitPtls [4] = {
                    // target (new)
                    { rclNew.xLeft, rclNew.yTop - minh },
                    { rclNew.xLeft + minw, rclNew.yTop },
                    // source (old)
                    { rclOld.xLeft, rclOld.yTop - minh },
                };
                // proceed with blitting only if target and source rects differ
                if ( blitPtls[0].x !=  blitPtls[2].x ||
                     blitPtls[0].y !=  blitPtls[2].y )
                {
                    // Substract delta region from affected region (to minimize
                    // flicker)
                    GpiCombineRegion( hps, hrgnAff, hrgnAff, hrgnDelta, CRGN_DIFF );
                    // set affected region to parent PS
                    GpiSetClipRegion( hps, hrgnAff, NULL );
                    // blit minimal rectangle
                    GpiBitBlt( hps, hps, 3, blitPtls, ROP_SRCCOPY, BBO_IGNORE );
                    GpiSetClipRegion( hps, 0, NULL );
                }
                // substract new widget region from the parent update region
                // and invalidate it (with underlying children)
                GpiCombineRegion( hps, hrgnUpd, hrgnUpd, hrgnNew, CRGN_DIFF );
                qt_WinInvalidateRegionEx( parent->winId(), hrgnUpd,
                                          WinQueryWindow( winId(), QW_NEXT ),
                                          HWND_BOTTOM );
                // intersect pure delta region with new region
                // (to detect areas clipped off to minimize flicker when blitting)
                GpiCombineRegion( hps, hrgnDelta, hrgnDelta, hrgnNew, CRGN_AND );
                // substract blitted rectangle from new region
                GpiSetRegion( hps, hrgnAff, 1, (PRECTL) &blitPtls );
                GpiCombineRegion( hps, hrgnNew, hrgnNew, hrgnAff, CRGN_DIFF );
                // combine the rest with intersected delta region
                GpiCombineRegion( hps, hrgnNew, hrgnNew, hrgnDelta, CRGN_OR );
                // shift the result back to widget coords and invalidate
                GpiOffsetRegion( hps, hrgnNew, &ptlToSelf );
                WinInvalidateRegion( winId(), hrgnNew, TRUE );
                // free resources
                GpiDestroyRegion( hps, hrgnDelta );
                GpiDestroyRegion( hps, hrgnUpd );
                GpiDestroyRegion( hps, hrgnObst );
                GpiDestroyRegion( hps, hrgnAff );
            }
            // free resources
            GpiDestroyRegion( hps, hrgnOld );
            GpiDestroyRegion( hps, hrgnNew );
            WinReleasePS( hps );
#endif
	}
	clearWState( WState_ConfigPending );
    }

    if ( isVisible() ) {
	if ( isMove && pos() != oldPos ) {
	    QMoveEvent e( pos(), oldPos );
	    QApplication::sendEvent( this, &e );
	}
	if ( isResize ) {
	    QResizeEvent e( size(), oldSize );
	    QApplication::sendEvent( this, &e );
	    if ( !testWFlags( WStaticContents ) )
		repaint( testWFlags(WNoAutoErase) != WNoAutoErase );
	}
    } else {
	if ( isMove && pos() != oldPos )
	    QApplication::postEvent( this,
                new QMoveEvent( pos(), oldPos ) );
	if ( isResize )
	    QApplication::postEvent( this,
                new QResizeEvent( size(), oldSize ) );
    }
}

void QWidget::setMinimumSize( int minw, int minh )
{
#if defined(QT_CHECK_RANGE)
    if ( minw < 0 || minh < 0 )
	qWarning("QWidget::setMinimumSize: The smallest allowed size is (0,0)");
#endif
    createExtra();
    if ( extra->minw == minw && extra->minh == minh )
	return;
    extra->minw = minw;
    extra->minh = minh;
    if ( minw > width() || minh > height() ) {
	bool resized = testWState( WState_Resized );
	resize( QMAX(minw,width()), QMAX(minh,height()) );
	if ( !resized )
	    clearWState( WState_Resized ); //not a user resize
    }
    updateGeometry();
}

void QWidget::setMaximumSize( int maxw, int maxh )
{
#if defined(QT_CHECK_RANGE)
    if ( maxw > QWIDGETSIZE_MAX || maxh > QWIDGETSIZE_MAX ) {
	qWarning("QWidget::setMaximumSize: The largest allowed size is (%d,%d)",
		 QWIDGETSIZE_MAX, QWIDGETSIZE_MAX );
	maxw = QMIN( maxw, QWIDGETSIZE_MAX );
	maxh = QMIN( maxh, QWIDGETSIZE_MAX );
    }
    if ( maxw < 0 || maxh < 0 ) {
	qWarning("QWidget::setMaximumSize: (%s/%s) Negative sizes (%d,%d) "
		"are not possible",
		name( "unnamed" ), className(), maxw, maxh );
	maxw = QMAX( maxw, 0 );
	maxh = QMAX( maxh, 0 );
    }
#endif
    createExtra();
    if ( extra->maxw == maxw && extra->maxh == maxh )
	return;
    extra->maxw = maxw;
    extra->maxh = maxh;
    if ( maxw < width() || maxh < height() ) {
	bool resized = testWState( WState_Resized );
	resize( QMIN(maxw,width()), QMIN(maxh,height()) );
	if ( !resized )
	    clearWState( WState_Resized ); //not a user resize
    }
    updateGeometry();
}

void QWidget::setSizeIncrement( int w, int h )
{
    createTLExtra();
    extra->topextra->incw = w;
    extra->topextra->inch = h;
}

void QWidget::setBaseSize( int w, int h )
{
    createTLExtra();
    extra->topextra->basew = w;
    extra->topextra->baseh = h;
}

extern void qt_erase_background( HPS, int, int, int, int,
			 const QColor &, const QPixmap *, int, int, int );

void QWidget::erase( int x, int y, int w, int h )
{
    // SIMILAR TO region ERASE BELOW

    if ( backgroundMode()==NoBackground )
	return;
    if ( w < 0 )
	w = crect.width() - x;
    if ( h < 0 )
	h = crect.height() - y;

    HPS lhps;
    bool tmphps;

    if( QPainter::redirect( this ) ) {
	tmphps = FALSE;
	lhps = QPainter::redirect( this )->handle();
	Q_ASSERT( lhps );
    } else if ( !hps ) {
	tmphps = TRUE;
	lhps = getTargetPS( ClipAll );
    } else {
	tmphps = FALSE;
	lhps = hps;
    }

    QPoint offset = backgroundOffset();
    int ox = offset.x();
    int oy = offset.y();

    qt_erase_background(
        lhps, x, y, w, h, bg_col, backgroundPixmap(),
        ox, oy, crect.height()
    );

    if ( tmphps )
	WinReleasePS( lhps );
}

void QWidget::erase( const QRegion& rgn )
{
    // SIMILAR TO rect ERASE ABOVE

    if ( backgroundMode()==NoBackground )
	return;

    HPS lhps;
    bool tmphps;

    if( QPainter::redirect( this ) ) {
	tmphps = FALSE;
	lhps = QPainter::redirect( this )->handle();
	Q_ASSERT( lhps );
    } else if ( !hps ) {
	tmphps = TRUE;
	lhps = getTargetPS( ClipAll );
    } else {
	tmphps = FALSE;
	lhps = hps;
    }

    HRGN oldRegion = GpiQueryClipRegion( lhps );
    HRGN newRegion = 0;
    if ( oldRegion ) {
        GpiSetClipRegion( lhps, 0, NULL );
        newRegion = GpiCreateRegion( lhps, 0, NULL );
        GpiCombineRegion( lhps, newRegion, oldRegion, rgn.handle( height() ),
                          CRGN_AND );
    } else {
	newRegion = rgn.handle( height() );
    }
    GpiSetClipRegion( lhps, newRegion, NULL );

    QPoint offset = backgroundOffset();
    int ox = offset.x();
    int oy = offset.y();

    qt_erase_background(
        lhps, 0, 0, crect.width(), crect.height(),
        bg_col, backgroundPixmap(), ox, oy, crect.height()
    );

    GpiSetClipRegion( lhps, oldRegion, NULL );

    if ( oldRegion )
	GpiDestroyRegion( lhps, newRegion );
    if ( tmphps )
	WinReleasePS( lhps );
}

#if defined (QT_PM_NO_WIDGETMASK)        

// helper function to extract regions of all windows that overlap the given
// hwnd subject to their z-order (excluding children of hwnd) from the given
// hrgn. hps is the presentation space of hrgn.
void qt_WinExcludeOverlappingWindows( HWND hwnd, HPS hps, HRGN hrgn )
{
    HRGN vr = GpiCreateRegion( hps, 0, NULL );
    RECTL r;

    // enumerate all windows that are on top of this hwnd
    HWND id = hwnd, deskId = QApplication::desktop()->winId();
    do {
        HWND i = id;
        while( (i = WinQueryWindow( i, QW_PREV )) ) {
            if ( WinIsWindowVisible( i ) ) {
                WinQueryWindowRect( i, &r );
                WinMapWindowPoints( i, hwnd, (PPOINTL) &r, 2 );
                GpiSetRegion( hps, vr, 1, &r );
                GpiCombineRegion( hps, hrgn, hrgn, vr, CRGN_DIFF );
            }
        }
        id = WinQueryWindow( id, QW_PARENT );
    } while( id != deskId );

    GpiDestroyRegion( hps, vr );
}

// helper function to scroll window contents. WinScrollWindow() is a bit
// buggy and corrupts update regions sometimes (which leaves some outdated
// areas unpainted after scrolling), so we reimplement its functionality in
// this function. dy and clip->yBottom/yTop should be GPI coordinates, not Qt.
// all clip coordinates are inclusive.
void qt_WinScrollWindowWell( HWND hwnd, LONG dx, LONG dy, const PRECTL clip = NULL )
{
    WinLockVisRegions( HWND_DESKTOP, TRUE );

    HPS lhps = WinGetClipPS(
        hwnd, HWND_TOP,
        PSF_LOCKWINDOWUPDATE | PSF_CLIPSIBLINGS
    );
    if ( clip )
        GpiIntersectClipRectangle( lhps, clip );

    // remember the update region and validate it. it will be shifted and
    // invalidated again later
    HRGN update = GpiCreateRegion( lhps, 0, NULL );
    WinQueryUpdateRegion( hwnd, update );
    WinValidateRegion( hwnd, update, TRUE );

    POINTL ptls[4];
    RECTL &sr = *(PRECTL) &ptls[2];
    RECTL &tr = *(PRECTL) &ptls[0];

    // get the source rect for scrolling
    GpiQueryClipBox( lhps, &sr );
    sr.xRight++; sr.yTop++; // inclusive -> exclusive

    // get the visible region ignoring areas covered by children
    HRGN visible = GpiCreateRegion( lhps, 1, &sr );
    qt_WinExcludeOverlappingWindows( hwnd, lhps, visible );

    // scroll visible region rectangles separately to avoid the flicker
    // that could be produced by scrolling parts of overlapping windows
    RGNRECT ctl;
    ctl.ircStart = 1;
    ctl.crc = 0;
    ctl.crcReturned = 0;
    if ( dx >= 0 ) {
        if ( dy >= 0 ) ctl.ulDirection = RECTDIR_RTLF_TOPBOT;
        else ctl.ulDirection = RECTDIR_RTLF_BOTTOP;
    } else {
        if ( dy >= 0 ) ctl.ulDirection = RECTDIR_LFRT_TOPBOT;
        else ctl.ulDirection = RECTDIR_LFRT_BOTTOP;
    }
    GpiQueryRegionRects( lhps, visible, NULL, &ctl, NULL );
    ctl.crc = ctl.crcReturned;
    int rclcnt = ctl.crcReturned;
    PRECTL rcls = new RECTL[rclcnt];
    GpiQueryRegionRects( lhps, visible, NULL, &ctl, rcls );
    PRECTL r = rcls;
    for ( int i = 0; i < rclcnt; i++, r++ ) {
        // source rect
        sr = *r;
        // target rect
        tr.xLeft = sr.xLeft + dx;
        tr.xRight = sr.xRight + dx;
        tr.yBottom = sr.yBottom + dy;
        tr.yTop = sr.yTop + dy;
        GpiBitBlt( lhps, lhps, 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
    }
    delete [] rcls;

    // make the scrolled version of the visible region
    HRGN scrolled = GpiCreateRegion( lhps, 0, NULL );
    GpiCombineRegion( lhps, scrolled, visible, 0, CRGN_COPY );
    // invalidate the initial update region
    GpiCombineRegion( lhps, scrolled, scrolled, update, CRGN_DIFF );
    // shift the region
    POINTL dp = { dx, dy };
    GpiOffsetRegion( lhps, scrolled, &dp );
    // substract scrolled from visible to get invalid areas
    GpiCombineRegion( lhps, scrolled, visible, scrolled, CRGN_DIFF );

    WinInvalidateRegion( hwnd, scrolled, TRUE );

    GpiDestroyRegion( lhps, scrolled );
    GpiDestroyRegion( lhps, visible );
    GpiDestroyRegion( lhps, update );

    WinReleasePS( lhps );

    WinLockVisRegions( HWND_DESKTOP, FALSE );

    WinUpdateWindow( hwnd );
}

#else

/**
 *  \internal
 *  Helper to scroll window contents.
 *  All coordinates are GPI, not Qt.
 */
static void scrollWindow( HWND hwnd, int w, int h,
                          int dx, int dy, const PRECTL clip = NULL )
{
    POINTL ptlDelta = { dx, dy }; 
    
    POINTL ptls[4];
    RECTL &rclSrc = *(PRECTL) &ptls[2];
    RECTL &rclDst = *(PRECTL) &ptls[0];

    if ( clip ) {
        rclSrc = *clip;
    } else {
        rclSrc.xLeft = rclSrc.yBottom = 0;
        rclSrc.xRight = w;
        rclSrc.yTop = h;
    }
    rclDst = rclSrc;
    rclDst.xLeft += dx;
    rclDst.xRight += dx;
    rclDst.yBottom += dy;
    rclDst.yTop += dy;
    
    if ( !clip ) {
        // move all child widgets silently
        SWP swp;
        HWND child = WinQueryWindow( hwnd, QW_BOTTOM );
        if ( child != NULLHANDLE ) {
            for ( ; child != HWND_TOP; child = swp.hwndInsertBehind ) {
                WinQueryWindowPos( child, &swp );
                swp.x += dx;
                swp.y += dy;
                WinSetWindowPos( child, 0, swp.x, swp.y, 0, 0, 
                                 SWP_MOVE | SWP_NOADJUST | SWP_NOREDRAW );
                // WinSetWindowPos() doesn't send WM_MOVE to windows w/o
                // CS_MOVENOTIFY, but having this style for non-toplevel
                // widgets is unwanted, so we send them WM_MOVE manually
                // to let their geometry to be properly updated by
                // QETWidget::translateConfigEvent().
                WinSendMsg( child, WM_MOVE, 0, 0 );
            }
        }
    }
    
    HPS hps = WinGetPS( hwnd );

    // get the current update (invalid) region
    HRGN hrgnInv = GpiCreateRegion( hps, 0, NULL );
    if ( WinQueryUpdateRegion( hwnd, hrgnInv ) != RGN_NULL ) {
        // validate it (since it's no more up-to-date after scrolling)
        WinValidateRegion( hwnd, hrgnInv, clip == NULL ); 
        // scroll it to get the new invalid region
        GpiOffsetRegion( hps, hrgnInv, &ptlDelta );
    }
    
    // get widget bounds (clip region or rect)
    HRGN hrgnClip = GpiCreateRegion( hps, 0, NULL );
    qt_WinQueryClipRegionOrRect( hwnd, hrgnClip );

    if ( clip ) {
        // intersect it with the given clip rect
        HRGN hrgn = GpiCreateRegion( hps, 1, clip );
        GpiCombineRegion( hps, hrgnClip, hrgn, hrgnClip, CRGN_AND );
        GpiDestroyRegion( hps, hrgn );
    }

    // compose a region uncovered after scrolling
    HRGN hrgnUpd = GpiCreateRegion( hps, 0, NULL );
    GpiCombineRegion( hps, hrgnUpd, hrgnClip, 0, CRGN_COPY );
    GpiOffsetRegion( hps, hrgnUpd, &ptlDelta );
    GpiCombineRegion( hps, hrgnUpd, hrgnClip, hrgnUpd, CRGN_DIFF );

    int pwoFlags = PWO_Ancestors | PWO_Sibings | PWO_TopLevel | PWO_Screen;
    if ( clip )
        pwoFlags |= PWO_Children;
    
    // get the region of obstacles
    HRGN hrgnObst = GpiCreateRegion( hps, 0, NULL );
    qt_WinProcessWindowObstacles( hwnd, &rclSrc, hrgnObst, CRGN_OR, pwoFlags );
    // create the delta region of obstacles
    HRGN hrgnDelta = GpiCreateRegion( hps, 0, NULL );
    GpiCombineRegion( hps, hrgnDelta, hrgnObst, 0, CRGN_COPY );
    GpiOffsetRegion( hps, hrgnDelta, &ptlDelta );
    // substract the region of obstaces from the delta region to get pure delta
    GpiCombineRegion( hps, hrgnDelta, hrgnDelta, hrgnObst, CRGN_DIFF );
    // substract obstacles from the clip region 
    GpiCombineRegion( hps, hrgnClip, hrgnClip, hrgnObst, CRGN_DIFF );
    // substract pure delta from the clip region (to reduce flicker) 
    GpiCombineRegion( hps, hrgnClip, hrgnClip, hrgnDelta, CRGN_DIFF );
    // substract the new invalid region from the clip region (to reduce flicker) 
    GpiCombineRegion( hps, hrgnClip, hrgnClip, hrgnInv, CRGN_DIFF );
    
    // scroll the area
    GpiSetClipRegion( hps, hrgnClip, NULL );
    GpiBitBlt( hps, hps, 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
    GpiSetClipRegion( hps, 0, NULL );
    
    // invalidate the delta region (clipped off when blitting)
    WinInvalidateRegion( hwnd, hrgnDelta, clip == NULL );

    // invalidate the region uncovered after scrolling
    WinInvalidateRegion( hwnd, hrgnUpd, clip == NULL );

    // put the new invalid region back 
    WinInvalidateRegion( hwnd, hrgnInv, clip == NULL );
    
    GpiDestroyRegion( hps, hrgnInv );
    GpiDestroyRegion( hps, hrgnDelta );
    GpiDestroyRegion( hps, hrgnObst );
    GpiDestroyRegion( hps, hrgnUpd );
    GpiDestroyRegion( hps, hrgnClip );
    WinReleasePS( hps );
    
    // In order to get the effect of "instant" scrolling, we process WM_PAINT
    // messages pending after invalidation manually instead of calling
    // WinUpdateWindow(). The latter tends to skip WM_PAINT messages under the
    // heavy load of the GUI thread (probably due to the low WM_PAINT priority),
    // which may cause parts of scrolled areas to look "stuck" (not updated in
    // time but with some delay).
    QMSG qmsg;
    while( WinPeekMsg( 0, &qmsg, NULLHANDLE, WM_PAINT, WM_PAINT, PM_REMOVE ) )
        WinDispatchMsg( 0, &qmsg );
}

#endif

void QWidget::scroll( int dx, int dy )
{
    if ( testWState( WState_BlockUpdates ) && !children() )
	return;

#if defined (QT_PM_NO_WIDGETMASK)

    // move non-toplevel children
    if ( children() ) {
        QObjectListIt it(*children());
        register QObject *obj;
        int h = crect.height();
        while ( (obj=it.current()) ) {
            ++it;
            if ( obj->isWidgetType() ) {
                QWidget *w = (QWidget*)obj;
                if ( !w->isTopLevel() ) {
                    WinSetWindowPos(
                        w->winId(), 0,
                        w->crect.left() + dx,
                        // flip y coordinate
                        h - (w->crect.bottom() + dy + 1),
                        0, 0,
                        SWP_MOVE | SWP_NOADJUST | SWP_NOREDRAW
                    );
                    // WinSetWindowPos() doesn't send WM_MOVE to windows w/o
                    // CS_MOVENOTIFY, but having this style for non-toplevel
                    // widgets is unwanted, so we send them WM_MOVE manually
                    // to let their geometry to be properly updated by
                    // QETWidget::translateConfigEvent().
                    WinSendMsg( w->winId(), WM_MOVE, 0, 0 );
                }
            }
        }
    }

    qt_WinScrollWindowWell( winId(), dx, -dy, NULL );
    
#else

    scrollWindow( winId(), width(), height(), dx, -dy, NULL );
    
#endif
}

void QWidget::scroll( int dx, int dy, const QRect& r )
{
    if ( testWState( WState_BlockUpdates ) )
	return;

#if defined (QT_PM_NO_WIDGETMASK)
    
    int h = crect.height();
    // flip y coordinate (all coordinates are inclusive)
    RECTL rcl = { r.left(), h - (r.bottom() + 1), r.right(), h - (r.top() + 1) };
    qt_WinScrollWindowWell( winId(), dx, -dy, &rcl );

#else

    int h = crect.height();
    // flip y coordinate
    RECTL rcl = { r.left(), h - (r.bottom() + 1),
                  r.right() + 1, h - r.top() };
    scrollWindow( winId(), width(), height(), dx, -dy, &rcl );
    
#endif
}

void QWidget::drawText( int x, int y, const QString &str )
{
    if ( testWState(WState_Visible) ) {
	QPainter paint;
	paint.begin( this );
	paint.drawText( x, y, str );
	paint.end();
    }
}


int QWidget::metric( int m ) const
{
    LONG val;
    if ( m == QPaintDeviceMetrics::PdmWidth ) {
	val = crect.width();
    } else if ( m == QPaintDeviceMetrics::PdmHeight ) {
	val = crect.height();
    } else {
	HDC hdc = GpiQueryDevice( qt_display_ps() );
	switch ( m ) {
            case QPaintDeviceMetrics::PdmDpiX:
            case QPaintDeviceMetrics::PdmPhysicalDpiX:
                DevQueryCaps( hdc, CAPS_HORIZONTAL_FONT_RES, 1, &val );
                break;
            case QPaintDeviceMetrics::PdmDpiY:
            case QPaintDeviceMetrics::PdmPhysicalDpiY:
                DevQueryCaps( hdc, CAPS_VERTICAL_FONT_RES, 1, &val );
                break;
            case QPaintDeviceMetrics::PdmWidthMM:
                DevQueryCaps( hdc, CAPS_HORIZONTAL_RESOLUTION, 1, &val );
                val = width() * 1000 / val;
                break;
            case QPaintDeviceMetrics::PdmHeightMM:
                DevQueryCaps( hdc, CAPS_VERTICAL_RESOLUTION, 1, &val );
                val = width() * 1000 / val;
                break;
            case QPaintDeviceMetrics::PdmNumColors:
                DevQueryCaps( hdc, CAPS_COLORS, 1, &val );
                break;
            case QPaintDeviceMetrics::PdmDepth:
                LONG colorInfo [2];
                DevQueryCaps( hdc, CAPS_COLOR_PLANES, 2, colorInfo );
                val = colorInfo [0] * colorInfo [1];
                break;
            default:
                val = 0;
#if defined(QT_CHECK_RANGE)
                qWarning( "QWidget::metric: Invalid metric command" );
#endif
	}
    }
    return val;
}

void QWidget::createSysExtra()
{
}

void QWidget::deleteSysExtra()
{
}

void QWidget::createTLSysExtra()
{
    extra->topextra->fId = 0;
    extra->topextra->swEntry = 0;
    extra->topextra->pmIcon = 0;
    extra->topextra->in_sendWindowState = 0;
}

void QWidget::deleteTLSysExtra()
{
    if ( extra->topextra->pmIcon )
        WinDestroyPointer( extra->topextra->pmIcon );
    if ( extra->topextra->swEntry )
        WinRemoveSwitchEntry( extra->topextra->swEntry );
    // frame window (extra->topextra->fId) is destroyed in
    // destroy() if it exists
}

bool QWidget::acceptDrops() const
{
    return testWState( WState_DND );    
}

void QWidget::setAcceptDrops( bool on )
{
    if ( testWState(WState_DND) != on ) {
        if ( on )
            setWState( WState_DND );
        else
            clearWState( WState_DND );
    }
}

#if !defined (QT_PM_NO_WIDGETMASK)        

// helper for QWidget::setMask()
static void setClipRegion( HWND hwnd, int x, int y, int w, int h, HRGN hrgn,
                           HWND parent )
{
    if ( !WinIsWindowVisible( hwnd ) ) {
        // if the window is hidden, no need to invalidate anything
        WinSetClipRegion( hwnd, hrgn );
        return;
    }

    HPS hps = qt_display_ps();
    const RECTL rcl = { 0, 0, w, h };
    
    // get the old bounds (clip region or rect)
    HRGN hrgnOld = GpiCreateRegion( hps, 0, NULL );
    qt_WinQueryClipRegionOrRect( hwnd, hrgnOld );
    // set the new clip region
    WinSetClipRegion( hwnd, hrgn );
    
    HRGN hrgnUpd;
    if ( hrgn != 0 ) {
        hrgnUpd = GpiCreateRegion( hps, 0, NULL );
        // substract the new clip region from the old one
        GpiCombineRegion( hps, hrgnUpd, hrgnOld, hrgn, CRGN_DIFF );
        // move the result to the parent coordinate space
        POINTL ptl = { x, y }; 
        GpiOffsetRegion( hps, hrgnUpd, &ptl );
        // invalidate areas in parent uncovered by the new clip region
        qt_WinInvalidateRegionEx( parent, hrgnUpd,
                                  WinQueryWindow( hwnd, QW_NEXT ), HWND_BOTTOM );
    } else {
        // the new region is being set to NULL, which means to widget bounds
        // (no need to substract new from old, because it will produce RGN_NULL)
        hrgnUpd = GpiCreateRegion( hps, 1, &rcl );
        hrgn = hrgnUpd;
    }
    // substract the old clip region from the new one
    GpiCombineRegion( hps, hrgnUpd, hrgn, hrgnOld, CRGN_DIFF );
    // invalidate areas in hwnd uncovered by the new clip region
    WinInvalidateRegion( hwnd, hrgnUpd, TRUE );
    
    GpiDestroyRegion( hps, hrgnUpd );
    GpiDestroyRegion( hps, hrgnOld );
}

#endif

/*!
    \overload

    Causes only the parts of the widget which overlap \a region to be
    visible. If the region includes pixels outside the rect() of the
    widget, window system controls in that area may or may not be
    visible, depending on the platform.

    Note that this effect can be slow if the region is particularly
    complex.

    Note that on OS/2, masks for top-level widgets are not currently
    supported, so setting a mask on such a widget has no effect.
    
    \sa setMask(), clearMask()
*/

void QWidget::setMask( const QRegion &region )
{
#if !defined (QT_PM_NO_WIDGETMASK)        
    if (isTopLevel()) {
#if defined (QT_CHECK_STATE)        
        qWarning( "QWidget::setMask() for top-level widgets "
                  "is not implemented on OS/2" );
#endif        
        return;
    }
    setClipRegion( winId(), x(), parentWidget()->height() - (y() + height()),
                   width(), height(), region.handle( height() ),
                   parentWidget()->winId() );
#else
    Q_UNUSED( region );
#endif    
}

/*!
    Causes only the pixels of the widget for which \a bitmap has a
    corresponding 1 bit to be visible. If the region includes pixels
    outside the rect() of the widget, window system controls in that
    area may or may not be visible, depending on the platform.

    Note that this effect can be slow if the region is particularly
    complex.

    Note that on OS/2, masks for top-level widgets are not currently
    supported, so setting a mask on such a widget has no effect.
    
    See \c examples/tux for an example of masking for transparency.

    \sa setMask(), clearMask()
*/

void QWidget::setMask( const QBitmap &bitmap )
{
#if !defined (QT_PM_NO_WIDGETMASK)
    if (isTopLevel()) {
#if defined (QT_CHECK_STATE)        
        qWarning( "QWidget::setMask() for top-level widgets "
                  "is not implemented on OS/2" );
#endif        
        return;
    }
    QRegion rgn = QRegion( bitmap );
    setClipRegion( winId(), x(), parentWidget()->height() - (y() + height()),
                   width(), height(), rgn.handle( height() ),
                   parentWidget()->winId() );
#else
    Q_UNUSED( bitmap );
#endif    
}

void QWidget::clearMask()
{
#if !defined (QT_PM_NO_WIDGETMASK)
    if (isTopLevel())
        return;
    setClipRegion( winId(), x(), parentWidget()->height() - (y() + height()),
                   width(), height(), 0,
                   parentWidget()->winId() );
#endif    
}

void QWidget::setName( const char *name )
{
    QObject::setName( name );
}

void QWidget::updateFrameStrut() const
{
    QWidget *that = (QWidget *) this;

    if ( !isVisible() || isDesktop() ) {
	that->fstrut_dirty = isVisible();
	return;
    }

    QTLWExtra *top = that->topData();
    if ( top->fId ) {
        // this widget has WC_FRAME
        SWP cswp;
        WinQueryWindowPos( winId(), &cswp );
        SWP fswp;
        WinQueryWindowPos( top->fId, &fswp );
        // flip y coordinates
        fswp.y = QApplication::desktop()->height() - (fswp.y + fswp.cy);
        cswp.y = fswp.cy - (cswp.y + cswp.cy);
        that->crect.setRect(
            fswp.x + cswp.x, fswp.y + cswp.y,
            cswp.cx, cswp.cy
        );

        top->fleft = cswp.x;
        top->ftop = cswp.y;
        top->fright = fswp.cx - cswp.x - cswp.cx;
        top->fbottom = fswp.cy - cswp.y - cswp.cy;
    }
    that->fstrut_dirty = FALSE;
}

void QWidget::setMouseTracking( bool enable )
{
    if ( enable )
        setWState( WState_MouseTracking );
    else
        clearWState( WState_MouseTracking );
}

void QWidget::setWindowOpacity(double)
{
}

double QWidget::windowOpacity() const
{
    return 1.0;
}

/**
 *  \internal  
 *  Returns the frame wihdow handle if this widget is a top-level widget and
 *  has the standard WC_FRAME as its parent/owner (where it is FID_CLIENT).
 *  If the widget does not have the standard frame or it is not top-level, this
 *  function simply retuns the winId() value.
 */
WId QWidget::winFId()
{
    HWND fId = 0;
    if ( isTopLevel() )
        fId = topData()->fId;
    if ( !fId )
        fId = winId();
    return fId;
}

#if !defined (QT_PM_NO_WIDGETMASK)

/*!
 *  \internal
 *
 *  Validates areas of this widget covered by (intersected with) its children
 *  and sibling widgets.
 *
 *  Clip regions of all relative widgets (set by WinSetClipRegion()) are taken
 *  into account.
 */
void QWidget::validateObstacles()
{
    RECTL updateRcl;
    if ( WinQueryUpdateRect( winId(), &updateRcl ) ) {
        // the update rectangle may be empty
        if ( updateRcl.xLeft != updateRcl.xRight &&
             updateRcl.yBottom != updateRcl.yTop ) {
            qt_WinProcessWindowObstacles( winId(), &updateRcl, 0, 0 );
        }
    }
}

#endif // !defined (QT_PM_NO_WIDGETMASK)

/*!
 *  \internal
 *
 *  Obtains a presentaiton space to draw on this widget, set up according
 *  mode \a m and to widget flags.
 *
 *  The returned handle must be freed using WinReleasePS() after usage.
 */
HPS QWidget::getTargetPS( ClipMode m /* = ClipDefault */ )
{
    HPS widgetPS = 0;
    
    if ( isDesktop() ) {
        widgetPS = WinGetScreenPS( HWND_DESKTOP );
        return widgetPS;
    } else {
        if ( m == Unclipped || (m == ClipDefault &&
                                testWFlags( WPaintUnclipped )) ) {
            widgetPS = WinGetClipPS( winId(), 0, PSF_PARENTCLIP );
            return widgetPS;
        } else {
            widgetPS = WinGetPS( winId() );
        }
    }

#if !defined (QT_PM_NO_WIDGETMASK)
    RECTL rcl = { 0, 0, crect.width(), crect.height() };
    HRGN hrgn = GpiCreateRegion( widgetPS, 1, &rcl );
    qt_WinProcessWindowObstacles( winId(), NULL, hrgn, CRGN_DIFF );
    HRGN hrgnOld = 0;
    GpiSetClipRegion( widgetPS, hrgn, &hrgnOld );
    Q_ASSERT( !hrgnOld );
    if ( hrgnOld )
        GpiDestroyRegion( widgetPS, hrgnOld );
#endif    
    
    return widgetPS;
}

