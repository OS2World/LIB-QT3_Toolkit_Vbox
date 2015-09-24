/****************************************************************************
** $Id: qpixmap_pm.cpp 124 2006-09-12 21:25:42Z dmik $
**
** Implementation of QPixmap class for OS/2
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

#include "qbitmap.h"
#include "qimage.h"
#include "qpaintdevicemetrics.h"
#include "qwmatrix.h"
#include "qapplication.h"
#include "qapplication_p.h"
#include "qt_os2.h"

extern const uchar *qt_get_bitflip_array();		// defined in qimage.cpp

static inline HPS alloc_mem_ps( int w, int h, HPS compat = 0 )
{
    HDC hdcCompat = 0;
    if ( compat )
        hdcCompat = GpiQueryDevice( compat );

    static const PSZ hdcData[4] = { "Display", NULL, NULL, NULL };
    HDC hdc = DevOpenDC( 0, OD_MEMORY, "*", 4, (PDEVOPENDATA) hdcData, hdcCompat );
    if ( !hdc ) {
#if defined(QT_CHECK_NULL)
	qSystemWarning( "alloc_mem_dc: DevOpenDC failed!" );
#endif
	return 0;
    }
    SIZEL size = { w, h };
    HPS hps = GpiCreatePS( 0, hdc, &size, PU_PELS | GPIA_ASSOC | GPIT_MICRO );
    if ( !hps ) {
#if defined(QT_CHECK_NULL)
	qSystemWarning( "alloc_mem_dc: GpiCreatePS failed!" );
#endif
	return 0;
    }
    if ( QColor::hPal() ) {
	GpiSelectPalette( hps, QColor::hPal() );
    } else {
        // direct RGB mode
        GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );
    }
    return hps;
}

static inline void free_mem_ps( HPS hps )
{
    HDC hdc = GpiQueryDevice( hps );
    GpiAssociate( hps, 0 );
    GpiDestroyPS( hps );
    DevCloseDC( hdc );
}

extern HPS qt_alloc_mem_ps( int w, int h, HPS compat = 0 )
{
    return alloc_mem_ps( w, h, compat );
}

extern void qt_free_mem_ps( HPS hps )
{
    free_mem_ps( hps );
}


void QPixmap::init( int w, int h, int d, bool bitmap, Optimization optim )
{
#if defined(QT_CHECK_STATE)
    if ( qApp->type() == QApplication::Tty ) {
	qWarning( "QPixmap: Cannot create a QPixmap when no GUI "
		  "is being used" );
    }
#endif

    static int serial = 0;
    const int dd = defaultDepth();

    if ( optim == DefaultOptim )		// use default optimization
	optim = defOptim;

    data = new QPixmapData;
    Q_CHECK_PTR( data );

    memset( data, 0, sizeof(QPixmapData) );
    data->count  = 1;
    data->uninit = TRUE;
    data->bitmap = bitmap;
    data->ser_no = ++serial;
    data->optim	 = optim;

    bool make_null = w == 0 || h == 0;		// create null pixmap
    if ( d == 1 )				// monocrome pixmap
	data->d = 1;
    else if ( d < 0 || d == dd )		// compatible pixmap
	data->d = dd;
    if ( make_null || w < 0 || h < 0 || data->d == 0 ) {
	hps = 0;
	data->hbm = 0;
#if defined(QT_CHECK_RANGE)
	if ( !make_null )			// invalid parameters
	    qWarning( "QPixmap: Invalid pixmap parameters" );
#endif
	return;
    }
    data->w = w;
    data->h = h;

    hps = alloc_mem_ps( w, h );
    BITMAPINFOHEADER2 bmh;
    memset( &bmh, 0, sizeof(BITMAPINFOHEADER2) );
    bmh.cbFix = sizeof(BITMAPINFOHEADER2);
    bmh.cx = w;
    bmh.cy = h;
    bmh.cPlanes = 1;
    if ( data->d == 1 )                         // monocrome bitmap
        bmh.cBitCount = 1;
    else                                        // compatible bitmap
        bmh.cBitCount = dd;
    data->hbm = GpiCreateBitmap( hps, &bmh, 0, NULL, NULL );

    if ( !data->hbm ) {
	data->w = 0;
	data->h = 0;
        free_mem_ps( hps );
	hps = 0;
#if defined(QT_CHECK_NULL)
	qSystemWarning( "QPixmap: Pixmap allocation failed" );
#endif
	return;
    }

    GpiSetBitmap( hps, data->hbm );
}


void QPixmap::deref()
{
    if ( data && data->deref() ) {		// last reference lost
        if ( hps )
            GpiSetBitmap( hps, 0 );
        if ( data->hasRealAlpha && data->realAlphaBits ) {
            delete[] data->realAlphaBits;
            data->realAlphaBits = 0;
        }
	if ( data->mask ) {
	    delete data->mask;
            if ( data->maskedHbm ) {
                GpiDeleteBitmap( data->maskedHbm );
                data->maskedHbm = 0;
            }
        }
	if ( data->hbm ) {
            GpiDeleteBitmap( data->hbm );
            data->hbm = 0;
	}
	if ( hps ) {
	    free_mem_ps( hps );
	    hps = 0;
	}
	delete data;
        data = 0;
    }
}


QPixmap::QPixmap( int w, int h, const uchar *bits, bool isXbitmap )
    : QPaintDevice( QInternal::Pixmap )
{						// for bitmaps only
    init( 0, 0, 0, FALSE, NormalOptim );
    if ( w <= 0 || h <= 0 )			// create null pixmap
	return;

    data->uninit = FALSE;
    data->w = w;
    data->h = h;
    data->d = 1;

    int bitsbpl = (w + 7) / 8;			// original # bytes per line

    int bpl = ((w + 31) / 32) * 4;              // bytes per scanline,
                                                // doubleword aligned

    uchar *newbits = new uchar[bpl*h];
    uchar *p	= newbits + bpl*(h - 1);        // last scanline
    int x, y, pad;
    pad = bpl - bitsbpl;
    const uchar *f = isXbitmap ? qt_get_bitflip_array() : 0;

    // flip bit order if Xbitmap and flip bitmap top to bottom
    for ( y=0; y<h; y++ ) {
        if ( f ) {
            for ( x=0; x<bitsbpl; x++ )
                *p++ = f[*bits++];
            for ( x=0; x<pad; x++ )
                *p++ = 0;
            p -= bpl;
        } else {
            memcpy( p, bits, bitsbpl );
            memset( p + bitsbpl, 0, pad );
            bits += bitsbpl;
        }
        p -= bpl;
    }

    hps = alloc_mem_ps( w, h );
    // allocate header + 2 palette entries
    char *bmi_data = new char[ sizeof(BITMAPINFOHEADER2) + 4 * 2 ];
    memset( bmi_data, 0, sizeof(BITMAPINFOHEADER2) );
    BITMAPINFOHEADER2 &bmh = *(PBITMAPINFOHEADER2) bmi_data;
    PULONG pal = (PULONG) (bmi_data + sizeof(BITMAPINFOHEADER2));
    bmh.cbFix = sizeof(BITMAPINFOHEADER2);
    bmh.cx = w;
    bmh.cy = h;
    bmh.cPlanes = 1;
    bmh.cBitCount = 1;
    bmh.cclrUsed = 2;
    pal[0] = 0;
    pal[1] = 0x00FFFFFF;
    data->hbm = GpiCreateBitmap(
        hps, &bmh, CBM_INIT, (PBYTE) newbits, (PBITMAPINFO2) bmi_data
    );

    GpiSetBitmap( hps, data->hbm );

    delete [] bmi_data;
    delete [] newbits;
    if ( defOptim != NormalOptim )
	setOptimization( defOptim );
}


void QPixmap::detach()
{
    if ( data->uninit || data->count == 1 )
	data->uninit = FALSE;
    else
	*this = copy();
}


int QPixmap::defaultDepth()
{
    static int dd = 0;
    if ( dd == 0 ) {
        LONG formats[ 2 ];
        GpiQueryDeviceBitmapFormats( qt_display_ps(), 2, formats );
        dd = formats[ 0 ] * formats[ 1 ];
    }
    return dd;
}


void QPixmap::setOptimization( Optimization optimization )
{
    if ( optimization == data->optim )
	return;
    detach();
    data->optim = optimization == DefaultOptim ?
	    defOptim : optimization;
}


void QPixmap::fill( const QColor &fillColor )
{
    if ( isNull() )
	return;
    detach();					// detach other references

    const ULONG AreaAttrs = ABB_COLOR | ABB_MIX_MODE | ABB_SET | ABB_SYMBOL;
    AREABUNDLE oldAb;
    GpiQueryAttrs( hps, PRIM_AREA, AreaAttrs, (PBUNDLE) &oldAb );
    AREABUNDLE newAb;
    newAb.lColor = fillColor.pixel();
    newAb.usMixMode = FM_OVERPAINT;
    newAb.usSet = LCID_DEFAULT;
    newAb.usSymbol = PATSYM_SOLID;
    GpiSetAttrs( hps, PRIM_AREA, AreaAttrs, 0, (PBUNDLE) &newAb );
    POINTL ptl = { 0, 0 };
    GpiMove( hps, &ptl );
    ptl.x = data->w - 1;
    ptl.y = data->h - 1;
    GpiBox( hps, DRO_FILL, &ptl, 0, 0 );
    GpiSetAttrs( hps, PRIM_AREA, AreaAttrs, 0, (PBUNDLE) &newAb );
}


int QPixmap::metric( int m ) const
{
    if ( m == QPaintDeviceMetrics::PdmWidth ) {
	return width();
    } else if ( m == QPaintDeviceMetrics::PdmHeight ) {
	return height();
    } else {
        LONG val;
        HDC hdc = GpiQueryDevice( hps );
	switch ( m ) {
	    case QPaintDeviceMetrics::PdmDpiX:
                DevQueryCaps( hdc, CAPS_HORIZONTAL_FONT_RES, 1, &val );
		break;
	    case QPaintDeviceMetrics::PdmDpiY:
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
                if (depth() == 1) {
                    // it's a monochrome bitmap
                    val = 1;
                } else {
                    DevQueryCaps( hdc, CAPS_COLORS, 1, &val );
                }
		break;
	    case QPaintDeviceMetrics::PdmDepth:
                val = depth();
		break;
	    default:
		val = 0;
#if defined(QT_CHECK_RANGE)
		qWarning( "QPixmap::metric: Invalid metric command" );
#endif
	}
	return val;
    }
}

QImage QPixmap::convertToImage() const
{
    if ( isNull() )
	return QImage(); // null image

    int	w = width();
    int	h = height();
    const QBitmap *m = data->hasRealAlpha ? 0 : mask();
    int	d = depth();
    int	ncols = 2;

    if ( d > 1 && d <= 8 ) {	// set to nearest valid depth
	d = 8;					//   2..7 ==> 8
	ncols = 256;
    } else if ( d > 8 ) {
	d = 32;					//   > 8  ==> 32
	ncols = 0;
        // Note: even if trueColorDepth() != 32 here, we don't return hoping
        // that the video driver can at least convert from/to 32-bpp bitmaps
        // (which is the case for SNAP that doesn't report 32-bpp as the
        // supported depth when a lower color depth is specified in Screen
        // settings, but works well when converting).
    }

    QImage image( w, h, d, ncols, QImage::BigEndian );

    // allocate header + ncols palette entries
    char *bmi_data = new char[ sizeof(BITMAPINFOHEADER2) + 4 * ncols ];
    memset( bmi_data, 0, sizeof(BITMAPINFOHEADER2) );
    BITMAPINFOHEADER2 &bmh = *(PBITMAPINFOHEADER2) bmi_data;
    PULONG pal = (PULONG) (bmi_data + sizeof(BITMAPINFOHEADER2));
    bmh.cbFix = sizeof(BITMAPINFOHEADER2);
    bmh.cx = w;
    bmh.cy = h;
    bmh.cPlanes = 1;
    bmh.cBitCount = d;
    bmh.cclrUsed = ncols;
    GpiQueryBitmapBits( hps, 0, h, (PBYTE) image.bits(), (PBITMAPINFO2) bmi_data );

    if ( data->hasRealAlpha ) {
        if ( data->realAlphaBits ) {
            // incorporate the alpha channel to image data
            // (see comments in convertFromImage() for details)
            uchar *src = data->realAlphaBits;
            int y = 0;
            while ( y < image.height() ) {
                uchar *dst = image.scanLine( y++ );
                uchar *end = dst + image.bytesPerLine();
                dst += 3;
                while ( dst < end ) {
                    *dst = *(src++);
                    dst += 4;
                }
            }
        }
	image.setAlphaBuffer( TRUE );
    }    
    
    // flip the image top to bottom
    {
        int bpl = image.bytesPerLine();
        uchar *line = new uchar[bpl];
        uchar *top = image.scanLine( 0 );
        uchar *bottom = image.scanLine( h - 1 );
        while( top < bottom ) {
            memcpy( line, top, bpl );
            memcpy( top, bottom, bpl );
            memcpy( bottom, line, bpl );
            top += bpl;
            bottom -= bpl;
        }
        delete [] line;
    }

    if ( d == 1 ) {
        // use a white/black palette for monochrome pixmaps (istead of
        // black/white supplied by GPI) to make the result of possible
        // converson to a higher depth compatible with other platforms.
        if ( m ) {
            image.setAlphaBuffer( TRUE );
            image.setColor( 0, qRgba( 255, 255, 255, 0 ) );
            image.setColor( 1, qRgba( 0, 0, 0, 255 ) );
        } else {
            image.setColor( 0, qRgb( 255, 255, 255 ) );
            image.setColor( 1, qRgb( 0, 0, 0 ) );
        }
    } else {
        for ( int i=0; i<ncols; i++ ) {		// copy color table
            PRGB2 r = (PRGB2) &pal[i];
            if ( m )
                image.setColor( i, qRgba(r->bRed,
                                   r->bGreen,
                                   r->bBlue,255) );
            else
                image.setColor( i, qRgb(r->bRed,
                                   r->bGreen,
                                   r->bBlue) );
        }
    }

    delete [] bmi_data;

    if ( d != 1 && m ) {
	image.setAlphaBuffer(TRUE);
	QImage msk = m->convertToImage();

	switch ( d ) {
	  case 8: {
	    int used[256];
	    memset( used, 0, sizeof(int)*256 );
	    uchar* p = image.bits();
	    int l = image.numBytes();
	    while (l--) {
		used[*p++]++;
	    }
	    int trans=0;
	    int bestn=INT_MAX;
	    for ( int i=0; i<256; i++ ) {
		if ( used[i] < bestn ) {
		    bestn = used[i];
		    trans = i;
		    if ( !bestn )
			break;
		}
	    }
	    image.setColor( trans, image.color(trans)&0x00ffffff );
	    for ( int y=0; y<image.height(); y++ ) {
		uchar* mb = msk.scanLine(y);
		uchar* ib = image.scanLine(y);
		uchar bit = 0x80;
		int i=image.width();
		while (i--) {
		    if ( !(*mb & bit) )
			*ib = trans;
		    bit /= 2; if ( !bit ) mb++,bit = 0x80; // ROL
		    ib++;
		}
	    }
	  } break;
	  case 32: {
	    for ( int y=0; y<image.height(); y++ ) {
		uchar* mb = msk.scanLine(y);
		QRgb* ib = (QRgb*)image.scanLine(y);
		uchar bit = 0x80;
		int i=image.width();
		while (i--) {
		    if ( *mb & bit )
			*ib |= 0xff000000;
		    else
			*ib &= 0x00ffffff;
		    bit /= 2; if ( !bit ) mb++,bit = 0x80; // ROL
		    ib++;
		}
	    }
	  } break;
	}
    }

    return image;
}

bool QPixmap::convertFromImage( const QImage &img, int conversion_flags )
{
    if ( img.isNull() ) {
#if defined(QT_CHECK_NULL)
	qWarning( "QPixmap::convertFromImage: Cannot convert a null image" );
#endif
	return FALSE;
    }

    QImage image = img;
    int	   d     = image.depth();
    int    dd    = defaultDepth();
    bool force_mono = (dd == 1 || isQBitmap() ||
		       (conversion_flags & ColorMode_Mask)==MonoOnly );

    if ( force_mono ) {				// must be monochrome
	if ( d != 1 ) {				// dither
	    image = image.convertDepth( 1, conversion_flags );
	    d = 1;
	}
    } else {					// can be both
	bool conv8 = FALSE;
	if ( d > 8 && dd <= 8 ) {		// convert to 8 bit
	    if ( (conversion_flags & DitherMode_Mask) == AutoDither )
		conversion_flags = (conversion_flags & ~DitherMode_Mask)
					| PreferDither;
	    conv8 = TRUE;
	} else if ( (conversion_flags & ColorMode_Mask) == ColorOnly ) {
	    conv8 = d == 1;			// native depth wanted
	} else if ( d == 1 ) {
	    if ( image.numColors() == 2 ) {
                // Note: 1-bpp images other than true black-white cannot be
                // correctly stored as GPI bitmaps (it doesn't store the palette
                // for them), so the code below ensures they will be converted
                // to 8-bpp images first. However, black-white images with alpha
                // channel will be also converted, though they can be stored
                // as bitmaps. The correct code to prevent setting conv8 to TRUE
                // for black-white bitmaps both with and w/o alpha should be:
                //      QRgb c0 = image.color(0) | ~RGB_MASK;
                //      QRgb c1 = image.color(1) | ~RGB_MASK;
                // but it is left as is solely for compatibility with Qt/Win32
                // (otherwise we would get visually different pixmaps).
		QRgb c0 = image.color(0);	// Auto: convert to best
		QRgb c1 = image.color(1);
		conv8 = QMIN(c0,c1) != qRgb(0,0,0) || QMAX(c0,c1) != qRgb(255,255,255);
	    } else {
		// eg. 1-color monochrome images (they do exist).
		conv8 = TRUE;
	    }
	}
	if ( conv8 ) {
	    image = image.convertDepth( 8, conversion_flags );
	    d = 8;
	}
    }

    if ( d == 1 ) {				// 1 bit pixmap (bitmap)
	image = image.convertBitOrder( QImage::BigEndian );
    }

    bool hasRealAlpha = FALSE;
    if ( img.hasAlphaBuffer() ) {
        if (image.depth() == 8) {
	    const QRgb * const rgb = img.colorTable();
	    for (int i = 0, count = img.numColors(); i < count; ++i) {
		const int alpha = qAlpha(rgb[i]);
		if (alpha != 0 && alpha != 0xff) {
		    hasRealAlpha = true;
		    break;
		}
	    }
	    if (hasRealAlpha) {
		image = image.convertDepth(32, conversion_flags);
		d = image.depth();
	    }
	} else if (image.depth() == 32) {
	    int i = 0;
	    while ( i<image.height() && !hasRealAlpha ) {
		uchar *p = image.scanLine(i);
		uchar *end = p + image.bytesPerLine();
		p += 3;
		while ( p < end ) {
		    if ( *p!=0 && *p!=0xff ) {
			hasRealAlpha = TRUE;
			break;
		    }
		    p += 4;
		}
		++i;
	    }
	}
    }

    int w = image.width();
    int h = image.height();

    if ( width() == w && height() == h &&
         ( (d == 1 && depth() == 1) ||
           (d != 1 && depth() != 1 && hasRealAlpha == hasAlphaChannel()) ) ) {
        // same size etc., use the existing pixmap
        detach();
        if ( data->mask ) {			// get rid of the mask
            delete data->mask;
            data->mask = 0;
            if ( data->maskedHbm )
                prepareForMasking( FALSE, TRUE );
        }
    } else {
	// different size or depth, make a new pixmap
	QPixmap pm( w, h, d == 1 ? 1 : -1, data->bitmap, data->optim );
	*this = pm;
    }

    // Note: even if trueColorDepth() != 32 here, we don't return hoping
    // that the video driver can at least convert from/to 32-bpp bitmaps
    // (which is the case for SNAP that doesn't report 32-bpp as the
    // supported depth when a lower color depth is specified in Screen
    // settings, but works well when converting).

    int	  ncols	   = image.numColors();

    // allocate header + ncols palette entries
    char *bmi_data = new char[ sizeof(BITMAPINFOHEADER2) + 4 * ncols ];
    memset( bmi_data, 0, sizeof(BITMAPINFOHEADER2) );
    BITMAPINFOHEADER2 &bmh = *(PBITMAPINFOHEADER2) bmi_data;
    PULONG pal = (PULONG) (bmi_data + sizeof(BITMAPINFOHEADER2));
    bmh.cbFix = sizeof(BITMAPINFOHEADER2);
    bmh.cx = w;
    bmh.cy = h;
    bmh.cPlanes = 1;
    bmh.cBitCount = d;
    bmh.cclrUsed = ncols;
    bool doAlloc = ( QApplication::colorSpec() == QApplication::CustomColor
		     && QColor::hPal() );

    if ( d == 1 ) {
        // always use the black/white palette for monochrome pixmaps;
        // otherwise GPI can swap 0s and 1s when creating a bitmap.
        pal [0] = 0;
        pal [1] = 0x00FFFFFF;
    } else {
        for ( int i=0; i<ncols; i++ ) {		// copy color table
            PRGB2 r = (PRGB2) &pal[i];
            QRgb c = image.color(i);
            r->bBlue = qBlue( c );
            r->bGreen = qGreen( c );
            r->bRed = qRed( c );
            r->fcOptions = 0;
            if ( doAlloc ) {
                QColor cl( c );
                cl.alloc();
            }
        }
    }

    data->hasRealAlpha = hasRealAlpha;

    // If we have real alpha bits and the default video driver depth is 32-bpp,
    // we store the alpha channel in the unused high byte of the 32-bit pixel
    // value to save memory and slightly optimize the performance (QImage already
    // contains the alpha channel there, so nothing has to be done when setting
    // bitmap bits). Otherwise we create a separate array to store the alpha
    // channel from QImage. 
    
    if ( hasRealAlpha && depth() != 32 ) {
        if ( data->realAlphaBits == 0 ) {
            // alpha bits buffer doesn't exist yet, create it
            data->realAlphaBits = new uchar [w * h];
        }
        // store alpha bits flipping scan lines top to bottom
        uchar *dst = data->realAlphaBits + w * (h - 1);
        int y = 0;
        while ( y < image.height() ) {
            uchar *src = image.scanLine( y++ );
            uchar *end = src + image.bytesPerLine();
            src += 3;
            while ( src < end ) {
                *(dst++) = *src;
                src += 4;
            }
            // go to the previous row
            dst -= w * 2;
        }
    } 

    // flip the image top to bottom
    image.detach();
    int bpl = image.bytesPerLine();
    uchar *line = new uchar[bpl];
    uchar *top = image.scanLine( 0 );
    uchar *bottom = image.scanLine( h - 1 );
    while( top < bottom ) {
        memcpy( line, top, bpl );
        memcpy( top, bottom, bpl );
        memcpy( bottom, line, bpl );
        top += bpl;
        bottom -= bpl;
    }
    delete [] line;
    GpiSetBitmapBits( hps, 0, h, (PBYTE) image.bits(), (PBITMAPINFO2) bmi_data );

    if ( img.hasAlphaBuffer() ) {
	QBitmap m;
	m = img.createAlphaMask( conversion_flags );
        data->mask = new QBitmap( m ); 
    }

    delete [] bmi_data;
    data->uninit = FALSE;

    return TRUE;
}


QPixmap QPixmap::grabWindow( WId window, int x, int y, int w, int h )
{
    RECTL rcl;
    WinQueryWindowRect( window, &rcl );
    if ( w <= 0 || h <= 0 ) {
	if ( w == 0 || h == 0 ) {
	    QPixmap nullPixmap;
	    return nullPixmap;
	}
	if ( w < 0 )
	    w = rcl.xRight;
	if ( h < 0 )
	    h = rcl.yTop;
    }
    // flip y coordinate
    y = rcl.yTop - (y + h);
    QPixmap pm( w, h );
    HPS hps = WinGetPS( window );
    POINTL pnts[] = { {0, 0}, {w, h}, {x, y} };
    GpiBitBlt( pm.hps, hps, 3, pnts, ROP_SRCCOPY, BBO_IGNORE );
    WinReleasePS( hps );
    return pm;
}

#ifndef QT_NO_PIXMAP_TRANSFORMATION
QPixmap QPixmap::xForm( const QWMatrix &matrix ) const
{
    int	   w, h;				// size of target pixmap
    int	   ws, hs;				// size of source pixmap
    uchar *dptr;				// data in target pixmap
    int	   dbpl, dbytes;			// bytes per line/bytes total
    uchar *sptr;				// data in original pixmap
    int	   sbpl;				// bytes per line in original
    int	   bpp;					// bits per pixel
    bool   depth1 = depth() == 1;

    if ( isNull() )				// this is a null pixmap
	return copy();

    ws = width();
    hs = height();

    QWMatrix mat( matrix.m11(), matrix.m12(), matrix.m21(), matrix.m22(), 0., 0. );

    if ( matrix.m12() == 0.0F  && matrix.m21() == 0.0F &&
	 matrix.m11() >= 0.0F  && matrix.m22() >= 0.0F ) {
	if ( mat.m11() == 1.0F && mat.m22() == 1.0F )
	    return *this;			// identity matrix

	h = qRound( mat.m22()*hs );
	w = qRound( mat.m11()*ws );
	h = QABS( h );
	w = QABS( w );
	if ( !data->hasRealAlpha ) {
	    QPixmap pm( w, h, depth(), optimization() );
            POINTL ptls[] = {
                { 0, 0 }, { w, h },
                { 0, 0 }, { ws, hs }
            };
            GpiBitBlt( pm.hps, hps, 4, ptls, ROP_SRCCOPY, BBO_IGNORE );
	    if ( data->mask ) {
		QBitmap bm =
		    data->selfmask ? *((QBitmap*)(&pm)) :
		    data->mask->xForm( matrix );
		pm.setMask( bm );
	    }
	    return pm;
	}
    } else {
	// rotation or shearing
	QPointArray a( QRect(0,0,ws+1,hs+1) );
	a = mat.map( a );
	QRect r = a.boundingRect().normalize();
	w = r.width()-1;
	h = r.height()-1;
    }

    mat = trueMatrix( mat, ws, hs ); // true matrix

    bool invertible;
    mat = mat.invert( &invertible );		// invert matrix

    if ( h == 0 || w == 0 || !invertible ) {	// error, return null pixmap
	QPixmap pm;
	pm.data->bitmap = data->bitmap;
	return pm;
    }

    if ( data->hasRealAlpha ) {
	bpp = 32;
    } else {
	bpp  = depth();
	if ( bpp > 1 && bpp < 8 )
	    bpp = 8;
    }

    sbpl = ((ws * bpp + 31) / 32) * 4;
    sptr = new uchar[ hs * sbpl ];
    int ncols;
    if ( bpp <= 8 ) {
	ncols = 1 << bpp;
    } else {
	ncols = 0;
    }

    // Note: even if trueColorDepth() != 32 here, we don't return hoping
    // that the video driver can at least convert from/to 32-bpp bitmaps
    // (which is the case for SNAP that doesn't report 32-bpp as the
    // supported depth when a lower color depth is specified in Screen
    // settings, but works well when converting).
    
    // allocate header + ncols palette entries
    int bmi_data_len = sizeof(BITMAPINFOHEADER2) + 4 * ncols;
    char *bmi_data = new char[ bmi_data_len ];
    memset( bmi_data, 0, sizeof(BITMAPINFOHEADER2) );
    BITMAPINFOHEADER2 &bmh = *(PBITMAPINFOHEADER2) bmi_data;
    bmh.cbFix = sizeof(BITMAPINFOHEADER2);
    bmh.cx = ws;
    bmh.cy = hs;
    bmh.cPlanes = 1;
    bmh.cBitCount = bpp;
    bmh.cclrUsed = ncols;
    if ( depth1 ) {
        // always use the black/white palette for monochrome pixmaps;
        // otherwise GPI can swap 0s and 1s when creating a bitmap.
        PULONG pal = (PULONG) (bmi_data + sizeof(BITMAPINFOHEADER2));
        pal[0] = 0;
        pal[1] = 0x00FFFFFF;
    }

    GpiQueryBitmapBits( hps, 0, hs, (PBYTE) sptr, (PBITMAPINFO2) bmi_data );
    
    if ( data->hasRealAlpha && data->realAlphaBits ) {
        // incorporate the alpha channel to image data
        // (see comments in convertFromImage() for details)
        int bpx = bpp / 8;
        int inc = sbpl - bpx * ws;
        uchar *src = data->realAlphaBits;
        uchar *dst = sptr + 3; // move to the alpha byte 
        for ( int y = 0; y < hs; ++ y ) {
            for ( int x = 0; x < ws; ++ x ) {
                *dst = *(src++); 
                dst += bpx;
            }
            dst += inc;
        }
    }
    
    dbpl   = ((w * bpp + 31) / 32) * 4;
    dbytes = dbpl * h;

    dptr = new uchar[ dbytes ];			// create buffer for bits
    Q_CHECK_PTR( dptr );
    if ( depth1 )
	memset( dptr, 0x00, dbytes );
    else if ( bpp == 8 )
	memset( dptr, white.pixel(), dbytes );
    else if ( data->hasRealAlpha )
	memset( dptr, 0x00, dbytes );
    else
	memset( dptr, 0xff, dbytes );

    int	  xbpl, p_inc;
    if ( depth1 ) {
	xbpl  = (w + 7) / 8;
	p_inc = dbpl - xbpl;
    } else {
	xbpl  = (w * bpp) / 8;
	p_inc = dbpl - xbpl;
    }

    // OS/2 stores first image row at the bottom of the pixmap data,
    // so do xForm starting from the bottom scanline to the top one
    if ( !qt_xForm_helper( mat, 0, QT_XFORM_TYPE_MSBFIRST, bpp,
                           dptr + dbytes - dbpl, xbpl, p_inc - dbpl * 2, h,
                           sptr + (hs*sbpl) - sbpl, -sbpl, ws, hs ) ) {
#if defined(QT_CHECK_RANGE)
	qWarning( "QPixmap::xForm: display not supported (bpp=%d)",bpp);
#endif
	QPixmap pm;
	delete [] sptr;
	delete [] bmi_data;
	delete [] dptr;
	return pm;
    }

    delete [] sptr;

    QPixmap pm( w, h, depth(), data->bitmap, optimization() );
    pm.data->uninit = FALSE;
    bmh.cx = w;
    bmh.cy = h;
    GpiSetBitmapBits( pm.hps, 0, h, (PBYTE) dptr, (PBITMAPINFO2) bmi_data );
    
    if ( data->mask ) {
	QBitmap bm = data->selfmask ? *((QBitmap*)(&pm)) :
				     data->mask->xForm(matrix);
	pm.setMask( bm );
    }
    
    if ( data->hasRealAlpha ) {
        pm.data->hasRealAlpha = TRUE; 
        if ( pm.depth() != 32 ) {
            // we are not able to store the alpha channel in the bitmap bits,
            // store it separately
            pm.data->realAlphaBits = new uchar[ w * h ];
            int bpx = bpp / 8;
            uchar *dst = pm.data->realAlphaBits;
            uchar *src = dptr + 3; // move to the alpha byte
            for ( int y = 0; y < h; ++ y ) {
                for ( int x = 0; x < w; ++ x ) {
                    *(dst++) = *src;
                    src += bpx;
                }
                src += p_inc;
            }
        }
    }
    
    delete [] bmi_data;
    delete [] dptr;
    
    return pm;
}
#endif // QT_NO_PIXMAP_TRANSFORMATION

/*!
  \fn HBITMAP QPixmap::hbm() const
  \internal
*/

