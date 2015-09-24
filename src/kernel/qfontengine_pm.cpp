/****************************************************************************
** $Id: qfontengine_pm.cpp 170 2007-04-10 21:12:37Z dmik $
**
** ???
**
** Copyright (C) 2003 Trolltech AS.  All rights reserved.
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

#include "qfontengine_p.h"
#include <qglobal.h>
#include "qt_os2.h"
#include "qapplication_p.h"

#include <qpaintdevice.h>
#include <qpainter.h>

#include <private/qunicodetables_p.h>
#include <qbitmap.h>

#include <qthreadstorage.h>

/// @todo need?
//#ifndef M_PI
//#define M_PI 3.14159265358979
//#endif

// per-thread unique screen ps to perform font operations

/// @todo (r=dmik) optimize: use small mem hps'es 1x1 px instead of screen ps?
class QDisplayPS
{
public:
    QDisplayPS() { hps = WinGetScreenPS( HWND_DESKTOP ); }
    ~QDisplayPS() { WinReleasePS( hps ); }
    HPS hps;
};
static QThreadStorage<QDisplayPS *> display_ps;


// general font engine

QFontEngine::~QFontEngine()
{
    hps = 0;
    delete pfm;
}

int QFontEngine::lineThickness() const
{
/// @todo (r=dmik) values from FONTMETRICS are not always good (line is too
//  thick or too close to glyphs). The algorithm below is not always perfect
//  either. Which one to leave?
//    // ad hoc algorithm
//    int score = fontDef.weight * fontDef.pixelSize;
//    int lw = score / 700;
//
//    // looks better with thicker line for small pointsizes
//    if ( lw < 2 && score >= 1050 ) lw = 2;
//    if ( lw == 0 ) lw = 1;
//
//    return lw;
    return pfm->lUnderscoreSize;
}

int QFontEngine::underlinePosition() const
{
/// @todo (r=dmik) values from FONTMETRICS are not always good (line is too
//  thick or too close to glyphs). The algorithm below is not always perfect
//  either. Which one to leave?
//    int pos = ( ( lineThickness() * 2 ) + 3 ) / 6;
    int pos = pfm->lUnderscorePosition;
    // ensure one pixel between baseline and underline
    return pos > 1 ? pos : 2;
}

HPS QFontEngine::ps() const
{
    // if hps is not zero this means a printer font.
    // NOTE: such (printer) font engines can be currently created only by
    // QPainter::updateFont() and they are removed from the font cache by
    // QFontCache::cleanupPrinterFonts() that is called by QPainter::end(),
    // so it is safe to store only a hps of the printer paint device instead
    // the whole QPaintDevice object (these font engines cannot live after
    // the printer paint device is destroyed -- they are destroyed first,
    // when the painting has ended on a particular printer device).

    HPS ps = hps;
    if ( !ps ) {
        if ( !display_ps.hasLocalData() )
            display_ps.setLocalData( new QDisplayPS );
        ps = display_ps.localData()->hps;
    }
    selectTo( ps );
    return ps;
}

static inline void uint2STR8( uint num, PSTR8 str )
{
    uint *p = (uint *) str;
    *(p++) = (num & 0x0F0F0F0F) + 0x41414141;
    *p = ((num >> 4) & 0x0F0F0F0F) + 0x41414141;
}

static inline uint STR82uint( PSTR8 str )
{
    uint *p = (uint *) str;
    uint num = (*(p++)) - 0x41414141;
    num |= ((*p) - 0x41414141) << 4;
    return num;
}

void QFontEngine::selectTo( HPS ps ) const
{
    // innotek ft2lib 2.40 release 1 has a strange bug: when we pass the STR8
    // identifier to GpiCreateLogFont() with the fourth char being zero it
    // causes a trap. So we use uint2STR8() to convert pointers to a string
    // containing 8 uppercase letters instead of passing pointers directly.

    STR8 id;
    FATTRS dummy;
    uint2STR8( 0, &id );
    GpiQueryLogicalFont( ps, LCID_QTFont, &id, &dummy, sizeof(FATTRS) );
    if ( STR82uint( &id ) != (uint) this ) {
#if 0
        qDebug( "QFontEngine::selectTo: selecting font '%i:%i.%s' to %08lX",
            fontDef.pixelSize, fontDef.pointSize/10, fa.szFacename, ps );
#endif
        uint2STR8( (uint) this, &id );
        GpiCreateLogFont( ps, &id, LCID_QTFont, &fa );
        GpiSetCharSet( ps, LCID_QTFont );
        if ( fa.fsFontUse & FATTR_FONTUSE_OUTLINE ) {
            SIZEF sz;
            sz.cy = MAKEFIXED( fontDef.pixelSize, 0 );
            sz.cx = sz.cy;
            GpiSetCharBox( ps, &sz );
        }
    }
#if 0
    else {
        qDebug( "QFontEngine::selectTo: font '%i:%i.%s' is already selected to %08lX",
            fontDef.pixelSize, fontDef.pointSize/10, fa.szFacename, ps );
    }
#endif
}

QFontEnginePM::QFontEnginePM( HPS ps, PFATTRS pfa, int pixelSize, int pointSize )
{
    hps = ps;
    fa = *pfa;
    pfm = new FONTMETRICS;

    fontDef.pixelSize = pixelSize;
    fontDef.pointSize = pointSize;
    // the rest of fontDef is initialized by QFontDatabase::findFont()

    GpiQueryFontMetrics( this->ps(), sizeof(FONTMETRICS), pfm );

    // cache cost here should be in bytes. it is converted to
    // kbytes by QFontCache::increaseCost()
    /// @todo is this formula for cost ok?
    cache_cost = pfm->lMaxBaselineExt * pfm->lAveCharWidth * 2000;

    widthCache = new unsigned char [widthCacheSize];
    memset( widthCache, 0, widthCacheSize );
}

#define isDBCSGlyph(g) ( (g) & 0xFF00 )

#define queryGlyphSize(g) ( isDBCSGlyph( g ) ? 2 : 1 )

/// @todo (r=dmik) current implementation of this fn uses the local8bit QChar
//  code as the glyph index, i.e. glyphs are just unicode chars converted to
//  8 bit chars according to the current (system) code page. This will be
//  changed when all font-related code is rewritten to support true unicode
//  (for example, by using innotek ft2lib, since native OS/2 unicode support
//  relative to text output using GPI is pretty buggy).
QFontEngine::Error QFontEnginePM::stringToCMap( const QChar *str, int len,
                                                glyph_t *glyphs, advance_t *advances,
                                                int *nglyphs, bool mirrored ) const
{
    /// @todo mirrored is currently ignored.
    Q_UNUSED( mirrored );

    const char *uconvFirstByteTable = qt_os2UconvFirstByteTable();

    // convert multibyte chars to "wide char" glyphs
    QCString s = QConstString( str, len ).string().local8Bit();
    // sanity check (uint value must fit into positive int)
    Q_ASSERT( (int) s.size() >= 1 );
    if ( (int) s.size() < 1 ) {
        *nglyphs = 0;
        return OutOfMemory;
    }
    // we use QCString::size() instead of length() because the str array may
    // contain embedded zero chars that must be also processed
    int slen = (int) s.size() - 1; // exclude terminating zero
    if ( slen == 0 ) {
        // null string or no valid conversion, nothing to do
        *nglyphs = 0;
        return NoError;
    }
    int givenLen = *nglyphs;
    int realLen = 0;
    for( int i = 0; i < slen; i++ ) {
        bool isDBCSLeadByte = uconvFirstByteTable[ (uchar) s[ i ] ] == 2;
        if ( realLen < givenLen ) {
            // enough space, store the glyph
            glyphs[ realLen ] = s[ i ] & 0xFF;
            if( isDBCSLeadByte )
                glyphs[ realLen ] |= ( s[ ++i ] << 8 ) & 0xFF00;
        } else {
            // not enough space, keep calulating the length
            if( isDBCSLeadByte )
                ++i;
        }
        realLen++;
    }

    if ( givenLen < realLen ) {
        *nglyphs = realLen;
        return OutOfMemory;
    }

    if ( advances ) {
        HPS ps = 0;
        glyph_t glyph;
        for( register int i = 0; i < realLen; i++ ) {
            glyph = *(glyphs + i);
            advances[i] = (glyph < widthCacheSize) ? widthCache[glyph] : 0;
            // font-width cache failed
            if ( !advances[i] ) {
                if ( !ps ) ps = this->ps();
                POINTL ptls [TXTBOX_COUNT];
                GpiQueryTextBox( ps, queryGlyphSize( glyph ),
                                 (PCH) &glyph, TXTBOX_COUNT, ptls );
                advances[i] = ptls[4].x;
                // if glyph's within cache range, store it for later
                if ( glyph < widthCacheSize && advances[i] < 0x100 )
                    ((QFontEnginePM *)this)->widthCache[glyph] = advances[i];
            }
        }
    }

    *nglyphs = realLen;
    return NoError;
}

// QRgb has the same RGB format (0xaarrggbb) as OS/2 uses (ignoring the
// highest alpha byte) so we just mask alpha to get the valid OS/2 color.
#define COLOR_VALUE(c) ((p->flags & QPainter::RGBColor) ? (c.rgb() & RGB_MASK) : c.pixel())

// defined in qpaintdevice_pm.cpp
extern const LONG qt_ropCodes_2ROP[];

void QFontEnginePM::draw( QPainter *p, int x, int y, const QTextEngine
                          *engine, const QScriptItem *si, int textFlags )
{
    HPS ps = p->handle();

    glyph_t *glyphs = engine->glyphs( si );
    advance_t *advances = engine->advances( si );
    qoffset_t *offsets = engine->offsets( si );

    // GPI version of the opaque rectangle below the text is rather strange,
    // so we draw it ourselves
    if ( p->backgroundMode() == Qt::OpaqueMode ) {
        LONG oldMix = GpiQueryMix( ps );
        GpiSetMix( ps, FM_LEAVEALONE );
        // use drawRect to have the rectangle properly transformed
        /// @todo (r=dmik)
        //      we don't add 1 to si->ascent + si->descent to exactly match
        //      QFontMetrics::boundingRect(). This stuff needs to be reviewed
        //      and synchronized everywhere (first we should define
        //      is total_height = acsent + descent or greater by one?).
        //      This also relates to the baseline issue below.
        p->drawRect( x, y - si->ascent, si->width, si->ascent + si->descent );
        GpiSetMix( ps, oldMix );
    }

    if ( !(pfm->fsDefn & FM_DEFN_OUTLINE) && p->txop > QPainter::TxTranslate ) {
        // GPI is not capable (?) to scale/shear/rotate bitmap fonts,
        // so we do it by hand
        QRect bbox( 0, 0, si->width, si->ascent + si->descent + 1 );
        int w = bbox.width(), h = bbox.height();
        if ( w == 0 || h == 0 )
            return;
        QWMatrix tmat = QPixmap::trueMatrix( p->xmat, w, h );
        QBitmap bm( w, h, TRUE );
        QPainter paint;

        // draw text into the bitmap
        paint.begin( &bm );
        // select the proper font manually
        paint.clearf( QPainter::DirtyFont );
        selectTo ( paint.handle() );
        draw( &paint, 0, si->ascent, engine, si, textFlags );
        paint.end();
        // transform bitmap
        QBitmap wx_bm = bm.xForm( tmat );
        if ( wx_bm.isNull() )
            return;

        // compute the position of the bitmap
        double fx = x, fy = y, nfx, nfy;
        p->xmat.map( fx,fy, &nfx, &nfy );
        double tfx = 0., tfy = si->ascent, dx, dy;
        tmat.map( tfx, tfy, &dx, &dy );
        x = qRound( nfx - dx );
        y = qRound( nfy - dy );

        // flip y coordinate of the bitmap
        if ( p->devh )
            y = p->devh - (y + wx_bm.height());

        POINTL ptls[] = { { x, y }, { x + wx_bm.width(), y + wx_bm.height() },
                          { 0, 0 } };

        // leave bitmap background transparent when blitting
        LONG oldBackMix = GpiQueryBackMix( p->handle() );
        GpiSetBackMix( p->handle(), BM_SRCTRANSPARENT );

        GpiBitBlt( p->handle(), wx_bm.handle(), 3, ptls,
                   qt_ropCodes_2ROP[p->rop], BBO_IGNORE );
        GpiSetBackMix( p->handle(), oldBackMix );

        return;
    }

    if ( p->testf(QPainter::DirtyFont) )
        p->updateFont();

    // leave text background transparent -- we've already drawn it if OpaqueMode
    LONG oldBackMix = GpiQueryBackMix( ps );
    GpiSetBackMix( ps, BM_LEAVEALONE );

    /// @todo (dmik)
    //      OS/2 GPI thinks that regular font letters (without any lower elements)
    //      should "overlap" the baseline, while Qt/Win32 thinks they should "sit"
    //      on it. For now, we simply mimic the Qt/Win32 behavior (for compatibilty)
    //      by correcting the y coordinate by one pixel. This is rather stupid.
    //      The proper way is to review all the code that deals with
    //      ascents/descents, bounding boxes/rects etc. (currently, all this
    ///     stuff has various issues regarding to misaligmnent between the opaque
    //      rectangle of the char and its bounding box etc.) and make a correct
    //      desicion afterwards... Btw, Qt/Win32 is rather buggy here too.
    y--;

    // prepare transformations
    bool nativexform = FALSE;
    LONG ySign = 1;

    if ( p->txop == QPainter::TxTranslate ) {
        p->map( x, y, &x, &y );
    } else if ( p->txop > QPainter::TxTranslate ) {
        y = -y;
        ySign = -1;
        nativexform = p->setNativeXForm( TRUE /* assumeYNegation */ );
        if( !nativexform )
            return;
    }

    ULONG options = 0;

    // We draw underscores and strikeouts ourselves (since OS/2 font rasterizer
    // usually does this very ugly, and sometimes it doesn't work at all with
    // some fonts (i.e. WarpSans)).
    /*
    if ( textFlags & Qt::Underline ) options |= CHS_UNDERSCORE;
    if ( textFlags & Qt::StrikeOut ) options |= CHS_STRIKEOUT;
    */

    int xo = x;
    POINTL ptl;

    if ( !(si->analysis.bidiLevel % 2) ) {
        bool haveOffsets = FALSE;
        int w = 0;
        for( int i = 0; i < si->num_glyphs; i++ ) {
            if ( offsets[i].x || offsets[i].y ) {
                haveOffsets = TRUE;
                break;
            }
            w += advances[i];
        }

        if ( haveOffsets ) {
            for( int i = 0; i < si->num_glyphs; i++ ) {
                ptl.x = x + offsets->x;
                ptl.y = y + ySign * offsets->y;
                // flip y coordinate
                if ( !nativexform && p->devh )
                    ptl.y = p->devh - (ptl.y + 1);
                GpiCharStringPosAt( ps, &ptl, NULL, options,
                                    queryGlyphSize( *glyphs ), (PCH) glyphs, NULL );
                x += *advances;
                glyphs++;
                offsets++;
                advances++;
            }
        } else {
            // fast path
            ptl.x = x;
            ptl.y = y;
            // flip y coordinate
            if ( !nativexform && p->devh )
                ptl.y = p->devh - (ptl.y + 1);
            GpiMove( ps, &ptl );
            // draw glyphs in 512 char chunks: it's a GpiCharString* limitation
            advance_t adv[ 512 ];
            char str[ 512 ];
            int len = 0;
            // convert "wide char" glyphs to a multi byte string
            glyph_t *pg = glyphs;
            advance_t *pa = advances;
            for( int i = 0; i < si->num_glyphs; i++, pg++, pa++ ) {
                str[ len ] = *pg & 0xFF;
                adv[ len++ ] = *pa;

                if( isDBCSGlyph( *pg ) ) {
                    str[ len ] = ( *pg & 0xFF00 ) >> 8;
                    adv[ len++ ] = 0;
                }

                if( len > 510 ) {
                    GpiCharStringPos( ps, NULL, options | CHS_VECTOR, len, str, (PLONG) adv );
                    len = 0;
                }
            }

            if( len > 0 )
                GpiCharStringPos( ps, NULL, options | CHS_VECTOR, len, str, (PLONG) adv );

            x += w;
        }
    } else {
        offsets += si->num_glyphs;
        advances += si->num_glyphs;
        glyphs += si->num_glyphs;
        for( int i = 0; i < si->num_glyphs; i++ ) {
            glyphs--;
            offsets--;
            advances--;
            ptl.x = x + offsets->x;
            ptl.y = y + ySign * offsets->y;
            // flip y coordinate
            if ( !nativexform && p->devh )
                ptl.y = p->devh - (ptl.y + 1);
            GpiCharStringPosAt( ps, &ptl, NULL, options,
                                queryGlyphSize( *glyphs ), (PCH) glyphs, NULL );
            x += *advances;
        }
    }

    if ( textFlags & (Qt::Overline | Qt::Underline | Qt::StrikeOut) ) {
        AREABUNDLE oldAb, ab;
        const LONG aa = ABB_COLOR | ABB_SET | ABB_SYMBOL;
        GpiQueryAttrs( ps, PRIM_AREA, aa, &oldAb );
        ab.lColor = COLOR_VALUE( p->cpen.color() );
        ab.usSet = LCID_DEFAULT;
        ab.usSymbol = PATSYM_SOLID;
        GpiSetAttrs( ps, PRIM_AREA, aa, 0, &ab );

        int lw = lineThickness() - 1; // inclusive
        int lp;
        // flip y coordinate
        int yp = y;
        if ( !nativexform && p->devh )
            yp = p->devh - (yp + 1);
        if ( textFlags & (Qt::Underline) ) {
            ptl.x = xo;
            ptl.y = yp - underlinePosition();
            GpiMove( ps, &ptl );
            ptl.x = x - 1; ptl.y -= lw;
            GpiBox( ps, DRO_FILL, &ptl, 0, 0 );
        }
        if ( textFlags & (Qt::Overline) ) {
            lp = ascent() + 1; // corresponds to QFontMetrics::overlinePos()
            if ( !lp ) lp = 1;
            ptl.x = xo;
            ptl.y = yp + lp;
            GpiMove( ps, &ptl );
            ptl.x = x - 1; ptl.y -= lw;
            GpiBox( ps, DRO_FILL, &ptl, 0, 0 );
        }
        if ( textFlags & (Qt::StrikeOut) ) {
            lp = ascent() / 3; // corresponds to QFontMetrics::strikeOutPos()
            if ( !lp ) lp = 1;
            ptl.x = xo;
            ptl.y = yp + lp;
            GpiMove( ps, &ptl );
            ptl.x = x - 1; ptl.y -= lw;
            GpiBox( ps, DRO_FILL, &ptl, 0, 0 );
        }
        GpiSetAttrs( ps, PRIM_AREA, aa, 0, &oldAb );
    }

    if ( nativexform ) {
        p->clearNativeXForm();
    }

    GpiSetBackMix( ps, oldBackMix );
}

