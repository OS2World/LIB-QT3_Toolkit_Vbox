/****************************************************************************
** $Id: qregion_pm.cpp 142 2006-10-23 22:44:11Z dmik $
**
** Implementation of QRegion class for OS/2
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

#include "qregion.h"
#include "qpointarray.h"
#include "qbuffer.h"
#include "qbitmap.h"
#include "qimage.h"
#include "qt_os2.h"

//  To compensate the difference between Qt (where y axis goes downwards) and
//  GPI (where y axis goes upwards) coordinate spaces when dealing with regions
//  we use the following technique: when a GPI resource is allocated for a Qt
//  region, we simply change the sign of all y coordinates to quickly flip it
//  top to bottom in a manner that doesn't depend on the target device height.
//  All we have to do to apply the created GPI region to a particular GPI device
//  is to align its y axis to the top of the device (i.e. offset the region
//  up by the height of the device), and unalign it afterwards to bring it back
//  to the coordinate space of other device-independent (unaligned) regions.
//  To optimize this, we remember (in data->hgt) the last height value used to
//  align the region, and align it again only if the target device height
//  changes. Zero height indicates a device-independent target (such as other
//  unaligned Qt region).
//
//  The handle() function, used for external access to the region, takes an
//  argument that must be always set to the height of the target device to
//  guarantee the correct coordinate space alignment.

#if defined(Q_CC_GNU) && !defined(USE_OS2_TOOLKIT_HEADERS)

// Innotek GCC lacks some API functions in its version of OS/2 Toolkit headers

extern "C" HRGN APIENTRY GpiCreateEllipticRegion( HPS hps,
                                                  PRECTL prclRect );

extern "C" HRGN APIENTRY GpiCreatePolygonRegion( HPS hps,
                                                 ULONG ulCount,
                                                 PPOLYGON paplgn,
                                                 ULONG flOptions );
#endif

QRegion::QRegion()
{
    data = new QRegionData;
    Q_CHECK_PTR( data );
    data->rgn = 0;
    data->hgt = 0;
    data->is_null = TRUE;
}

QRegion::QRegion( bool is_null )
{
    data = new QRegionData;
    Q_CHECK_PTR( data );
    data->rgn = 0;
    data->hgt = 0;
    data->is_null = is_null;
}

QRegion::QRegion( HRGN hrgn, int target_height )
{
    data = new QRegionData;
    Q_CHECK_PTR( data );
    data->rgn = hrgn;
    data->hgt = target_height;
    data->is_null = FALSE;
}

QRegion::QRegion( const QRect &r, RegionType t )
{
    data = new QRegionData;
    Q_CHECK_PTR( data );
    data->hgt = 0;
    data->is_null = FALSE;
    if ( r.isEmpty() ) {
	data->rgn = 0;
    } else {
        HPS hps = qt_display_ps();
	if ( t == Rectangle ) {			// rectangular region
            RECTL rcl = { r.left(), -(r.bottom()+1), r.right()+1, -r.top() };
            data->rgn = GpiCreateRegion( hps, 1, &rcl );
	} else if ( t == Ellipse ) {		// elliptic region
            // if the width or height of the ellipse is odd, GPI always
            // converts it to a nearest even value, which is obviously stupid
            // (see also QPainter::drawArcInternal()). So, we don't use
            // GpiCreateEllipticRegion(), but create an array of points to
            // call GpiCreatePolygonRegion() instead.
            QPointArray a;
            a.makeArc( r.x(), r.y(), r.width(), r.height(), 0, 360 * 16 );
            for ( uint i = 0; i < a.size(); ++ i )
                a[i].ry() = -(a[i].y() + 1);
            // GpiCreatePolygonRegion() is bogus and always starts a poligon from
            // the current position. Make the last point the current one and reduce
            // the number of points by one.
            GpiMove( hps, (PPOINTL) &a[ a.size() - 1 ] );
            POLYGON poly = { a.size() - 1, (PPOINTL) a.data() };
            data->rgn = GpiCreatePolygonRegion( hps, 1, &poly, POLYGON_ALTERNATE );
	}
    }
}

QRegion::QRegion( const QPointArray &a, bool winding )
{
    data = new QRegionData;
    Q_CHECK_PTR( data );
    data->hgt = 0;
    data->is_null = FALSE;
    QRect r = a.boundingRect();
    if ( a.isEmpty() || r.isEmpty() ) {
	data->rgn = 0;
    } else {
        HPS hps = qt_display_ps();
        POINTL *pts = new POINTL[ a.size() ]; 
        for ( uint i = 0; i < a.size(); ++ i ) {
            pts[i].x = a[i].x();
            pts[i].y = - (a[i].y() + 1);
        }
        // GpiCreatePolygonRegion() is bogus and always starts a poligon from
        // the current position. Make the last point the current one and reduce
        // the number of points by one.
        GpiMove( hps, &pts[ a.size() - 1 ] );
        POLYGON poly = { a.size() - 1, pts };
        ULONG opts = winding ? POLYGON_WINDING : POLYGON_ALTERNATE;
        data->rgn = GpiCreatePolygonRegion( hps, 1, &poly, opts );
        delete[] pts;
    }
}

QRegion::QRegion( const QRegion &r )
{
    data = r.data;
    data->ref();
}

HRGN qt_pm_bitmapToRegion( const QBitmap& bitmap )
{
    HRGN region = 0;
    QImage image = bitmap.convertToImage();
    const int maxrect = 256;
    RECTL rects[maxrect];
    HPS hps = qt_display_ps();

#define FlushSpans \
    { \
        HRGN r = GpiCreateRegion( hps, n, rects ); \
	if ( region ) { \
	    GpiCombineRegion( hps, region, region, r, CRGN_OR ); \
	    GpiDestroyRegion( hps, r ); \
	} else { \
	    region = r; \
	} \
    }

#define AddSpan \
    { \
        rects[n].xLeft = prev1; \
        rects[n].yBottom = -(y+1); \
        rects[n].xRight = x-1+1; \
        rects[n].yTop = -y; \
        n++; \
        if ( n == maxrect ) { \
            FlushSpans \
            n = 0; \
        } \
    }

    int n = 0;
    int zero = 0x00;

    int x, y;
    for ( y = 0; y < image.height(); y++ ) {
	uchar *line = image.scanLine(y);
	int w = image.width();
	uchar all = zero;
	int prev1 = -1;
	for ( x = 0; x < w; ) {
	    uchar byte = line[x/8];
	    if ( x > w-8 || byte != all ) {
		for ( int b = 8; b > 0 && x < w; b-- ) {
		    if ( !(byte & 0x80) == !all ) {
			// More of the same
		    } else {
			// A change.
			if ( all != zero ) {
			    AddSpan;
			    all = zero;
			} else {
			    prev1 = x;
			    all = ~zero;
			}
		    }
		    byte <<= 1;
		    x++;
		}
	    } else {
		x += 8;
	    }
	}
	if ( all != zero ) {
	    AddSpan;
	}
    }
    if ( n ) {
	FlushSpans;
    }

    if ( !region )
        region = GpiCreateRegion( hps, 0, NULL );

    return region;
}


QRegion::QRegion( const QBitmap & bm )
{
    data = new QRegionData;
    Q_CHECK_PTR( data );
    data->hgt = 0;
    data->is_null = FALSE;
    if ( bm.isNull() )
        data->rgn = 0;
    else
	data->rgn = qt_pm_bitmapToRegion( bm );
}


QRegion::~QRegion()
{
    if ( data->deref() ) {
	if ( data->rgn )
            GpiDestroyRegion( qt_display_ps(), data->rgn );
	delete data;
    }
}

QRegion &QRegion::operator=( const QRegion &r )
{
    r.data->ref();				// beware of r = r
    if ( data->deref() ) {
	if ( data->rgn )
            GpiDestroyRegion( qt_display_ps(), data->rgn );
	delete data;
    }
    data = r.data;
    return *this;
}


QRegion QRegion::copy() const
{
    QRegion r ( data->is_null );
    r.data->hgt = 0;
    if ( !data->is_null && data->rgn ) {
        HPS hps = qt_display_ps();
        r.data->rgn = GpiCreateRegion( hps, 0, NULL );
        GpiCombineRegion( hps, r.data->rgn, data->rgn, NULL, CRGN_COPY );
        r.data->hgt = data->hgt;
    }
    return r;
}


bool QRegion::isNull() const
{
    return data->is_null;
}

bool QRegion::isEmpty() const
{
    if ( data->is_null || data->rgn == 0 )
        return TRUE;
    RECTL rcl;
    return GpiQueryRegionBox( qt_display_ps(), data->rgn, &rcl ) == RGN_NULL;
}


bool QRegion::contains( const QPoint &p ) const
{
    LONG rc = PRGN_OUTSIDE;
    if ( data->rgn ) {
        POINTL ptl = { p.x(), data->hgt - (p.y() + 1) };
        rc = GpiPtInRegion( qt_display_ps(), data->rgn, &ptl );
    }
    return rc == PRGN_INSIDE;
}

bool QRegion::contains( const QRect &r ) const
{
    LONG rc = PRGN_OUTSIDE;
    if ( data->rgn ) {
        RECTL rcl = { r.left(), data->hgt - (r.bottom() + 1),
                      r.right() + 1, data->hgt - r.top() };
        rc = GpiRectInRegion( qt_display_ps(), data->rgn, &rcl );
    }
    return rc == RRGN_INSIDE || rc == RRGN_PARTIAL;
}


void QRegion::translate( int dx, int dy )
{
    if ( !data->rgn )
	return;
    detach();
    POINTL ptl = { dx, -dy };
    GpiOffsetRegion( qt_display_ps(), data->rgn, &ptl);
}


#define CRGN_NOP -1

/*
  Performs the actual OR, AND, SUB and XOR operation between regions.
  Sets the resulting region handle to 0 to indicate an empty region.
*/