bool QPixmap::hasAlpha() const
{
    return data->hasRealAlpha || data->mask;
}

bool QPixmap::hasAlphaChannel() const
{
    return data->hasRealAlpha;
}

/**
 *  \internal
 *  If \a prepare is TRUE, prepares for painting a pixmap with mask: precomposes
 *  and selects a masked bitmap (with transparent pixels made black) into this
 *  pixmap's hps. If \a prepare is FALSE, it does the cleanup. If \a force is
 *  TRUE on cleanup, the masked bitmap is destroyed regardless of the
 *  optimization type.
 * 
 *  Currently used in ::bitBlt() and in QPixmap::createIcon().
 */
void QPixmap::prepareForMasking( bool prepare, bool force /* = FALSE */ )
{
    if ( !data->mask || data->selfmask )
        return;
    if ( prepare ) {
        if ( !data->maskedHbm ) {
            GpiSetBitmap( hps, 0 );
            BITMAPINFOHEADER2 bmh;
            memset( &bmh, 0, sizeof(BITMAPINFOHEADER2) );
            bmh.cbFix = sizeof(BITMAPINFOHEADER2);
            bmh.cx = data->w;
            bmh.cy = data->h;
            bmh.cPlanes = 1;
            bmh.cBitCount = data->d;
            data->maskedHbm = GpiCreateBitmap( hps, &bmh, 0, NULL, NULL );
            GpiSetBitmap( hps, data->maskedHbm );
            POINTL ptls[] = {
                // dst is inclusive
                { 0, 0 }, { data->w - 1, data->h - 1 },
                { 0, 0 }, { data->w, data->h },
            };
            GpiWCBitBlt( hps, data->hbm, 4, ptls, ROP_SRCCOPY, BBO_IGNORE );
            // dst is exclusive
            ptls[1].x++;
            ptls[1].y++;
            // make transparent pixels black
            const ULONG AllImageAttrs =
                IBB_COLOR | IBB_BACK_COLOR |
                IBB_MIX_MODE | IBB_BACK_MIX_MODE;
            IMAGEBUNDLE oldIb;
            GpiQueryAttrs( hps, PRIM_IMAGE, AllImageAttrs, (PBUNDLE) &oldIb );
            IMAGEBUNDLE newIb = {
                CLR_TRUE, CLR_FALSE, FM_OVERPAINT, BM_OVERPAINT
            };
            GpiSetAttrs( hps, PRIM_IMAGE, AllImageAttrs, 0, (PBUNDLE) &newIb );
            GpiBitBlt( hps, data->mask->hps, 3, ptls, ROP_SRCAND, BBO_IGNORE );
            GpiSetAttrs( hps, PRIM_IMAGE, AllImageAttrs, 0, (PBUNDLE) &oldIb );
        } else {
            GpiSetBitmap( hps, data->maskedHbm );
        }
    } else {
        GpiSetBitmap( hps, data->hbm );
        // delete the precomposed masked pixmap when memory optimization
        // is in force (or force is TRUE)
        if ( data->maskedHbm && (force || (data->optim != NormalOptim &&
                                           data->optim != BestOptim)) ) {
            GpiDeleteBitmap( data->maskedHbm );
            data->maskedHbm = 0;
        }
    }
}