glyph_metrics_t QFontEnginePM::boundingBox( const glyph_t *glyphs,
				const advance_t *advances, const qoffset_t *offsets, int numGlyphs )
{
    Q_UNUSED( glyphs );
    Q_UNUSED( offsets );

    if ( numGlyphs == 0 )
	return glyph_metrics_t();

    int w = 0;
    const advance_t *end = advances + numGlyphs;
    while( end > advances )
	w += *(--end);

    /// @todo (r=dmik) look at usage of this fn, is the return correct?
    return glyph_metrics_t(
        0, -pfm->lMaxAscender, w, pfm->lMaxAscender + pfm->lMaxDescender, w, 0
    );
}

glyph_metrics_t QFontEnginePM::boundingBox( glyph_t glyph )
{
    POINTL ptls [TXTBOX_COUNT];
    GpiQueryTextBox( ps(), queryGlyphSize( glyph ),
                     (PCH) &glyph, TXTBOX_COUNT, ptls );
    int minx = 0, miny = 0, maxx = 0, maxy = 0;
    for ( int i = 0; i < 4; i++ ) {
        minx = QMIN( minx, ptls[i].x );
        miny = QMIN( miny, ptls[i].y );
        maxx = QMAX( maxx, ptls[i].x );
        maxy = QMAX( maxy, ptls[i].y );
    }
    return glyph_metrics_t(
        minx, -maxy, maxx - minx + 1, maxy - miny + 1, ptls[4].x, -ptls[4].y
    );
}