QRegion QRegion::pmCombine( const QRegion &r, int op ) const
{
    LONG both = CRGN_NOP, left = CRGN_NOP, right = CRGN_NOP;
    switch ( op ) {
	case QRGN_OR:
	    both = CRGN_OR;
	    left = right = CRGN_COPY;
	    break;
	case QRGN_AND:
	    both = CRGN_AND;
	    break;
	case QRGN_SUB:
	    both = CRGN_DIFF;
	    left = CRGN_COPY;
	    break;
	case QRGN_XOR:
	    both = CRGN_XOR;
	    left = right = CRGN_COPY;
	    break;
	default:
#if defined(QT_CHECK_RANGE)
	    qWarning( "QRegion: Internal error in pmCombine" );
#else
	    ;
#endif
    }

    QRegion result( FALSE );
    if ( !data->rgn && !r.data->rgn )
        return result;
    HPS hps = qt_display_ps();
    result.data->rgn = GpiCreateRegion( hps, 0, NULL );
    LONG rc = RGN_NULL; 
    if ( data->rgn && r.data->rgn ) {
        updateHandle( r.data->hgt ); // bring to the same coordinate space
	rc = GpiCombineRegion( hps, result.data->rgn, data->rgn, r.data->rgn, both );
        result.data->hgt = r.data->hgt;
    } else if ( data->rgn && left != CRGN_NOP ) {
	rc = GpiCombineRegion( hps, result.data->rgn, data->rgn, 0, left );
        result.data->hgt = data->hgt;
    } else if ( r.data->rgn && right != CRGN_NOP ) {
	rc = GpiCombineRegion( hps, result.data->rgn, r.data->rgn, 0, right );
        result.data->hgt = r.data->hgt;
    }
    if ( rc == RGN_NULL || rc == RGN_ERROR ) {
        GpiDestroyRegion( hps, result.data->rgn );
        result.data->rgn = result.data->hgt = 0;
    }
    return result;
}

