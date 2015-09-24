/****************************************************************************
** $Id: qpainter_pm.cpp 63 2006-02-25 20:58:01Z dmik $
**
** Implementation of QPainter class for OS/2
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

#include "qpainter.h"
#include "qpaintdevicemetrics.h"
#include "qwidget.h"
#include "qbitmap.h"
#include "qpixmapcache.h"
#include "qptrlist.h"
/// @todo (dmik) later
//#include "qprinter.h"
#include <stdlib.h>
#include <math.h>
#include "qapplication_p.h"
#include "qfontdata_p.h"
#include "qt_os2.h"
#include "qtextlayout_p.h"
#include "qtextengine_p.h"
#include "qfontengine_p.h"

// QRgb has the same RGB format (0xaarrggbb) as OS/2 uses (ignoring the
// highest alpha byte) so we just mask alpha to get the valid OS/2 color.
#define COLOR_VALUE(c) ((flags & RGBColor) ? (c.rgb() & RGB_MASK) : c.pixel())

// Innotek GCC lacks some API functions in its version of OS/2 Toolkit headers
#if defined(Q_CC_GNU) && !defined(USE_OS2_TOOLKIT_HEADERS)
extern "C" LONG  APIENTRY GpiQueryNearestPaletteIndex(HPAL hpal, ULONG color);
#endif

// this function fixes a bug in OS/2 GPI: GpiSetBackColor() doesn't
// change the background color attribute of line primitives.
BOOL qt_GpiSetBackColor( HPS hps, LONG color )
{
    BOOL rc = FALSE;
    rc = GpiSetBackColor( hps, color );
    LINEBUNDLE lb;
    lb.lBackColor = color;
    rc &= GpiSetAttrs( hps, PRIM_LINE, LBB_BACK_COLOR, 0, (PBUNDLE) &lb );
    return rc;
}

// this function fixes a bug in OS/2 GPI: GpiSetBackMix() doesn't
// change the mix color attribute of line primitives. it also takes into
// account the current pen style.
BOOL qt_GpiSetBackMix( HPS hps, LONG mix, BOOL noPen )
{
    BOOL rc = FALSE;
    rc = GpiSetBackMix( hps, mix );
    LINEBUNDLE lb;
    lb.usBackMixMode = noPen ? BM_LEAVEALONE : mix;
    rc &= GpiSetAttrs( hps, PRIM_LINE, LBB_BACK_MIX_MODE, 0, (PBUNDLE) &lb );
    return rc;
}

// helpers to draw geometric width lines

struct qt_geom_line_handle
{
    enum { AreaAttrs = ABB_COLOR | ABB_SET | ABB_SYMBOL | ABB_BACK_MIX_MODE };
    LONG aa;
    AREABUNDLE ab;
};

void qt_begin_geom_line( HPS hps, LONG color, BOOL noPen, qt_geom_line_handle *h )
{
    Q_ASSERT( h );
    h->aa = 0;
    GpiQueryAttrs( hps, PRIM_AREA, h->AreaAttrs, &h->ab );
    AREABUNDLE ab;
    if ( h->ab.lColor != color ) {
        ab.lColor = color;
        h->aa |= ABB_COLOR;
    }
    if ( h->ab.usSet != LCID_DEFAULT ) {
        ab.usSet = LCID_DEFAULT;
        h->aa |= ABB_SET;
    }
    ab.usSymbol = noPen ? PATSYM_BLANK : PATSYM_SOLID;
    if ( h->ab.usSymbol != ab.usSymbol ) {
        h->aa |= ABB_SYMBOL;
    }
    if ( noPen && h->ab.usBackMixMode != BM_LEAVEALONE ) {
        ab.usBackMixMode = BM_LEAVEALONE;
        h->aa |= ABB_BACK_MIX_MODE;
    }
    GpiSetAttrs( hps, PRIM_AREA, h->aa, 0, &ab );
    GpiBeginPath( hps, 1 );
}

void qt_end_geom_line( HPS hps, qt_geom_line_handle *h )
{
    GpiEndPath( hps );
    GpiStrokePath( hps, 1, 0 );
    GpiSetAttrs( hps, PRIM_AREA, h->aa, 0, &h->ab );
}

/* These functions are implemented in qapplication_pm.cpp */
extern void qt_fill_tile( QPixmap *, const QPixmap & );
extern void qt_draw_tiled_pixmap( HPS, int, int, int, int,
			   const QPixmap *, int, int, int );

void qt_erase_background(
    HPS hps, int x, int y, int w, int h,
    const QColor &bg_color,
    const QPixmap *bg_pixmap, int off_x, int off_y, int devh
)
{
    if ( bg_pixmap && bg_pixmap->isNull() )	// empty background
	return;
    HPAL oldPal = 0;
    if ( QColor::hPal() ) {
	oldPal = GpiSelectPalette( hps, QColor::hPal() );
    } else {
        // direct RGB mode
        GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );
    }
    if ( bg_pixmap ) {
        // normalize pixmap offsets according to x and y
        // to ensure that the offsets are always relative to
        // the background origin point, regardless of what x and y are.
        off_x = (off_x + x) % bg_pixmap->width();
        off_y = (off_y + y) % bg_pixmap->height();
	qt_draw_tiled_pixmap( hps, x, y, w, h, bg_pixmap, off_x, off_y, devh );
    } else {
        // flip y coordinate
        if ( devh )
            y = devh - (y + h);
        // this function should be called only with window presentation space
        // so simply use WinFillRect instead of GpiBox.
        RECTL rcl = { x, y, x + w, y + h };
        WinFillRect( hps, &rcl, bg_color.pixel() );
    }
    if ( QColor::hPal() ) {
	GpiSelectPalette( hps, oldPal );
    }
}

/*****************************************************************************
  QPainter member functions
 *****************************************************************************/

void QPainter::initialize()
{
}

void QPainter::cleanup()
{
}

void QPainter::destroy()
{

}

void QPainter::init()
{
    d = 0;
    flags = IsStartingUp;
    pdev = 0;
    bg_col = white;				// default background color
    bg_mode = TransparentMode;			// default background mode
    rop = CopyROP;				// default ROP
    tabstops = 0;				// default tabbing
    tabarray = 0;
    tabarraylen = 0;
    block_ext = FALSE;
    txop = txinv = 0;
    ps_stack = 0;
    wm_stack = 0;
    hps = 0;
    holdpal = 0;
    holdrgn = HRGN_ERROR;
    hbrushbm = 0;
    pixmapBrush = nocolBrush = FALSE;
    devh = 0;
    pfont = 0;
}


void QPainter::setFont( const QFont &font )
{
#if defined(QT_CHECK_STATE)
    if ( !isActive() )
	qWarning( "QPainter::setFont: Will be reset by begin()" );
#endif
    if ( cfont.d != font.d || testf(VolatileDC) ) {
	cfont = font;
	setf(DirtyFont);
    }
}


void QPainter::updateFont()
{
    QFont *temp = 0;
    clearf(DirtyFont);
    if ( testf(ExtDev) ) {
	QPDevCmdParam param[1];
	param[0].font = &cfont;
	if ( !pdev->cmd( QPaintDevice::PdcSetFont, this, param ) || !hps )
	    return;
    }
    if ( pdev->devType() == QInternal::Printer ) {
	//int dw = pdev->metric( QPaintDeviceMetrics::PdmWidth );
	//int dh = pdev->metric( QPaintDeviceMetrics::PdmHeight );
	// ### fix compat mode
	//bool vxfScale = testf(Qt2Compat) && testf(VxF)
	//		&& ( dw != ww || dw != vw || dh != wh || dh != vh );

	if ( pfont ) temp = pfont;
	pfont = new QFont( cfont.d, pdev );
        pfont->selectTo( hps );
    } else {
	if ( pfont ) {
	    temp = pfont;
	    pfont = 0;
	}
        cfont.selectTo( hps );
    }
    delete temp;
}


void QPainter::updatePen()
{
    if ( testf(ExtDev) ) {
	QPDevCmdParam param[1];
	param[0].pen = &cpen;
	if ( !pdev->cmd(QPaintDevice::PdcSetPen,this,param) || !hps )
	    return;
    }

    ULONG LineAttrs = LBB_COLOR | LBB_TYPE | LBB_BACK_MIX_MODE;
    LINEBUNDLE lb;
    lb.lColor = COLOR_VALUE( cpen.color() );
    lb.usBackMixMode = GpiQueryBackMix( hps );

    switch ( cpen.style() ) {
	case NoPen:
            lb.usType = LINETYPE_INVISIBLE;
            lb.usBackMixMode = BM_LEAVEALONE;
	    break;
	case SolidLine:
            lb.usType = LINETYPE_SOLID;
	    break;
	case DashLine:
            lb.usType = LINETYPE_LONGDASH;
	    break;
	case DotLine:
	    lb.usType = LINETYPE_DOT;
	    break;
	case DashDotLine:
	    lb.usType = LINETYPE_DASHDOT;
	    break;
	case DashDotDotLine:
	    lb.usType = LINETYPE_DASHDOUBLEDOT;
	    break;
	case MPenStyle:
#if defined(QT_CHECK_STATE)
            qWarning("QPainter::updatePen: MPenStyle used as a value. It is a mask!");
#endif
	    break;
    }

    if ( cpen.width() > 1 )  {
        LineAttrs |= LBB_END;
	switch ( cpen.capStyle() ) {
	    case SquareCap:
		lb.usEnd = LINEEND_SQUARE;
		break;
	    case RoundCap:
		lb.usEnd = LINEEND_ROUND;
		break;
	    case FlatCap:
		lb.usEnd = LINEEND_FLAT;
		break;
            case MPenCapStyle:
                LineAttrs &= ~LBB_END;
#if defined(QT_CHECK_STATE)
	        qWarning("QPainter::updatePen: MPenCapStyle used as a value. It is a mask!");
#endif
                break;
	}
        LineAttrs |= LBB_JOIN;
	switch ( cpen.joinStyle() ) {
	    case BevelJoin:
		lb.usJoin = LINEJOIN_BEVEL;
		break;
	    case RoundJoin:
		lb.usJoin = LINEJOIN_ROUND;
		break;
	    case MiterJoin:
		lb.usJoin = LINEJOIN_MITRE;
		break;
            case MPenJoinStyle:
                LineAttrs &= ~LBB_JOIN;
#if defined(QT_CHECK_STATE)
	        qWarning("QPainter::updatePen: MPenJoinStyle used as a value. It is a mask!");
#endif
                break;
	}
        LineAttrs |= LBB_GEOM_WIDTH;
        lb.lGeomWidth = cpen.width();
    }
    GpiSetAttrs( hps, PRIM_LINE, LineAttrs, 0, (PBUNDLE) &lb );
    // also set the image color attribute used to draw bitmaps
    // and char color attribute for drawing text
    GpiSetAttrs( hps, PRIM_IMAGE, IBB_COLOR, 0, (PBUNDLE) &lb );
    GpiSetAttrs( hps, PRIM_CHAR, CBB_COLOR, 0, (PBUNDLE) &lb );
}