int QFontEnginePM::ascent() const
{
    return pfm->lMaxAscender;
}

int QFontEnginePM::descent() const
{
    return pfm->lMaxDescender;
}

int QFontEnginePM::leading() const
{
    return pfm->lExternalLeading;
}

int QFontEnginePM::maxCharWidth() const
{
    return pfm->lMaxCharInc;
}

int QFontEnginePM::minLeftBearing() const
{
    /// @todo (r=dmik) later
    return 0;
//    if ( lbearing == SHRT_MIN )
//	minRightBearing(); // calculates both
//
//    return lbearing;
}

int QFontEnginePM::minRightBearing() const
{
    /// @todo (r=dmik) later
    return 0;
//#ifdef Q_OS_TEMP
//	return 0;
//#else
//    if ( rbearing == SHRT_MIN ) {
//	int ml = 0;
//	int mr = 0;
//	if ( ttf ) {
//	    HDC hdc = dc();
//	    SelectObject( hdc, hfont );
//	    ABC *abc = 0;
//	    int n = QT_WA_INLINE( tm.w.tmLastChar - tm.w.tmFirstChar, tm.a.tmLastChar - tm.a.tmFirstChar );
//	    if ( n <= max_font_count ) {
//		abc = new ABC[n+1];
//		QT_WA( {
//		    GetCharABCWidths(hdc, tm.w.tmFirstChar, tm.w.tmLastChar, abc);
//		}, {
//		    GetCharABCWidthsA(hdc,tm.a.tmFirstChar,tm.a.tmLastChar,abc);
//		} );
//	    } else {
//		abc = new ABC[char_table_entries+1];
//		QT_WA( {
//		    for( int i = 0; i < char_table_entries; i++ )
//			GetCharABCWidths(hdc, char_table[i], char_table[i], abc+i);
//		}, {
//		    for( int i = 0; i < char_table_entries; i++ ) {
//			QCString w = QString(QChar(char_table[i])).local8Bit();
//			if ( w.length() == 1 ) {
//			    uint ch8 = (uchar)w[0];
//			    GetCharABCWidthsA(hdc, ch8, ch8, abc+i );
//			}
//		    }
//		} );
//		n = char_table_entries;
//	    }
//	    ml = abc[0].abcA;
//	    mr = abc[0].abcC;
//    	    for ( int i = 1; i < n; i++ ) {
//		if ( abc[i].abcA + abc[i].abcB + abc[i].abcC != 0 ) {
//		    ml = QMIN(ml,abc[i].abcA);
//		    mr = QMIN(mr,abc[i].abcC);
//		}
//	    }
//	    delete [] abc;
//	} else {
//	    QT_WA( {
//		ABCFLOAT *abc = 0;
//		int n = tm.w.tmLastChar - tm.w.tmFirstChar+1;
//		if ( n <= max_font_count ) {
//		    abc = new ABCFLOAT[n];
//		    GetCharABCWidthsFloat(hdc, tm.w.tmFirstChar, tm.w.tmLastChar, abc);
//		} else {
//		    abc = new ABCFLOAT[char_table_entries];
//		    for( int i = 0; i < char_table_entries; i++ )
//			GetCharABCWidthsFloat(hdc, char_table[i], char_table[i], abc+i);
//		    n = char_table_entries;
//		}
//		float fml = abc[0].abcfA;
//		float fmr = abc[0].abcfC;
//		for (int i=1; i<n; i++) {
//		    if ( abc[i].abcfA + abc[i].abcfB + abc[i].abcfC != 0 ) {
//			fml = QMIN(fml,abc[i].abcfA);
//			fmr = QMIN(fmr,abc[i].abcfC);
//		    }
//		}
//		ml = int(fml-0.9999);
//		mr = int(fmr-0.9999);
//		delete [] abc;
//	    } , {
//		ml = 0;
//		mr = -tm.a.tmOverhang;
//	    } );
//	}
//	((QFontEngine *)this)->lbearing = ml;
//	((QFontEngine *)this)->rbearing = mr;
//    }
//
//    return rbearing;
//#endif
}

