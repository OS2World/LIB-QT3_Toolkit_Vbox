/****************************************************************************
** $Id: qpaintdevice_pm.cpp 65 2006-03-12 19:33:32Z dmik $
**
** Implementation of QPaintDevice class for OS/2
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

#include "qpaintdevice.h"
#include "qpaintdevicemetrics.h"
#include "qpainter.h"
#include "qwidget.h"
#include "qbitmap.h"
#include "qapplication.h"
#include "qapplication_p.h"
#include "qt_os2.h"


QPaintDevice::QPaintDevice( uint devflags )
{
    if ( !qApp ) {				// global constructor
#if defined(QT_CHECK_STATE)
	qFatal( "QPaintDevice: Must construct a QApplication before a "
		"QPaintDevice" );
#endif
	return;
    }
    devFlags = devflags;
    hps = 0;
    painters = 0;
}

QPaintDevice::~QPaintDevice()
{
#if defined(QT_CHECK_STATE)
    if ( paintingActive() )
	qWarning( "QPaintDevice: Cannot destroy paint device that is being "
		  "painted.  Be sure to QPainter::end() painters!" );
#endif
}

HPS QPaintDevice::handle() const
{
    return hps;
}

bool QPaintDevice::cmd( int, QPainter *, QPDevCmdParam * )
{
#if defined(QT_CHECK_STATE)
    qWarning( "QPaintDevice::cmd: Device has no command interface" );
#endif
    return FALSE;
}

int QPaintDevice::metric( int ) const
{
#if defined(QT_CHECK_STATE)
    qWarning( "QPaintDevice::metrics: Device has no metric information" );
#endif
    return 0;
}

int QPaintDevice::fontMet( QFont *, int, const char*, int ) const
{
    return 0;
}

int QPaintDevice::fontInf( QFont *, int ) const
{
    return 0;
}

// these are defined in qpixmap_pm.cpp
extern HPS qt_alloc_mem_ps( int w, int h, HPS compat = 0 );
extern void qt_free_mem_ps( HPS hps );

// ROP translation table to convert Qt::RasterOp to ROP_ constants
extern const LONG qt_ropCodes_2ROP[] = {
    ROP_SRCCOPY, 		// CopyROP
    ROP_SRCPAINT,		// OrROP
    ROP_SRCINVERT,		// XorROP
    0x22,	                // NotAndROP
    ROP_NOTSRCCOPY,		// NotCopyROP
    ROP_MERGEPAINT,	        // NotOrROP
    0x99,	                // NotXorROP
    ROP_SRCAND,		        // AndROP
    ROP_DSTINVERT,		// NotROP
    ROP_ZERO,		        // ClearROP
    ROP_ONE,		        // SetROP
    0xAA,	                // NopROP
    ROP_SRCERASE,		// AndNotROP
    0xDD,	                // OrNotROP
    0x77,	                // NandROP
    ROP_NOTSRCERASE		// NorROP
};

// draws the pixmap with a mask using the black source method
static void drawMaskedPixmap(
    HPS dst_ps, int dst_depth, bool grab_dest, HPS src_ps, HPS mask_ps,
    int dx, int dy, int sx, int sy, int w, int h, Qt::RasterOp rop )
{
    IMAGEBUNDLE oldIb;

    if ( !mask_ps ) {
        // self-masked pixmap
        POINTL ptls[] = { { dx, dy }, { dx + w, dy + h },
                          { sx, sy } };
        IMAGEBUNDLE newIb;
        GpiQueryAttrs( dst_ps, PRIM_IMAGE, IBB_BACK_MIX_MODE, (PBUNDLE) &oldIb );
        newIb.usBackMixMode = BM_SRCTRANSPARENT;
        GpiSetAttrs( dst_ps, PRIM_IMAGE, IBB_BACK_MIX_MODE, 0, (PBUNDLE) &newIb );
        GpiBitBlt( dst_ps, src_ps, 3, ptls, qt_ropCodes_2ROP[rop], BBO_IGNORE );
        GpiSetAttrs( dst_ps, PRIM_IMAGE, IBB_BACK_MIX_MODE, 0, (PBUNDLE) &oldIb );
    } else {
        bool simple = (rop == Qt::CopyROP) || (rop == Qt::XorROP);
        // origial helper buffer handles 
        HPS hpsBuf = 0;
        HBITMAP hbmBuf = 0;
        // two pointers to the helper buffer 
        HPS hpsBuf1 = dst_ps;
        HPS hpsBuf2 = 0;
        // first helper buffer's coordinates 
        int buf1_x = dx, buf1_y = dy;
        // second helper buffer's coordinates 
        int buf2_x = -1, buf2_y = -1;
        
        if ( grab_dest || !simple ) {
            // create the helper buffer (we need two buffers if grab_dest and
            // not simple -- double the height in this case)
            BITMAPINFOHEADER2 bmh;
            memset( &bmh, 0, sizeof(BITMAPINFOHEADER2) );
            bmh.cbFix = sizeof(BITMAPINFOHEADER2);
            bmh.cx = w;
            bmh.cy = grab_dest && !simple ? h * 2 : h;
            bmh.cPlanes = 1;
            bmh.cBitCount = dst_depth;
            hpsBuf = qt_alloc_mem_ps( bmh.cx, bmh.cy, dst_ps );
            hbmBuf = GpiCreateBitmap( hpsBuf, &bmh, 0, NULL, NULL );
            GpiSetBitmap( hpsBuf, hbmBuf );
            // setup buffer handles and coordinates
            if ( grab_dest && !simple ) {
                hpsBuf1 = hpsBuf2 = hpsBuf;
                buf1_x = buf1_y = 0;
                buf2_x = 0;
                buf2_y = h;
            } else if ( grab_dest ) {
                hpsBuf1 = hpsBuf;
                buf1_x = buf1_y = 0;
            } else { // i.e. !grab_dest && !simple
                hpsBuf2 = hpsBuf;
                buf2_x = buf2_y = 0;
            }
        }

        // save current destination colors
        ULONG oldColor = GpiQueryColor( dst_ps );
        ULONG oldBackColor = GpiQueryBackColor( dst_ps );
        
        // query the destination color for 1-bpp bitmaps
        GpiQueryAttrs( dst_ps, PRIM_IMAGE, IBB_COLOR, (PBUNDLE) &oldIb );
        // setup colors for masking
        GpiSetColor( hpsBuf1, CLR_TRUE );
        GpiSetBackColor( hpsBuf1, CLR_FALSE );
        if ( hpsBuf2 != hpsBuf1 ) {
            GpiSetColor( hpsBuf2, CLR_TRUE );
            GpiSetBackColor( hpsBuf2, CLR_FALSE );
        }

        if ( grab_dest ) {
            // grab the destination
            POINTL ptls[] = { { buf1_x, buf1_y }, { buf1_x + w, buf1_y + h },
                              { dx, dy } };
            GpiBitBlt( hpsBuf1, dst_ps, 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
        }
        
        // compose a masked pixmap with a given rop into the second buffer
        if ( !simple ) {
            // grab the destination to the 2nd buffer
            POINTL ptDB2[] = { { buf2_x, buf2_y }, { buf2_x + w, buf2_y + h },
                               { dx, dy } };
            GpiBitBlt( hpsBuf2, dst_ps, 3, ptDB2, ROP_SRCCOPY, BBO_IGNORE );
            // bitblt pixmap using rop
            POINTL ptSB2[] = { { buf2_x, buf2_y }, { buf2_x + w, buf2_y + h },
                               { sx, sy } };
            GpiSetColor( hpsBuf2, oldIb.lColor );
            GpiBitBlt( hpsBuf2, src_ps, 3, ptSB2, qt_ropCodes_2ROP[rop], BBO_IGNORE );
            // make transparent pixels black
            GpiSetColor( hpsBuf2, CLR_TRUE );
            GpiBitBlt( hpsBuf2, mask_ps, 3, ptSB2, ROP_SRCAND, BBO_IGNORE );
        }

        // draw the mask: make non-transparent pixels (corresponding to
        // ones in the mask) black. skip this step when rop = XorROP
        // to get the XOR effect.
        if ( rop != Qt::XorROP ) {
            POINTL ptls[] = { { buf1_x, buf1_y }, { buf1_x + w, buf1_y + h },
                              { sx, sy } };
            GpiBitBlt( hpsBuf1, mask_ps, 3, ptls, 0x22, BBO_IGNORE );
        }
        
        if ( !simple ) {
            // draw masked pixmap from the 2nd buffer to the 1st
            POINTL ptB2B1[] = { { buf1_x, buf1_y }, { buf1_x + w, buf1_y + h },
                                { buf2_x, buf2_y } };
            GpiBitBlt( hpsBuf1, hpsBuf2, 3, ptB2B1, ROP_SRCPAINT, BBO_IGNORE );
        } else {
            // draw masked pixmap; transparent pixels are zeroed there
            // by prepareForMasking( TRUE )
            POINTL ptls[] = { { buf1_x, buf1_y }, { buf1_x + w, buf1_y + h },
                              { sx, sy } };
            GpiSetColor( hpsBuf1, oldIb.lColor );
            GpiBitBlt( hpsBuf1, src_ps, 3, ptls, ROP_SRCINVERT, BBO_IGNORE );
        }

        // flush the buffer
        if ( hpsBuf1 != dst_ps ) {
            POINTL ptls[] = { { dx, dy }, { dx + w, dy + h },
                              { buf1_x, buf1_y } };
            GpiBitBlt( dst_ps, hpsBuf1, 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
        }
        
        // free resources
        if ( hpsBuf ) {
            GpiSetBitmap( hpsBuf, 0 );
            GpiDeleteBitmap( hbmBuf );
            qt_free_mem_ps( hpsBuf );
        }
        
        // restore current destination colors
        GpiSetColor( dst_ps, oldColor );
        GpiSetBackColor( dst_ps, oldBackColor );
    }
}

// draws the pixmap with a mask using the black source method
static void drawAlphaPixmap (
    HPS dst_ps, int dst_w, HPS src_ps, int src_w, uchar *alpha,
    int dx, int dy, int sx, int sy, int w, int h, int depth )
{
    Q_ASSERT( depth == 32 || depth == 24 );

    bool grab_dest = false;
    int dst_x = dx;
    
    if ( dst_w == 0 ) {
        grab_dest = true;
        dst_w = w;
        dst_x = 0;
    }
    
    int dst_bpl = ((depth * dst_w + 31) / 32) * 4;
    int src_bpl = ((depth * src_w + 31) / 32) * 4;
    
    uchar *dst = new uchar [dst_bpl * h];
    uchar *src = new uchar [src_bpl * h];
    
    BITMAPINFOHEADER2 bmh;
    memset( &bmh, 0, sizeof(BITMAPINFOHEADER2) );
    bmh.cbFix = sizeof(BITMAPINFOHEADER2);
    bmh.cPlanes = 1;
    bmh.cBitCount = depth;
    if ( grab_dest ) {
        bmh.cx = w;
        bmh.cy = h;
        
        // helper hps for dblbuf
        HPS hpsBuf = qt_alloc_mem_ps( w, h, dst_ps );
        // helper bitmap for dblbuf
        HBITMAP hbmBuf = GpiCreateBitmap( hpsBuf, &bmh, 0, NULL, NULL );
        GpiSetBitmap( hpsBuf, hbmBuf );
        
        // grab the destination and get bits
        POINTL ptls[] = { { 0, 0 }, { w, h }, { dx, dy } };
        GpiBitBlt ( hpsBuf, dst_ps, 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
        GpiQueryBitmapBits( hpsBuf, 0, h, (PBYTE) dst, (PBITMAPINFO2) &bmh );
    
        // free dblbuf
        GpiSetBitmap( hpsBuf, 0 );
        GpiDeleteBitmap( hbmBuf );
        qt_free_mem_ps( hpsBuf );
    } else {
        // get bits of the destination
        GpiQueryBitmapBits( dst_ps, dy, h, (PBYTE) dst, (PBITMAPINFO2) &bmh );
    }
    
    // get bits of the source pixmap
    GpiQueryBitmapBits( src_ps, sy, h, (PBYTE) src, (PBITMAPINFO2) &bmh );
    
    uchar px_sz = depth / 8;
    uchar *dst_ptr = dst + px_sz * dst_x;
    uchar *src_ptr = src + px_sz * sx;
    uchar *a_ptr = alpha ? alpha + sy * src_w + sx : 0; 
    int dst_inc = dst_bpl - px_sz * w;
    int src_inc = src_bpl - px_sz * w;
    int a_inc = src_w - w;
    int px_skip = px_sz - 3;
    
    // use two separate code paths to increase the speed slightly by
    // eliminating one comparison op (a_ptr != 0) inside the most nested loop 
    if ( a_ptr ) {
        for ( int y = 0; y < h; y++ ) {
            for ( int x = 0; x < w; x++ ) {
                uchar a = *(a_ptr++);
                if ( a == 0 ) {
                    dst_ptr += px_sz;
                    src_ptr += px_sz;
                    continue;
                } else if ( a == 255 ) {
                    *(dst_ptr++) = *(src_ptr++);
                    *(dst_ptr++) = *(src_ptr++);
                    *(dst_ptr++) = *(src_ptr++);
                } else {
                    *dst_ptr += (*(src_ptr++) - *dst_ptr) * a / 255;
                    dst_ptr ++;
                    *dst_ptr += (*(src_ptr++) - *dst_ptr) * a / 255;
                    dst_ptr ++;
                    *dst_ptr += (*(src_ptr++) - *dst_ptr) * a / 255;
                    dst_ptr ++;
                }
                dst_ptr += px_skip;
                src_ptr += px_skip;
            }
            dst_ptr += dst_inc;
            src_ptr += src_inc;
            a_ptr += a_inc;
        }
    } else {
        for ( int y = 0; y < h; y++ ) {
            for ( int x = 0; x < w; x++ ) {
                uchar a = src_ptr [3];
                if ( a == 0 ) {
                    dst_ptr += px_sz;
                    src_ptr += px_sz;
                    continue;
                } else if ( a == 255 ) {
                    *(dst_ptr++) = *(src_ptr++);
                    *(dst_ptr++) = *(src_ptr++);
                    *(dst_ptr++) = *(src_ptr++);
                } else {
                    *dst_ptr += (*(src_ptr++) - *dst_ptr) * a / 255;
                    dst_ptr ++;
                    *dst_ptr += (*(src_ptr++) - *dst_ptr) * a / 255;
                    dst_ptr ++;
                    *dst_ptr += (*(src_ptr++) - *dst_ptr) * a / 255;
                    dst_ptr ++;
                }
                dst_ptr += px_skip;
                src_ptr += px_skip;
            }
            dst_ptr += dst_inc;
            src_ptr += src_inc;
        }
    }
    
    // flush the dst buffer
    {
        bmh.cx = dst_w;
        bmh.cy = h;
        POINTL ptls[] = { { dx, dy }, { dx + w - 1, dy + h - 1 },
                          { dst_x, 0 }, { dst_x + w, h } };
        GpiDrawBits( dst_ps, (PVOID) dst, (PBITMAPINFO2) &bmh, 4, ptls,
                     ROP_SRCCOPY, BBO_IGNORE );
    }

    delete[] src;
    delete[] dst;
}

void bitBlt( QPaintDevice *dst, int dx, int dy,
	     const QPaintDevice *src, int sx, int sy, int sw, int sh,
	     Qt::RasterOp rop, bool ignoreMask  )
{
    if ( !src || !dst || sw == 0 || sh == 0 ) {
#if defined(QT_CHECK_NULL)
	Q_ASSERT( src != 0 );
	Q_ASSERT( dst != 0 );
#endif
	return;
    }
    if ( src->isExtDev() )
	return;

    QPaintDevice *pdev = QPainter::redirect( dst );
    if ( pdev )
	dst = pdev;

    int ts = src->devType();			// from device type
    int td = dst->devType();			// to device type

    int src_w = src->metric(QPaintDeviceMetrics::PdmWidth);
    int src_h = src->metric(QPaintDeviceMetrics::PdmHeight);
    int dst_w = dst->metric(QPaintDeviceMetrics::PdmWidth);
    int dst_h = dst->metric(QPaintDeviceMetrics::PdmHeight);
    
    if ( sw < 0 )				// special width
        sw = src_w - sx;
    if ( sh < 0 )				// special height
        sh = src_h - sy;

    // ensure coordinates are within borders, clip if necessary
    if ( sx < 0 || sy < 0 || sx + sw > src_w || sy + sh > src_h ||
         dx < 0 || dy < 0 || dx + sw > dst_w || dy + sh > dst_h )
    {
        int tx = dx - sx;
        int ty = dy - sy;
        QRect dr( 0, 0, dst_w, dst_h );  // dst rect
        QRect sr( tx, ty, src_w, src_h ); // src rect in dst coords
        QRect bltr( dx, dy, sw, sh ); // blit rect in dst coords
        bltr &= (dr & sr);
        if (bltr.isEmpty())
            return;
        dx = bltr.x();
        dy = bltr.y();
        sx = dx - tx;
        sy = dy - ty;
        sw = bltr.width();
        sh = bltr.height();
    }
    
    if ( dst->paintingActive() && dst->isExtDev() ) {
	QPixmap *pm;				// output to picture/printer
	bool	 tmp_pm = TRUE;
	if ( ts == QInternal::Pixmap ) {
	    pm = (QPixmap*)src;
	    if ( sx != 0 || sy != 0 ||
		 sw != pm->width() || sh != pm->height() ) {
		QPixmap *pm_new = new QPixmap( sw, sh, pm->depth() );
		bitBlt( pm_new, 0, 0, pm, sx, sy, sw, sh );
		pm = pm_new;
	    } else {
		tmp_pm = FALSE;
	    }
	} else if ( ts == QInternal::Widget ) {	// bitBlt to temp pixmap
	    pm = new QPixmap( sw, sh );
	    Q_CHECK_PTR( pm );
	    bitBlt( pm, 0, 0, src, sx, sy, sw, sh );
	} else {
#if defined(QT_CHECK_RANGE)
	    qWarning( "bitBlt: Cannot bitBlt from device" );
#endif
	    return;
	}
	QPDevCmdParam param[3];
	QPoint p(dx,dy);
	param[0].point	= &p;
	param[1].pixmap = pm;
	dst->cmd( QPaintDevice::PdcDrawPixmap, 0, param );
	if ( tmp_pm )
	    delete pm;
	return;
    }

    switch ( ts ) {
	case QInternal::Widget:
	case QInternal::Pixmap:
	case QInternal::System:			// OK, can blt from these
	    break;
	default:
#if defined(QT_CHECK_RANGE)
	    qWarning( "bitBlt: Cannot bitBlt from device type %x", ts );
#endif
	    return;
    }
    switch ( td ) {
	case QInternal::Widget:
	case QInternal::Pixmap:
	case QInternal::System:			// OK, can blt to these
	    break;
	default:
#if defined(QT_CHECK_RANGE)
	    qWarning( "bitBlt: Cannot bitBlt to device type %x", td );
#endif
	    return;
    }

    if ( rop > Qt::LastROP ) {
#if defined(QT_CHECK_RANGE)
	qWarning( "bitBlt: Invalid ROP code" );
#endif
	return;
    }

    if ( dst->isExtDev() ) {
#if defined(QT_CHECK_NULL)
	qWarning( "bitBlt: Cannot bitBlt to device" );
#endif
	return;
    }

    if ( td == QInternal::Pixmap )
	((QPixmap*)dst)->detach();		// changes shared pixmap

    HPS	 src_ps = src->hps, dst_ps = dst->hps;
    bool src_tmp = FALSE, dst_tmp = FALSE;

    QPixmap *src_pm;
    QBitmap *mask;
    if ( ts == QInternal::Pixmap ) {
	src_pm = (QPixmap *)src;
	mask = ignoreMask ? 0 : (QBitmap *)src_pm->mask();
    } else {
	src_pm = 0;
	mask   = 0;
	if ( !src_ps && ts == QInternal::Widget ) {
	    src_ps = WinGetPS( ((QWidget*)src)->winId() );
	    src_tmp = TRUE;
	}
    }
    if ( td != QInternal::Pixmap ) {
	if ( !dst_ps && td == QInternal::Widget ) {
	    dst_ps = ((QWidget*)dst)->getTargetPS();
	    dst_tmp = TRUE;
	}
    }
#if defined(QT_CHECK_NULL)
    Q_ASSERT( src_ps && dst_ps );
#endif

    // flip y coordinates
    int fsy = sy;
    if ( ts == QInternal::Widget || ts == QInternal::Pixmap ) {
        int devh = (ts == QInternal::Widget) ?
                ((QWidget*)src)->height() : ((QPixmap*)src)->height();
        fsy = devh - (fsy + sh);
    }
    int fdy = dy;
    if ( td == QInternal::Widget || td == QInternal::Pixmap ) {
        int devh = (td == QInternal::Widget) ?
                ((QWidget*)dst)->height() : ((QPixmap*)dst)->height();
        fdy = devh - (fdy + sh);
    }
    POINTL ptls[] = {
        { dx, fdy }, { dx + sw, fdy + sh },
        { sx, fsy },
    };

    if ( td == QInternal::Pixmap ) {
        // Ensure the auxiliary masked bitmap created for masking is destroyed
        // (to cause it to be recreated when the destination is blitted next
        // time). Also ensure the alpha channel of the destination pixmap is
        // unfolded -- any GPI call will clear the high byte that may be storing
        // the alpha bits, so the pixmap will appear as fully transparent.
        QPixmap *dst_pm = (QPixmap *) dst;
        if ( dst_pm->data->mask )
            dst_pm->prepareForMasking( FALSE, TRUE );
        if ( dst_pm->data->hasRealAlpha )
            dst_pm->unfoldAlphaChannel();
    }

    if ( src_pm && src_pm->data->hasRealAlpha && !ignoreMask ) {
        int dst_w = 0; // implies grab_dest = true
        if ( td == QInternal::Pixmap )
            dst_w = ((QPixmap *) dst)->data->w; // implies grab_dest = false
        drawAlphaPixmap( dst_ps, dst_w, src_ps, src_pm->data->w,
                         src_pm->data->realAlphaBits,
                         dx, fdy, sx, fsy, sw, sh, QPixmap::trueColorDepth() );
    } else if ( mask ) {
#if 0
        //  Unfortunately, this nice method of setting the mask as a pattern
        //  and using the corresponding ROPs to do masking in one step will
        //  not apways work on many video drivers (including SDD/SNAP) since
        //  they can spontaneously simplify the bitmap set as a pattern (for
        //  example, take only first 8 pixels of the mask width), which will
        //  produce wrong results.
        const ULONG AllAreaAttrs =
            ABB_COLOR | ABB_BACK_COLOR |
            ABB_MIX_MODE | ABB_BACK_MIX_MODE |
            ABB_SET | ABB_SYMBOL | ABB_REF_POINT;
        AREABUNDLE oldAb;
        GpiQueryAttrs( dst_ps, PRIM_AREA, AllAreaAttrs, (PBUNDLE) &oldAb );
        AREABUNDLE newAb = {
            CLR_TRUE, CLR_FALSE, FM_OVERPAINT, BM_OVERPAINT,
            LCID_QTMaskBitmap, 0, ptls [0]
        };
        GpiSetBitmap( mask->hps, 0 );
        GpiSetBitmapId( dst_ps, mask->hbm(), LCID_QTMaskBitmap );
        GpiSetAttrs( dst_ps, PRIM_AREA, AllAreaAttrs, 0, (PBUNDLE) &newAb );
        // we use the same rop codes for masked bitblt after setting the
        // low half of the rop byte to 0xA, which means destination bits
        // should be preserved where the corresponding bits in the mask
        // are zeroes.
        GpiBitBlt( dst_ps, src_ps, 3, ptls, (qt_ropCodes_2ROP[rop] & 0xF0) | 0x0A, BBO_IGNORE );
        GpiSetAttrs( dst_ps, PRIM_AREA, AllAreaAttrs, 0, (PBUNDLE) &oldAb );
        GpiDeleteSetId( dst_ps, LCID_QTMaskBitmap );
        GpiSetBitmap( mask->hps, mask->hbm() );
#else
        bool grab_dest = td != QInternal::Pixmap;
        src_pm->prepareForMasking( TRUE );
        drawMaskedPixmap(
            dst_ps, dst->metric( QPaintDeviceMetrics::PdmDepth ), grab_dest,
            src_ps, src_pm->data->selfmask ? 0 : mask->hps,
            dx, fdy, sx, fsy, sw, sh, rop );
        src_pm->prepareForMasking( FALSE );
#endif
    } else {
        GpiBitBlt( dst_ps, src_ps, 3, ptls, qt_ropCodes_2ROP[rop], BBO_IGNORE );
    }

    if ( src_tmp )
	WinReleasePS( src_ps );
    if ( dst_tmp )
	WinReleasePS( dst_ps );
}


void QPaintDevice::setResolution( int )
{
}

int QPaintDevice::resolution() const
{
    return metric( QPaintDeviceMetrics::PdmDpiY );
}