void QPainter::updateBrush()
{
    if ( testf(ExtDev) ) {
	QPDevCmdParam param[1];
	param[0].brush = &cbrush;
	if ( !pdev->cmd(QPaintDevice::PdcSetBrush,this,param) || !hps )
	    return;
    }

    AREABUNDLE ab;
    ab.lColor = COLOR_VALUE( cbrush.data->color );
    GpiSetAttrs( hps, PRIM_AREA, ABB_COLOR, 0, &ab );

    // pattern attrs make sence for area attrs only, so we use global
    // GpiSetPattern*() calls below (it's necessary to set them separately).

    // delete the brush LCID if any
    if ( hbrushbm ) {
        GpiSetPatternSet( hps, LCID_DEFAULT );
        GpiDeleteSetId( hps, LCID_QTPixmapBrush );
    }

    /// @todo (dmik) possibly pixmapBrush is not needed, hbrushbm and
    //  CustomPattern can substitute it.
    pixmapBrush = nocolBrush = FALSE;
    hbrushbm = 0;

    int bs = cbrush.style();
    if ( bs == CustomPattern ) {
        HPS hbrushps = cbrush.pixmap()->hps;
        hbrushbm = cbrush.pixmap()->hbm();
        pixmapBrush = TRUE;
        nocolBrush = cbrush.pixmap()->depth() == 1;
        GpiSetBitmap( hbrushps, 0 );
        GpiSetBitmapId( hps, hbrushbm, LCID_QTPixmapBrush );
        GpiSetPatternSet( hps, LCID_QTPixmapBrush );
        GpiSetBitmap( hbrushps, hbrushbm );
    } else {
	LONG pat;
        if ( bs >= Dense1Pattern && bs <= Dense7Pattern ) {
            pat = PATSYM_DENSE2 + (bs - Dense1Pattern);
        } else {
            switch ( bs ) {
                case NoBrush:
                    pat = PATSYM_BLANK;
                    break;
                case SolidPattern:
                    pat = PATSYM_SOLID;
                    break;
                case HorPattern:
                    pat = PATSYM_HORIZ;
                    break;
                case VerPattern:
                    pat = PATSYM_VERT;
                    break;
                case CrossPattern:
                    pat = PATSYM_HATCH;
                    break;
                case BDiagPattern:
                    pat = PATSYM_DIAG1;
                    break;
                case FDiagPattern:
                    pat = PATSYM_DIAG3;
                    break;
                case DiagCrossPattern:
                    pat = PATSYM_DIAGHATCH;
                    break;
                default:
                    pat = PATSYM_HORIZ;
            }
        }
        nocolBrush = TRUE;
        GpiSetPatternSet( hps, LCID_DEFAULT );
        GpiSetPattern( hps, pat );
    }
}


bool QPainter::begin( const QPaintDevice *pd, bool unclipped )
{
    if ( isActive() ) {				// already active painting
#if defined(QT_CHECK_STATE)
	qWarning( "QPainter::begin: Painter is already active."
		 "\n\tYou must end() the painter before a second begin()" );
#endif
	return FALSE;
    }
    if ( pd == 0 ) {
#if defined(QT_CHECK_NULL)
	qWarning( "QPainter::begin: Paint device cannot be null" );
#endif
	return FALSE;
    }

    const QWidget *copyFrom = 0;
    pdev = redirect( (QPaintDevice*)pd );
    if ( pdev ) {				    // redirected paint device?
	if ( pd->devType() == QInternal::Widget )
	    copyFrom = (const QWidget *)pd;	    // copy widget settings
    } else {
	pdev = (QPaintDevice*)pd;
    }

    if ( pdev->isExtDev() && pdev->paintingActive() ) {		// somebody else is already painting
#if defined(QT_CHECK_STATE)
	qWarning( "QPainter::begin: Another QPainter is already painting "
		  "this device;\n\tAn extended paint device can only be "
		  "painted by one QPainter at a time" );
#endif
	return FALSE;
    }

    bool reinit = flags != IsStartingUp;	// 2nd, 3rd,.... time called
    flags = 0x0;				// init flags
    int dt = pdev->devType();			// get the device type

    if ( (pdev->devFlags & QInternal::ExternalDevice) != 0 )
	setf(ExtDev);				// this is an extended device
    if ( (pdev->devFlags & QInternal::CompatibilityMode) != 0 )
	setf(Qt2Compat);
    else if ( dt == QInternal::Pixmap )	{       // device is a pixmap
        QPixmap *pm = (QPixmap*) pdev;          // will modify it 
	pm->detach();		                
        // Ensure the auxiliary masked bitmap created for masking is destroyed
        // (to cause it to be recreated when the pixmap is blitted next time).
        // Also ensure the alpha channel of the pixmap is unfolded -- any GPI
        // call will clear the high byte that may be storing the alpha bits,
        // so the pixmap will appear as fully transparent.
        if ( pm->data->mask )
            pm->prepareForMasking( FALSE, TRUE );
        if ( pm->data->hasRealAlpha )
            pm->unfoldAlphaChannel();
    }

    hps = 0;
    holdpal = 0;
    holdrgn = HRGN_ERROR;
    devh = 0;

    QRegion *repaintRgn = 0;

    if ( testf(ExtDev) ) {			// external device
	if ( !pdev->cmd(QPaintDevice::PdcBegin,this,0) ) {
	    pdev = 0;				// could not begin
	    clearf( IsActive | DirtyFont | ExtDev );
	    return FALSE;
	}
	if ( tabstops )				// update tabstops for device
	    setTabStops( tabstops );
	if ( tabarray )				// update tabarray for device
	    setTabArray( tabarray );
    }

    setf( IsActive );
    pdev->painters++;				// also tell paint device
    bro = QPoint( 0, 0 );
    if ( reinit ) {
	bg_mode = TransparentMode;		// default background mode
	rop = CopyROP;				// default ROP
	wxmat.reset();				// reset world xform matrix
	xmat.reset();
	ixmat.reset();
	txop = txinv = 0;
	if ( dt != QInternal::Widget ) {
	    QFont  defaultFont;			// default drawing tools
	    QPen   defaultPen;
	    QBrush defaultBrush;
	    cfont  = defaultFont;		// set these drawing tools
	    cpen   = defaultPen;
	    cbrush = defaultBrush;
	    bg_col = white;			// default background color
	}
    }
    wx = wy = vx = vy = 0;			// default view origins
    ww = 0;

    if ( dt == QInternal::Widget ) {		// device is a widget
	QWidget *w = (QWidget*)pdev;
	cfont = w->font();			// use widget font
	cpen = QPen( w->foregroundColor() );	// use widget fg color
	if ( reinit ) {
	    QBrush defaultBrush;
	    cbrush = defaultBrush;
	}
	bg_col	= w->backgroundColor();		// use widget bg color
	ww = vw = w->width();			// default view size
	wh = vh = w->height();
        devh = wh;                              // for final y coord. flipping
        if ( !w->isDesktop() )
            repaintRgn =                        // clip rgn passed from repaint()
                (QRegion *) WinQueryWindowULong( w->winId(), QWL_QTCLIPRGN );
	if ( w->testWState(Qt::WState_InPaintEvent) ) {
	    hps = w->hps;			// during paint event
	} else {
            hps = w->getTargetPS ( unclipped ? QWidget::Unclipped :
                                               QWidget::ClipDefault ); 
	    w->hps = hps;
	}
    } else if ( dt == QInternal::Pixmap ) {	// device is a pixmap
	QPixmap *pm = (QPixmap*)pdev;
	if ( pm->isNull() ) {
#if defined(QT_CHECK_NULL)
	    qWarning( "QPainter::begin: Cannot paint null pixmap" );
#endif
	    end();
	    return FALSE;
	}
	hps = pm->hps;
	ww = vw = pm->width();			// default view size
	wh = vh = pm->height();
        devh = wh;                              // for final y coord. flipping
	if ( pm->depth() == 1 ) {		// monochrome pixmap
	    setf( MonoDev );
	    bg_col = color0;
	    cpen.setColor( color1 );
	}
/// @todo (dmik) later
//    } else if ( dt == QInternal::Printer ) {	// device is a printer
//	if ( pdev->handle() )
//	    hdc = pdev->handle();
//	flags |= (NoCache | RGBColor);
//	if ( qt_winver & WV_DOS_based )
//	    flags |= VolatileDC;
    } else if ( dt == QInternal::System ) {	// system-dependent device
	hps = pdev->handle();
	if ( hps ) {
	    SIZEL sz;
            GpiQueryPS( hps, &sz );
	    ww = vw = sz.cx;
	    wh = vh = sz.cy;
	}
    }
    if ( testf(ExtDev) ) {
	ww = vw = pdev->metric( QPaintDeviceMetrics::PdmWidth );
	wh = vh = pdev->metric( QPaintDeviceMetrics::PdmHeight );
    }
    if ( ww == 0 )
	ww = wh = vw = vh = 1024;
    if ( copyFrom ) {				// copy redirected widget
	cfont = copyFrom->font();
	cpen = QPen( copyFrom->foregroundColor() );
	bg_col = copyFrom->backgroundColor();
        repaintRgn =
            (QRegion *) WinQueryWindowULong( copyFrom->winId(), QWL_QTCLIPRGN );
    }
    if ( testf(ExtDev) && hps == 0 ) {		// external device
	setBackgroundColor( bg_col );		// default background color
	setBackgroundMode( TransparentMode );	// default background mode
	setRasterOp( CopyROP );			// default raster operation
    }
    if ( hps ) {				// initialize hps
	if ( QColor::hPal() && dt != QInternal::Printer ) {
	    holdpal = GpiSelectPalette( hps, QColor::hPal() );
	} else {
            // direct RGB mode
            GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );
        }
        qt_GpiSetBackColor( hps, COLOR_VALUE( bg_col ) );
        GpiSetMix( hps, FM_OVERPAINT );
        qt_GpiSetBackMix( hps, BM_LEAVEALONE, cpen.style() == NoPen );
        GpiSetTextAlignment( hps, TA_NORMAL_HORIZ, TA_BASE );
    }
    updatePen();
    updateBrush();
    setBrushOrigin( 0, 0 );
    if ( hps ) {
        holdrgn = GpiQueryClipRegion( hps );
        if ( repaintRgn ) {
            // ensure that def_crgn is not null after assignment in any case
            def_crgn = repaintRgn->isNull() ? QRegion( FALSE ) : *repaintRgn; 
        }
        // clip region in GPI cannot be used for anything else, so detach it
        def_crgn.detach();
        if ( holdrgn ) {
            // intersect the original clip region and the region from repaint()
            if ( def_crgn.isNull() )
                def_crgn = QRegion( 0, 0, ww, wh );
            GpiSetClipRegion( hps, 0, NULL ); // deselect holdrgn
            HRGN hrgn = def_crgn.handle( devh );
            GpiCombineRegion( hps, hrgn, hrgn, holdrgn, CRGN_AND );  
        }
        cur_crgn = def_crgn;
        if ( !cur_crgn.isNull() )
            GpiSetClipRegion( hps, cur_crgn.handle( devh ), NULL );
    } else {
        holdrgn = HRGN_ERROR;
    }
    setf(DirtyFont);

    return TRUE;
}