bool QFontEnginePM::canRender( const QChar *string,  int len )
{
    /// @todo (r=dmik) later
    return TRUE;

//    if ( symbol ) {
//	while( len-- ) {
//	    if ( getGlyphIndex( cmap, string->unicode() ) == 0 ) {
//		if( string->unicode() < 0x100 ) {
//		    if(getGlyphIndex( cmap, string->unicode()+0xf000) == 0)
//    			return FALSE;
//		} else {
//		    return FALSE;
//		}
//	    }
//	    string++;
//	}
//    } else if ( ttf ) {
//	while( len-- ) {
//	    if ( getGlyphIndex( cmap, string->unicode() ) == 0 )
//		return FALSE;
//	    string++;
//	}
//    } else {
//	QT_WA( {
//	    while( len-- ) {
//		if ( tm.w.tmFirstChar > string[len].unicode() || tm.w.tmLastChar < string[len].unicode() )
//		    return FALSE;
//	    }
//	}, {
//	    while( len-- ) {
//		if ( tm.a.tmFirstChar > string[len].unicode() || tm.a.tmLastChar < string[len].unicode() )
//		    return FALSE;
//	    }
//	} );
//    }
//    return TRUE;
}

QFontEngine::Type QFontEnginePM::type() const
{
    return QFontEngine::PM;
}