QRegion QRegion::unite( const QRegion &r ) const
{
    return pmCombine( r, QRGN_OR );
}

QRegion QRegion::intersect( const QRegion &r ) const
{
     return pmCombine( r, QRGN_AND );
}

QRegion QRegion::subtract( const QRegion &r ) const
{
    return pmCombine( r, QRGN_SUB );
}

QRegion QRegion::eor( const QRegion &r ) const
{
    return pmCombine( r, QRGN_XOR );
}


QRect QRegion::boundingRect() const
{
    RECTL rcl;
    LONG rc = RGN_NULL;
    if ( data->rgn )
        rc = GpiQueryRegionBox( qt_display_ps(), data->rgn, &rcl );
    if ( rc == RGN_NULL || rc == RGN_ERROR )
	return QRect(0,0,0,0);
    else
        return QRect( rcl.xLeft, data->hgt - rcl.yTop,
                      rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom );
}


QMemArray<QRect> QRegion::rects() const
{
    QMemArray<QRect> a;
    if ( !data->rgn )
	return a;

    HPS hps = qt_display_ps();
    RGNRECT ctl = {1, 0, 0, RECTDIR_LFRT_TOPBOT};
    if ( !GpiQueryRegionRects( hps, data->rgn, NULL, &ctl, NULL ) )
        return a;

    ctl.crc = ctl.crcReturned;
    PRECTL rcls = new RECTL[ctl.crcReturned];
    if ( !GpiQueryRegionRects( hps, data->rgn, NULL, &ctl, rcls ) ) {
        delete [] rcls;
        return a;
    }

    a = QMemArray<QRect>( ctl.crcReturned );
    PRECTL r = rcls;
    for ( int i=0; i<(int)a.size(); i++ ) {
        a[i].setRect( r->xLeft, data->hgt - r->yTop,
                      r->xRight - r->xLeft, r->yTop - r->yBottom );
	r++;
    }

    delete [] rcls;

    return a;
}