bool QPainter::end()
{
    if ( !isActive() ) {
#if defined(QT_CHECK_STATE)
	qWarning( "QPainter::end: Missing begin() or begin() failed" );
#endif
	return FALSE;
    }

    killPStack();

    // delete the brush LCID if any
    if ( hbrushbm ) {
        GpiSetPatternSet( hps, LCID_DEFAULT );
        GpiDeleteSetId( hps, LCID_QTPixmapBrush );
        hbrushbm = 0;
    }
    if ( holdrgn != HRGN_ERROR ) {
        GpiSetClipRegion( hps, holdrgn, NULL );
        holdrgn = HRGN_ERROR;
    }
    if ( holdpal ) {
	GpiSelectPalette( hps, holdpal );
        holdpal = 0;
    }
    if ( !pdev )
	return FALSE;

    if ( testf(ExtDev) )
	pdev->cmd( QPaintDevice::PdcEnd, this, 0 );

    if ( pdev->devType() == QInternal::Widget ) {
	if ( !((QWidget*)pdev)->testWState(Qt::WState_InPaintEvent) ) {
	    QWidget *w = (QWidget*)pdev;
            WinReleasePS( w->hps );
	    w->hps = 0;
	}
    }

    if ( !def_crgn.isNull() )
        def_crgn = QRegion();
    cur_crgn = def_crgn;
    
    if ( pfont ) {
	delete pfont;
	pfont = 0;
    }
    // cleanup printer font engines that could have been created by updateFont()
    if( testf(ExtDev) )
	QFontCache::instance->cleanupPrinterFonts();

    flags = 0;
    pdev->painters--;
    pdev = 0;
    hps	 = 0;
    return TRUE;
}

void QPainter::flush(const QRegion &, CoordinateMode)
{
    flush();
}

void QPainter::flush()
{
}


void QPainter::setBackgroundColor( const QColor &c )
{
    if ( !isActive() ) {
#if defined(QT_CHECK_STATE)
	qWarning( "QPainter::setBackgroundColor: Call begin() first" );
#endif
	return;
    }
    bg_col = c;
    if ( testf(ExtDev) ) {
	QPDevCmdParam param[1];
	param[0].color = &bg_col;
	if ( !pdev->cmd(QPaintDevice::PdcSetBkColor,this,param) || !hps )
	    return;
    }
    // set the back color for all primitives
    qt_GpiSetBackColor( hps, COLOR_VALUE(c) );
}

// background rop codes are the same as foreground according to os2tk45
static const LONG qt_ropCodes[] = {
    FM_OVERPAINT,       // CopyROP
    FM_OR,              // OrROP
    FM_XOR,	        // XorROP
    FM_SUBTRACT,	// NotAndROP
    FM_NOTCOPYSRC,	// NotCopyROP
    FM_MERGENOTSRC,	// NotOrROP
    FM_NOTXORSRC,	// NotXorROP ??
    FM_AND,	        // AndROP
    FM_INVERT,	        // NotROP
    FM_ZERO,	        // ClearROP
    FM_ONE,	        // SetROP
    FM_LEAVEALONE,      // NopROP
    FM_MASKSRCNOT,	// AndNotROP
    FM_MERGESRCNOT,	// OrNotROP
    FM_NOTMASKSRC,	// NandROP
    FM_NOTMERGESRC	// NorROP
};

void QPainter::setBackgroundMode( BGMode m )
{
    if ( !isActive() ) {
#if defined(QT_CHECK_STATE)
	qWarning( "QPainter::setBackgroundMode: Call begin() first" );
#endif
	return;
    }
    if ( m != TransparentMode && m != OpaqueMode ) {
#if defined(QT_CHECK_RANGE)
	qWarning( "QPainter::setBackgroundMode: Invalid mode" );
#endif
	return;
    }
    bg_mode = m;
    if ( testf(ExtDev) ) {
	QPDevCmdParam param[1];
	param[0].ival = m;
	if ( !pdev->cmd(QPaintDevice::PdcSetBkMode,this,param) || !hps )
	    return;
    }
    // set the back mix for all primitives
    qt_GpiSetBackMix(
        hps, m == TransparentMode ? BM_LEAVEALONE : qt_ropCodes[rop],
        cpen.style() == NoPen
    );
}

void QPainter::setRasterOp( RasterOp r )
{
    if ( !isActive() ) {
#if defined(QT_CHECK_STATE)
	qWarning( "QPainter::setRasterOp: Call begin() first" );
#endif
	return;
    }
    if ( (uint)r > LastROP ) {
#if defined(QT_CHECK_RANGE)
	qWarning( "QPainter::setRasterOp: Invalid ROP code" );
#endif
	return;
    }
    rop = r;
    if ( testf(ExtDev) ) {
	QPDevCmdParam param[1];
	param[0].ival = r;
	if ( !pdev->cmd(QPaintDevice::PdcSetROP,this,param) || !hps )
	    return;
   }
   // set the mixes for all primitives
   GpiSetMix( hps, qt_ropCodes[r] );
   if ( bg_mode != TransparentMode )
       qt_GpiSetBackMix( hps, qt_ropCodes[r], cpen.style() == NoPen );
}

void QPainter::setBrushOrigin( int x, int y )
{
    if ( !isActive() ) {
#if defined(QT_CHECK_STATE)
	qWarning( "QPainter::setBrushOrigin: Call begin() first" );
#endif
	return;
    }
    bro = QPoint(x,y);
    if ( testf(ExtDev) ) {
	QPDevCmdParam param[1];
	param[0].point = &bro;
	if ( !pdev->cmd(QPaintDevice::PdcSetBrushOrigin,this,param) || !hps )
	    return;
    }
    POINTL ptl = { x, y };
    // make y coordinate bottom-relative
    if ( devh ) ptl.y = devh - ptl.y;
    GpiSetPatternRefPoint( hps, &ptl );
}

/**
 *  Sets up a native transformation matrix on this painter.
 *  If the paint device has a defined height (i.e. it's QPixmap or
 *  QWidget), the matrix is adjusted to automatically flip the y axis
 *  (to make it increase downwards).
 *
 *  @param assumeYNegation
 *      If true, it is assumed that all y coordinates are negated before
 *      passing them to GPI calls. This mode is useful for drawing text
 *      because font letters are already flipped by GPI when the text is drawn,
 *      so using full y axis flip would flip letters again (that is definitely
 *      unwanted).
 */