// ----------------------------------------------------------------------------
// True type support methods
// ----------------------------------------------------------------------------



/// @todo (r=dmik) all this may be necessary when ft2lib is used
#if 0
#define MAKE_TAG(ch1, ch2, ch3, ch4) (\
    (((DWORD)(ch4)) << 24) | \
    (((DWORD)(ch3)) << 16) | \
    (((DWORD)(ch2)) << 8) | \
    ((DWORD)(ch1)) \
    )

static inline Q_UINT32 getUInt(unsigned char *p)
{
    Q_UINT32 val;
    val = *p++ << 24;
    val |= *p++ << 16;
    val |= *p++ << 8;
    val |= *p;

    return val;
}

static inline Q_UINT16 getUShort(unsigned char *p)
{
    Q_UINT16 val;
    val = *p++ << 8;
    val |= *p;

    return val;
}

static inline void tag_to_string( char *string, Q_UINT32 tag )
{
    string[0] = (tag >> 24)&0xff;
    string[1] = (tag >> 16)&0xff;
    string[2] = (tag >> 8)&0xff;
    string[3] = tag&0xff;
    string[4] = 0;
}

static Q_UINT16 getGlyphIndex( unsigned char *table, unsigned short unicode )
{
    unsigned short format = getUShort( table );
    if ( format == 0 ) {
	if ( unicode < 256 )
	    return (int) *(table+6+unicode);
    } else if ( format == 2 ) {
	qWarning("format 2 encoding table for Unicode, not implemented!");
    } else if ( format == 4 ) {
	Q_UINT16 segCountX2 = getUShort( table + 6 );
	unsigned char *ends = table + 14;
	Q_UINT16 endIndex = 0;
	int i = 0;
	for ( ; i < segCountX2/2 && (endIndex = getUShort( ends + 2*i )) < unicode; i++ );

	unsigned char *idx = ends + segCountX2 + 2 + 2*i;
	Q_UINT16 startIndex = getUShort( idx );

	if ( startIndex > unicode )
	    return 0;

	idx += segCountX2;
	Q_INT16 idDelta = (Q_INT16)getUShort( idx );
	idx += segCountX2;
	Q_UINT16 idRangeoffset_t = (Q_UINT16)getUShort( idx );

	Q_UINT16 glyphIndex;
	if ( idRangeoffset_t ) {
	    Q_UINT16 id = getUShort( idRangeoffset_t + 2*(unicode - startIndex) + idx);
	    if ( id )
		glyphIndex = ( idDelta + id ) % 0x10000;
	    else
		glyphIndex = 0;
	} else {
	    glyphIndex = (idDelta + unicode) % 0x10000;
	}
	return glyphIndex;
    }

    return 0;
}


