/****************************************************************************
** $Id: qcolor_pm.cpp 61 2006-02-06 21:43:39Z dmik $
**
** Implementation of QColor class for OS/2
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

#include "qcolor.h"
#include <private/qcolor_p.h>
#include "qapplication.h"
#include "qt_os2.h"

//@@TODO (dmik): Innotek GCC 3.2.2 beta 4 CSD1 lacks these functions in
//  its version of the toolkit header. This should be fixed soon.
#if defined(Q_CC_GNU) && !defined(USE_OS2_TOOLKIT_HEADERS)
extern "C" ULONG APIENTRY GpiResizePalette(HPAL hpal, ULONG newsize);
extern "C" LONG  APIENTRY GpiQueryNearestPaletteIndex(HPAL hpal, ULONG color);
#endif

/*****************************************************************************
  QColor static member functions
 *****************************************************************************/

HPAL QColor::hpal = 0;                          // application global palette

static QRgb* colArray = 0;			// allocated pixel value
static int* ctxArray = 0;			// allocation context
static int numPalEntries = 0;

static int current_alloc_context = 0;

inline ULONG qrgb2rgb2( QRgb rgb )
{
// QRgb has the same RGB format (0xaarrggbb) as OS/2 uses (ignoring the
// highest alpha byte) so we just mask alpha to get the valid OS/2 color.
// The line below is intentionally commented out.
//    return (ULONG) (qRed(rgb) << 16 | qGreen(rgb) << 8 | qBlue(rgb));
    return (ULONG) (rgb & RGB_MASK);
}

int QColor::maxColors()
{
    static int maxcols = 0;
    if ( maxcols == 0 ) {
	HDC hdc = GpiQueryDevice( qt_display_ps() );
        LONG addcaps = 0, colors = 0;
        DevQueryCaps( hdc, CAPS_ADDITIONAL_GRAPHICS, 1, &addcaps );
        DevQueryCaps( hdc, CAPS_COLORS, 1, &colors );

        // We use palettes only when the number of colors is 256 (8 bits).
        // For numbers of bits per pixel greater than 8 OS/2 GPI knows only 24-
        // and 32-bit color modes which are RGB-based. For numbers of BPP less
        // than 8, it seems better to use RGB mode also (leaving dithering job
        // for GPI).
        if ( ( addcaps & CAPS_PALETTE_MANAGER ) == 0 || colors != 256)
            maxcols = -1;
        else
            maxcols = colors;
    }
    return maxcols;
}

int QColor::numBitPlanes()
{
    HDC hdc = GpiQueryDevice( qt_display_ps() );
    LONG colorInfo [2];
    DevQueryCaps( hdc, CAPS_COLOR_PLANES, 2, colorInfo );
    return colorInfo [0] * colorInfo [1];
}


void QColor::initialize()
{
    if ( color_init )
	return;

    color_init = TRUE;
    if ( QApplication::colorSpec() == QApplication::NormalColor )
	return;

    int numCols = maxColors();
    if ( numCols <= 16 || numCols > 256 )	// no need to create palette
	return;

    colormodel = d8;

    PRGB2 pal = 0;
    if ( QApplication::colorSpec() == QApplication::ManyColor ) {
	numPalEntries = 6*6*6; // cube
        pal = new RGB2 [numPalEntries];

        // Make 6x6x6 color cube
	int idx = 0;
	for( int ir = 0x0; ir <= 0xff; ir+=0x33 ) {
	    for( int ig = 0x0; ig <= 0xff; ig+=0x33 ) {
		for( int ib = 0x0; ib <= 0xff; ib+=0x33 ) {
		    pal[idx].bRed = ir;
		    pal[idx].bGreen = ig;
		    pal[idx].bBlue = ib;
		    pal[idx].fcOptions = 0;
		    idx++;
		}
	    }
	}
    } else {
	numPalEntries = 2; // black&white + allocate in alloc()
        pal = new RGB2 [numPalEntries];

	pal[0].bRed = 0;
	pal[0].bGreen = 0;
	pal[0].bBlue = 0;
	pal[0].fcOptions = 0;

	pal[1].bRed = 255;
	pal[1].bGreen = 255;
	pal[1].bBlue = 255;
	pal[1].fcOptions = 0;
    }

    // Store palette in our own array
    colArray = new QRgb[numCols];	// Maximum palette size
    ctxArray = new int[numCols];        // Maximum palette size

    for( int i = 0; i < numCols; i++ ) {
	if ( i < numPalEntries ) {
	    colArray[i] = qRgb( pal[i].bRed,
				pal[i].bGreen,
				pal[i].bBlue ) & RGB_MASK;
	    ctxArray[i] = current_alloc_context;
	} else {
	    colArray[i] = 0;
	    ctxArray[i] = -1;
	}
    }

    hpal = GpiCreatePalette( 0, 0, LCOLF_CONSECRGB, numPalEntries, (PULONG) pal );
#if defined(QT_CHECK_STATE)
    if ( !hpal )
	qSystemWarning( "QColor: Failed to create logical palette!" );
#endif
    delete[] pal;

    // make sure "black" and "white" are initialized
    initGlobalColors();

    ((QColor*)(&Qt::black))->alloc();
    ((QColor*)(&Qt::white))->alloc();

//@@TODO (dmik): is it necessary to call GpiSelectPalette() / WinRealizePalette()
//  on the display PS / desktop HWND here to force colors to change immediately?
}