bool QPainter::setNativeXForm( bool assumeYNegation )
{
    QWMatrix mtx;

    if ( testf(VxF) ) {
        mtx.translate( vx, vy );
        mtx.scale( 1.0*vw/ww, 1.0*vh/wh );
        mtx.translate( -wx, -wy );
        mtx = wxmat * mtx;
    } else {
        mtx = wxmat;
    }

    // Qt rotates clockwise (despite stated in QWMatrix::rotate() docs),
    // while GPI rotates counterclockwise. Correct this by changing
    // m12 and m21 signs and also apply optional y translation.

    MATRIXLF m;
    m.fxM11 = (FIXED) (mtx.m11() * 65536.0);
    m.fxM12 = (FIXED) (-mtx.m12() * 65536.0);
    m.fxM21 = (FIXED) (-mtx.m21() * 65536.0);
    m.fxM22 = (FIXED) (mtx.m22() * 65536.0);
    m.lM31 = (LONG) mtx.dx();
    m.lM32 = devh && assumeYNegation ? ((LONG) -mtx.dy()) : (LONG) mtx.dy();
    m.lM13 = m.lM23 = 0;

    BOOL ok = GpiSetDefaultViewMatrix( hps, 8, &m, TRANSFORM_REPLACE );
    if (ok && devh) {
        // add a matrix to do automatic y flip or just to move negative
        // y coordinates back to the viewable area
        m.fxM11 = MAKEFIXED( 1, 0 );
        m.fxM12 = 0;
        m.fxM21 = 0;
        m.fxM22 = assumeYNegation ? MAKEFIXED( 1, 0 ) : MAKEFIXED( -1, 0 );
        m.lM31 = 0;
        m.lM32 = devh - 1;
        m.lM13 = m.lM23 = 0;
        ok = GpiSetDefaultViewMatrix( hps, 8, &m, TRANSFORM_ADD );
    }

    return ok;
}

void QPainter::clearNativeXForm()
{
    // restore identity matrix
    GpiSetDefaultViewMatrix( hps, 0, NULL, TRANSFORM_REPLACE );
}

void QPainter::setClipping( bool enable )
{
    if ( !isActive() ) {
#if defined(QT_CHECK_STATE)
	qWarning( "QPainter::setClipping: Will be reset by begin()" );
#endif
	return;
    }

    if ( !isActive() || enable == testf(ClipOn) )
	return;

    setf( ClipOn, enable );
    if ( testf(ExtDev) ) {
	if ( block_ext )
	    return;
	QPDevCmdParam param[1];
	param[0].ival = enable;
	if ( !pdev->cmd(QPaintDevice::PdcSetClip,this,param) || !hps )
	    return;
    }

    // the current clip region is stored in cur_crgn, so deselect the handle
    // to let us operate on it. Note that the assignment op below will destroy
    // the handle if it is no more referred, so there is no need to do it
    // explicitly.
    GpiSetClipRegion( hps, 0, NULL );

    if ( enable ) {
        if ( !def_crgn.isNull() )
            cur_crgn = crgn.intersect( def_crgn );
        else
            cur_crgn = crgn.isNull() ? QRegion( FALSE ) : crgn;
#ifndef QT_NO_PRINTER
/// @todo (dmik) later
//	if ( pdev->devType() == QInternal::Printer ) {
//	    double xscale = ((float)pdev->metric( QPaintDeviceMetrics::PdmPhysicalDpiX )) /
//    		((float)pdev->metric( QPaintDeviceMetrics::PdmDpiX ));
//	    double yscale = ((float)pdev->metric( QPaintDeviceMetrics::PdmPhysicalDpiY )) /
//    		((float)pdev->metric( QPaintDeviceMetrics::PdmDpiY ));
//	    double xoff = 0;
//	    double yoff = 0;
//	    QPrinter* printer = (QPrinter*)pdev;
//	    if ( printer->fullPage() ) {	// must adjust for margins
//		xoff = - GetDeviceCaps( printer->handle(), PHYSICALOFFSETX );
//		yoff = - GetDeviceCaps( printer->handle(), PHYSICALOFFSETY );
//	    }
//	    rgn = QWMatrix( xscale, 0, 0, yscale, xoff, yoff ) * rgn;
//	}
#endif
        // clip region in GPI cannot be used for anything else, so detach it
        cur_crgn.detach();
    } else {
        cur_crgn = def_crgn;
    }

    if ( !cur_crgn.isNull() )
        GpiSetClipRegion( hps, cur_crgn.handle( devh ), NULL );
}


void QPainter::setClipRect( const QRect &r, CoordinateMode m )
{
    QRegion rgn( r );
    setClipRegion( rgn, m );
}

void QPainter::setClipRegion( const QRegion &rgn, CoordinateMode m )
{
#if defined(QT_CHECK_STATE)
    if ( !isActive() )
	qWarning( "QPainter::setClipRegion: Will be reset by begin()" );
#endif
    if ( m == CoordDevice )
	crgn = rgn;
    else
	crgn = xmat * rgn;

    if ( testf(ExtDev) ) {
	if ( block_ext )
	    return;
	QPDevCmdParam param[2];
	param[0].rgn = &rgn;
	param[1].ival = m;
	if ( !pdev->cmd(QPaintDevice::PdcSetClipRegion,this,param) || !hps )
	    return;
    }
    clearf( ClipOn );				// be sure to update clip rgn
    setClipping( TRUE );
}

// Note: this function does not check the array range!
void QPainter::drawPolyInternal(
    const QPointArray &a, bool close, bool winding, int index, int npoints,
    bool disjoint
)
{
    // QCOORD is a double word that is the same as POINTL members, and since
    // the order of xp and yp fields in the QPoint is also the same, we simply
    // cast QPointArray::data() to PPOINTL below.

    if ( npoints < 0 )
	npoints = a.size() - index;

    QPointArray pa = a;
    // flip y coordinates
    if ( devh ) {
        pa = QPointArray( npoints );
        for ( int i = 0; i < npoints; i++ ) {
            pa[i].setX( a[index+i].x() );
            pa[i].setY( devh - (a[index+i].y() + 1) );
        }
        index = 0;
    }

    ULONG opts = winding ? BA_WINDING : BA_ALTERNATE;
    const PPOINTL ptls = ((PPOINTL) pa.data()) + index;
    if ( cpen.width() > 1 ) {
        if ( close ) {
            GpiBeginArea( hps, BA_NOBOUNDARY | opts );
            if ( disjoint ) {
                GpiPolyLineDisjoint( hps, npoints, ptls );
            } else {
                GpiMove( hps, ptls );
                GpiPolyLine( hps, npoints - 1, ptls + 1 );
            }
            GpiEndArea( hps );
        }
        qt_geom_line_handle gh;
        qt_begin_geom_line( hps, COLOR_VALUE(cpen.color()), cpen.style() == NoPen, &gh );
        if ( disjoint ) {
            GpiPolyLineDisjoint( hps, npoints, ptls );
        } else {
            GpiMove( hps, ptls );
            GpiPolyLine( hps, npoints - 1, ptls + 1 );
        }
        if ( close )
            GpiCloseFigure( hps );
        qt_end_geom_line( hps, &gh );
    } else {
        if ( close )
            GpiBeginArea( hps, BA_BOUNDARY | opts );
        else
            GpiBeginPath( hps, 1 );
        if ( disjoint ) {
            GpiPolyLineDisjoint( hps, npoints, ptls );
        } else {
            GpiMove( hps, ptls );
            GpiPolyLine( hps, npoints - 1, ptls + 1 );
        }
        if ( close )
            GpiEndArea( hps );
        else {
            GpiEndPath( hps );
            GpiOutlinePath( hps, 1, 0 );
        }
    }
}

// angles in Qt are 1/16th of a degree, so the integer part of FIXED is
// angle/16 and the fixed part is (65536/16) * angle%16.
#define MAKEDEGREE(a16) MAKEFIXED((a16>>4), (a16<<12))

static void doDrawArc( HPS hps, PPOINTL cptl, int first, int a, int alen )
{
    if ( first >= 0 ) {
        int sa, ea;
        for ( int i = first; i < 4; i++, cptl++ ) {
            sa = i * 90*16;
            ea = sa + 90*16;
            if (sa < a) sa = a;
            if (ea > a + alen) ea = a + alen;
            if (ea <= sa) break;
            ea -= sa;
            GpiPartialArc( hps, cptl, MAKEFIXED(1,0), MAKEDEGREE(sa), MAKEDEGREE(ea) );
        }
    } else {
        GpiPartialArc( hps, cptl, MAKEFIXED(1,0), MAKEDEGREE(a), MAKEDEGREE(alen) );
    }
}