static unsigned char *getCMap( HDC hdc, bool &symbol )
{
    const DWORD CMAP = MAKE_TAG( 'c', 'm', 'a', 'p' );

    unsigned char header[4];

    // get the CMAP header and the number of encoding tables
    DWORD bytes =
#ifndef Q_OS_TEMP
	GetFontData( hdc, CMAP, 0, &header, 4 );
#else
	0;
#endif
    if ( bytes == GDI_ERROR )
	return 0;
    unsigned short version = getUShort( header );
    if ( version != 0 )
	return 0;

    unsigned short numTables = getUShort( header+2);
    unsigned char *maps = new unsigned char[8*numTables];

    // get the encoding table and look for Unicode
#ifndef Q_OS_TEMP
    bytes = GetFontData( hdc, CMAP, 4, maps, 8*numTables );
#endif
    if ( bytes == GDI_ERROR )
	return 0;

    symbol = TRUE;
    unsigned int unicode_table = 0;
    for ( int n = 0; n < numTables; n++ ) {
	Q_UINT32 version = getUInt( maps + 8*n );
	// accept both symbol and Unicode encodings. prefer unicode.
	if ( version == 0x00030001 || version == 0x00030000 ) {
	    unicode_table = getUInt( maps + 8*n + 4 );
	    if ( version == 0x00030001 ) {
		symbol = FALSE;
		break;
	    }
	}
    }

    if ( !unicode_table ) {
	// qDebug("no unicode table found" );
	return 0;
    }

    delete [] maps;

    // get the header of the unicode table
#ifndef Q_OS_TEMP
    bytes = GetFontData( hdc, CMAP, unicode_table, &header, 4 );
#endif
    if ( bytes == GDI_ERROR )
	return 0;

    unsigned short length = getUShort( header+2 );
    unsigned char *unicode_data = new unsigned char[length];

    // get the cmap table itself
#ifndef Q_OS_TEMP
    bytes = GetFontData( hdc, CMAP, unicode_table, unicode_data, length );
#endif
    if ( bytes == GDI_ERROR ) {
	delete [] unicode_data;
	return 0;
    }
    return unicode_data;
}
#endif // if 0