HPOINTER QPixmap::createIcon( bool pointer, int hotX, int hotY, bool mini )
{
    HPOINTER icon = NULLHANDLE;

    if ( !isNull() ) {
        int w = WinQuerySysValue( HWND_DESKTOP, pointer ? SV_CXPOINTER : SV_CXICON );
        int h = WinQuerySysValue( HWND_DESKTOP, pointer ? SV_CYPOINTER : SV_CYICON );
        if ( mini ) {
            w /= 2;
            h /= 2;
        }

        QPixmap pm = *this;

#if !defined(QT_PM_NO_ICON_THRESHOLD)
        // non-scalable threshold
        enum { tmax = 12 };
        int tw = w / 4;
        int th = h / 4;
        // difference
        int dw = w - data->w;
        int dh = h - data->h;
        
        if ( (tw <= tmax && th <= tmax) &&
             ((data->w >= data->h && dw > 0 && dw <= tw) ||
              (dh > 0 && dh <= th)) ) {
            pm = QPixmap( w, h );
            LONG ofsx = dw / 2;
            LONG ofsy = dh / 2;
            POINTL ptls[] = {
                { ofsx, ofsy }, { data->w + ofsx, data->h + ofsy },
                { 0, 0 },
            };
            GpiBitBlt( pm.hps, hps, 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
            QBitmap msk = QBitmap( w, h );
            msk.fill( color0 );
            if ( data->mask ) {
                GpiBitBlt( msk.hps, data->mask->hps, 3, ptls, ROP_SRCCOPY, BBO_IGNORE );
            } else {
                GpiBitBlt( msk.hps, NULL, 3, ptls, ROP_ONE, BBO_IGNORE );
            }
            pm.setMask( msk );
            // correct the hot spot
            hotX += ofsx;
            hotY += ofsy;
        } else
#endif        
        {
            if ( data->w != w || data->h != h ) {
                QImage i = convertToImage();
#if !defined(QT_NO_IMAGE_SMOOTHSCALE)
                // do smooth resize until icon is two or more times bigger
                // than the pixmap
                if ( data->w * 2 > w || data->h * 2 > h ) {
                    pm = QPixmap( i.smoothScale( w, h ) );
                } else
#endif
                {
                    pm = QPixmap( i.scale( w, h ) );
                }
                // correct the hot spot
                hotX = hotX * w / data->w;
                hotY = hotY * h / data->h;
            }
        }
        
        QBitmap msk( w, h * 2, TRUE );
        if ( pm.data->mask ) {
            // create AND mask (XOR mask is left zeroed -- it's ok)  
            POINTL ptls[] = {
                { 0, h }, { w, h * 2 },
                { 0, 0 }, { w, h },
            };
            GpiBitBlt( msk.hps, pm.data->mask->hps, 4, ptls, ROP_NOTSRCCOPY, BBO_IGNORE );
            pm.prepareForMasking( TRUE );
        }
        
        // unselect bitmap handles from hps
        GpiSetBitmap( pm.hps, 0 );        
        GpiSetBitmap( msk.hps, 0 );
        
        // Due to a bug in WinCreatePointerIndirect, it always ignores
        // hbmMiniPointer and hbmMiniColor fields, so we use only hbmPointer
        // and hbmColor. Thankfully, PM will assign it correctly either to
        // the "normal" or to the "mini" slot depending on the actual icon
        // width and height. 
        
        POINTERINFO info;
        info.fPointer = pointer;
        info.xHotspot = hotX;
        info.yHotspot = hotY;
        info.hbmPointer = msk.data->hbm;
        info.hbmColor = pm.data->mask ? pm.data->maskedHbm : pm.data->hbm;
        info.hbmMiniPointer = 0;
        info.hbmMiniColor = 0;
        icon = WinCreatePointerIndirect( HWND_DESKTOP, &info );

        if ( pm.mask() )
            pm.prepareForMasking( FALSE );
        
        GpiSetBitmap( msk.hps, msk.data->hbm );        
        GpiSetBitmap( pm.hps, pm.data->hbm );        
    }
    
    return icon;
}

void QPixmap::attachHandle( HBITMAP hbm )
{
    if ( paintingActive() )
	return;

    BITMAPINFOHEADER bmh;
    if (!GpiQueryBitmapParameters( hbm, &bmh ))
        return;

    if ( !data->uninit && !isNull() ) { // has existing pixmap
        deref();
        init( 0, 0, 0, FALSE, defOptim );
    }

    data->uninit = FALSE;
    data->w = bmh.cx;
    data->h = bmh.cy;
    data->d = bmh.cPlanes * bmh.cBitCount;

    hps = alloc_mem_ps( bmh.cx, bmh.cy );
    data->hbm = hbm;

    GpiSetBitmap( hps, data->hbm );
}

HBITMAP QPixmap::detachHandle()
{
    if ( paintingActive() )
	return 0;

    HBITMAP hbm = data->hbm;

    // do what deref() will not do after data->hbm is set to 0
    if ( hps )
        GpiSetBitmap( hps, 0 );

    data->hbm = 0;
    deref();

    return hbm;
}

void QPixmap::unfoldAlphaChannel()
{
    // do nothing if there is no alpha channel or if it is already unfolded
    // (i.e. stored in a separate array)
    if ( !data->hasRealAlpha || data->realAlphaBits )
        return;

    Q_ASSERT( data->d == 32 );
    if ( !data->d == 32 )
        return;
    
    data->realAlphaBits = new uchar[ data->w * data->h ];
    
    // no space for palette, since the depth is 32-bpp
    BITMAPINFOHEADER2 bmh;
    memset( &bmh, 0, sizeof(BITMAPINFOHEADER2) );
    bmh.cbFix = sizeof(BITMAPINFOHEADER2);
    bmh.cx = data->w;
    bmh.cy = data->h;
    bmh.cPlanes = 1;
    bmh.cBitCount = data->d;
    
    int bpl = ((data->d * data->w + 31) / 32) * 4;
    uchar *bits = new uchar [bpl * data->h];
    
    GpiQueryBitmapBits( hps, 0, data->h, (PBYTE) bits, (PBITMAPINFO2) &bmh );

    int bpx = data->d / 8;
    int inc = bpl - bpx * data->w;
    uchar *src = bits + 3; // move to the alpha byte
    uchar *dst = data->realAlphaBits;
    for ( int y = 0; y < data->h; ++ y ) {
        for ( int x = 0; x < data->w; ++ x ) {
            *(dst++) = *src;
            src += bpx;
        }
        src += inc;
    }
    
    delete[] bits;
}

/**
 *  \internal
 *  Converts the pixmap (actually, the pixmap's mask, if any) to the alpha
 *  channel. Implies #unfoldAlphaChannel().
 */
void QPixmap::convertToAlpha()
{
    if ( isNull() || data->hasRealAlpha )
        return;
    
    if ( data->mask ) {
        // allocate header + 2 palette entries
        char *bmi_data = new char[ sizeof(BITMAPINFOHEADER2) + 8 ];
        memset( bmi_data, 0, sizeof(BITMAPINFOHEADER2) );
        BITMAPINFOHEADER2 &bmh = *(PBITMAPINFOHEADER2) bmi_data;
        bmh.cbFix = sizeof(BITMAPINFOHEADER2);
        bmh.cx = data->w;
        bmh.cy = data->h;
        bmh.cPlanes = 1;
        bmh.cBitCount = 1;
        bmh.cclrUsed = 2;
        
        int bpl = ((data->w + 31) / 32) * 4;
        uchar *bits = new uchar [bpl * data->h];
        
        GpiQueryBitmapBits( data->mask->hps, 0, data->h, (PBYTE) bits,
                            (PBITMAPINFO2) bmi_data );

        data->realAlphaBits = new uchar[ data->w * data->h ];

        int inc = bpl - data->w / 8;
        uchar *src = bits;
        uchar *dst = data->realAlphaBits;
        uchar m = 0x80;
        for ( int y = 0; y < data->h; ++ y ) {
            for ( int x = 0; x < data->w; ++ x ) {
                *(dst ++) = (*src) & m ? 0xFF : 0x00; 
                m >>= 1;
                if ( !m ) {
                    m = 0x80;
                    ++ src;
                }
            }
            src += inc;
        }
        
        data->hasRealAlpha = TRUE;
        
        delete[] bits;
        delete[] bmi_data;
    } else {
        data->realAlphaBits = new uchar[ data->w * data->h ];
        memset( data->realAlphaBits, 0xFF, data->w * data->h );
        data->hasRealAlpha = TRUE;
    }
}

/**
 *  \internal
 *  This depth is used when querying and setting bitmap bits for pixmaps
 *  with alpha channel, either 24 or 32 (depending on the video driver caps).
 *  \note This depth is not used to create bitmaps (#defaultDepth() is always
 *  in charge for that purpose). 
 */
//static
int QPixmap::trueColorDepth()
{
    static int tcd = 0;
    if ( tcd == 0 ) {
        HDC display_dc = GpiQueryDevice( qt_display_ps() );
        LONG formatCnt = 0;
        DevQueryCaps( display_dc, CAPS_BITMAP_FORMATS, 1, &formatCnt );
        LONG *formats = new LONG[ formatCnt * 2 ];
        GpiQueryDeviceBitmapFormats( qt_display_ps(), formatCnt * 2, formats );
        for( int i = 0; i < formatCnt * 2; i += 2 ) {
            if ( formats[ i ] == 1 && formats[ i + 1 ] == 32 ) {
                tcd = 32;
                break;
            }
        }
        delete[] formats;
        // if the 32-bpp format is not supported, we assume the 24-bpp
        // format that must be supported by all video drivers
        if ( tcd == 0 )
            tcd = 24;
    }
    return tcd;
}

/*!
    \internal

    Creates an OS/2 icon or pointer from two given pixmaps.
    
    If either of the pixmaps doesn't match eaxctly to the system-defined icon
    or pointer size, it will be scaled to that size without keeping proportions.
    However, to avoid scaling of small pixmaps that will most likely make them
    unreadable, there is a non-scalable threshold that causes a pixmap to be
    centered rather than scaled. The non-scalable threshold value is 1/4 of the
    system-defined size but no more than 12 pixels (if more, then the threshold
    is not applied). Here are examples of non-scalable pixmap sizes for common
    icon/pointer sizes: 
    
    icon size         non-scalable pixmap sizes
    ---------         -------------------------
    16 px             12..16 px 
    20 px             15..20 px 
    32 px             24..32 px 
    40 px             30..40 px 
    
    \param pointer true to create a pointer, false to create an icon
    \param hotX X coordinate of the action point within the pointer/icon
    \param hotY Y coordinate of the action point within the pointer/icon
    \param normal pixmap for a normal-sized pointer/icon
    \param mini pixmap for a mini-sized (half-sized) pointer/icon
    
    \return PM handle of the created pointer or 0 in case of any error
    
    \note Due to a bug in WinCreatePointerIndirect, you can specify either a
    \a normal pixmap or \a mini pixmap, but not both. If both are specified,
    the \a normal will be used so that if its size is equal to or larger than
    the normal pointer/icon size (taking the threshold into account), it will
    be used as a normal pointer/icon, otherwise as a mini one. PM will use
    its own scaling to draw a pointer/icon of the other size when necessary.
    This bug may be fixed later.
    
    \warning This function is not portable.    
*/
// static
HPOINTER QPixmap::createIcon( bool pointer, int hotX, int hotY,
                              const QPixmap *normal, const QPixmap *mini )
{
    // Due to a bug in WinCreatePointerIndirect (it always ignores
    // hbmMiniPointer and hbmMiniColor fields), we can specify either a normal
    // icon, but not both. This method still accepts pixmaps for both icon
    // sizes (assuming that we will find a workaround one day) but uses only
    // one of them.
    
    if ( normal && !normal->isNull() ) {
        // If the specified normal pixmap is equal to or larger than the normal
        // icon size (taking the threshold into account), then we use it to
        // create a normal icon, otherwise a mini one.
        int w = WinQuerySysValue( HWND_DESKTOP, pointer ? SV_CXPOINTER : SV_CXICON );
        int h = WinQuerySysValue( HWND_DESKTOP, pointer ? SV_CYPOINTER : SV_CYICON );
#if !defined(QT_PM_NO_ICON_THRESHOLD)
        // apply the threshold (see private QPixmap::createIcon() above)
        if ( w / 4 <= 12 ) w -= w / 4;
        if ( h / 4 <= 12 ) h -= h / 4;
#endif        
        bool mini = normal->width() < w && normal->height() < h;
        return const_cast <QPixmap *> (normal)->createIcon( pointer, hotX, hotY, mini );
    }
    
    if ( mini && !mini->isNull() )
        return const_cast <QPixmap *> (mini)->createIcon( pointer, hotX, hotY, TRUE );
    
    return 0;
}

Q_EXPORT void copyBlt( QPixmap *dst, int dx, int dy,
		       const QPixmap *src, int sx, int sy, int sw, int sh )
{
    if ( ! dst || ! src || sw == 0 || sh == 0 || dst->depth() != src->depth() ) {
#ifdef QT_CHECK_NULL
	Q_ASSERT( dst != 0 );
	Q_ASSERT( src != 0 );
#endif
	return;
    }
    
    if ( sw < 0 )
        sw = src->width() - sx;
    if ( sh < 0 )
        sh = src->height() - sy;
    
    // ensure coordinates are within borders, clip if necessary
    if ( sx < 0 || sy < 0 || sx + sw > src->width() || sy + sh > src->height() ||
         dx < 0 || dy < 0 || dx + sw > dst->width() || dy + sh > dst->height() )
    {
        int tx = dx - sx;
        int ty = dy - sy;
        QRect dr( 0, 0, dst->width(), dst->height() ); // dst rect
        QRect sr( tx, ty, src->width(), src->height() ); // src rect in dst coords
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
    
    dst->detach();
        
    // copy mask data
    if ( src->data->mask ) {
	if ( !dst->data->mask ) {
	    dst->data->mask = new QBitmap( dst->width(), dst->height() );
	    // new masks are fully opaque by default
	    dst->data->mask->fill( Qt::color1 );
	} else if ( dst->data->maskedHbm ) {
            // reset the precomposed masked pixmap
            dst->prepareForMasking( FALSE, TRUE );
        }

	bitBlt( dst->data->mask, dx, dy,
		src->data->mask, sx, sy, sw, sh, Qt::CopyROP, TRUE );
    }
    
    // copy alpha bits
    if ( src->data->hasRealAlpha ||
         (src->data->mask && dst->data->hasRealAlpha) ) {
        if ( src->data->hasRealAlpha )
            const_cast <QPixmap *> (src)->unfoldAlphaChannel();
        else
            const_cast <QPixmap *> (src)->convertToAlpha();
        if ( dst->data->hasRealAlpha )
            dst->unfoldAlphaChannel();
        else
            dst->convertToAlpha();
        uchar *srca = src->data->realAlphaBits + src->width() * sy + sx;
        uchar *dsta = dst->data->realAlphaBits + dst->width() * dy + dx;
        for ( int y = 0; y < sh; ++ y ) {
            memcpy( dsta, srca, sw );
            srca += src->width();
            dsta += dst->width();
        }
    }
    
    // copy pixel data
    bitBlt( dst, dx, dy, src, sx, sy, sw, sh, Qt::CopyROP, TRUE );
}