void QPainter::drawArcInternal(
    int x, int y, int w, int h, int a, int alen, ArcClose close
)
{
    if ( w <= 0 || h <= 0 ) {
	if ( w == 0 || h == 0 )
	    return;
	fix_neg_rect( &x, &y, &w, &h );
    }

    const int FullArc = 360*16;
    // correct angles
    if ( alen < 0 ) {
        a += alen;
        alen = -alen;
    }
    alen = QMIN( alen, FullArc );
    if ( a < 0 )
        a = FullArc - ((-a) % (FullArc));
    else
        a = a % FullArc;

    int axr = (w - 1) / 2;
    int ayr = (h - 1) / 2;
    ARCPARAMS arcparams = { axr, ayr, 0, 0 };
    GpiSetArcParams( hps, &arcparams );
    int xc, yc;
    xc = x + axr;
    yc = y + ayr;
    // flip y coordinate
    if ( devh ) yc = devh - (yc + 1);

    POINTL sptl = { xc, yc }, cptls [4];
    PPOINTL cptl = cptls;
    int first = -1;

    // in order to draw arcs whose bounding reactangle has even width
    // and/or height we draw each quarter of the arc separately,
    // correspondingly moving its center point by one pixel.
    int extX = (w - 1) % 2;
    int extY = (h - 1) % 2;
    if ( extX != 0 || extY != 0 ) {
        for ( int i = 0, sa = 90*16; i < 4; i++, sa += 90*16, cptl++ ) {
            if ( first < 0 && sa > a )
                first = i;
            if ( first >= 0 ) {
                switch ( i ) {
                    case 0: cptl->x = xc + extX; cptl->y = yc;        break;
                    case 1: cptl->x = xc;        cptl->y = yc;        break;
                    case 2: cptl->x = xc;        cptl->y = yc - extY; break;
                    case 3: cptl->x = xc + extX; cptl->y = yc - extY; break;
                }
            }
        }
        cptl = cptls + first;
    } else {
        *cptl = sptl;
    }

    if ( close != ClosePie ) {
        // set the current position to the start of the arc
        LONG oldMix = GpiQueryMix( hps );
        LONG oldType = GpiQueryLineType( hps );
        GpiSetMix( hps, FM_LEAVEALONE );
        GpiSetLineType( hps, LINETYPE_SOLID );
        GpiPartialArc( hps, cptl, MAKEFIXED(1,0), MAKEDEGREE(a), 0 );
        GpiSetLineType( hps, oldType );
        GpiSetMix( hps, oldMix );
        // cause the start point to be drawn (seems like a bug in OS/2 GPI)
        GpiQueryCurrentPosition( hps, &sptl );
    }

    if ( cpen.width() > 1 ) {
        if ( close != CloseNone ) {
            GpiBeginArea( hps, BA_NOBOUNDARY | BA_ALTERNATE );
            GpiMove( hps, &sptl );
            doDrawArc( hps, cptl, first, a, alen );
            GpiEndArea( hps );
        }
        qt_geom_line_handle gh;
        qt_begin_geom_line( hps, COLOR_VALUE(cpen.color()),
                            cpen.style() == NoPen, &gh );

        GpiMove( hps, &sptl );
        doDrawArc( hps, cptl, first, a, alen );

        if ( close != CloseNone )
            GpiCloseFigure( hps );
        qt_end_geom_line( hps, &gh );
    } else {
        if ( close != CloseNone )
            GpiBeginArea( hps, BA_BOUNDARY | BA_ALTERNATE );
        else
            GpiBeginPath( hps, 1 );

        GpiMove( hps, &sptl );
        doDrawArc( hps, cptl, first, a, alen );

        if ( close != CloseNone )
            GpiEndArea( hps );
        else {
            GpiEndPath( hps );
            GpiOutlinePath( hps, 1, 0 );
        }
    }
}

void QPainter::drawPoint( int x, int y )
{
    if ( !isActive() )
	return;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[1];
	    QPoint p( x, y );
	    param[0].point = &p;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawPoint,this,param) || !hps )
		return;
	}
	map( x, y, &x, &y );
    }
    POINTL ptl = { x, y };
    if ( devh ) ptl.y = devh - ( ptl.y + 1 );
    GpiSetPel( hps, &ptl );
}


void QPainter::drawPoints( const QPointArray& a, int index, int npoints )
{
    if ( npoints < 0 )
	npoints = a.size() - index;
    if ( index + npoints > (int)a.size() )
	npoints = a.size() - index;
    if ( !isActive() || npoints < 1 || index < 0 )
	return;
    QPointArray pa = a;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[1];
	    for (int i=0; i<npoints; i++) {
		QPoint p( pa[index+i].x(), pa[index+i].y() );
		param[0].point = &p;
		if ( !pdev->cmd(QPaintDevice::PdcDrawPoint,this,param))
		    return;
	    }
	    if ( !hps ) return;
	}
	if ( txop != TxNone ) {
	    pa = xForm( a, index, npoints );
	    if ( pa.size() != a.size() ) {
		index = 0;
		npoints = pa.size();
	    }
	}
    }
    for (int i=0; i<npoints; i++) {
        POINTL ptl = { pa[index+i].x(), pa[index+i].y() };
        if ( devh ) ptl.y = devh - ( ptl.y + 1 );
        GpiSetPel( hps, &ptl );
    }
}

void QPainter::moveTo( int x, int y )
{
    if ( !isActive() )
	return;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[1];
	    QPoint p( x, y );
	    param[0].point = &p;
	    if ( !pdev->cmd(QPaintDevice::PdcMoveTo,this,param) || !hps )
		return;
	}
	map( x, y, &x, &y );
    }
    POINTL ptl = { x, y };
    if ( devh ) ptl.y = devh - ( ptl.y + 1 );
    GpiMove( hps, &ptl );
}


void QPainter::lineTo( int x, int y )
{
    if ( !isActive() )
	return;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[1];
	    QPoint p( x, y );
	    param[0].point = &p;
	    if ( !pdev->cmd(QPaintDevice::PdcLineTo,this,param) || !hps )
		return;
	}
	map( x, y, &x, &y );
    }

    qt_geom_line_handle gh;
    if ( cpen.width() > 1 ) {
        qt_begin_geom_line( hps, COLOR_VALUE(cpen.color()), cpen.style() == NoPen, &gh );
    }

    POINTL ptl = { x, y };
    if ( devh ) ptl.y = devh - ( ptl.y + 1 );
    GpiLine( hps, &ptl );

    if ( cpen.width() > 1 ) {
        qt_end_geom_line( hps, &gh );
    }
}


void QPainter::drawLine( int x1, int y1, int x2, int y2 )
{
    if ( !isActive() )
	return;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[2];
	    QPoint p1(x1, y1), p2(x2, y2);
	    param[0].point = &p1;
	    param[1].point = &p2;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawLine,this,param) || !hps )
		return;
	}
	map( x1, y1, &x1, &y1 );
	map( x2, y2, &x2, &y2 );
    }

    qt_geom_line_handle gh;
    if ( cpen.width() > 1 ) {
        qt_begin_geom_line( hps, COLOR_VALUE(cpen.color()), cpen.style() == NoPen, &gh );
    }

    POINTL ptl = { x1, y1 };
    if ( devh ) ptl.y = devh - (ptl.y + 1);
    GpiMove( hps, &ptl );
    ptl.x = x2;
    ptl.y = y2;
    if ( devh ) ptl.y = devh - (ptl.y + 1);
    GpiLine( hps, &ptl );

    if ( cpen.width() > 1 ) {
        qt_end_geom_line( hps, &gh );
    }
}

static void doDrawRect( HPS hps, int x, int y, int w, int h, int axr, int ayr )
{
    const FIXED RightAngle = MAKEFIXED(90,0);
    const FIXED One = MAKEFIXED(1,0);
    
    // top/right coordinates are inclusive
    -- w;
    -- h;
    
    // beginning of the bottom side
    POINTL ptl = { x + axr, y };
    GpiMove( hps, &ptl );
    // center of the bottom-right arc
    ptl.x = x + w - axr; ptl.y = y + ayr;
    GpiPartialArc( hps, &ptl, One, MAKEFIXED(270,0), RightAngle );
    // center of the top-right arc
    /* ptl.x = x + w - axr; */ ptl.y = y + h - ayr;
    GpiPartialArc( hps, &ptl, One, MAKEFIXED(0,0), RightAngle );
    // center of the top-left arc
    ptl.x = x + axr; /* ptl.y = y + h - ayr; */
    GpiPartialArc( hps, &ptl, One, RightAngle, RightAngle );
    // center of the bottom-left arc
    /* ptl.x = x + axr; */ ptl.y = y + ayr;
    GpiPartialArc( hps, &ptl, One, MAKEFIXED(180,0), RightAngle );
}

void QPainter::drawRectInternal(
    int x, int y, int w, int h, int wRnd, int hRnd
) {
    if ( w <= 0 || h <= 0 ) {
	if ( w == 0 || h == 0 )
	    return;
	fix_neg_rect( &x, &y, &w, &h );
    }

    // GpiBox API is buggy when it comes to rounded corners
    // combined with filling the interior (see ticket:16).
    // The fix is to draw rounded corners ourselves.

    if ( wRnd == 0 || hRnd == 0 ) { 
        POINTL ptl1 = { x, y };
        if ( devh ) ptl1.y = devh - ( ptl1.y + h );
        POINTL ptl2 = { x + w - 1, ptl1.y + h - 1 };
        if ( cpen.width() > 1 ) {
            GpiMove( hps, &ptl1 );
            GpiBox( hps, DRO_FILL, &ptl2, wRnd, hRnd );
            qt_geom_line_handle gh;
            qt_begin_geom_line( hps, COLOR_VALUE(cpen.color()),
                                cpen.style() == NoPen, &gh );
            GpiMove( hps, &ptl1 );
            GpiBox( hps, DRO_OUTLINE, &ptl2, 0, 0 );
            qt_end_geom_line( hps, &gh );
        } else {
            GpiMove( hps, &ptl1 );
            GpiBox( hps, DRO_FILL | DRO_OUTLINE, &ptl2, 0, 0 );
        }
        return;
    }
    
    // flip y coordinate
    if ( devh ) y = devh - ( y + h );

    int axr = (wRnd - 1) / 2;
    int ayr = (hRnd - 1) / 2;
    ARCPARAMS arcparams = { axr, ayr, 0, 0 };
    GpiSetArcParams( hps, &arcparams );
    
    if ( cpen.width() > 1 ) {
        GpiBeginArea( hps, BA_NOBOUNDARY | BA_ALTERNATE );
        doDrawRect( hps, x, y, w, h, axr, ayr );
        GpiEndArea( hps );
        qt_geom_line_handle gh;
        qt_begin_geom_line( hps, COLOR_VALUE(cpen.color()),
                            cpen.style() == NoPen, &gh );
        doDrawRect( hps, x, y, w, h, axr, ayr );
        GpiCloseFigure( hps );
        qt_end_geom_line( hps, &gh );
    } else {
        GpiBeginArea( hps, BA_BOUNDARY | BA_ALTERNATE );
        doDrawRect( hps, x, y, w, h, axr, ayr );
        GpiEndArea( hps );
    }
}