void QRegion::setRects( const QRect *rects, int num )
{
    // Could be optimized
    *this = QRegion();
    for (int i=0; i<num; i++)
	*this |= rects[i];
}

bool QRegion::operator==( const QRegion &r ) const
{
    if ( data == r.data )			// share the same data
	return TRUE;
    bool is_empty = data->is_null || data->rgn == 0;
    bool r_is_empty = r.data->is_null || r.data->rgn == 0;
    if ( (is_empty ^ r_is_empty ) )             // one is empty, not both
	return FALSE;
    if ( is_empty )				// both empty
        return TRUE;
    updateHandle( r.data->hgt ); // bring to the same coordinate space
    return
	GpiEqualRegion( qt_display_ps(), data->rgn, r.data->rgn ) == EQRGN_EQUAL;
}

/*!
 *  \internal
 *  Updates the region handle so that it is suitable for selection to
 *  a device with the given \a height.
 */
void QRegion::updateHandle( int target_height ) const
{
    QRegion *that = const_cast< QRegion *>( this ); // we're const here
    if ( !data->rgn ) {
        // a handle of a null region is requested, allocate an empty region
        that->data->rgn = GpiCreateRegion( qt_display_ps(), 0, NULL );
        that->data->hgt = target_height;
    } else if ( data->hgt != target_height ) {
        // align region y axis to the top of the device
        POINTL ptl = { 0, target_height - data->hgt };
        GpiOffsetRegion( qt_display_ps(), data->rgn, &ptl );
        that->data->hgt = target_height;
    }
}