/*! \internal */
const QRgb* QColor::palette( int* numEntries )
{
    if ( numEntries )
	*numEntries = numPalEntries;
    if ( !hpal )
	return 0;
    return colArray;
}

/*! \internal */
int QColor::setPaletteEntries( const QRgb* pal, int numEntries, int base )
{
    if ( !hpal || !pal || numEntries < 1 )
	return -1;
    if ( base < 0 )
	base = 0;
    int maxSize = maxColors();
    if ( base >= maxSize )
	return -1;
    if ( base + numEntries > maxSize )
	numEntries = maxSize - base;
    int newSize = base + numEntries;
    if ( newSize > numPalEntries )
	GpiResizePalette( hpal, newSize );
    PRGB2 newEntries = new RGB2 [numEntries];
    for ( int i = 0; i < numEntries; i++ ) {
	newEntries[i].bRed = qRed( pal[i] );
	newEntries[i].bGreen = qGreen( pal[i] );
	newEntries[i].bBlue = qBlue( pal[i] );
	newEntries[i].fcOptions = 0;
    }
    GpiSetPaletteEntries( hpal, LCOLF_CONSECRGB, base, numEntries, (PULONG) newEntries );
    delete[] newEntries;
    newEntries = 0;

    numPalEntries = newSize;
    if ( QApplication::colorSpec() == QApplication::CustomColor ) {
	for ( int j = base; j < base+numEntries; j++ ) {
	    colArray[j] = pal[j-base];
	    ctxArray[j] = current_alloc_context;
	}
    }

//@@TODO (dmik): is it necessary to call GpiSelectPalette() / WinRealizePalette()
//  on the display PS / desktop HWND here to force colors to change immediately?
    return base;
}

void QColor::cleanup()
{
    if ( hpal ) {				// delete application global
	GpiDeletePalette( hpal );		// palette
	hpal = 0;
    }
    if ( colArray ) {
	delete colArray;
	colArray = 0;
    }
    if ( ctxArray ) {
	delete ctxArray;
	ctxArray = 0;
    }
    color_init = FALSE;
}

/*! \internal */
/// @todo (dmik) this seems not to be used anywhere, remove?
//uint QColor::realizePal( QWidget *widget )
//{
//    if ( !hpal )				// not using palette
//	return 0;
//    HPS hps = WinGetPS( widget->winId() );
//    HPAL hpalT = GpiSelectPalette( hps, hpal );
//    ULONG physColors = 0;
//    LONG changed = WinRealizePalette( widget->winId(), hps, &physColors );
//    SelectPalette( hps, hpalT );
//    WinReleasePS( hps );
//    return (uint) changed;
//}

/*! \fn HPALETTE QColor::hPal()
  \internal
*/

/*****************************************************************************
  QColor member functions
 *****************************************************************************/