void QPainter::drawRect( int x, int y, int w, int h )
{
    if ( !isActive() )
	return;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[1];
	    QRect r( x, y, w, h );
	    param[0].rect = &r;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawRect,this,param) || !hps )
		return;
	}
	if ( txop == TxRotShear ) {		// rotate/shear polygon
	    QPointArray a( QRect(x,y,w,h) );
	    drawPolyInternal( xForm(a) );
	    return;
	}
	map( x, y, w, h, &x, &y, &w, &h );
    }

    drawRectInternal( x, y, w, h, 0, 0 );
}

void QPainter::drawWinFocusRectInternal(
    int x, int y, int w, int h, const QColor &bgCol, bool xormode
)
{
    if ( !isActive() || txop == TxRotShear )
	return;

    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[1];
	    QRect r( x, y, w, h );
	    param[0].rect = &r;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawRect,this,param) || !hps )
		return;
	}
	map( x, y, w, h, &x, &y, &w, &h );
    }
    if ( w <= 0 || h <= 0 ) {
	if ( w == 0 || h == 0 )
	    return;
	fix_neg_rect( &x, &y, &w, &h );
    }

    // flip y coordinate
    if ( devh ) y = devh - (y + h);

    ULONG la =
        LBB_COLOR | LBB_MIX_MODE | LBB_BACK_MIX_MODE | LBB_TYPE;
    LINEBUNDLE oldLb, lb;
    GpiQueryAttrs( hps, PRIM_LINE, la, &oldLb );
    if ( !xormode ) {                   // black/white mode
        if( qGray( bgCol.rgb() ) <= 128 )
            lb.lColor = CLR_WHITE;
        else
            lb.lColor = CLR_BLACK;
        lb.usMixMode = FM_OVERPAINT;
    } else {                            // xor mode
        lb.lColor = CLR_TRUE;
        lb.usMixMode = FM_XOR;
    }
    lb.usBackMixMode = BM_LEAVEALONE;
    lb.usType = LINETYPE_ALTERNATE;
    GpiSetAttrs( hps, PRIM_LINE, la, 0, &lb );

    POINTL ptl = { x, y };
    GpiMove( hps, &ptl );
    ptl.x += w - 1;
    ptl.y += h - 1;
    GpiBox( hps, DRO_OUTLINE, &ptl, 0, 0 );

    GpiSetAttrs( hps, PRIM_LINE, la, 0, &oldLb );
}

void QPainter::drawWinFocusRect( int x, int y, int w, int h, const QColor &bgCol )
{
    drawWinFocusRectInternal( x, y, w, h, bgCol, FALSE );
}

void QPainter::drawWinFocusRect( int x, int y, int w, int h )
{
    if (
        pdev->devType() == QInternal::Widget &&
        GpiQueryClipRegion( hps )
    ) {
        // GPI bug: LINETYPE_ALTERNATE lines are drawn wrongly in XOR mode
        // when the clip region is set on the hps. we use doublebuffering
        // to avoid this.
        QSize ws = ((QWidget*) pdev)->size();
        POINTL ptls[] = { { 0, 0 }, { ws.width(), ws.height() }, { 0, 0 } };
        QPixmap pm( ws );
        GpiBitBlt( pm.hps, hps, 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
        HPS thisHps = hps;
        hps = pm.hps;
        drawWinFocusRectInternal( x, y, w, h, bg_col, TRUE );
        hps = thisHps;
        GpiBitBlt( hps, pm.hps, 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
    } else {
        drawWinFocusRectInternal( x, y, w, h, bg_col, TRUE );
    }
}


void QPainter::drawRoundRect( int x, int y, int w, int h, int xRnd, int yRnd )
{
    if ( !isActive() )
	return;
    if ( xRnd <= 0 || yRnd <= 0 ) {
	drawRect( x, y, w, h );			// draw normal rectangle
	return;
    }
    if ( xRnd >= 100 )				// fix ranges
	xRnd = 99;
    if ( yRnd >= 100 )
	yRnd = 99;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[3];
	    QRect r( x, y, w, h );
	    param[0].rect = &r;
	    param[1].ival = xRnd;
	    param[2].ival = yRnd;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawRoundRect,this,param) || !hps)
		return;
	}
	if ( txop == TxRotShear ) {		// rotate/shear polygon
	    if ( w <= 0 || h <= 0 )
		fix_neg_rect( &x, &y, &w, &h );
	    int rxx = w*xRnd/200;
	    int ryy = h*yRnd/200;
	    // were there overflows?
	    if ( rxx < 0 )
		rxx = w/200*xRnd;
	    if ( ryy < 0 )
		ryy = h/200*yRnd;
	    int rxx2 = 2*rxx;
	    int ryy2 = 2*ryy;
	    QPointArray a[4];
	    a[0].makeArc( x, y, rxx2, ryy2, 1*16*90, 16*90, xmat );
	    a[1].makeArc( x, y+h-ryy2, rxx2, ryy2, 2*16*90, 16*90, xmat );
	    a[2].makeArc( x+w-rxx2, y+h-ryy2, rxx2, ryy2, 3*16*90, 16*90, xmat );
	    a[3].makeArc( x+w-rxx2, y, rxx2, ryy2, 0*16*90, 16*90, xmat );
	    // ### is there a better way to join QPointArrays?
	    QPointArray aa;
	    aa.resize( a[0].size() + a[1].size() + a[2].size() + a[3].size() );
	    uint j = 0;
	    for ( int k=0; k<4; k++ ) {
		for ( uint i=0; i<a[k].size(); i++ ) {
		    aa.setPoint( j, a[k].point(i) );
		    j++;
		}
	    }
	    drawPolyInternal( aa );
	    return;
	}
	map( x, y, w, h, &x, &y, &w, &h );
    }

    drawRectInternal( x, y, w, h, w*xRnd/99, h*yRnd/99 );
}


void QPainter::drawEllipse( int x, int y, int w, int h )
{
    if ( !isActive() )
	return;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[1];
	    QRect r( x, y, w, h );
	    param[0].rect = &r;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawEllipse,this,param) || !hps )
		return;
	}
	if ( txop == TxRotShear ) {		// rotate/shear polygon
	    QPointArray a;
	    int increment = cpen.style() == NoPen ? 1 : 0;
	    a.makeArc( x, y, w+increment, h+increment, 0, 360*16, xmat );
	    drawPolyInternal( a );
	    return;
	}
	map( x, y, w, h, &x, &y, &w, &h );
    }

    drawArcInternal( x, y, w, h );
}


void QPainter::drawArc( int x, int y, int w, int h, int a, int alen )
{
    if ( !isActive() )
	return;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[3];
	    QRect r( x, y, w, h );
	    param[0].rect = &r;
	    param[1].ival = a;
	    param[2].ival = alen;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawArc,this,param) || !hps )
		return;
	}
	if ( txop == TxRotShear ) {		// rotate/shear
	    QPointArray pa;
	    pa.makeArc( x, y, w, h, a, alen, xmat );	// arc polyline
	    drawPolyInternal( pa, FALSE );
	    return;
	}
	map( x, y, w, h, &x, &y, &w, &h );
    }

    drawArcInternal( x, y, w, h, a, alen, CloseNone );
}


void QPainter::drawPie( int x, int y, int w, int h, int a, int alen )
{
    if ( !isActive() )
	return;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[3];
	    QRect r( x, y, w, h );
	    param[0].rect = &r;
	    param[1].ival = a;
	    param[2].ival = alen;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawPie,this,param) || !hps )
		return;
	}
	if ( txop == TxRotShear ) {		// rotate/shear
	    QPointArray pa;
	    int increment = cpen.style() == NoPen ? 1 : 0;
	    pa.makeArc( x, y, w+increment, h+increment, a, alen, xmat ); // arc polyline
	    int n = pa.size();
	    int cx, cy;
	    xmat.map(x+w/2, y+h/2, &cx, &cy);
	    pa.resize( n+1 );
	    pa.setPoint( n, cx, cy );	// add legs
	    drawPolyInternal( pa );
	    return;
	}
	map( x, y, w, h, &x, &y, &w, &h );
    }

    drawArcInternal( x, y, w, h, a, alen, ClosePie );
}


void QPainter::drawChord( int x, int y, int w, int h, int a, int alen )
{
    if ( !isActive() )
	return;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[3];
	    QRect r( x, y, w, h );
	    param[0].rect = &r;
	    param[1].ival = a;
	    param[2].ival = alen;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawChord,this,param) || !hps )
		return;
	}
	if ( txop == TxRotShear ) {		// rotate/shear
	    QPointArray pa;
	    int increment = cpen.style() == NoPen ? 1 : 0;
	    pa.makeArc( x, y, w+increment, h+increment, a, alen, xmat ); // arc polygon
	    int n = pa.size();
	    pa.resize( n+1 );
	    pa.setPoint( n, pa.at(0) );		// connect endpoints
	    drawPolyInternal( pa );
	    return;
	}
	map( x, y, w, h, &x, &y, &w, &h );
    }

    drawArcInternal( x, y, w, h, a, alen, CloseChord );
}


void QPainter::drawLineSegments( const QPointArray &a, int index, int nlines )
{
    if ( nlines < 0 )
	nlines = a.size()/2 - index/2;
    if ( index + nlines*2 > (int)a.size() )
	nlines = (a.size() - index)/2;
    if ( !isActive() || nlines < 1 || index < 0 )
	return;
    QPointArray pa = a;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    if ( 2*nlines != (int)pa.size() ) {
		pa = QPointArray( nlines*2 );
		for ( int i=0; i<nlines*2; i++ )
		    pa.setPoint( i, a.point(index+i) );
		index = 0;
	    }
	    QPDevCmdParam param[1];
	    param[0].ptarr = (QPointArray*)&pa;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawLineSegments,this,param)
		 || !hps )
		return;
	}
	if ( txop != TxNone ) {
	    pa = xForm( a, index, nlines*2 );
	    if ( pa.size() != a.size() ) {
		index  = 0;
		nlines = pa.size()/2;
	    }
	}
    }
    drawPolyInternal( pa, FALSE, FALSE, index, nlines*2, TRUE );
}


void QPainter::drawPolyline( const QPointArray &a, int index, int npoints )
{
    if ( npoints < 0 )
	npoints = a.size() - index;
    if ( index + npoints > (int)a.size() )
	npoints = a.size() - index;
    if ( !isActive() || npoints < 2 || index < 0 )
	return;
    QPointArray pa = a;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    if ( npoints != (int)pa.size() ) {
		pa = QPointArray( npoints );
		for ( int i=0; i<npoints; i++ )
		    pa.setPoint( i, a.point(index+i) );
		index = 0;
	    }
	    QPDevCmdParam param[1];
	    param[0].ptarr = (QPointArray*)&pa;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawPolyline,this,param) || !hps )
		return;
	}
	if ( txop != TxNone ) {
	    pa = xForm( pa, index, npoints );
	    if ( pa.size() != a.size() ) {
		index   = 0;
		npoints = pa.size();
	    }
	}
    }
    drawPolyInternal( pa, FALSE, FALSE, index, npoints );
}


void QPainter::drawConvexPolygon( const QPointArray &pa,
			     int index, int npoints )
{
    // Any efficient way?
    drawPolygon(pa,FALSE,index,npoints);
}

void QPainter::drawPolygon( const QPointArray &a, bool winding, int index,
			    int npoints )
{
    if ( npoints < 0 )
	npoints = a.size() - index;
    if ( index + npoints > (int)a.size() )
	npoints = a.size() - index;
    if ( !isActive() || npoints < 2 || index < 0 )
	return;
    QPointArray pa = a;
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) ) {
	    if ( npoints != (int)a.size() ) {
		pa = QPointArray( npoints );
		for ( int i=0; i<npoints; i++ )
		    pa.setPoint( i, a.point(index+i) );
	    }
	    QPDevCmdParam param[2];
	    param[0].ptarr = (QPointArray*)&pa;
	    param[1].ival = winding;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawPolygon,this,param) || !hps )
		return;
	}
	if ( txop != TxNone ) {
	    pa = xForm( a, index, npoints );
	    if ( pa.size() != a.size() ) {
		index   = 0;
		npoints = pa.size();
	    }
	}
    }
    drawPolyInternal( pa, TRUE, winding, index, npoints );
}

#ifndef QT_NO_BEZIER
void QPainter::drawCubicBezier( const QPointArray &a, int index )
{
    if ( !isActive() )
	return;
    if ( (int)a.size() - index < 4 ) {
#if defined(QT_CHECK_RANGE)
	qWarning( "QPainter::drawCubicBezier: Cubic Bezier needs 4 control "
		 "points" );
#endif
	return;
    }
    QPointArray pa( a );
    if ( testf(ExtDev|VxF|WxF) ) {
	if ( index != 0 || a.size() > 4 ) {
	    pa = QPointArray( 4 );
	    for ( int i=0; i<4; i++ )
		pa.setPoint( i, a.point(index+i) );
	    index = 0;
	}
	if ( testf(ExtDev) ) {
	    QPDevCmdParam param[1];
	    param[0].ptarr = (QPointArray*)&pa;
	    if ( !pdev->cmd(QPaintDevice::PdcDrawCubicBezier,this,param)
		 || !hps )
		return;
	}
	if ( txop != TxNone )
	    pa = xForm( pa );
    }

    // flip y coordinates
    if ( devh ) {
        for ( int i = index; i < 4; i++ )
            pa[i].setY( devh - (pa[i].y() + 1) );
    }

    qt_geom_line_handle gh;
    if ( cpen.width() > 1 )
        qt_begin_geom_line( hps, COLOR_VALUE(cpen.color()), cpen.style() == NoPen, &gh );
    GpiMove( hps, (PPOINTL)(pa.data()+index) );
    GpiPolySpline( hps, 3, (PPOINTL)(pa.data()+index+1) );
    if ( cpen.width() > 1 )
        qt_end_geom_line( hps, &gh );
}
#endif // QT_NO_BEZIER

void QPainter::drawPixmap( int x, int y, const QPixmap &pixmap,
			   int sx, int sy, int sw, int sh )
{
    if ( !isActive() || pixmap.isNull() )
	return;
    if ( sw < 0 )
	sw = pixmap.width() - sx;
    if ( sh < 0 )
	sh = pixmap.height() - sy;

    // Sanity-check clipping
    if ( sx < 0 ) {
	x -= sx;
	sw += sx;
	sx = 0;
    }
    if ( sw + sx > pixmap.width() )
	sw = pixmap.width() - sx;
    if ( sy < 0 ) {
	y -= sy;
	sh += sy;
	sy = 0;
    }
    if ( sh + sy > pixmap.height() )
	sh = pixmap.height() - sy;

    if ( sw <= 0 || sh <= 0 )
	return;

    if ( testf(ExtDev|VxF|WxF) ) {
	if ( testf(ExtDev) || (txop == TxScale && pixmap.mask()) ||
	     txop == TxRotShear ) {
	    if ( sx != 0 || sy != 0 ||
		 sw != pixmap.width() || sh != pixmap.height() ) {
		QPixmap tmp( sw, sh, pixmap.depth(), QPixmap::NormalOptim );
		bitBlt( &tmp, 0, 0, &pixmap, sx, sy, sw, sh, CopyROP, TRUE );
		if ( pixmap.mask() ) {
		    QBitmap mask( sw, sh );
		    bitBlt( &mask, 0, 0, pixmap.mask(), sx, sy, sw, sh,
			    CopyROP, TRUE );
		    tmp.setMask( mask );
		}
		drawPixmap( x, y, tmp );
		return;
	    }
	    if ( testf(ExtDev) ) {
		QPDevCmdParam param[2];
		QRect r( x, y, pixmap.width(), pixmap.height() );
		param[0].rect  = &r;
		param[1].pixmap = &pixmap;
		if ( !pdev->cmd(QPaintDevice::PdcDrawPixmap,this,param)
		     || !hps )
		    return;
	    }
	}
	if ( txop == TxTranslate )
	    map( x, y, &x, &y );
    }

    if ( txop <= TxTranslate ) {		// use optimized bitBlt
	bitBlt( pdev, x, y, &pixmap, sx, sy, sw, sh, (RasterOp)rop );
	return;
    }

    QPixmap *pm	  = (QPixmap*)&pixmap;

/// @todo (dmik):
//
//      For some unknown reason, the code below doesn't work for some selected
//      target paint device width and scale factor combinations on my machine
//      with ATI Radeon 9600 Pro and SNAP Version 2.9.2, build 446 -- GpiBitBlt
//      simply does nothing (leaving the target hps untouched) but returns
//      GPI_OK. This looks like a (video driver?) bug...
//
//      An easy way to reproduce is to run the xform example, select the
//      Image mode and set the scale factor to 360% (making sure the window
//      size was not changed after starting the application).
//
#if 0
    if ( txop == TxScale && !pm->mask() ) {
	// Plain scaling and no mask, then GpiBitBlt is faster
	int w, h;
	map( x, y, sw, sh, &x, &y, &w, &h );
        // flip y coordinate
        if ( devh )
            y = devh - (y + h);
        POINTL ptls[] = {
            { x, y }, { x + w, y + h },
            { sx, sy }, { sw, sh }
        };
        GpiBitBlt( hps, pm->handle(), 4, ptls, ROP_SRCCOPY, BBO_IGNORE );
    } else
#endif
    {
#ifndef QT_NO_PIXMAP_TRANSFORMATION
	// We have a complex xform or scaling with mask, then xform the
	// pixmap (and possible mask) and bitBlt again.
	QWMatrix mat( m11(), m12(),
		      m21(), m22(),
		      dx(),  dy() );
	mat = QPixmap::trueMatrix( mat, sw, sh );
	QPixmap pmx;
	if ( sx == 0 && sy == 0 &&
	     sw == pixmap.width() && sh == pixmap.height() ) {
	    pmx = pixmap;			// xform the whole pixmap
	} else {
	    pmx = QPixmap( sw, sh );		// xform subpixmap
	    bitBlt( &pmx, 0, 0, pm, sx, sy, sw, sh );
	}
	pmx = pmx.xForm( mat );
	if ( pmx.isNull() )			// xformed into nothing
	    return;
	if ( !pmx.mask() && txop == TxRotShear ) {
	    QBitmap bm_clip( sw, sh, 1 );	// make full mask, xform it
	    bm_clip.fill( color1 );
	    pmx.setMask( bm_clip.xForm(mat) );
	}
	map( x, y, &x, &y );	//### already done above? // compute position of pixmap
	int dx, dy;
	mat.map( 0, 0, &dx, &dy );
	bitBlt( pdev, x - dx, y - dy, &pmx );
#endif
    }
}