uint QColor::alloc()
{
    if ( !color_init ) {
	return d.d32.pix = 0;
    } else {
	uint pix = qrgb2rgb2( d.argb );
	if ( hpal ) {
	    ULONG idx = GpiQueryNearestPaletteIndex( hpal, pix );
	    if ( QApplication::colorSpec() == QApplication::CustomColor ) {
		RGB2 fe;
                GpiQueryPaletteInfo( hpal, 0, 0, idx, 1, (PULONG) &fe );
		QRgb fc = qRgb( fe.bRed, fe.bGreen, fe.bBlue );
		if ( fc != d.argb ) {	// Color not found in palette
		    // Find a free palette entry
		    bool found = FALSE;
		    for ( int i = 0; i < numPalEntries; i++ ) {
			if ( ctxArray[i] < 0 ) {
			    found = TRUE;
			    idx = i;
			    break;
			}
		    }
		    if ( !found && numPalEntries < maxColors() ) {
			idx = numPalEntries;
			numPalEntries++;
			GpiResizePalette( hpal, numPalEntries );
			found = TRUE;
		    }
		    if ( found ) {
			// Change unused palette entry into the new color
			RGB2 ne;
			ne.bRed = qRed( d.argb );
			ne.bGreen = qGreen( d.argb );
			ne.bBlue = qBlue( d.argb );
			ne.fcOptions = 0;
			GpiSetPaletteEntries( hpal, LCOLF_CONSECRGB, idx, 1, (PULONG) &ne );
			colArray[idx] = d.argb;
			ctxArray[idx] = current_alloc_context;
//@@TODO (dmik): is it necessary to call GpiSelectPalette() / WinRealizePalette()
//  on the display PS / desktop HWND here to force colors to change immediately?
//  As a variant we can broadcast WM_REALIZEPALETTE here which should then be
//  handled in the main event loop, causing all top-level widgets to select
//  the new palette, realize it and invalidate themselves.
		    }
		}
		if ( (int)idx < numPalEntries ) { 	// Sanity check
		    if ( ctxArray[idx] < 0 )
			ctxArray[idx] = current_alloc_context; // mark it
		    else if ( ctxArray[idx] != current_alloc_context )
			ctxArray[idx] = 0;	// Set it to default ctx
		}
	    }
	    d.d8.pix = idx;
	    d.d8.dirty = FALSE;
	    return idx;
	} else {
	    return d.d32.pix = pix;
	}
    }
}

void QColor::setSystemNamedColor( const QString& name )
{
    // setSystemNamedColor should look up rgb values from the built in
    // color tables first (see qcolor_p.cpp), and failing that, use
    // the window system's interface for translating names to rgb values...
    // we do this so that things like uic can load an XPM file with named colors
    // and convert it to a png without having to use window system functions...
    d.argb = qt_get_rgb_val( name.latin1() );
    QRgb rgb;
    if ( qt_get_named_rgb( name.latin1(), &rgb ) ) {
	setRgb( qRed(rgb), qGreen(rgb), qBlue(rgb) );
	if ( colormodel == d8 ) {
	    d.d8.invalid = FALSE;
	    d.d8.dirty = TRUE;
	    d.d8.pix = 0;
	} else {
	    alloc();
	}
    } else {
	// set to invalid color
	*this = QColor();
    }
}


#define MAX_CONTEXTS 16
static int  context_stack[MAX_CONTEXTS];
static int  context_ptr = 0;

static void init_context_stack()
{
    static bool did_init = FALSE;
    if ( !did_init ) {
	did_init = TRUE;
	context_stack[0] = current_alloc_context = 0;
    }
}


int QColor::enterAllocContext()
{
    static int context_seq_no = 0;
    init_context_stack();
    if ( context_ptr+1 == MAX_CONTEXTS ) {
#if defined(QT_CHECK_STATE)
	qWarning( "QColor::enterAllocContext: Context stack overflow" );
#endif
	return 0;
    }
    current_alloc_context = context_stack[++context_ptr] = ++context_seq_no;
    return current_alloc_context;
}


void QColor::leaveAllocContext()
{
    init_context_stack();
    if ( context_ptr == 0 ) {
#if defined(QT_CHECK_STATE)
	qWarning( "QColor::leaveAllocContext: Context stack underflow" );
#endif
	return;
    }

    current_alloc_context = context_stack[--context_ptr];
}


int QColor::currentAllocContext()
{
    return current_alloc_context;
}


void QColor::destroyAllocContext( int context )
{
    if ( !hpal || QApplication::colorSpec() != QApplication::CustomColor )
	return;

    init_context_stack();

    for ( int i = 2; i < numPalEntries; i++ ) {	  // 2: keep black & white
	switch ( context ) {
	case -2:
	    if ( ctxArray[i] > 0 )
		ctxArray[i] = -1;
	    break;
	case -1:
	    ctxArray[i] = -1;
	    break;
	default:
	    if ( ctxArray[i] == context )
		ctxArray[i] = -1;
	break;
	}
    }

    //# Should reset unused entries in hpal to 0, to minimize the app's demand

}