/* Internal, used by drawTiledPixmap */

static void drawTile( QPainter *p, int x, int y, int w, int h,
		      const QPixmap &pixmap, int xOffset, int yOffset )
{
    int yPos, xPos, drawH, drawW, yOff, xOff;
    yPos = y;
    yOff = yOffset;
    while( yPos < y + h ) {
	drawH = pixmap.height() - yOff;    // Cropping first row
	if ( yPos + drawH > y + h )	   // Cropping last row
	    drawH = y + h - yPos;
	xPos = x;
	xOff = xOffset;
	while( xPos < x + w ) {
	    drawW = pixmap.width() - xOff; // Cropping first column
	    if ( xPos + drawW > x + w )	   // Cropping last column
		drawW = x + w - xPos;
	    p->drawPixmap( xPos, yPos, pixmap, xOff, yOff, drawW, drawH );
	    xPos += drawW;
	    xOff = 0;
	}
	yPos += drawH;
	yOff = 0;
    }
}

void QPainter::drawTiledPixmap( int x, int y, int w, int h,
				const QPixmap &pixmap, int sx, int sy )
{
    int sw = pixmap.width();
    int sh = pixmap.height();
    if (!sw || !sh )
	return;
    if ( sx < 0 )
	sx = sw - -sx % sw;
    else
	sx = sx % sw;
    if ( sy < 0 )
	sy = sh - -sy % sh;
    else
	sy = sy % sh;
    /*
      Requirements for optimizing tiled pixmaps:
       - not an external device
       - not scale or rotshear
       - no mask
    */
    QBitmap *mask = (QBitmap *)pixmap.mask();
    if ( !testf(ExtDev) && txop <= TxTranslate && mask == 0 ) {
	if ( txop == TxTranslate )
	    map( x, y, &x, &y );
	qt_draw_tiled_pixmap( hps, x, y, w, h, &pixmap, sx, sy, devh );
	return;
    }
    if ( sw*sh < 8192 && sw*sh < 16*w*h ) {
	int tw = sw, th = sh;
	while ( tw*th < 32678 && tw < w/2 )
	    tw *= 2;
	while ( tw*th < 32678 && th < h/2 )
	    th *= 2;
	QPixmap tile( tw, th, pixmap.depth(), QPixmap::BestOptim );
	qt_fill_tile( &tile, pixmap );
	if ( mask ) {
	    QBitmap tilemask( tw, th, FALSE, QPixmap::NormalOptim );
	    qt_fill_tile( &tilemask, *mask );
	    tile.setMask( tilemask );
	}
	drawTile( this, x, y, w, h, tile, sx, sy );
    } else {
	drawTile( this, x, y, w, h, pixmap, sx, sy );
    }
}

#if 0
//
// Generate a string that describes a transformed bitmap. This string is used
// to insert and find bitmaps in the global pixmap cache.
//

static QString gen_text_bitmap_key( const QWMatrix &m, const QFont &font,
				    const QString &str, int pos, int len )
{
    QString fk = font.key();
    int sz = 4*2 + len*2 + fk.length()*2 + sizeof(double)*6;
    QByteArray buf(sz);
    uchar *p = (uchar *)buf.data();
    *((double*)p)=m.m11();  p+=sizeof(double);
    *((double*)p)=m.m12();  p+=sizeof(double);
    *((double*)p)=m.m21();  p+=sizeof(double);
    *((double*)p)=m.m22();  p+=sizeof(double);
    *((double*)p)=m.dx();   p+=sizeof(double);
    *((double*)p)=m.dy();   p+=sizeof(double);
    QChar h1( '$' );
    QChar h2( 'q' );
    QChar h3( 't' );
    QChar h4( '$' );
    *((QChar*)p)=h1;  p+=2;
    *((QChar*)p)=h2;  p+=2;
    *((QChar*)p)=h3;  p+=2;
    *((QChar*)p)=h4;  p+=2;
    memcpy( (char*)p, (char*)(str.unicode()+pos), len*2 );  p += len*2;
    memcpy( (char*)p, (char*)fk.unicode(), fk.length()*2 ); p += fk.length()*2;
    return QString( (QChar*)buf.data(), buf.size()/2 );
}

static QBitmap *get_text_bitmap( const QString &key )
{
    return (QBitmap*)QPixmapCache::find( key );
}

static void ins_text_bitmap( const QString &key, QBitmap *bm )
{
    if ( !QPixmapCache::insert(key,bm) )	// cannot insert pixmap
	delete bm;
}
#endif

void QPainter::drawText( int x, int y, const QString &str, int len, QPainter::TextDirection dir )
{
    drawText( x, y, str, 0, len, dir );
}

void QPainter::drawText( int x, int y, const QString &str, int pos, int len, QPainter::TextDirection dir)
{
    if ( !isActive() )
	return;

    const int slen = str.length();
    if (len < 0)
        len = slen - pos;
    if ( len == 0 || pos >= slen ) // empty string
        return;
    if ( pos + len > slen )
        len = slen - pos;

    if ( testf(DirtyFont) )
	updateFont();

    if ( testf(ExtDev) ) {
	QPDevCmdParam param[3];
	QString string = str.mid( pos, len );
	QPoint p( x, y );
	param[0].point = &p;
	param[1].str = &string;
	param[2].ival = QFont::NoScript;
	if ( !pdev->cmd(QPaintDevice::PdcDrawText2,this,param) || !hps )
	    return;
    }

    // we can't take the complete string here as we would otherwise
    // get quadratic behaviour when drawing long strings in parts.
    // we do however need some chars around the part we paint to get arabic shaping correct.
    // ### maybe possible to remove after cursor restrictions work in QRT
    int start = QMAX( 0,  pos - 8 );
    int end = QMIN( (int)str.length(), pos + len + 8 );
    QConstString cstr( str.unicode() + start, end - start );
    pos -= start;

    QTextLayout layout( cstr.string(), this );
    layout.beginLayout( QTextLayout::SingleLine );

    layout.setBoundary( pos );
    layout.setBoundary( pos + len );

    QTextEngine *engine = layout.d;
    if ( dir != Auto ) {
	int level = (dir == RTL) ? 1 : 0;
	for ( int i = engine->items.size(); i >= 0; i-- )
	    engine->items[i].analysis.bidiLevel = level;
    }

    // small hack to force skipping of unneeded items
    start = 0;
    while ( engine->items[start].position < pos )
	++start;
    engine->currentItem = start;
    layout.beginLine( 0xfffffff );
    end = start;
    while ( !layout.atEnd() && layout.currentItem().from() < pos + len ) {
	layout.addCurrentItem();
	end++;
    }
    int ascent = fontMetrics().ascent();
    layout.endLine( 0, 0, Qt::SingleLine|Qt::AlignLeft, &ascent, 0 );

    // do _not_ call endLayout() here, as it would clean up the shaped items and we would do shaping another time
    // for painting.

    for ( int i = start; i < end; i++ ) {
	QScriptItem *si = &engine->items[i];

	QFontEngine *fe = si->fontEngine;
	Q_ASSERT( fe );

	int xpos = x + si->x;
	int ypos = y + si->y - ascent;

/// @todo (dmik) do we really need this? we already have painter->handle()
//  inside QFontEngine::draw(), and the font should have been selected into
//  hps by updateFont()
//        HPS oldPs = fe->hps;
//        fe->hps = hps;
//        fe->selectTo( hps );

	int textFlags = 0;
	if ( cfont.d->underline ) textFlags |= Qt::Underline;
	if ( cfont.d->overline ) textFlags |= Qt::Overline;
	if ( cfont.d->strikeOut ) textFlags |= Qt::StrikeOut;

	fe->draw( this, xpos, ypos, engine, si, textFlags );
/// @todo (dmik) need?
//        fe->hps = oldPs;
    }
}



void QPainter::drawTextItem( int x,  int y, const QTextItem &ti, int textFlags )
{
    if ( testf(ExtDev) ) {
	QPDevCmdParam param[2];
	QPoint p(x, y);
	param[0].point = &p;
	param[1].textItem = &ti;
	bool retval = pdev->cmd(QPaintDevice::PdcDrawTextItem, this, param);
	if ( !retval || !hps )
	    return;
    }

    QTextEngine *engine = ti.engine;
    QScriptItem *si = &engine->items[ti.item];

    engine->shape( ti.item );
    QFontEngine *fe = si->fontEngine;
    Q_ASSERT( fe );

    x += si->x;
    y += si->y;

/// @todo (dmik) do we really need this? we already have painter->handle()
//  inside QFontEngine::draw(), and the font should have been selected into
//  hps by updateFont()
//    HPS oldPs = fe->hps;
//    fe->hps = hps;
//    fe->selectTo( hps );

    fe->draw( this, x,  y, engine, si, textFlags );
/// @todo (dmik) need?
//    fe->hps = oldPs;
}


QPoint QPainter::pos() const
{
    QPoint p;
    if ( !isActive() || !hps )
	return p;
    POINTL ptl;
    GpiQueryCurrentPosition( hps, &ptl );
    if ( devh ) ptl.y = devh - ( ptl.y + 1 );
    p.rx() = ptl.x;
    p.ry() = ptl.y;
    return  xFormDev( p );
}

