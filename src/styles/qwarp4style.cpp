/****************************************************************************
** $Id: qwarp4style.cpp 199 2011-06-19 06:18:56Z rudi $
**
** Implementation of OS/2 Warp-like style class
**
** Copyright (C) 1992-2003 Trolltech AS.  All rights reserved.
** Copyright (C) 2007-2007 netlabs.org. OS/2 Development.
**
** This file is part of the widgets module of the Qt GUI Toolkit.
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

#if !defined(QT_NO_STYLE_WARP4) || defined(QT_PLUGIN)

#include <limits.h>
#include "qapplication.h"
#include "qpainter.h"
#include "qdrawutil.h" // for now
#include "qlabel.h"
#include "qimage.h"
#include "qcombobox.h"
#include "qlistbox.h"
#include "qrangecontrol.h"
#include "qscrollbar.h"
#include "qslider.h"
#include "qtabwidget.h"
#include "qlistview.h"
#include "qbitmap.h"
#include "qcleanuphandler.h"
#include "qdockwindow.h"
#include "qobjectlist.h"
#include "qmenubar.h"
#include "qprogressbar.h"
#include "qptrlist.h"
#include "qvaluelist.h"
#include "qmainwindow.h"
#include "qdockarea.h"

#include "qpixmap.h"
#include "qmap.h"
#include "qtabbar.h"
#include "qwidget.h"
#include "qwidgetstack.h"
#include "qtoolbutton.h"
#include "qpushbutton.h"
#include "qpopupmenu.h"
#include "qevent.h"

#include "qwarp4style.h"

#ifndef QT_NO_ICONSET
#include "qiconset.h"
#endif

// defined in qapplication_pm.cpp
extern QRgb qt_sysclr2qrgb(LONG sysClr);

/*----------------------------------------------------------------------*\
 * Internal pixmaps
\*----------------------------------------------------------------------*/

// Note: All these colors only serve as placeholders; they are being replaced
// by system colors in the QWarpPixmap constructor. After that, they are being
// cached as pixmaps (like the system bitmaps and icons)

static char* arrowup_xpm[] =
{
/* width height ncolors chars_per_pixel */
"12 12 4 1",
/* colors */
"  c #0000FF", // transparent
"! c #808080", // dark grey
"# c #FFFFFF", // light
"$ c #000000", // shadow
/* pixels */
"            ",
"            ",
"            ",
"     ##     ",
"    #!!#    ",
"   #!!!!#   ",
"  #!!!!!!#  ",
" #!!!!!!!!# ",
"#$$$$$$$$$$#",
"            ",
"            ",
"            "
};

static char* arrowdown_xpm[] =
{
/* width height ncolors chars_per_pixel */
"12 12 4 1",
/* colors */
"  c #0000FF", // transparent
"! c #808080", // dark grey
"# c #FFFFFF", // light
"$ c #000000", // shadow
/* pixels */
"            ",
"            ",
"            ",
"#$$$$$$$$$$#",
" #!!!!!!!!# ",
"  #!!!!!!#  ",
"   #!!!!#   ",
"    #!!#    ",
"     ##     ",
"            ",
"            ",
"            "
};

static char* arrowright_xpm[] =
{
/* width height ncolors chars_per_pixel */
"12 12 4 1",
/* colors */
"  c #0000FF", // transparent
"! c #808080", // dark grey
"# c #FFFFFF", // light
"$ c #000000", // shadow
/* pixels */
"   #        ",
"   $#       ",
"   $!#      ",
"   $!!#     ",
"   $!!!#    ",
"   $!!!!#   ",
"   $!!!!#   ",
"   $!!!#    ",
"   $!!#     ",
"   $!#      ",
"   $#       ",
"   #        "
};

static char* arrowleft_xpm[] =
{
/* width height ncolors chars_per_pixel */
"12 12 4 1",
/* colors */
"  c #0000FF", // transparent
"! c #808080", // dark grey
"# c #FFFFFF", // light
"$ c #000000", // shadow
/* pixels */
"        #   ",
"       #$   ",
"      #!$   ",
"     #!!$   ",
"    #!!!$   ",
"   #!!!!$   ",
"   #!!!!$   ",
"    #!!!$   ",
"     #!!$   ",
"      #!$   ",
"       #$   ",
"        #   "
};

static char* checkmark_xpm[] =
{
/* width height ncolors chars_per_pixel */
"8 8 4 1",
/* colors */
"  c #0000FF", // transparent
"! c #808080", // dark grey
"# c #FFFFFF", // light
"$ c #000000", // shadow
/* pixels */
"   #!   ",
"  ##!!  ",
" ###!!! ",
"####!!!!",
"!!!!$$$$",
" !!!$$$ ",
"  !!$$  ",
"   !$   "
};

static char* tabhdrplus_xpm[] =
{
/* width height ncolors chars_per_pixel */
"7 7 4 1",
/* colors */
"  c #0000FF", // transparent
"! c #C0C0C0", // button (lignt grey)
"# c #808080", // dark grey
"- c #FFFFFF", // light
/* pixels */
"  ---  ",
"  -!#  ",
"---!#--",
"-!!!!!#",
"-##!###",
"  -!#  ",
"  -##  "
};

static char* tabhdrminus_xpm[] =
{
/* width height ncolors chars_per_pixel */
"7 7 4 1",
/* colors */
"  c #0000FF", // transparent
"! c #C0C0C0", // button (lignt grey)
"# c #808080", // dark grey
"- c #FFFFFF", // light
/* pixels */
"       ",
"       ",
"-------",
"-!!!!!#",
"#######",
"       ",
"       "
};

#ifndef QT_NO_ICONSET

class Q_EXPORT QIconFactoryPM: public QIconFactory
{
public:
    virtual QPixmap* createPixmap(QIconSet const& iconSet, QIconSet::Size size,
                                  QIconSet::Mode mode, QIconSet::State state);
};

QPixmap* QIconFactoryPM::createPixmap(QIconSet const& iconSet, QIconSet::Size size,
                                      QIconSet::Mode mode, QIconSet::State state)
{
    // we are only providing a way to "disable" icons
    if(QIconSet::Disabled != mode)
        return 0;

    // generate "normal" pixmap, and return 0 if this fails
    QPixmap normPixmap = iconSet.pixmap(size, QIconSet::Normal, state);
    if(normPixmap.isNull())
        return 0;

    // For PM/Warp style, "disabling" a pixmap means overimpose a raster
    // of transparency.
    // Note: if possible we realize this transparency raster with a mask;
    // only if the pixmap comes with an alpha channel already, we are making
    // use of that one

    QBitmap mask;
    QImage alphaImg;
    if(normPixmap.mask())
    {
        // if a mask exists, use it
        mask = *normPixmap.mask();
    }
    else if(normPixmap.hasAlphaChannel())
    {
        // if an alpha channel exists, use it
        alphaImg = normPixmap.convertToImage();
    }
    else
    {
        // if none exists, generate a mask and initialize it with "fully opaque)
        mask = QBitmap(normPixmap.size());
        mask.fill(Qt::color1);
    }

    QPixmap* pixmap = 0;
    int r, c;

    // alpha channel case
    if(mask.isNull())
    {
        if(32 != alphaImg.depth())
            alphaImg = alphaImg.convertDepth(32);
        for(r = 0; r < alphaImg.height(); ++r)
        {
            for(c = 0; c < alphaImg.width(); ++c);
            {
                if(0 ==((r + c) % 2))
                    alphaImg.setPixel(c, r, 0);
            }
        }
        pixmap = new QPixmap(alphaImg);
    }

    // mask case
    else
    {
        QImage img = mask.convertToImage();
        for(r = 0; r < img.height(); ++r)
        {
            int pix;
            for(c = 0; c < img.width(); ++c)
            {
                pix = img.pixelIndex(c, r);
                pix = ((0 == pix) || (0 == ((r + c) % 2))) ? 0 : 1;
                img.setPixel(c, r, pix);
            }
        }
        mask.convertFromImage(img);
        pixmap = new QPixmap(normPixmap);
        pixmap->setMask(mask);
    }

    return pixmap;
}

#endif // ifndef QT_NO_ICONSET

/*! \internal
  Helper function to make the "background" of an image transparent.
  this is done heuristically by assuming that if all corners have the
  same color, and if that color happens to be also the same as the
  system dialog background color, then this color is actually intended
  to be "transparent", and it is changed to that, starting at the
  four corners and as far as this color is continuously there (flood fill).
*/
static void qFloodFill(QImage& im, int x, int y, QRgb color)
{
    // note: this is a primitive, completely un-optimized flood fill,
    // but since 1) only rather small images are treated and 2) they are
    // even cached, the overhead does not seem to be excessive!

    // get current color and change to new color
    QRgb currentCol = im.pixel(x, y);
    im.setPixel(x, y, color);

    // recursive calls in all four directions, if color is right
    if((x > 0) && (im.pixel(x - 1, y) == currentCol))
        qFloodFill(im, x - 1, y, color);
    if((x < (im.width() - 1)) && (im.pixel(x + 1, y) == currentCol))
        qFloodFill(im, x + 1, y, color);
    if((y > 0) && (im.pixel(x, y - 1) == currentCol))
        qFloodFill(im, x, y - 1, color);
    if((y < (im.height() - 1)) && (im.pixel(x, y + 1) == currentCol))
        qFloodFill(im, x, y + 1, color);
}

static void qImageBackgroundTransparent(QImage& im)
{
    // if colors in all four corners are equal and equal to the system
    // standard dialog background color, we assume this color is meant to
    // be the "background" and change that to "transparent"
    // this makes radiobuttons look ok on other than a standard dialog
    // background, like inside a listview control
    int xm = im.width() - 1,
        ym = im.height() - 1;
    static QRgb dlgBackCol = qt_sysclr2qrgb(SYSCLR_DIALOGBACKGROUND) | 0xff000000;
    QRgb cornerCol = im.pixel(0, 0);
    if((cornerCol == dlgBackCol) &&
       (cornerCol == im.pixel(0, ym)) &&
       (cornerCol == im.pixel(xm, 0)) &&
       (cornerCol == im.pixel(xm, ym)))
    {
        im.setAlphaBuffer(true);
        qFloodFill(im, 0, 0, (QRgb)0);
        qFloodFill(im, 0, ym, (QRgb)0);
        qFloodFill(im, xm, 0, (QRgb)0);
        qFloodFill(im, xm, ym, (QRgb)0);
    }
}

/*----------------------------------------------------------------------*\
 * QWarpPixmap class
 * Describes a pixmap that is being read from the PM system
\*----------------------------------------------------------------------*/

/*! \internal
  Describes a pixmap that is being read from the PM system.
*/
class QWarpPixmap: public QPixmap
{
public:
    typedef enum
    {
        bmp,
        ico,
        internal
    }
    QWarpPixmapType;

    typedef enum
    {
        arrowup,
        arrowdown,
        arrowleft,
        arrowright,
        tabheadertr,
        tabheaderbr,
        tabheadertl,
        tabheaderbl,
        tabhdrplus,
        tabhdrminus,
        chkboxnormal,
        chkboxsunken,
        chkboxchecked,
        chkboxcheckedsunken,
        chkboxtri,
        chkboxtrisunken,
        radbutnormal,
        radbutsunken,
        radbutchecked,
        radbutcheckedsunken,
        checkmark
    }
    QWarpInternalPixmap;

    QWarpPixmap(unsigned long id, QWarpPixmapType type, long sz = -1);

    bool isOk() const { return ok; }

private:
    bool ok;
    static QMap<QString, QPixmap>* tabHeaderCorners;

    bool indicatorPixmap(bool isCheckbox, uint status, bool sunken);
    bool drawTabHeaderTopRight(int sz);
    bool drawTabHeaderBottomRight(int sz);
    bool drawTabHeaderTopLeft(int sz);
    bool drawTabHeaderBottomLeft(int sz);
};

QMap<QString, QPixmap>* QWarpPixmap::tabHeaderCorners = 0;

QWarpPixmap::QWarpPixmap(unsigned long id, QWarpPixmapType type, long sz)
    :   ok(false)
{
    QImage img;

    if(internal == type)
    {
        switch(id)
        {
            case arrowleft:
                img = QImage(arrowleft_xpm);
                break;
            case arrowright:
                img = QImage(arrowright_xpm);
                break;
            case arrowup:
                img = QImage(arrowup_xpm);
                break;
            case arrowdown:
                img = QImage(arrowdown_xpm);
                break;
            case tabheadertr:
                ok = drawTabHeaderTopRight(sz);
                break;
            case tabheaderbr:
                ok = drawTabHeaderBottomRight(sz);
                break;
            case tabheadertl:
                ok = drawTabHeaderTopLeft(sz);
                break;
            case tabheaderbl:
                ok = drawTabHeaderBottomLeft(sz);
                break;
            case tabhdrplus:
                img = QImage(tabhdrplus_xpm);
                break;
            case tabhdrminus:
                img = QImage(tabhdrminus_xpm);
                break;
            case chkboxnormal:
                ok = indicatorPixmap(true, 0, false);
                break;
            case chkboxsunken:
                ok = indicatorPixmap(true, 0, true);
                break;
            case chkboxchecked:
                ok = indicatorPixmap(true, 1, false);
                break;
            case chkboxcheckedsunken:
                ok = indicatorPixmap(true, 1, true);
                break;
            case chkboxtri:
                ok = indicatorPixmap(true, 2, false);
                break;
            case chkboxtrisunken:
                ok = indicatorPixmap(true, 2, true);
                break;
            case radbutnormal:
                ok = indicatorPixmap(false, 0, false);
                break;
            case radbutsunken:
                ok = indicatorPixmap(false, 0, true);
                break;
            case radbutchecked:
                ok = indicatorPixmap(false, 1, false);
                break;
            case radbutcheckedsunken:
                ok = indicatorPixmap(false, 1, true);
                break;
            case checkmark:
            default:
                img = QImage(checkmark_xpm);
                break;
        }

        img.setAlphaBuffer(true);

        QColorGroup const& cg = QApplication::palette().active();
        int col;
        for(col = 0; col < img.numColors(); ++col)
        {
            switch(img.color(col))
            {
                case 0xff0000ff: // transparent
                    img.setColor(col, QRgb(0x00000000));
                    break;
                case 0xffc0c0c0: // button (light grey)
                    img.setColor(col, QRgb(cg.button().rgb() | 0xff000000));
                    break;
                case 0xff808080: // dark grey
                    img.setColor(col, QRgb(cg.dark().rgb() | 0xff000000));
                    break;
                case 0xffffffff: // light
                    img.setColor(col, QRgb(cg.light().rgb() | 0xff000000));
                    break;
                case 0xff000000: // shadow
                    img.setColor(col, QRgb(cg.shadow().rgb() | 0xff000000));
                    break;
            }
        }
    }

    else
    {
        HBITMAP hbits = NULLHANDLE,
                hmask = NULLHANDLE;
        bool color = true;

        switch(type)
        {
            case bmp:
            {
                hbits = WinGetSysBitmap(HWND_DESKTOP, id);
                break;
            }

            default: // ico
            {
                HPOINTER ptr = WinQuerySysPointer(HWND_DESKTOP, id, FALSE);
                if(NULLHANDLE != ptr)
                {
                    POINTERINFO ptrInfo;
                    WinQueryPointerInfo(ptr, (PPOINTERINFO)&ptrInfo);
                    hmask = ptrInfo.hbmPointer;
                    color = NULLHANDLE != ptrInfo.hbmColor;
                    if(color)
                        hbits = ptrInfo.hbmColor;
                    else
                        hbits = ptrInfo.hbmPointer;
                }
                break;
            }
        }

        if(NULLHANDLE == hbits)
            return;

        BITMAPINFOHEADER2 bmpHdr;
        ::memset(&bmpHdr, 0, sizeof(bmpHdr));
        bmpHdr.cbFix = sizeof(bmpHdr);
        GpiQueryBitmapInfoHeader(hbits, (PBITMAPINFOHEADER2)&bmpHdr);

        BITMAPINFOHEADER2 maskHdr;
        if(NULLHANDLE != hmask)
        {
            ::memset(&maskHdr, 0, sizeof(maskHdr));
            maskHdr.cbFix = sizeof(maskHdr);
            GpiQueryBitmapInfoHeader(hmask, (PBITMAPINFOHEADER2)&maskHdr);
        }

        HAB hab = WinQueryAnchorBlock(HWND_DESKTOP);
        HDC hdc = DevOpenDC(hab, OD_MEMORY, "*", 0, NULL, NULLHANDLE);
        if(0 == hdc)
            return;

        SIZEL size;
        size.cx = bmpHdr.cx;
        size.cy = bmpHdr.cy;
        HPS hps = GpiCreatePS(hab, hdc, (PSIZEL)&size,
                              PU_PELS | GPIF_LONG | GPIT_NORMAL | GPIA_ASSOC);
        if(0 == hps)
            return;

        if((ico == type) && !color)
            size.cy /= 2;
        img = QImage(size.cx, size.cy, 32);

        unsigned long bmpInfoSize = sizeof(BITMAPINFO2) + 255 * sizeof(RGB2);
        BITMAPINFO2* bmpInfo = (BITMAPINFO2*)new BYTE[bmpInfoSize];
        int lBufSize = 4 * size.cx * size.cy;

        PBYTE mBuf = 0;
        if(NULLHANDLE != hmask)
        {
            GpiSetBitmap(hps, hmask);
            img.setAlphaBuffer(true);

            ::memset(bmpInfo, 0, bmpInfoSize);
            ::memcpy(bmpInfo, &maskHdr, sizeof(BITMAPINFOHEADER2));
            bmpInfo->cBitCount = 32; // note: this avoids padding at row ends!
            bmpInfo->ulCompression = BCA_UNCOMP;

            mBuf = new BYTE[lBufSize];
            ::memset(mBuf, 0, lBufSize);

            if(size.cy != GpiQueryBitmapBits(hps, size.cy, size.cy, mBuf, bmpInfo))
            {
                delete[] mBuf;
                delete[] (PBYTE)bmpInfo;
                return;
            }
        }

        GpiSetBitmap(hps, hbits);

        ::memset(bmpInfo, 0, bmpInfoSize);
        ::memcpy(bmpInfo, &bmpHdr, sizeof(BITMAPINFOHEADER2));
        bmpInfo->cBitCount = 32;
        bmpInfo->ulCompression = BCA_UNCOMP;

        PBYTE lBuf = new BYTE[lBufSize];
        ::memset(lBuf, 0, lBufSize);

        if(size.cy != GpiQueryBitmapBits(hps, 0, size.cy, lBuf, bmpInfo))
        {
            delete[] lBuf;
            delete[] mBuf;
            delete[] (PBYTE)bmpInfo;
            return;
        }

        int r, c;
        uint col;
        for(r = 0; r < size.cy; ++r)
        {
            for(c = 0; c < size.cx; ++c)
            {
                col = *((uint*)&lBuf[(r * size.cx + c) * 4]);
                uint alpha = 255;
                if(NULLHANDLE != hmask)
                    alpha -= (uint)(unsigned char)mBuf[(r * size.cx + c) * 4];
                col |= 0x01000000 * alpha;
                img.setPixel(c, size.cy - r - 1, col);
            }
        }

        delete[] mBuf;
        delete[] lBuf;
        delete[] (PBYTE)bmpInfo;
        GpiSetBitmap(hps, NULLHANDLE);
        GpiDestroyPS(hps);
        DevCloseDC(hdc);
    }

    if(!img.isNull())
    {
        // images that are to be made transparent from the corners, if
        // standard dialog background is the color there
        // note: there seems to be nothing like the STL set<> in Qt, which
        // would be more appropriate for the purpose here!
        static QValueList<int> tbl;
        if(tbl.empty())
        {
            tbl.push_back(52);
            tbl.push_back(54);
            tbl.push_back(56);
            tbl.push_back(SBMP_RESTOREBUTTON);
            tbl.push_back(SBMP_MINBUTTON);
            tbl.push_back(SBMP_MAXBUTTON);
        }
        if((bmp == type) && (tbl.end() != tbl.find(id)))
            qImageBackgroundTransparent(img);

        ok = convertFromImage(img);
    }
}

bool QWarpPixmap::indicatorPixmap(bool isCheckbox, uint status, bool sunken)
{
    QImage ii = QWarpPixmap(SBMP_CHECKBOXES, QWarpPixmap::bmp).convertToImage();

    int c = 0;
    if(sunken)
        c += 2;
    if(0 < status)
        ++c;
    int r = (isCheckbox ? 0 : 1);
    if(2 <= status)
        r = 2;

    unsigned long w = ii.width() / 4,
                  h = ii.height() / 3;
    QRect rect(0, 0, w, h);
    rect.moveBy(c * w, r * h);

    // cut out the right sub-image and make "background" transparent
    ii = ii.copy(rect);
    qImageBackgroundTransparent(ii);

    // convert to pixmap
    convertFromImage(ii);

    return true;
}

bool QWarpPixmap::drawTabHeaderTopRight(int sz)
{
    QColorGroup const& cg = QApplication::palette().active();

    resize(sz, sz);
    fill(cg.button());
    QPainter p(this);

    // short lines in the corners
    p.setPen(cg.dark());
    QPoint pt(0, 0);
    QPoint pt2(2, 0);
    p.drawLine(pt, pt2);
    pt2 += QPoint(1, 1);
    p.drawLine(pt2, pt += QPoint(5, 1));
    pt2 = pt + QPoint(sz - 7, sz - 7);
    p.drawLine(pt2 + QPoint(0, 1), pt2 + QPoint(0, 5));

    // corner
    p.drawLine(pt, QPoint(pt.x(), pt2.y()));
    p.drawLine(pt2, QPoint(pt.x(), pt2.y()));

    // corner shadow
    p.drawLine(pt.x() + 5, pt2.y() + 2, pt2.x() - 1, pt2.y() + 2);
    QPoint pd(pt.x() + 6, pt2.y() + 3);
    while(pd.x() < pt2.x())
    {
        p.drawPoint(pd);
        pd += QPoint(2,  0);
    }
    p.setPen(cg.shadow());
    p.drawLine(pt.x() + 5, pt2.y() + 1, pt2.x() - 1, pt2.y() + 1);

    // bend
    p.drawLine(pt += QPoint(1, 1), pt2);
    p.setPen(cg.dark());
    p.drawLine(pt + QPoint(2, 1), pt2 += QPoint(0, -1));
    p.drawLine(pt += QPoint(0, 1), pt2 += QPoint(-2, 0));
    p.drawLine(pt += QPoint(0, 1), pt2 += QPoint(-1, 0));
    p.setPen(cg.light());
    p.drawLine(pt + QPoint(0, 3), pt2 + QPoint(-3, 0));

    return true;
}

bool QWarpPixmap::drawTabHeaderBottomRight(int sz)
{
    QColorGroup const& cg = QApplication::palette().active();

    resize(sz, sz);
    fill(cg.button());
    QPainter p(this);

    // short lines in the corners
    p.setPen(cg.dark());
    QPoint pt(0, sz - 1);
    QPoint pt2(2, sz - 1);
    p.drawLine(pt, pt2);
    pt2 += QPoint(1, -1);
    p.drawLine(pt2, pt += QPoint(5, -1));
    pt2 = pt + QPoint(sz - 7, -sz + 7);
    p.drawLine(pt2 + QPoint(0, -1), pt2 + QPoint(0, -5));

    // corner
    p.drawLine(pt, QPoint(pt.x(), pt2.y()));
    p.drawLine(pt2, QPoint(pt.x(), pt2.y()));

    // bend
    p.setPen(cg.shadow());
    p.drawLine(pt += QPoint(1, -1), pt2);
    p.drawLine(pt += QPoint(1, 1), pt2 += QPoint(0, 2));
    p.setPen(cg.dark());
    p.drawLine(pt += QPoint(1, 0), pt2 += QPoint(0, 1));
    p.drawLine(pt += QPoint(-2, 0), pt2 += QPoint(0, -2));
    p.drawLine(pt += QPoint(0, -2), pt2 += QPoint(-2, 0));
    p.drawLine(pt += QPoint(0, -1), pt2 += QPoint(-1, 0));
    QPoint pd(pt + QPoint(5, 1));
    while(pd.x() < (pt2.x() + 2))
    {
        p.drawPoint(pd);
        pd += QPoint(2,  -2);
    }
    p.setPen(cg.light());
    p.drawLine(pt + QPoint(0, -3), pt2 + QPoint(-3, 0));

    return true;
}

bool QWarpPixmap::drawTabHeaderTopLeft(int sz)
{
    QColorGroup const& cg = QApplication::palette().active();

    resize(sz, sz);
    fill(cg.button());
    QPainter p(this);

    // short lines in the corners
    p.setPen(cg.dark());
    QPoint pt(sz - 1, 0);
    QPoint pt2(sz - 3, 0);
    p.drawLine(pt, pt2);
    pt2 += QPoint(-1, 1);
    p.drawLine(pt2, pt += QPoint(-5, 1));
    pt2 = pt + QPoint(7 - sz, sz - 7);
    p.drawLine(pt2 + QPoint(0, 1), pt2 + QPoint(0, 5));

    // corner
    p.drawLine(pt, QPoint(pt.x(), pt2.y()));
    p.drawLine(pt2, QPoint(pt.x(), pt2.y()));

    // corner shadow
    p.drawLine(pt.x() + 2, pt2.y() + 2, pt2.x() + 5, pt2.y() + 2);
    p.drawLine(pt.x() + 2, pt.y() + 5, pt.x() + 2, pt2.y() + 2);
    QPoint pd(pt2.x() + 6, pt2.y() + 3);
    while(pd.x() < (pt.x() + 3))
    {
        p.drawPoint(pd);
        pd += QPoint(2,  0);
    }
    pd -= QPoint(1, 1);
    while(pd.y() > (pt.y() + 5))
    {
        p.drawPoint(pd);
        pd += QPoint(0, -2);
    }
    p.setPen(cg.shadow());
    p.drawLine(pt.x() + 1, pt2.y() + 1, pt2.x() + 5, pt2.y() + 1);
    p.drawLine(pt.x() + 1, pt.y() + 5, pt.x() + 1, pt2.y() + 1);

    // bend
    p.drawLine(pt += QPoint(-1, 1), pt2 += QPoint(1, -1));
    p.setPen(cg.dark());
    p.drawLine(pt += QPoint(0, 1), pt2 += QPoint(1, 0));
    p.drawLine(pt += QPoint(0, 1), pt2 += QPoint(1, 0));
    p.setPen(cg.light());
    p.drawLine(pt + QPoint(0, 3), pt2 + QPoint(3, 0));

    return true;
}

bool QWarpPixmap::drawTabHeaderBottomLeft(int sz)
{
    QColorGroup const& cg = QApplication::palette().active();

    resize(sz, sz);
    fill(cg.button());
    QPainter p(this);

    // short lines in the corners
    p.setPen(cg.dark());
    QPoint pt(sz - 1, sz - 1);
    QPoint pt2(sz - 3, sz - 1);
    p.drawLine(pt, pt2);
    pt2 += QPoint(-1, -1);
    p.drawLine(pt2, pt += QPoint(-5, -1));
    pt2 = pt + QPoint(7 - sz, 7 - sz);
    p.drawLine(pt2 + QPoint(0, -1), pt2 + QPoint(0, -5));

    // corner
    p.drawLine(pt, QPoint(pt.x(), pt2.y()));
    p.drawLine(pt2, QPoint(pt.x(), pt2.y()));

    // corner shadow
    p.drawLine(pt.x() + 2, pt.y() - 1, pt.x() + 2, pt2.y() + 5);
    QPoint pd(pt.x() + 3, pt.y() - 2);
    while(pd.y() > (pt2.y() + 5))
    {
        p.drawPoint(pd);
        pd += QPoint(0, -2);
    }
    p.setPen(cg.shadow());
    p.drawLine(pt.x() + 1, pt.y() - 1, pt.x() + 1, pt2.y() + 5);

    // bend
    p.drawLine(pt += QPoint(-1, -1), pt2 += QPoint(1, 1));
    p.setPen(cg.dark());
    p.drawLine(pt += QPoint(0, 1), pt2 += QPoint(-1, 0));
    p.drawLine(pt += QPoint(0, -2), pt2 += QPoint(2, 0));
    p.drawLine(pt += QPoint(0, -1), pt2 += QPoint(1, 0));
    p.setPen(cg.light());
    p.drawLine(pt + QPoint(0, -3), pt2 + QPoint(3, 0));

    return true;
}

/*----------------------------------------------------------------------*\
 * QWarpSystemPixmaps class
\*----------------------------------------------------------------------*/

/*! \internal
  Retrieve and buffer PM system pixmaps.
*/
class QWarpSystemPixmaps
{
public:
    QWarpSystemPixmaps() {}
    ~QWarpSystemPixmaps();
    QPixmap const* getPixmap(unsigned long id, QWarpPixmap::QWarpPixmapType type,
                             long sz = -1);

private:
    QMap<QString, QWarpPixmap*> pmr;
};

QWarpSystemPixmaps::~QWarpSystemPixmaps()
{
    QMap<QString, QWarpPixmap*>::iterator it;
    for(it = pmr.begin(); it != pmr.end(); ++it)
        delete *it;
}

QPixmap const* QWarpSystemPixmaps::getPixmap(unsigned long id,
    QWarpPixmap::QWarpPixmapType type,
    long sz /*= -1*/)
{
    QWarpPixmap* pix = 0;

    QString key = QString("%1.%2.%3")
        .arg((unsigned long)type, 8, 16)
        .arg(id, 8, 16)
        .arg(sz, 8, 16)
        .replace(' ', '0');

    QMap<QString, QWarpPixmap*>::iterator pix_it = pmr.find(key);
    if(pmr.end() != pix_it)
    {
        pix = *pix_it;
    }
    else
    {
        pix = new QWarpPixmap(id, type, sz);
        if(pix->isOk())
        {
            pmr.insert(key, pix);
        }
        else
        {
            delete pix;
            pix = 0;
        }
    }

    return (QPixmap const*)pix;
}

static QWarpSystemPixmaps qWarpSystemPixmaps;

/*----------------------------------------------------------------------*\
 * functions for drawing PM frames and pushbuttons
\*----------------------------------------------------------------------*/

static
void qDrawWarpDefFrame(QPainter* p, QRect const& rect,
                       QColorGroup const& cg, bool def = FALSE)
{
    QRect rr(rect);

    // outer frame of button that shows "default" state, but is unchanged
    // if "sunken"

    if(def)
    {
        p->setPen(cg.shadow());
        p->drawRect(rr);
    }

    else
    {
        p->setPen(cg.dark());
        p->drawLine(rr.left(), rr.top(), rr.right() - 1, rr.top());
        p->drawLine(rr.left(), rr.top(), rr.left(), rr.bottom() - 1);
        p->setPen(cg.light());
        p->drawLine(rr.left() + 1, rr.bottom(), rr.right(), rr.bottom());
        p->drawLine(rr.right(), rr.top() + 1, rr.right(), rr.bottom());
    }
}

static
void qDrawWarpFilledRect(QPainter* p, QRect const& rect,
                         QColor dark, QColor light, QBrush const* fill,
                         unsigned long mult, bool sunken = FALSE)
{
    QRect rr(rect);

    // inner frame of button that shows "sunken" state, but is
    // unchanged if "default"

    unsigned long l;
    for(l = 0; l < mult; ++l)
    {
        p->setPen(sunken ? dark : light);
        p->drawLine(rr.left(), rr.top(), rr.right() - 1, rr.top());
        p->drawLine(rr.left(), rr.top(), rr.left(), rr.bottom());
        p->setPen(sunken ? light : dark);
        p->drawLine(rr.left() + 1, rr.bottom(), rr.right(), rr.bottom());
        p->drawLine(rr.right(), rr.top(), rr.right(), rr.bottom());
        rr.addCoords(1, 1, -1, -1);
    }

    if(0 != fill)
    {
        p->fillRect(rr, *fill);
    }
}

static
void qDrawWarpPanel(QPainter* p, QRect const& rect,
                    QColorGroup const& cg, int sunkenState = 1,
                    QBrush const* fill = 0)
                    // sunkenState: -1 = sunken
                    //               0 = flat
                    //               1 = raised
{
    QRect rr(rect);

    if(0 == sunkenState) // flat
    {
        rr.addCoords(1, 1, 0, 0);
        p->setPen(cg.light());
        p->drawRect(rr);
        rr.addCoords(-1, -1, -1, -1);
        p->setPen(cg.dark());
        p->drawRect(rr);
    }

    else if(0 > sunkenState) // sunken
    {
        qDrawWarpFilledRect(p, rect, cg.dark(), cg.light(), 0, 1, true);
        rr.addCoords(1, 1, -1, -1);
        qDrawWarpFilledRect(p, rr, cg.shadow(), cg.button(), fill, 1, true);
    }

    else // raised
    {
        qDrawWarpFilledRect(p, rect, cg.shadow(), cg.dark(), 0, 1, false);
        rr.addCoords(1, 1, -1, -1);
        qDrawWarpFilledRect(p, rr, cg.dark(), cg.light(), fill, 1, false);
    }
}

static
void qDrawTabBar(QPainter* p, QRect const& r,
                 QColorGroup const& cg, QColor color,
                 bool selected, bool bottom)
{
    QPointArray ptDark, ptColor, ptLight;

    if(selected)
    {
        ptDark.putPoints(0, 4,
                         r.right() - 9,  r.top(),
                         r.right() - 5,  r.top() + 4,
                         r.right(),      r.bottom() - 5,
                         r.right() - 2,  r.bottom() - 5);
        ptColor.putPoints(0, 11,
                          r.left(),       r.bottom() - 4,
                          r.left() + 6,   r.top() + 2,
                          r.left() + 8,   r.top() + 1,
                          r.right() - 9,  r.top() + 1,
                          r.right() - 8,  r.top() + 4,
                          r.right() - 4,  r.bottom() - 9,
                          r.right() - 2,  r.bottom() - 4,
                          r.right() - 3,  r.bottom() - 2,
                          r.right() - 5,  r.bottom(),
                          r.left() + 4,   r.bottom(),
                          r.left() + 2,   r.bottom() - 2);
        ptLight.putPoints(0, 4,
                          r.left(),       r.bottom() - 5,
                          r.left() + 6,   r.top() + 2,
                          r.left() + 8,   r.top(),
                          r.right() - 10, r.top());
    }
    else
    {
        ptDark.putPoints(0, 5,
                         r.right() - 9,  r.top() + 1,
                         r.right() - 5,  r.top() + 5,
                         r.right() - 1,  r.bottom() - 7,
                         r.right() - 1,  r.bottom() - 5,
                         r.right() - 3,  r.bottom() - 5);
        ptColor.putPoints(0, 7,
                          r.left() + 1,   r.bottom() - 5,
                          r.left() + 6,   r.top() + 4,
                          r.left() + 8,   r.top() + 2,
                          r.right() - 9,  r.top() + 2,
                          r.right() - 8,  r.top() + 5,
                          r.right() - 5,  r.bottom() - 8,
                          r.right() - 3,  r.bottom() - 5);
        ptLight.putPoints(0, 8,
                          r.right(),      r.bottom() - 4,
                          r.left(),       r.bottom() - 4,
                          r.left() + 1,   r.bottom() - 4,
                          r.left() + 1,   r.bottom() - 7,
                          r.left() + 5,   r.top() + 5,
                          r.left() + 6,   r.top() + 3,
                          r.left() + 8,   r.top() + 1,
                          r.right() - 10, r.top() + 1);
    }

    if(bottom)
    {
        unsigned long n;
        for(n = 0; n < ptDark.size(); ++n)
        {
            QPoint pt = ptDark.point(n);
            pt.setY(r.bottom() - pt.y());
            ptDark.setPoint(n, pt);
        }
        for(n = 0; n < ptColor.size(); ++n)
        {
            QPoint pt = ptColor.point(n);
            pt.setY(r.bottom() - pt.y());
            ptColor.setPoint(n, pt);
        }
        for(n = 0; n < ptLight.size(); ++n)
        {
            QPoint pt = ptLight.point(n);
            pt.setY(r.bottom() - pt.y());
            ptLight.setPoint(n, pt);
        }
    }

    p->setBrush(cg.dark());
    p->setPen(QPen::NoPen);
    p->drawPolygon(ptDark);
    p->setBrush(color);
    p->drawPolygon(ptColor);
    p->setPen(cg.light());
    p->drawPolyline(ptLight);

    // if bottom orientation, some "light" lines need to be "dark"
    if(bottom)
    {
        p->setPen(cg.dark());
        p->drawLine(ptLight.point(ptLight.size() - 2), ptLight.point(ptLight.size() - 1));
        if(!selected)
            p->drawLine(ptLight.point(0), ptLight.point(1));
    }
}

// Note: This function is being copied 1:1 from QCommonStyle.cpp, only
// because we need to rewrite the code for drawing slider tick marks in
// two colors instead of only one in the "common" style! But the makers
// of Qt decided not to expose it or export it otherwise...

#ifndef QT_NO_RANGECONTROL
//  I really need this and I don't want to expose it in QRangeControl..
static int qPositionFromValue(QRangeControl const* rc, int logical_val,
                              int span)
{
    if(span <= 0 ||
       logical_val < rc->minValue() ||
       rc->maxValue() <= rc->minValue())
        return 0;
    if(logical_val > rc->maxValue())
        return span;

    uint range = rc->maxValue() - rc->minValue();
    uint p = logical_val - rc->minValue();

    if(range > (uint)INT_MAX/4096)
    {
        const int scale = 4096 * 2;
        return ((p / scale) * span) / (range / scale);
        // ### the above line is probably not 100% correct
        // ### but fixing it isn't worth the extreme pain...
    }
    else if(range > (uint)span)
    {
        return (2 * p * span + range) / (2 * range);
    }
    else
    {
        uint div = span / range;
        uint mod = span % range;
        return p * div + (2 * p * mod + range) / (2 * range);
    }
    // equiv. to (p * span) / range + 0.5
    // no overflow because of this implicit assumption:
    // span <= 4096
}
#endif // QT_NO_RANGECONTROL

/*----------------------------------------------------------------------*\
 * QWarpTabPanelHeader class
\*----------------------------------------------------------------------*/

/*! \internal
  Panel header class for tab widget display and functionality in PM style
  (the "new" Warp4 style with the colored tabs...).

  This is the parent/child setup of a QTabWidget:
  class QTabWidget
    class <QWidgetStack>, name <tab pages> -> panel(s) area
      class <QWidgetStackPrivate::Invisible>, name <qt_invisible_widgetstack>
      class <...panel...>
        class <QLabel>, name <unnamed>
        ...
    class <QWidget>, name <tab base> -> tab bar base area
    class <QTabBar>, name <tab control> -> tab bar
      class <QToolButton>, name <qt_left_btn> -> left tab bar button
      class <QToolButton>, name <qt_right_btn> -> right tab bar button
    class <QWarpTabPanelHeader>, name <qt_tabwidget_warpheader>
      -> this is the panel header widget that is added below, covering the
         tab bar base area
*/
class QWarpTabPanelHeader: public QWidget
{
    Q_OBJECT

#ifndef QT_PM_NO_TABWIDGET_HEADERS

public:

    QWarpTabPanelHeader (QTabWidget* tw);

protected:

    virtual bool eventFilter (QObject* watched, QEvent* ev);
    virtual void paintEvent (QPaintEvent* pev);
    virtual void mouseReleaseEvent (QMouseEvent* e);

private:

    bool _bottom;

    QTabWidget *_tabWidget;
    QWidgetStack *_widgetStack;
    QTabBar *_tabBar;
    QWidget *_tabBarBase;

    void setupLayout (QRect rect);

    static void drawBentCornerTR (QPoint pt, int h, QPainter& p);
    static void drawBentCornerTL (QPoint pt, int h, QPainter& p);
    static void drawBentCornerBR (QPoint pt, int h, QPainter& p);
    static void drawBentCornerBL (QPoint pt, int h, QPainter& p);
    static void drawPlusButton (QPoint pt, QPainter& p);
    static void drawMinusButton (QPoint pt, QPainter& p);

    int currentInx();

#endif // QT_PM_NO_TABWIDGET_HEADERS
};

#ifndef QT_PM_NO_TABWIDGET_HEADERS

QWarpTabPanelHeader::QWarpTabPanelHeader (QTabWidget *tw)
    : QWidget (tw, "qt_warp_tabpanel_header")
    , _bottom (false), _tabWidget (0), _widgetStack (0)
    , _tabBar (0), _tabBarBase (0)
{
    Q_ASSERT (tw);
    if (tw == 0)
        return;

    _bottom = tw->tabPosition() == QTabWidget::Bottom;

    _tabWidget = tw;

    _widgetStack = (QWidgetStack *) tw->child ("tab pages", "QWidgetStack", false);
    Q_ASSERT (_widgetStack != 0);
    if (_widgetStack == 0)
        return;

    _tabBar = (QTabBar *) tw->child (NULL, "QTabBar", false);
    Q_ASSERT (_tabBar != 0);
    if (_tabBar == 0)
        return;

    _tabBarBase = (QWidget *) tw->child ("tab base", "QWidget", false);
    Q_ASSERT (_tabBarBase != 0);
    if (_tabBarBase == 0)
        return;

    setupLayout (_tabBarBase->geometry());

    // catch resize/move events from the tab bar base widget in order to adapt
    // the size of this panel header widget accordingly

    _tabBarBase->installEventFilter (this);

    // make this visible: it displays the tab title
    show();
}

bool QWarpTabPanelHeader::eventFilter (QObject *watched, QEvent *ev)
{
    if ((ev->type() != QEvent::Resize && ev->type() != QEvent::Move))
        return false;

    // adapt the panel header widget position if the tab bar base widget is
    // being resized/moved
    setupLayout (((QWidget *) watched)->geometry());

    return false;
}

void QWarpTabPanelHeader::paintEvent(QPaintEvent* pev)
{
    if (_tabWidget == 0)
        return; // should never happen

    // draw the caption of the page in the tab bar base area

    QRect r (rect());
    QPainter p(this);
    bool reverse = QApplication::reverseLayout();

    QFont f = _tabWidget->font();
    f.setBold (true);
    p.setFont (f);
    QRect fr = QFontMetrics(f).boundingRect (caption());

    int x = (reverse ?
             r.right() - 5 - QFontMetrics(f).boundingRect(caption()).width() :
             r.left() + 5),
        y = (r.top() + r.bottom() + fr.height()) / 2 - 2;

    QString cap = caption();
    cap.remove (cap.find ('&'), 1);
    p.drawText (x, y, cap);

    QPoint pt1l, pt1r, pt2l, pt2r;
    int hh = r.height();
    if (_bottom)
    {
        pt1l = QPoint (1, r.bottom());
        pt1r = QPoint (r.width() - hh, r.bottom());
        pt2l = QPoint (5, r.top() + 1);
        pt2r = QPoint (r.width() - hh, r.top() + 1);
    }
    else
    {
        pt1l = QPoint (1, r.top());
        pt1r = QPoint (r.width() - hh, r.top());
        pt2l = QPoint (5, hh - 3);
        pt2r = QPoint (r.width() - hh, hh - 3);
    }
    if (reverse)
    {
        pt1l.setX (r.right() - pt1l.x());
        pt1r.setX (r.right() - pt1r.x());
        pt2l.setX (r.right() - pt2l.x());
        pt2r.setX (r.right() - pt2r.x());
    }

    const QColorGroup &cg = colorGroup();
    p.setPen (cg.dark());
    p.drawLine (pt1l, pt1r);
    p.drawLine (pt2l, pt2r);
    p.setPen (cg.light());
    ++ pt2l.ry();
    ++ pt2r.ry();
    p.drawLine (pt2l, pt2r);

    if (_tabBar == 0)
        return; // should never happen

    int inx = currentInx();
    bool noPlus = inx >= _tabBar->count() - 1,
         noMinus = inx <= 0;

    QPoint cpt (pt1r);
    if (_bottom)
    {
        if (reverse)
        {
            cpt += QPoint (-hh, -hh + 1);
            drawBentCornerBL (cpt, hh, p);
            if (!noPlus)
                drawPlusButton (cpt + QPoint (5, hh - 12), p);
            if (!noMinus)
                drawMinusButton (cpt + QPoint (hh - 15, 7), p);
        }
        else
        {
            cpt += QPoint (1, -hh + 1);
            drawBentCornerBR (cpt, hh, p);
            if (!noPlus)
                drawPlusButton (cpt + QPoint (hh - 12, hh - 12), p);
            if (!noMinus)
                drawMinusButton (cpt + QPoint (8, 7), p);
        }
    }
    else
    {
        if(reverse)
        {
            cpt += QPoint (-hh, 0);
            drawBentCornerTL (cpt, hh, p);
            if (!noPlus)
                drawPlusButton (cpt + QPoint (5, 5), p);
            if (!noMinus)
                drawMinusButton (cpt + QPoint (hh - 15, hh - 14), p);
        }
        else
        {
            cpt += QPoint (1, 0);
            drawBentCornerTR (cpt, hh, p);
            if (!noPlus)
                drawPlusButton (cpt + QPoint (hh - 12, 5), p);
            if (!noMinus)
                drawMinusButton (cpt + QPoint (8, hh - 14), p);
        }
    }
}

void QWarpTabPanelHeader::mouseReleaseEvent(QMouseEvent* e)
{
    QPoint p = e->pos();
    bool reverse = QApplication::reverseLayout(),
         corner = false;
    if(reverse)
        corner = p.x() < height();
    else
        corner = p.x() > (width() - height());
    if(!corner)
        return; // the click was outside the bent corner area

    // calculate sum of x and y coordinates, measured from the corner
    // where the + button is
    long sum = 0;
    if(_bottom)
    {
        if(reverse)
            sum = p.x() + height() - p.y();
        else
            sum = width() - p.x() + height() - p.y();
    }
    else
    {
        if(reverse)
            sum = p.x() + p.y();
        else
            sum = width() - p.x() + p.y();
    }

    // if this sum is less than the widget height, we are in the
    // + triangle of the bent corner, otherwise in the - triangle
    int inx = currentInx();
    if(height() > sum)
        ++inx;
    else
        --inx;

    if(_tabBar == 0)
        return; // should never happen

    if (inx >= 0 && _tabBar->count() > inx)
        _tabBar->setCurrentTab (_tabBar->tabAt (inx)->identifier());
}

void QWarpTabPanelHeader::drawBentCornerTR(QPoint pt, int h, QPainter& p)
{
    p.drawPixmap (pt, *qWarpSystemPixmaps.getPixmap (QWarpPixmap::tabheadertr,
                                                     QWarpPixmap::internal, h));
}

void QWarpTabPanelHeader::drawBentCornerTL(QPoint pt, int h, QPainter& p)
{
    p.drawPixmap (pt, *qWarpSystemPixmaps.getPixmap (QWarpPixmap::tabheadertl,
                                                     QWarpPixmap::internal, h));
}

void QWarpTabPanelHeader::drawBentCornerBR(QPoint pt, int h, QPainter& p)
{
    p.drawPixmap (pt, *qWarpSystemPixmaps.getPixmap (QWarpPixmap::tabheaderbr,
                                                     QWarpPixmap::internal, h));
}

void QWarpTabPanelHeader::drawBentCornerBL(QPoint pt, int h, QPainter& p)
{
    p.drawPixmap (pt, *qWarpSystemPixmaps.getPixmap (QWarpPixmap::tabheaderbl,
                                                     QWarpPixmap::internal, h));
}

void QWarpTabPanelHeader::drawPlusButton(QPoint pt, QPainter& p)
{
    p.drawPixmap (pt, *qWarpSystemPixmaps.getPixmap (QWarpPixmap::tabhdrplus,
                                                     QWarpPixmap::internal));
}

void QWarpTabPanelHeader::drawMinusButton(QPoint pt, QPainter& p)
{
    p.drawPixmap (pt, *qWarpSystemPixmaps.getPixmap (QWarpPixmap::tabhdrminus,
                                                     QWarpPixmap::internal));
}

int QWarpTabPanelHeader::currentInx()
{
    if (_tabBar == 0)
        return 0; // should never happen
    int id = _tabBar->currentTab();

    return (id == 0) ? 0 : _tabBar->indexOf (id);
}

void QWarpTabPanelHeader::setupLayout (QRect rect)
{
    QRect r (rect);
    bool reverse = QApplication::reverseLayout();
    if (_bottom)
    {
        if (reverse)
            r.addCoords (14, 0, -12, -13);
        else
            r.addCoords (12, 0, -14, -13);
    }
    else
    {
        if (reverse)
            r.addCoords (14, 13, -12, 0);
        else
            r.addCoords (12, 13, -14, 0);
    }

    setGeometry (r);
}

#endif // QT_PM_NO_TABWIDGET_HEADERS

/*----------------------------------------------------------------------*\
 * QWarpMainWindowLayouter class
\*----------------------------------------------------------------------*/

/*! \internal
  A widget that catches resize events for the main window and corrects
  the layout if necessary.
*/
class QWarpMainWindowLayouter: public QWidget
{
    Q_OBJECT

public:
    QWarpMainWindowLayouter(QMainWindow* mw);

protected:
    virtual bool eventFilter(QObject* watched, QEvent* ev);
    virtual bool event(QEvent* e);

private:
    QMainWindow* getMainWindow();

    void setupLayout();

public:
    static void addIfNotPresent(QMainWindow* mw);
};

QWarpMainWindowLayouter::QWarpMainWindowLayouter(QMainWindow* mw)
:   QWidget(mw, "qt_warp_mainwindow_layouter")
{
    // we need to catch resize events from the main window, in order to adapt
    // the size of the client area to really cover the top and bottom dock
    // areas if they are empty

    mw->installEventFilter(this);
    setupLayout();
}

bool QWarpMainWindowLayouter::eventFilter(QObject* watched, QEvent* ev)
{
    if(!watched->inherits("QMainWindow") ||
       (QEvent::Resize != ev->type()))
        return false;

    // we need to adapt the client area if the main window is being resized,
    // but only after the layouter has done its work, so we have to delay the
    // action by posting an event to ourselves

    QApplication::postEvent(this, new QEvent((QEvent::Type)(QEvent::User + 2318)));

    return false;
}

bool QWarpMainWindowLayouter::event(QEvent* e)
{
    if(((QEvent::Type)(QEvent::User + 2318)) == e->type())
        setupLayout();

    return QWidget::event(e);
}

QMainWindow* QWarpMainWindowLayouter::getMainWindow()
{
    QWidget* ww = parentWidget();
    if((0 == ww) || !ww->inherits("QMainWindow"))
        return 0;

    return (QMainWindow*)ww;
}

void QWarpMainWindowLayouter::setupLayout()
{
    QMainWindow* mw = getMainWindow();
    if(0 == mw)
        return;

    QWidget* cw = mw->centralWidget();
    if(0 == cw)
        return;

    QDockArea* td = mw->topDock();
    QDockArea* bd = mw->bottomDock();
    QRect gg = cw->geometry();
    QRect gg2(gg);

    if((0 != td) && (1 == td->height()))
        gg2.setTop(td->geometry().top());

    if((0 != bd) && (1 == bd->height()))
        gg2.setBottom(bd->geometry().bottom());

    if(gg != gg2)
        cw->setGeometry(gg2);
}

void QWarpMainWindowLayouter::addIfNotPresent(QMainWindow* mw)
{
    if(0 != mw->child("qt_warp_mainwindow_layouter", "QWarpMainWindowLayouter", false))
        return; // layouter already there

    // create if not yet present, with the main window as parent
    // note: deletion occurs when the main window is destroyed

    new QWarpMainWindowLayouter(mw);
}

/*! \internal
  Global function for determining whether a popup menu has submenus.

  Note: this function is "ugly" because it is called for every menu
  item once, while the popup widget in turn scans through the menu
  items - but there is no other way to correct the problem with having
  at the same time an accelerator key (string after \t (tab)) and a
  submenu arrow displayed!

  btw., the Windows style solves that problem by always having some space left
  for the arrow, and Motiv overpaints the arrow over the accelerator string,
  but native PM handles the case in a flexible and correct way!...
*/
static
bool qPopupMenuHasSub(QPopupMenu const* pm)
{
    bool hasSub = false;
    unsigned long ix;
    for(ix = 0; ix < pm->count(); ++ix)
    {
        QMenuItem const* mi = pm->findItem(pm->idAt(ix));
        hasSub = hasSub || (0 != mi->popup());
    }

    return hasSub;
}

/*----------------------------------------------------------------------*\
 * Auxiliary functions
\*----------------------------------------------------------------------*/

/*! \internal
  Debug log of widget hierarchy.
*/
#if 0
static void qDebugLog(QWidget* widget,
                      bool withChildren = false,
                      unsigned long indent = 0)
{
    qDebug("%*sclass <%s>, name <%s>", (int)indent, "", widget->className(), widget->name());
    QRect r(widget->geometry());
    qDebug("%*sgeom  (%d, %d) - (%d, %d) (size = %d, %d)", (int)indent, "", r.left(), r.top(), r.right(), r.bottom(), r.width(), r.height());
    r = widget->frameGeometry();
    qDebug("%*sframe (%d, %d) - (%d, %d)", (int)indent, "", r.left(), r.top(), r.right(), r.bottom());

    if(withChildren)
    {
        QObjectList* wl = widget->queryList("QWidget", 0, false, false);
        QObjectListIterator wlit(*wl);
        QWidget* cw;
        while((cw = (QWidget*)wlit.current()) != 0)
        {
            ++wlit;
            qDebugLog(cw, true, indent + 2);
        }
        delete wl;
    }
}
#endif

/*----------------------------------------------------------------------*\
 * QWarp4Style class
\*----------------------------------------------------------------------*/

/*!
    \class QWarp4Style qwarp4style.h
    \brief The QWarp4Style class provides an OS/2 Warp 4 like look and feel.

    \ingroup appearance

    This style is Qt's default GUI style on OS/2 (or eComStation).
*/

/*!
    Constructs a QWarp4Style
*/
QWarp4Style::QWarp4Style()
    : QCommonStyle()
    , oldDefIconFactory(0)
{
    pmDefIconFactory = new QIconFactoryPM();
}

/*! \reimp */
QWarp4Style::~QWarp4Style()
{
    delete pmDefIconFactory;
}

/*! \reimp */
void QWarp4Style::polish (QApplication *app)
{
#ifndef QT_NO_ICONSET
    // replace the "disabled" creation mechanism for pixmaps
    // and do it "the Warp way"
    oldDefIconFactory = QIconFactory::defaultFactory();
    bool oldAutoDelete = oldDefIconFactory->autoDelete();
    // prevent the old factory from being deleted when replacing
    oldDefIconFactory->setAutoDelete(FALSE);
    QIconFactory::installDefaultFactory(pmDefIconFactory);
    oldDefIconFactory->setAutoDelete(oldAutoDelete);
#endif
}

/*! \reimp */
void QWarp4Style::unPolish (QApplication *)
{
#ifndef QT_NO_ICONSET
    // restore the original default iconfactory (unless it has been changed
    // externally after we were polished)
    if (pmDefIconFactory == QIconFactory::defaultFactory())
    {
        QIconFactory::installDefaultFactory(oldDefIconFactory);
    }
    else
    {
        // We are now responsible for deleting the old factory.
        // Note: the QIconFactory reference management in Qt is unreliable
        // (see qiconset.cpp, qcleanuphandler.h) so even after our attempt to
        // be tidy there is still a zillion cases for memory leaks or freed
        // memory access. I'm not going to fix Qt in this part.
        if (oldDefIconFactory->autoDelete())
            delete oldDefIconFactory;
    }
#endif
}

/*! \reimp */
void QWarp4Style::polish (QWidget *widget)
{
    if (widget != 0 && widget->inherits ("QTabWidget"))
    {
        QTabWidget *tw = (QTabWidget *) widget;

#ifndef QT_PM_NO_TABWIDGET_HEADERS
        // create the tab panel header widget
        new QWarpTabPanelHeader (tw);
#endif

        // reduce the widget stack's frame rect to get space for drawing
        // PE_PanelTabWidget. Hackish, but there seems to be no any official way
        // in the QStyle framework to change the tab the width of the frame
        // drawn by PE_PanelTabWidget (QFrame::TabWidgetPanel style).
        QWidgetStack *ws = (QWidgetStack *) tw->child ("tab pages",
                                                       "QWidgetStack", false);
        Q_ASSERT (ws);
        if (ws)
        {
            QRect r = ws->rect();
#ifndef QT_PM_NO_TABWIDGET_HEADERS
            bool reverse = QApplication::reverseLayout();
            if (reverse)
            {
                if (tw->tabPosition() == QTabWidget::Bottom)
                    r.addCoords (15, 12, -12, 0);
                else
                    r.addCoords (15, 0, -12, -12);
            }
            else
            {
                if (tw->tabPosition() == QTabWidget::Bottom)
                    r.addCoords (12, 12, -15, 0);
                else
                    r.addCoords (12, 0, -15, -12);
            }
#else
            if (tw->tabPosition() == QTabWidget::Bottom)
                r.addCoords (12, 12, -12, 0);
            else
                r.addCoords (12, 0, -12, -12);
#endif
            ws->setFrameRect (r);
        }
    }

    QCommonStyle::polish (widget);
}

/*! \reimp */
void QWarp4Style::unPolish (QWidget *widget)
{
    if (widget != 0 && widget->inherits ("QTabWidget"))
    {
        QTabWidget *tw = (QTabWidget *) widget;

        // restore the widget stack's frame rect
        QWidgetStack *ws = (QWidgetStack *) tw->child ("tab pages",
                                                       "QWidgetStack", false);
        Q_ASSERT (ws);
        if (ws)
            ws->setFrameRect (QRect());

#ifndef QT_PM_NO_TABWIDGET_HEADERS
        // delete the tab panel header widget
        QWarpTabPanelHeader* tph =
            (QWarpTabPanelHeader *) tw->child ("qt_warp_tabpanel_header",
                                               "QWarpTabPanelHeader", false);
        Q_ASSERT (tph);
        if (tph != 0)
            delete tph;
#endif
    }

    QCommonStyle::unPolish (widget);
}

/*! \reimp */
void QWarp4Style::polish (QPalette &pal)
{
    QCommonStyle::polish (pal);
}

/*! \reimp */
void QWarp4Style::drawPrimitive (PrimitiveElement pe,
                                 QPainter *p,
                                 const QRect &r,
                                 const QColorGroup &cg,
                                 SFlags flags,
                                 const QStyleOption &opt) const
{
    QRect rr (r);

    switch (pe)
    {
        case PE_ButtonCommand:
        case PE_ButtonTool:
        {
            bool def = flags & Style_ButtonDefault;
            qDrawWarpDefFrame (p, rr, cg, def);
            rr.addCoords (1, 1, -1, -1);

            bool sunken = flags & (Style_Sunken | Style_Down | Style_On);
            QBrush fill = cg.brush (QColorGroup::Button);
            qDrawWarpFilledRect (p, rr, cg.dark(), cg.light(), &fill, 2, sunken);
            break;
        }

        case PE_HeaderArrow:
        {
            p->save();
            while(0 != (rr.width() % 4))
                rr.setWidth(rr.width() - 1);
            rr.moveTop(rr.top() + rr.height() / 2 - rr.width() / 4 - 1);
            rr.setHeight(rr.width() / 2 + 1);
            rr.setWidth(rr.width() + 1);
            QPointArray pa(3);
            if(flags & Style_Down)
            {
                pa.setPoint(0, rr.left() + rr.width() / 2, rr.bottom());
                pa.setPoint(1, rr.left(), rr.top());
                pa.setPoint(2, rr.right(), rr.top());
            }
            else
            {
                pa.setPoint(0, rr.left(), rr.bottom());
                pa.setPoint(1, rr.right(), rr.bottom());
                pa.setPoint(2, rr.left() + rr.width() / 2, rr.top());
            }
            p->setPen(Qt::NoPen);
            p->setBrush(cg.shadow());
            p->drawPolygon(pa);
            p->restore();
            break;
        }

        case PE_ButtonBevel:
        case PE_HeaderSection:
        {
            QBrush fill;

            if(!(flags & Style_Down) && (flags & Style_On))
                fill = QBrush(cg.light(), Dense4Pattern);
            else
                fill = cg.brush(QColorGroup::Button);

            if(flags & (Style_Raised | Style_Down | Style_On | Style_Sunken))
                qDrawWarpFilledRect(p, r, cg.dark(), cg.light(), &fill, 2,
                                    flags & (Style_Down | Style_On));
            else
                p->fillRect(r, fill);

            break;
        }

        case PE_ButtonDefault:
        {
            p->setPen(cg.shadow());
            p->drawRect(r);

            break;
        }

        case PE_Separator:
        {
            p->setPen(cg.dark());
            rr.addCoords(0, 0, 0, -1);
            p->drawLine(rr.bottomLeft(), rr.bottomRight());
            break;
        }

        case PE_FocusRect:
        {
            if(opt.isDefault())
                p->drawWinFocusRect(r);
            else
                p->drawWinFocusRect(r, opt.color());

            break;
        }

        case PE_Indicator:
        case PE_CheckListIndicator:
        case PE_ExclusiveIndicator:
        case PE_CheckListExclusiveIndicator:
        {
            bool sunken = flags & (Style_Sunken | Style_Down);
            QWarpPixmap::QWarpInternalPixmap type;
            switch(pe)
            {
                case PE_Indicator:
                case PE_CheckListIndicator:
                {
                    if(flags & Style_On)
                    {
                        if(sunken)
                            type = QWarpPixmap::chkboxcheckedsunken;
                        else
                            type = QWarpPixmap::chkboxchecked;
                    }
                    else if(flags & Style_NoChange)
                    {
                        if(sunken)
                            type = QWarpPixmap::chkboxtrisunken;
                        else
                            type = QWarpPixmap::chkboxtri;
                    }
                    else
                    {
                        if(sunken)
                            type = QWarpPixmap::chkboxsunken;
                        else
                            type = QWarpPixmap::chkboxnormal;
                    }
                    break;
                }
                case PE_ExclusiveIndicator:
                case PE_CheckListExclusiveIndicator:
                default:
                {
                    if(flags & Style_On)
                    {
                        if(sunken)
                            type = QWarpPixmap::radbutcheckedsunken;
                        else
                            type = QWarpPixmap::radbutchecked;
                    }
                    else
                    {
                        if(sunken)
                            type = QWarpPixmap::radbutsunken;
                        else
                            type = QWarpPixmap::radbutnormal;
                    }
                }
            }

            QPixmap const* pix = qWarpSystemPixmaps.getPixmap(type,
                                                              QWarpPixmap::internal);
            p->drawPixmap(r.x(), r.y(), *pix);

            break;
        }

        case PE_IndicatorMask:
        {
            p->fillRect(r, color1);
            break;
        }

        case PE_ExclusiveIndicatorMask:
        {
            p->setPen(color1);
            p->setBrush(color1);
            p->drawEllipse(r);

            break;
        }

        case PE_SpinWidgetPlus:
        case PE_SpinWidgetMinus:
        case PE_SpinWidgetUp:
        case PE_SpinWidgetDown:
        {
            p->save();

            // button background
            p->setPen(cg.shadow());
            p->fillRect(r, QBrush(cg.button()));

            // three possible states
            enum { normal, upPressed, downPressed } st;
            st = normal;
            if(flags & Style_Sunken)
            {
                if(flags & Style_On)
                    st = upPressed;
                else
                    st = downPressed;
            }

            // left and top border
            if(upPressed == st) p->setPen(cg.dark());
            else p->setPen(cg.light());
            p->drawLine(r.topLeft(), r.topRight());
            p->drawLine(r.topLeft(), r.bottomLeft());
            p->drawLine(r.left(), r.top() + 1, r.right(), r.top() + 1);
            p->drawLine(r.left() + 1, r.top(), r.left() + 1, r.bottom());

            // right and bottom border
            if(downPressed == st) p->setPen(cg.light());
            else p->setPen(cg.dark());
            p->drawLine(r.topRight(), r.bottomRight());
            p->drawLine(r.bottomLeft(), r.bottomRight());
            p->drawLine(r.right() - 1, r.top(), r.right() - 1, r.bottom());
            p->drawLine(r.left(), r.bottom() - 1, r.right(), r.bottom() - 1);

            // diagonal
            if(upPressed == st) p->setPen(cg.light());
            else p->setPen(cg.shadow());
            p->drawLine(r.bottomLeft(), r.topRight());
            if(upPressed == st) p->setPen(cg.light());
            else p->setPen(cg.dark());
            p->drawLine(r.left(), r.bottom() - 1, r.right() - 1, r.top());
            if(downPressed == st) p->setPen(cg.dark());
            else p->setPen(cg.light());
            p->drawLine(r.left() + 1, r.bottom(), r.right(), r.top() + 1);

            // rectangles for up/down symbols
            QRect upRect;
            upRect.setWidth((r.width() - 4) / 2 - 1);
            if(1 != (upRect.width() % 2))
            {
                // we want an odd number for the width and height, and in
                // order to avoid too small symbols, we add 1 in case we
                // already rounded off during the division by 2 above
                if(1 == (r.width() % 2))
                    upRect.setWidth(upRect.width() + 1);
                else
                    upRect.setWidth(upRect.width() - 1);
            }
            upRect.setHeight(upRect.width());
            upRect.moveTopLeft(r.topLeft() + QPoint(2, 3));
            QRect downRect(upRect);
            downRect.moveBottomRight(r.bottomRight() - QPoint(3, 3));
            if(upPressed == st)
                upRect.moveBy(1, 1);
            if(downPressed == st)
                downRect.moveBy(1, 1);

            switch(pe)
            {
                case PE_SpinWidgetPlus:
                case PE_SpinWidgetMinus:
                {
                    p->setPen(cg.buttonText());
                    p->drawLine(upRect.left(), upRect.center().y(),
                                upRect.right(), upRect.center().y());
                    p->drawLine(upRect.center().x(), upRect.top(),
                                upRect.center().x(), upRect.bottom());
                    p->drawLine(downRect.left(), downRect.center().y(),
                                downRect.right(), downRect.center().y());
                    break;
                }

                case PE_SpinWidgetUp:
                case PE_SpinWidgetDown:
                default:
                {
                    p->setBrush(cg.buttonText());
                    p->setPen(QPen::NoPen);
                    QPointArray a;
                    a.putPoints(0, 3,
                                upRect.left(), upRect.center().y(),
                                upRect.center().x(), upRect.top(),
                                upRect.right(), upRect.center().y());
                    p->drawPolygon(a);
                    a.resize(0);
                    a.putPoints(0, 3,
                                downRect.left(), downRect.center().y(),
                                downRect.center().x(), downRect.bottom(),
                                downRect.right(), downRect.center().y());
                    p->drawPolygon(a);
                }
            }

            p->restore();

            break;
        }

        case PE_Panel:
        {
            qDrawWarpPanel(p, rr, cg, -1);
            break;
        }

        case PE_PanelPopup:
        {
            QBrush br(cg.background());
            qDrawWarpPanel(p, rr, cg, 1, &br);
            break;
        }

        case PE_PanelTabWidget:
        {
            bool reverse = QApplication::reverseLayout();

            QRect rr2 (rr);

            // compensate for reduced frame rect in polish(). Note that due to
            // this frame rect hack, it's not possible to draw a
            // QFrame::TabWidgetPanel styled frame outside the QTabWidget
            // context.
#ifndef QT_PM_NO_TABWIDGET_HEADERS
            if (reverse)
            {
                rr.addCoords (-15, -12, 12, 12);
                rr2.addCoords (-4, -1, 1, 1);
            }
            else
            {
                rr.addCoords (-12, -12, 15, 12);
                rr2.addCoords (-1, -1, 4, 1);
            }
#else
            rr.addCoords (-12, -12, 12, 12);
            rr2.addCoords (-1, -1, 1, 1);
#endif

            p->setClipRect (rr);

            for (int i = 0; i < 2; ++ i)
            {
                p->setPen (i == 0 ? cg.light() : cg.dark());
                if (i > 0)
                {
                    QRect rt = rr2;
                    rr2 = rr;
                    rr = rt;
                }

                p->drawLine (rr.left(), rr.top(), rr.left(), rr.bottom() - 1);
                p->drawLine (rr.left(), rr.top(), rr.right(), rr.top());
                p->drawLine (rr2.left(), rr2.bottom(), rr2.right(), rr2.bottom());
                p->drawLine (rr2.right(), rr2.top() + 1, rr2.right(), rr2.bottom());
            }

#ifndef QT_PM_NO_TABWIDGET_HEADERS
            if (reverse)
                p->drawLine (rr.left() + 3, rr.bottom() - 2,
                             rr.left() + 3, rr.top() + 2);
            else
                p->drawLine (rr.right() - 3, rr.bottom() - 2,
                             rr.right() - 3, rr.top() + 2);
#endif
            break;
        }

        case PE_PanelMenuBar:
        {
            // It looks like this code is never being called!
            break;
        }

        case PE_PanelGroupBox:
        {
            qDrawWarpPanel(p, rr, cg, 0);
            break;
        }

        case PE_TabBarBase:
        {
            bool bottom = flags & Style_Bottom;

            QRect rr2 (rr);
            if (bottom)
                rr2.addCoords (11, 0, -11, -11);
            else
                rr2.addCoords (11, 11, -11, 0);

            for (int i = 0; i < 2; ++ i)
            {
                p->setPen (i == 0 ? cg.light() : cg.dark());
                if (i > 0)
                {
                    QRect rt = rr2;
                    rr2 = rr;
                    rr = rt;
                }

                if(bottom)
                {
                    p->drawLine (rr.left(), rr.top(), rr.left(), rr.bottom() - 1);
                    p->drawLine (rr2.right(), rr2.top(), rr2.right(), rr2.bottom());
                    p->drawLine (rr2.left(), rr2.bottom(), rr2.right(), rr2.bottom());
                }
                else
                {
                    p->drawLine (rr.left(), rr.top(), rr.right(), rr.top());
                    p->drawLine (rr.left(), rr.top(), rr.left(), rr.bottom());
                    p->drawLine (rr2.right(), rr2.top() + 1, rr2.right(), rr2.bottom());
                }
            }

            break;
        }

        case PE_Splitter:
        {
            QPen oldPen = p->pen();
            p->setPen(cg.light());

            if(flags & Style_Horizontal)
            {
                p->drawLine(r.x() + 1, r.y(), r.x() + 1, r.height());
                p->setPen(cg.dark());
                p->drawLine(r.x(), r.y(), r.x(), r.height());
                p->drawLine(r.right()-1, r.y(), r.right()-1, r.height());
                p->setPen(cg.shadow());
                p->drawLine(r.right(), r.y(), r.right(), r.height());
            }
            else
            {
                p->drawLine(r.x(), r.y() + 1, r.width(), r.y() + 1);
                p->setPen(cg.dark());
                p->drawLine(r.x(), r.bottom() - 1, r.width(), r.bottom() - 1);
                p->setPen(cg.shadow());
                p->drawLine(r.x(), r.bottom(), r.width(), r.bottom());
            }
            p->setPen(oldPen);

            break;
        }

        case PE_DockWindowResizeHandle:
        {
            QPen oldPen = p->pen();
            p->setPen(cg.light());

            if(flags & Style_Horizontal)
            {
                p->drawLine(r.x(), r.y(), r.width(), r.y());
                p->setPen(cg.dark());
                p->drawLine(r.x(), r.bottom() - 1, r.width(), r.bottom() - 1);
                p->setPen(cg.shadow());
                p->drawLine(r.x(), r.bottom(), r.width(), r.bottom());
            }
            else
            {
                p->drawLine(r.x(), r.y(), r.x(), r.height());
                p->setPen(cg.dark());
                p->drawLine(r.right()-1, r.y(), r.right()-1, r.height());
                p->setPen(cg.shadow());
                p->drawLine(r.right(), r.y(), r.right(), r.height());
            }
            p->setPen(oldPen);

            break;
        }

        case PE_ScrollBarSubLine:
        case PE_ScrollBarAddLine:
        {
            QPointArray pa;
            if(flags & Style_Horizontal)
            {
                int height2 = rr.height() / 2 - 4,
                    width2 = height2 / 2;
                if(pe == PE_ScrollBarSubLine)
                {
                    pa.putPoints(0, 4,
                                 width2, height2,
                                 width2, 1 - height2,
                                 1 - width2, 0,
                                 1 - width2, 1);
                }
                else
                {
                    pa.putPoints(0, 4,
                                 1 - width2, 1 - height2,
                                 1 - width2, height2,
                                 width2, 1,
                                 width2, 0);
                }
            }
            else
            {
                int width2 = rr.width() / 2 - 4,
                    height2 = width2 / 2;
                if(pe == PE_ScrollBarSubLine)
                {
                    pa.putPoints(0, 4,
                                 1 - width2, height2,
                                 width2, height2,
                                 1, 1 - height2,
                                 0, 1 - height2);
                }
                else
                {
                    pa.putPoints(0, 4,
                                 width2, 1 - height2,
                                 1 - width2, 1 - height2,
                                 0, height2,
                                 1, height2);
                }
            }
            pa.translate(rr.center().x(), rr.center().y());

            p->setPen(cg.dark());
            if(!((flags & Style_Horizontal) && (pe == PE_ScrollBarAddLine)))
            {
                p->drawLine(rr.topLeft(), rr.bottomLeft());
                rr.setLeft(rr.left() + 1);
            }
            if(!(!(flags & Style_Horizontal) && (pe == PE_ScrollBarAddLine)))
            {
                p->drawLine(rr.topLeft(), rr.topRight());
                rr.setTop(rr.top() + 1);
            }
            p->setPen(cg.light());
            if(!((flags & Style_Horizontal) && (pe == PE_ScrollBarSubLine)))
            {
                p->drawLine(rr.topRight(), rr.bottomRight());
                rr.setRight(rr.right() - 1);
            }
            if(!(!(flags & Style_Horizontal) && (pe == PE_ScrollBarSubLine)))
            {
                p->drawLine(rr.bottomLeft(), rr.bottomRight());
                rr.setBottom(rr.bottom() - 1);
            }

            if(!(flags & Style_Down))
                flags |= Style_Raised;
            drawPrimitive(PE_ButtonBevel, p, rr, cg, flags);
            p->setPen(cg.light());
            p->drawLine(rr.topRight(), rr.topRight() + QPoint(-1, 1));

            QBrush::BrushStyle bs = (flags & Style_Enabled) ? Qt::SolidPattern : Qt::Dense4Pattern;
            p->setBrush(QBrush(cg.shadow(), bs));
            p->setPen(QPen(Qt::NoPen));
            p->drawPolygon(pa);

            break;
        }

        case PE_ScrollBarAddPage:
        case PE_ScrollBarSubPage:
        case PE_ScrollBarSlider:
        {
            if(!rr.isValid())
                break;

            p->setPen(cg.light());
            if(flags & Style_Horizontal)
            {
                p->drawLine(rr.bottomLeft(), rr.bottomRight());
                rr.setBottom(rr.bottom() - 1);
            }
            else
            {
                p->drawLine(rr.topRight(), rr.bottomRight());
                rr.setRight(rr.right() - 1);
            }
            p->setPen(cg.dark());
            if(flags & Style_Horizontal)
            {
                p->drawLine(rr.topLeft(), rr.topRight());
                rr.setTop(rr.top() + 1);
            }
            else
            {
                p->drawLine(rr.topLeft(), rr.bottomLeft());
                rr.setLeft(rr.left() + 1);
            }
            p->drawLine(rr.topLeft(), rr.topRight());

            if(pe == PE_ScrollBarSlider)
            {
                if(!(flags & Style_Down))
                    flags |= Style_Raised;
                drawPrimitive(PE_ButtonBevel, p, rr, cg, flags);
                QPoint from, to, step;
                if(flags & Style_Horizontal)
                {
                    from = QPoint(rr.center().x() - 5, rr.top() + 4);
                    to   = QPoint(rr.center().x() - 5, rr.bottom() - 4);
                    step = QPoint(1, 0);
                }
                else
                {
                    from = QPoint(rr.left() + 4, rr.center().y() - 5);
                    to   = QPoint(rr.right() - 4, rr.center().y() - 5);
                    step = QPoint(0, 1);
                }

                int i;
                for(i = 0; i < 12; ++i)
                {
                    if(0 == (i % 2))
                        p->setPen(cg.shadow());
                    else
                        p->setPen(cg.light());
                    p->drawLine(from, to);
                    from += step;
                    to += step;
                }
            }

            else // ..AddPage or ..SubPage
            {
                rr.setTop(rr.top() + 1);
                p->drawLine(rr.topLeft(), rr.bottomLeft());
                rr.setLeft(rr.left() + 1);

                QBrush br;
                if(flags & Style_Down)
                    br = QBrush(cg.shadow());
                else
                    br = QBrush(qt_sysclr2qrgb(SYSCLR_SCROLLBAR));
                p->setPen(QPen(Qt::NoPen));

                p->fillRect(rr, br);
            }

            break;
        }

        case PE_WindowFrame:
        {
            QColorGroup popupCG = cg;
            popupCG.setColor(QColorGroup::Light, cg.background());
            popupCG.setColor(QColorGroup::Midlight, cg.light());

            int lw = opt.isDefault() ? pixelMetric(PM_MDIFrameWidth)
                        : opt.lineWidth();

            if(lw == 2)
            {
                qDrawWarpPanel(p, r, popupCG, -1);
            }
            else
            {
                QBrush fill = QBrush(cg.background());
                qDrawWarpPanel(p, r, popupCG, -1, &fill);
            }

            break;
        }

        default:
        {
            if((PE_CheckMark == pe) ||
               ((pe >= PE_ArrowUp) && (pe <= PE_ArrowLeft)))
            {
                QPixmap const* pm = 0;

                switch(pe)
                {
                    case PE_ArrowUp:
                        pm = qWarpSystemPixmaps.getPixmap(QWarpPixmap::arrowup,
                                                          QWarpPixmap::internal);
                        break;

                    case PE_ArrowDown:
                        pm = qWarpSystemPixmaps.getPixmap(QWarpPixmap::arrowdown,
                                                          QWarpPixmap::internal);
                        break;

                    case PE_ArrowRight:
                        pm = qWarpSystemPixmaps.getPixmap(QWarpPixmap::arrowright,
                                                          QWarpPixmap::internal);
                        break;

                    case PE_ArrowLeft:
                        pm = qWarpSystemPixmaps.getPixmap(QWarpPixmap::arrowleft,
                                                          QWarpPixmap::internal);
                        break;

                    default: // PE_CheckMark
                        pm = qWarpSystemPixmaps.getPixmap(QWarpPixmap::checkmark,
                                                          QWarpPixmap::internal);
                        break;
                }

                int x = rr.left() + (rr.width() - pm->width()) / 2,
                    y = rr.top() + (rr.height() - pm->height()) / 2;
                p->drawPixmap(x, y, *pm);
            }
            else
            {
                QCommonStyle::drawPrimitive(pe, p, r, cg, flags, opt);
            }
        }
    }
}

/*!
  \reimp
*/
void QWarp4Style::drawControl(ControlElement element,
                              QPainter* p,
                              QWidget const* widget,
                              QRect const& r,
                              QColorGroup const& cg,
                              SFlags flags,
                              QStyleOption const& opt) const
{
    switch (element)
    {
#ifndef QT_NO_TABBAR
        case CE_TabBarTab:
        {
            if(!widget || !widget->parentWidget() || !opt.tab())
                break;

            QTabBar const* tb = (QTabBar const*)widget;
            QTab const* t = opt.tab();
            bool selected = flags & Style_Selected,
                 bottom = (QTabBar::RoundedBelow == tb->shape()) ||
                          (QTabBar::TriangularBelow == tb->shape());
            int id = tb->indexOf(t->identifier());

            static const QColor tabCols[10] =
            {
                QColor(85, 219, 255),
                QColor(128, 219, 170),
                QColor(128, 146, 255),
                QColor(213, 182, 170),
                QColor(255, 255, 170),
                QColor(170, 146, 170),
                QColor(255, 146, 85),
                QColor(255, 219, 85),
                QColor(255, 182, 170),
                QColor(255, 219, 170)
            };

            qDrawTabBar(p, r, cg, tabCols[id % 10], selected, bottom);

            if(QApplication::reverseLayout())
            {
                p->setPen(cg.dark());
                int right = tb->size().width() - 1;
                if(bottom)
                    p->drawLine(right, r.top(), right, r.top() + 4);
                else
                    p->drawLine(right, r.bottom(), right, r.bottom() - 4);

            }
            p->setPen(cg.light());
            if(bottom)
                p->drawLine(0, r.top(), 0, r.top() + 4);
            else
                p->drawLine(0, r.bottom(), 0, r.bottom() - 4);

            break;
        }

        case CE_TabBarLabel:
        {
            if(opt.isDefault())
                break;

            QTabBar const* tb = (QTabBar const*)widget;
            QTab const* t = opt.tab();
            bool selected = flags & Style_Selected,
                 bottom = (QTabBar::RoundedBelow == tb->shape()) ||
                          (QTabBar::TriangularBelow == tb->shape());

            QRect tr(r);
            if(bottom)
                tr.addCoords(3, 4, 1, 0);
            else
                tr.addCoords(3, 1, 1, -3);

            if(selected)
            {
                QFont f = p->font();
                QRect rold = QFontMetrics(f).boundingRect(t->text());
                f.setBold(true);
                QRect rnew = QFontMetrics(f).boundingRect(t->text());
                p->setFont(f);
                int hdiff = rnew.width() - rold.width() + 1;
                tr.addCoords(-hdiff / 2, 0, hdiff / 2, 0);

#ifndef QT_PM_NO_TABWIDGET_HEADERS
                QWidget* pw = widget->parentWidget();
                if (pw != 0 && pw->inherits ("QTabWidget"))
                {
                    QTabWidget *tw = (QTabWidget *) pw;
                    QWarpTabPanelHeader *tph =
                        (QWarpTabPanelHeader *) tw->child ("qt_warp_tabpanel_header",
                                                           "QWarpTabPanelHeader", false);
                    if (tph != 0)
                    {
                        tph->setCaption (t->text());
                        QRect r (tph->visibleRect());
                        tph->repaint();
                    }
                }
#endif // QT_PM_NO_TABWIDGET_HEADERS
            }

            int alignment = AlignCenter | ShowPrefix;
            if(!styleHint(SH_UnderlineAccelerator, widget, QStyleOption::Default, 0))
               alignment |= NoAccel;
            drawItem(p, tr, alignment, cg, flags & Style_Enabled, 0, t->text());

            if((flags & Style_HasFocus) && !t->text().isEmpty())
                drawPrimitive(PE_FocusRect, p, tr, cg);

            break;
        }
#endif // QT_NO_TABBAR

        case CE_ToolBoxTab:
        {
            qDrawWarpPanel(p, r, cg, -1, &cg.brush(QColorGroup::Button));
            break;
        }

        case CE_ProgressBarGroove:
        {
            QRect rr(r);
            QBrush fill = cg.brush(QColorGroup::Button);
            qDrawWarpFilledRect(p, rr, cg.dark(), cg.light(), &fill, 2, true);
            // For OS/2, this can be also cg.shadow() if the user has clicked on
            // the bar with the mouse, but this is only due to the fact that an "OS/2
            // progressbar" is actually a slider, not just a display widget
            p->setPen(cg.dark());
            rr.addCoords(2, 2, -2, -1);
            p->drawRect(rr);
            break;
        }

#ifndef QT_NO_PROGRESSBAR
        case CE_ProgressBarContents:
        {
            QProgressBar const* progressbar = (QProgressBar const*)widget;
            QColorGroup cgh = cg;
            cgh.setColor(QColorGroup::Highlight, QColor(0, 0, 170));
            bool reverse = QApplication::reverseLayout();
            QRect br(r);
            br.addCoords(3, 3, -3, -2);

            if(!progressbar->totalSteps())
            {
                // draw busy indicator
                static const int indWidth = 30;
                br.addCoords(indWidth / 2, 0, -indWidth / 2, 0);
                int bw = br.width();
                int x = progressbar->progress() % (bw * 2);
                if(x > bw)
                    x = 2 * bw - x;
                x = reverse ? br.right() - x : br.left() + x;
                p->setPen(QPen(cgh.highlight(), indWidth));
                p->drawLine(x, br.top(), x, br.bottom());
            }

            else
            {
                int pos = (progressbar->progress() * br.width()) /
                          progressbar->totalSteps();
                pos = reverse ?
                      br.right() - pos :
                      br.left() + pos;

                p->setPen(cgh.dark());
                p->drawLine(pos, br.top(), pos, br.bottom());

                if(reverse)
                    br.setLeft(pos + 1);
                else
                    br.setRight(pos - 1);

                p->fillRect(br, cgh.highlight());
            }

            break;
        }

        case CE_ProgressBarLabel:
        {
            QProgressBar const* progressbar = (QProgressBar const*)widget;
            if(0 == progressbar->totalSteps())
                break;

            if(progressbar->indicatorFollowsStyle() ||
               progressbar->centerIndicator())
            {
                QRect br(r);
                br.addCoords(3, 3, -3, -2);
                int pos = (progressbar->progress() * br.width()) /
                          progressbar->totalSteps();
                bool reverse = QApplication::reverseLayout();
                if(reverse)
                    pos = br.right() - pos;
                else
                    pos = br.left() + pos;

                // paint label black in background area
                QRect clip(br);
                if(reverse)
                    clip.setRight(pos);
                else
                    clip.setLeft(pos);
                p->setClipRect(clip);
                drawItem(p, br, AlignCenter | SingleLine, cg, flags & Style_Enabled, 0,
                         progressbar->progressString());

                // paint label "light" in blue ribbonstrip area
                clip = br;
                if(reverse)
                    clip.setLeft(pos);
                else
                    clip.setRight(pos);
                p->setClipRect(clip);
                drawItem(p, br, AlignCenter | SingleLine, cg, flags & Style_Enabled, 0,
                         progressbar->progressString(), -1, &cg.highlightedText());
            }

            else
            {
                drawItem(p, r, AlignCenter | SingleLine, cg, flags & Style_Enabled, 0,
                         progressbar->progressString(), -1, 0);
            }

            break;
        }
#endif // QT_NO_PROGRESSBAR

#ifndef QT_NO_POPUPMENU
        case CE_PopupMenuItem:
        {
            if (!widget || opt.isDefault())
                break;

            // QPopupMenu has WResizeNoErase and WRepaintNoErase flags, so we
            // must erase areas not covered by menu items (this is requested by
            // QPopupMenu using 0 as the menu item argument).
            // note: we do this with the frame drawing (PE_PanelPopup)

            QPopupMenu const* popupmenu = (const QPopupMenu *) widget;
            QMenuItem* mi = opt.menuItem();
            if (!mi)
                break;

            int tabw = opt.tabWidth() + 3;
            int iconw = QMAX(11, opt.maxIconWidth()) + 6;
            bool dis = !(flags & Style_Enabled);
            bool act = flags & Style_Active;
            bool reverse = QApplication::reverseLayout();
            int subw = (qPopupMenuHasSub (popupmenu) ? 19 : 0);

            int x, y, w, h;
            r.rect (&x, &y, &w, &h);

            if (mi && mi->isSeparator()) // draw separator
            {
                p->setPen (cg.dark());
                p->drawLine (x, y + 3, x + w, y + 3);
                p->setPen (cg.light());
                p->drawLine (x, y + 4, x + w, y + 4);
                return;
            }

            if (act)
            {
                QRect hr (r);
                hr.addCoords (3, 0, -3, 0);
                p->fillRect (hr, cg.highlight());
                p->setPen (cg.highlightedText());
            }
            else
            {
                p->setPen (cg.text());
            }

            if (mi->iconSet()) // draw iconset
            {
                QIconSet::Mode mode = dis ? QIconSet::Disabled : QIconSet::Normal;
                if (act && !dis)
                    mode = QIconSet::Active;
                QPixmap pixmap;
                if (popupmenu->isCheckable() && mi->isChecked())
                    pixmap = mi->iconSet()->pixmap (QIconSet::Small, mode,
                                                    QIconSet::On);
                else
                    pixmap = mi->iconSet()->pixmap (QIconSet::Small, mode);
                int px = (reverse ? (w - pixmap.width() - 6) : 6);
                p->drawPixmap (px, y + (h - pixmap.height()) / 2, pixmap);
            }
            else if (popupmenu->isCheckable() && mi->isChecked()) // just "checking"...
            {
                SFlags cflags = Style_Default;
                if (!dis)
                    cflags |= Style_Enabled;
                if (act)
                    cflags |= Style_On;

                int xc = (reverse ? w - iconw + 15 : iconw - 15),
                    yc = (r.top() + r.bottom()) / 2 - 4;
                drawPrimitive (PE_CheckMark, p, QRect (xc, yc, 16, 8), cg, cflags);
            }

            if (mi->custom())
            {
                p->save();
                // margins must be in sync with the draw text code below
                int tx = (reverse ? tabw + subw : iconw);
                mi->custom()->paint (p, cg, act, !dis, tx, y,
                                     w - iconw - tabw - subw, h);
                p->restore();
            }

            QString s = mi->text();
            if (!s.isNull()) // draw text
            {
                int text_flags = AlignVCenter | ShowPrefix | DontClip | SingleLine;
                if (!styleHint (SH_UnderlineAccelerator, widget))
                    text_flags |= NoAccel;
                text_flags |= (reverse ? AlignRight : AlignLeft);

                // tab text
                int t = s.find ('\t');
                QString ts;
                if (t >= 0)
                {
                    ts = s.mid (t + 1);
                    s = s.left (t);
                    int tx = (reverse ? subw : w - tabw - subw);
                    p->drawText (tx, y, tabw, h, text_flags, ts);
                }

                // normal text
                int tx = (reverse ? tabw + subw : iconw);
                p->drawText (tx, y, w - iconw - tabw - subw, h, text_flags, s);
            }
            else if (mi->pixmap()) // draw pixmap
            {
                QPixmap* pixmap = mi->pixmap();
                if (pixmap->depth() == 1)
                    p->setBackgroundMode(OpaqueMode);
                int px = (reverse ? (w - pixmap->width() - iconw) : iconw);
                if (dis)
                {
                    QIconSet is (*pixmap);
                    p->drawPixmap (px, y, is.pixmap (QIconSet::Automatic,
                                                     QIconSet::Disabled));
                }
                else
                {
                    p->drawPixmap (px, y, *pixmap);
                }
                if (pixmap->depth() == 1)
                    p->setBackgroundMode (TransparentMode);
            }

            if (mi->popup()) // draw sub menu arrow
            {
                PrimitiveElement arrow;
                arrow = (reverse ? PE_ArrowLeft : PE_ArrowRight);
                int xa = (reverse ? 5 : w - 15),
                    ya = (r.top() + r.bottom()) / 2 - 5;
                drawPrimitive (arrow, p, QRect (xa, ya, 12, 12),
                               cg, dis ? Style_Default : Style_Enabled);
            }

            break;
        }
#endif // QT_NO_POPUPMENU

        case CE_MenuBarItem:
        {
            QWidget* w = (QWidget*)widget;
            while(true)
            {
                QWidget* p = w->parentWidget(true);
                if(0 == p)
                    break;
                else
                    w = p;
            }

            // here we let the two grey lines disappear at the top and bottom of
            // the client area that are actually the empty top and bottom docking
            // areas (only as long as they are empty, of course)
            if(w->inherits("QMainWindow"))
                QWarpMainWindowLayouter::addIfNotPresent((QMainWindow*)w);

            bool act = flags & Style_Active;
            QColorGroup mcg(cg);

            if(act)
            {
                QRect hr(r);
                hr.addCoords(0, 2, 0, 0);
                p->fillRect(hr, cg.highlight());
                mcg.setColor(QColorGroup::ButtonText, mcg.highlightedText());
            }
            else
            {
                mcg.setColor(QColorGroup::ButtonText, mcg.text());
            }

            QCommonStyle::drawControl(element, p, widget, r, mcg, flags, opt);
            break;
        }

#ifndef QT_NO_TOOLBUTTON
        case CE_ToolButtonLabel:
        {
            if (!qstrcmp (widget->name(), "qt_left_btn") ||
                !qstrcmp (widget->name(), "qt_right_btn"))
            {
                // do nothing in this case!
            }
            else
            {
                QToolButton const *toolbutton = (QToolButton const *) widget;
                QRect rect = r;
                Qt::ArrowType arrowType = opt.isDefault() ?
                                          Qt::DownArrow : opt.arrowType();

                if (flags & (Style_Down | Style_On))
                    rect.moveBy (pixelMetric(PM_ButtonShiftHorizontal, widget),
                                 pixelMetric(PM_ButtonShiftVertical, widget));

                if (!opt.isDefault())
                {
                    PrimitiveElement pe;
                    switch (arrowType)
                    {
                        case Qt::LeftArrow:
                            pe = PE_ArrowLeft;
                            break;
                        case Qt::RightArrow:
                            pe = PE_ArrowRight;
                            break;
                        case Qt::UpArrow:
                            pe = PE_ArrowUp;
                            break;
                        default:
                        case Qt::DownArrow:
                            pe = PE_ArrowDown;
                            break;
                    }

                    drawPrimitive (pe, p, rect, cg, flags, opt);
                }
                else
                {
                    QColor btext = toolbutton->paletteForegroundColor();

                    if (toolbutton->iconSet().isNull() &&
                        !toolbutton->text().isNull() &&
                        !toolbutton->usesTextLabel())
                    {
                        int alignment = AlignCenter | AlignVCenter | ShowPrefix;

                        if (!styleHint (SH_UnderlineAccelerator, widget,
                                        QStyleOption::Default, 0))
                            alignment |= NoAccel;

                        drawItem (p, rect, alignment, cg,
                                  flags & Style_Enabled, 0, toolbutton->text(),
                                  toolbutton->text().length(), &btext);
                    }
                    else
                    {
                        QPixmap pm;

                        QIconSet::Size size =
                            toolbutton->usesBigPixmap() ? QIconSet::Large : QIconSet::Small;

                        QIconSet::State state =
                            toolbutton->isOn() ? QIconSet::On : QIconSet::Off;

                        QIconSet::Mode mode;
                        if (!toolbutton->isEnabled())
                            mode = QIconSet::Disabled;
                        else if (flags & (Style_Down | Style_On | Style_Raised))
                            mode = QIconSet::Active;
                        else
                            mode = QIconSet::Normal;

                        pm = toolbutton->iconSet().pixmap(size, mode, state);

                        if (toolbutton->usesTextLabel())
                        {
                            if (toolbutton->textPosition() == QToolButton::Under)
                            {
                                p->setFont(toolbutton->font());

                                QRect pr = rect, tr = rect;
                                int fh = p->fontMetrics().height();
                                pr.addCoords (0, 1, 0, -fh - 3);
                                tr.addCoords (0, pr.bottom(), 0, -3);
                                drawItem (p, pr, AlignCenter, cg, TRUE, &pm, QString::null);
                                int alignment = AlignCenter | ShowPrefix;

                                if (!styleHint (SH_UnderlineAccelerator, widget, QStyleOption::Default, 0))
                                    alignment |= NoAccel;

                                drawItem (p, tr, alignment, cg,
                                          flags & Style_Enabled, 0, toolbutton->textLabel(),
                                          toolbutton->textLabel().length(), &btext);
                            }
                            else
                            {
                                p->setFont (toolbutton->font());

                                QRect pr = rect, tr = rect;
                                pr.setWidth (pm.width() + 8);
                                tr.addCoords (pr.right(), 0, 0, 0);
                                drawItem (p, pr, AlignCenter, cg, TRUE, &pm, QString::null);
                                int alignment = AlignLeft | AlignVCenter | ShowPrefix;

                                if (!styleHint(SH_UnderlineAccelerator, widget, QStyleOption::Default, 0))
                                    alignment |= NoAccel;

                                drawItem (p, tr, alignment, cg,
                                          flags & Style_Enabled, 0, toolbutton->textLabel(),
                                          toolbutton->textLabel().length(), &btext);
                            }
                        }
                        else
                        {
                            drawItem (p, rect, AlignCenter, cg, TRUE, &pm, QString::null);
                        }
                    }
                }
            }

            break;
        }
#endif // QT_NO_TOOLBUTTON

        default:
        {
            QCommonStyle::drawControl(element, p, widget, r, cg, flags, opt);
        }
    }
}

/*!
  \reimp
*/
int QWarp4Style::pixelMetric (PixelMetric metric, const QWidget *widget) const
{
    int ret;

    switch (metric)
    {
        case PM_ButtonDefaultIndicator:
        case PM_ButtonShiftHorizontal:
        case PM_ButtonShiftVertical:
        {
            ret = 1;
            break;
        }

        case PM_IndicatorWidth:
        case PM_ExclusiveIndicatorWidth:
        {
            QPixmap const* ip = qWarpSystemPixmaps.getPixmap (QWarpPixmap::chkboxnormal,
                                                              QWarpPixmap::internal);
            ret = ip->width();
            break;
        }

        case PM_IndicatorHeight:
        case PM_ExclusiveIndicatorHeight:
        {
            QPixmap const* ip = qWarpSystemPixmaps.getPixmap (QWarpPixmap::chkboxnormal,
                                                              QWarpPixmap::internal);
            ret = ip->height();
            break;
        }

#ifndef QT_NO_SCROLLBAR
        case PM_ScrollBarExtent:
        {
            if (!widget)
            {
                ret = 16;
            }
            else
            {
                const QScrollBar * bar = (const QScrollBar *) widget;
                int s = (bar->orientation() == Qt::Horizontal) ?
                        QApplication::globalStrut().height() :
                        QApplication::globalStrut().width();
                ret = QMAX(16, s);
            }

            break;
        }

        case PM_ScrollBarSliderMin:
        {
            ret = 20;
            break;
        }
#endif

        case PM_MaximumDragDistance:
        {
            ret = -1;
            break;
        }

#ifndef QT_NO_SLIDER
        case PM_SliderThickness:
        {
            ret = 20;
            break;
        }

        case PM_SliderLength:
        {
            // we increase the "length" by 2 x 2, but then draw the slider 2 pixels
            // smaller than indicated, so we get the effect of a slider that does
            // not fully touch the extreme ends of the "groove"
            ret = 12 + 4;
            break;
        }

        // Returns the number of pixels to use for the business part of the
        // slider (i.e., the non-tickmark portion). The remaining space is shared
        // equally between the tickmark regions.
        case PM_SliderControlThickness:
        {
            QSlider const* sl = (QSlider const*) widget;
            int space = (sl->orientation() == Horizontal) ? sl->height() : sl->width();
            int ticks = sl->tickmarks();
            int n = 0;
            if (ticks & QSlider::Above)
                n++;
            if (ticks & QSlider::Below)
                n++;
            if (!n)
            {
                ret = space;
                break;
            }

            int thick = 6;      // Magic constant to get 5 + 16 + 5
            if (ticks != QSlider::Both && ticks != QSlider::NoMarks)
                thick += pixelMetric (PM_SliderLength, sl) / 4;

            space -= thick;
            //### the two sides may be unequal in size
            if(space > 0)
                thick += (space * 2) / (n + 2);
            ret = thick;

            break;
        }
#endif // QT_NO_SLIDER

        case PM_MenuBarFrameWidth:
        {
            ret = 0;
            break;
        }

        case PM_MenuBarItemSpacing:
        {
            ret = 8;
            break;
        }

        case PM_PopupMenuFrameHorizontalExtra:
        {
            ret = 0;
            break;
        }

        case PM_PopupMenuFrameVerticalExtra:
        {
            ret = 1;
            break;
        }

        case PM_DefaultFrameWidth:
        {
            if (widget && widget->inherits ("QWidgetStack") &&
                !qstrcmp (widget->name(), "tab pages"))
            {
#ifndef QT_PM_NO_TABWIDGET_HEADERS
                // cause 1px overlap with the tab header base widget to get the
                // left vertical line joined with the bended corner (see the
                // PE_PanelTabWidget primitive and QTabWidget::setUpLayout())
                ret = 1;
#else
                // no frame is drawn within the the widget stack frame rect
                ret = 0;
#endif
                break;
            }
            ret = QCommonStyle::pixelMetric (metric, widget);
            break;
        }

        case PM_TabBarTabOverlap:
        {
            ret = 10;
            break;
        }

        case PM_TabBarBaseHeight:
        {
#ifndef QT_PM_NO_TABWIDGET_HEADERS
            ret = QFontMetrics (widget->font()).boundingRect ("X").height() + 28;
#else
            ret = 12;
#endif
            break;
        }

        case PM_TabBarBaseOverlap:
        {
            ret = 5;
            break;
        }

        case PM_TabBarTabHSpace:
        {
            ret = 28;
            break;
        }

        case PM_TabBarTabVSpace:
        {
            ret = 9;
            break;
        }

        case PM_TabBarScrollButtonWidth:
        {
            ret = 25;
            break;
        }

        case PM_TabBarTabShiftHorizontal:
        case PM_TabBarTabShiftVertical:
        {
            ret = 0;
            break;
        }

        case PM_SplitterWidth:
        {
            ret = QMAX(6, QApplication::globalStrut().width());
            break;
        }

        default:
        {
            ret = QCommonStyle::pixelMetric(metric, widget);
            break;
        }
    }

    return ret;
}

/*!
  \reimp
*/
QSize QWarp4Style::sizeFromContents(ContentsType contents,
                                    QWidget const* widget,
                                    QSize const& contentsSize,
                                    QStyleOption const& opt) const
{
    QSize sz(contentsSize);

    switch (contents)
    {
        case CT_PushButton:
        {
#ifndef QT_NO_PUSHBUTTON
            const QPushButton* button = (const QPushButton*)widget;
            sz = QCommonStyle::sizeFromContents(contents, widget, contentsSize, opt);
            int w = sz.width(),
                h = sz.height();

            int defwidth = 0;
            if(button->isDefault() || button->autoDefault())
                defwidth = 2 * pixelMetric(PM_ButtonDefaultIndicator, widget);
            if(w < 80 + defwidth && !button->pixmap())
                w = 80 + defwidth;
            if(h < 23 + defwidth)
                h = 23 + defwidth;

            sz = QSize(w, h);
#endif // QT_NO_PUSHBUTTON

            break;
        }

        case CT_ToolButton:
        {
            // this includes 3px margin drawn for PE_ButtonTool + 1px spacing
            // around contents + 1px extra width/height for pressed icon shift
            sz = QSize (sz.width() + 9, sz.height() + 9);
            break;
        }

        case CT_PopupMenuItem:
        {
#ifndef QT_NO_POPUPMENU
            if(! widget || opt.isDefault())
                break;

            QPopupMenu const* popup = (QPopupMenu const*)widget;
            QMenuItem* mi = opt.menuItem();
            int iconw = QMAX(11, opt.maxIconWidth());
            QString s = mi->text();
            int w = sz.width(),
                h = sz.height();

            if(mi->custom())
            {
                w = mi->custom()->sizeHint().width();
                h = mi->custom()->sizeHint().height();
                if(!mi->custom()->fullSpan())
                    w += iconw;
            }
            else if(mi->widget())
            {
            }
            else if(mi->isSeparator())
            {
                w = 10; // arbitrary
                h = 8;
            }
            else
            {
                if(mi->pixmap())
                    h = QMAX(h, mi->pixmap()->height() + 3);
                else if(!s.isNull())
                    h = QMAX(h, popup->fontMetrics().height() + 1);

                if(mi->iconSet())
                {
                    h = QMAX(h, mi->iconSet()->
                                pixmap(QIconSet::Small, QIconSet::Normal).height() + 3);
                    w = QMAX(w, iconw);
                }
            }

            if(!s.isNull())
            {
                w += 19;
            }

            if(qPopupMenuHasSub(popup))
            {
                w += 19;
            }

            w += 9;

            sz = QSize(w, h);
#endif // QT_NO_POPUPMENU

            break;
        }

        case CT_MenuBar:
        {
            // It looks like this code is never being called!
            break;
        }

        case CT_SpinBox:
        {
            sz.setHeight(sz.height() - 2);
            break;
        }

        case CT_TabWidget:
        {
            sz.setWidth(sz.width() + 32); // 16, 16
            sz.setHeight(sz.height() + 14); // 0, 14
            break;
        }

        case CT_LineEdit:
        {
            sz.setHeight(sz.height() - 1);
            break;
        }

        default:
        {
            sz = QCommonStyle::sizeFromContents(contents, widget, sz, opt);
            break;
        }
    }

    return sz;
}

/*! \reimp
*/
void QWarp4Style::polishPopupMenu(QPopupMenu* p)
{
    #ifndef QT_NO_POPUPMENU
        if(!p->testWState(WState_Polished))
            p->setCheckable(TRUE);
    #endif
}

/*!
 \reimp
 */
QPixmap QWarp4Style::stylePixmap(StylePixmap stylepixmap,
                                 QWidget const* widget,
                                 QStyleOption const& opt) const
{
#ifndef QT_NO_IMAGEIO_XPM
    switch (stylepixmap)
    {
        // SP_TitleBarShadeButton - shade button on titlebars.
        case SP_TitleBarShadeButton:
            return *qWarpSystemPixmaps.getPixmap(54, // any SBMP_ value??
                                                 QWarpPixmap::bmp);

        // SP_TitleBarUnshadeButton - unshade button on titlebars.
        case SP_TitleBarUnshadeButton:
            return *qWarpSystemPixmaps.getPixmap(56, // any SBMP_ value??
                                                 QWarpPixmap::bmp);

        // SP_TitleBarNormalButton - normal (restore) button on titlebars.
        case SP_TitleBarNormalButton:
            return *qWarpSystemPixmaps.getPixmap(SBMP_RESTOREBUTTON,
                                                 QWarpPixmap::bmp);

        // SP_TitleBarMinButton - minimize button on titlebars. For example, in a QWorkspace.
        case SP_TitleBarMinButton:
            return *qWarpSystemPixmaps.getPixmap(SBMP_MINBUTTON,
                                                 QWarpPixmap::bmp);

        // SP_TitleBarMaxButton - maximize button on titlebars.
        case SP_TitleBarMaxButton:
            return *qWarpSystemPixmaps.getPixmap(SBMP_MAXBUTTON,
                                                 QWarpPixmap::bmp);

        // SP_TitleBarCloseButton - close button on titlebars.
        case SP_TitleBarCloseButton:
            return *qWarpSystemPixmaps.getPixmap(52, // any SBMP_ value??
                                                 QWarpPixmap::bmp);

        // SP_DockWindowCloseButton - close button on dock windows; see also QDockWindow.
        case SP_DockWindowCloseButton:
            return *qWarpSystemPixmaps.getPixmap(52, // should be a bit smaller!
                                                 QWarpPixmap::bmp);

        // SP_MessageBoxInformation - the 'information' icon.
        case SP_MessageBoxInformation:
            return *qWarpSystemPixmaps.getPixmap(SPTR_ICONINFORMATION,
                                                 QWarpPixmap::ico);

        // SP_MessageBoxWarning - the 'warning' icon.
        case SP_MessageBoxWarning:
            return *qWarpSystemPixmaps.getPixmap(SPTR_ICONWARNING,
                                                 QWarpPixmap::ico);

        // SP_MessageBoxCritical - the 'critical' icon.
        case SP_MessageBoxCritical:
            return *qWarpSystemPixmaps.getPixmap(SPTR_ICONERROR,
                                                 QWarpPixmap::ico);

        // SP_MessageBoxQuestion - the 'question' icon.
        case SP_MessageBoxQuestion:
            return *qWarpSystemPixmaps.getPixmap(SPTR_ICONQUESTION,
                                                 QWarpPixmap::ico);

        default:
            break;
    }
#endif //QT_NO_IMAGEIO_XPM

    return QCommonStyle::stylePixmap(stylepixmap, widget, opt);
}

/*!\reimp
*/
void QWarp4Style::drawComplexControl(ComplexControl ctrl, QPainter *p,
                                     QWidget const* widget,
                                     QRect const& r,
                                     QColorGroup const& cg,
                                     SFlags flags,
                                     SCFlags sub,
                                     SCFlags subActive,
                                     QStyleOption const& opt) const
{
    switch (ctrl)
    {
#ifndef QT_NO_SCROLLBAR
        case CC_ScrollBar:
        {
            QScrollBar const* scrollbar = (QScrollBar const*)widget;
            bool maxedOut = (scrollbar->minValue() == scrollbar->maxValue());

            QRect subline = querySubControlMetrics(ctrl, widget, SC_ScrollBarSubLine, opt),
                  addline = querySubControlMetrics(ctrl, widget, SC_ScrollBarAddLine, opt),
                  subpage = querySubControlMetrics(ctrl, widget, SC_ScrollBarSubPage, opt),
                  addpage = querySubControlMetrics(ctrl, widget, SC_ScrollBarAddPage, opt),
                  slider  = querySubControlMetrics(ctrl, widget, SC_ScrollBarSlider,  opt);
            if(!scrollbar->isEnabled())
            {
                subpage.unite(addpage).unite(slider); // fill the whole, empty bar
                addpage = slider = QRect(); // invalid rectangle
                maxedOut = true; // to avoid setting the Style_Enabled flag
            }
            bool isMin = scrollbar->value() <= scrollbar->minValue(),
                 isMax = scrollbar->value() >= scrollbar->maxValue();

            if((sub & SC_ScrollBarSubLine) && subline.isValid())
                drawPrimitive(PE_ScrollBarSubLine, p, subline, cg,
                              ((maxedOut || isMin) ? Style_Default : Style_Enabled) |
                              ((!isMin && (subActive == SC_ScrollBarSubLine)) ? Style_Down : Style_Default) |
                              ((scrollbar->orientation() == Qt::Horizontal) ? Style_Horizontal : 0));
            if((sub & SC_ScrollBarAddLine) && addline.isValid())
                drawPrimitive(PE_ScrollBarAddLine, p, addline, cg,
                              ((maxedOut || isMax) ? Style_Default : Style_Enabled) |
                              ((!isMax && (subActive == SC_ScrollBarAddLine)) ? Style_Down : Style_Default) |
                              ((scrollbar->orientation() == Qt::Horizontal) ? Style_Horizontal : 0));
            if((sub & SC_ScrollBarSubPage) && subpage.isValid())
                drawPrimitive(PE_ScrollBarSubPage, p, subpage, cg,
                              ((maxedOut) ? Style_Default : Style_Enabled) |
                              ((subActive == SC_ScrollBarSubPage) ? Style_Down : Style_Default) |
                              ((scrollbar->orientation() == Qt::Horizontal) ? Style_Horizontal : 0));
            if((sub & SC_ScrollBarAddPage) && addpage.isValid())
                drawPrimitive(PE_ScrollBarAddPage, p, addpage, cg,
                              ((maxedOut) ? Style_Default : Style_Enabled) |
                              ((subActive == SC_ScrollBarAddPage) ? Style_Down : Style_Default) |
                              ((scrollbar->orientation() == Qt::Horizontal) ? Style_Horizontal : 0));
            if((sub & SC_ScrollBarSlider) && slider.isValid())
                drawPrimitive(PE_ScrollBarSlider, p, slider, cg,
                              ((maxedOut) ? Style_Default : Style_Enabled) |
                              ((subActive == SC_ScrollBarSlider) ? Style_Down : Style_Default) |
                              ((scrollbar->orientation() == Qt::Horizontal) ? Style_Horizontal : 0));

            break;
        }
#endif // QT_NO_SCROLLBAR

        case CC_SpinWidget:
        {
#ifndef QT_NO_SPINWIDGET
            QSpinWidget const* sw = (QSpinWidget const*)widget;
            SFlags flags;
            PrimitiveElement pe;

            if(sub & SC_SpinWidgetFrame)
            {
                qDrawWarpPanel(p, r, cg, -1);
            }

            if((sub & SC_SpinWidgetUp) ||
               (sub & SC_SpinWidgetDown))
            {
                flags = Style_Enabled;
                if((subActive == SC_SpinWidgetUp) ||
                   (subActive == SC_SpinWidgetDown))
                    flags |= Style_Sunken;
                else
                    flags |= Style_Raised;

                // we "misuse" the On flag to indicate whether the
                // "up" part or the "down" part of the button is sunken
                if(subActive == SC_SpinWidgetUp)
                    flags |= Style_On;

                if(sub & SC_SpinWidgetUp)
                {
                    if(sw->buttonSymbols() == QSpinWidget::PlusMinus)
                        pe = PE_SpinWidgetPlus;
                    else
                        pe = PE_SpinWidgetUp;
                }
                else
                {
                    if(sw->buttonSymbols() == QSpinWidget::PlusMinus)
                        pe = PE_SpinWidgetMinus;
                    else
                        pe = PE_SpinWidgetDown;
                }

                QRect re = sw->upRect();
                re.setWidth(re.width() - 2);
                bool reverse = QApplication::reverseLayout();
                if(!reverse)
                    re.moveLeft(re.left() + 2);
                drawPrimitive(pe, p, re, cg, flags);

                // doesn't work yet...
                int x = re.left() - 1;
                if(reverse)
                {
                    x = re.right() + 2;
                    p->setPen(cg.light());
                }
                else
                {
                    p->setPen(cg.dark());
                }
                p->drawLine(x, re.top(), x, re.bottom());
                p->setPen(cg.button());
                x--;
                p->drawLine(x, re.top(), x, re.bottom());
            }

            //if(sub & SC_SpinWidgetDown)
            //{
                // do nothing!
                // note: for the Warp style we put all the up/down button
                // functionality into the "up" button with the diagonal
                // separator
            //}
#endif

            break;
        }

#ifndef QT_NO_TOOLBUTTON
        case CC_ToolButton:
        {
            bool right;
            if((right = (QString("qt_right_btn") == widget->name())) ||
               (QString("qt_left_btn") == widget->name()))
            {
                QToolButton* tb = (QToolButton*)widget;
                QTabBar* ptb = (QTabBar*)tb->parentWidget();
                if((0 == ptb) || !ptb->inherits("QTabBar"))
                    return;
                bool bottom = (QTabBar::RoundedBelow == ptb->shape()) ||
                              (QTabBar::TriangularBelow == ptb->shape());

                QRect r(visualRect(querySubControlMetrics(ctrl, widget, SC_ToolButton, opt), widget));
                if(right)
                    r.addCoords(-3, 0, 0, 0);
                else
                    r.addCoords(0, 0, 3, 0);
                p->setClipRect(r);

                p->setPen(cg.dark());
                if(right)
                {
                    if(bottom)
                        p->drawLine(r.right(), r.top(), r.right(), r.top() + 3);
                    else
                        p->drawLine(r.right(), r.bottom(), r.right(), r.bottom() - 3);
                }

                QRect tr1(r), tr2(r);
                if(right)
                {
                    tr1.addCoords(-1, 0, 1, 0);
                    tr2.addCoords(-21, 0, -21, 0);
                }
                else
                {
                    tr1.addCoords(21, 0, 21, 0);
                    tr2.addCoords(-1, 0, 1, 0);
                }

                QColorGroup const& cg(tb->colorGroup());
                qDrawTabBar(p, tr1, cg, cg.background(), false, bottom);
                qDrawTabBar(p, tr2, cg, cg.background(), false, bottom);

                if(bottom)
                    r.addCoords(0, 2, 0, 2);
                QPointArray triang;
                if(right)
                {
                    triang.putPoints(0, 3,
                                     r.left() + 8,  r.top() + 6,
                                     r.left() + 19, r.top() + 12,
                                     r.left() + 8,  r.top() + 18);
                }
                else
                {
                    triang.putPoints(0, 3,
                                     r.right() - 10,  r.top() + 6,
                                     r.right() - 21, r.top() + 12,
                                     r.right() - 10,  r.top() + 18);
                }
                p->setBrush(QColor(0, 0, 255));
                p->setPen(QPen::NoPen);
                p->drawPolygon(triang);
            }

            else
            {
                QCommonStyle::drawComplexControl(ctrl, p, widget, r, cg, flags, sub,
                                                 subActive, opt);
            }

            break;
        }
#endif // QT_NO_TOOLBUTTON

#ifndef QT_NO_LISTVIEW
        case CC_ListView:
        {
            if(sub & SC_ListView)
            {
                QCommonStyle::drawComplexControl(ctrl, p, widget, r, cg, flags, sub, subActive, opt);
            }

            if(sub & (SC_ListViewBranch | SC_ListViewExpand))
            {
                QListViewItem *item = opt.listViewItem(),
                              *child = item->firstChild();
                QListView* v = item->listView();
                int y = r.y();
                int bx = r.width() / 2;
                int // linetop = 0,
                    linebot = 0;

                // skip the stuff above the exposed rectangle
                while(child && ((y + child->height()) <= 0))
                {
                    y += child->totalHeight();
                    child = child->nextSibling();
                }

                // notes about drawing lines for Warp:
                // 1. Warp type lines follow a different logic than the "normal" lines
                //    for "tree views" in Qt (which follows more or less Windows)
                // 2. In order to ensure partial repaints, it looks like we have to
                //    draw some lines twice: once with the parent and once with the
                //    child (and hopefully to the same place...)

                while(child && (y < r.height()))
                {
                    if(child->isVisible())
                    {
                        int lh;
                        if(!item->multiLinesEnabled())
                            lh = child->height();
                        else
                            lh = p->fontMetrics().height() + 2 * v->itemMargin();
                        lh = QMAX(lh, QApplication::globalStrut().height());
                        if((lh % 2) > 0)
                            lh++;
                        linebot = y + lh / 2;
                        p->setPen(cg.shadow());

                        // parents and grandparents
                        int vlx = bx - v->treeStepSize() + 2,
                            vlyt = linebot - child->height() + 8,
                            vlyb = linebot - child->height() + 9 + child->totalHeight();
                        if(0 <= item->depth())
                        {
                            // vertical line for parent
                            if(item->isOpen())
                            {
                                if(0 == child->nextSibling())
                                    p->drawLine(vlx, vlyt, vlx, linebot - 2);
                                else
                                    p->drawLine(vlx, vlyt, vlx, vlyb);
                            }

                            // vertical lines for grandparents and "older"
                            QListViewItem* gp = item;
                            while(0 < gp->depth())
                            {
                                gp = gp->parent();
                                vlx -= v->treeStepSize();
                                if(0 != gp->nextSibling())
                                    p->drawLine(vlx, vlyt, vlx, vlyb);
                            }
                        }

                        // draw child itself
                        if((child->isExpandable() || child->childCount()) &&
                           (child->height() > 0))
                        {
                            // horizontal line up to button
                            if((0 <= item->depth()) && item->isOpen())
                            {
                                p->drawLine(bx - v->treeStepSize() + 3, linebot - 2,
                                            bx - 6, linebot - 2);
                            }

                            // button
                            QRect rr(bx - 6, linebot - 9, 16, 16);
                            drawPrimitive(PE_ButtonCommand, p, rr, cg,
                                          Style_Raised | Style_ButtonDefault);

                            // plus or minus
                            p->setPen(cg.light());
                            p->drawLine(bx, linebot - 1, bx + 6, linebot - 1);
                            if(!child->isOpen())
                                p->drawLine(bx + 3, linebot + 2, bx + 3, linebot - 4);
                            p->setPen(cg.shadow());
                            p->drawLine(bx - 1, linebot - 2, bx + 5, linebot - 2);
                            if(!child->isOpen())
                                p->drawLine(bx + 2, linebot + 1, bx + 2, linebot - 5);

                            // lines below expanded child
                            if(child->isOpen())
                            {
                                // vertical line
                                p->drawLine(bx + 2,
                                            linebot + child->height() - 15,
                                            bx + 2,
                                            linebot + child->totalHeight() - child->height() - 2);

                                // horizontal lines at child's children
                                QListViewItem* child2 = child->firstChild();
                                int y2 = linebot + child->height() - 2;
                                while(child2 && (y2 < r.height()))
                                {
                                    if(child2->isVisible())
                                    {
                                        p->drawLine(bx + 2, y2,
                                                    bx + v->treeStepSize() - 12, y2);
                                        y2 += child2->totalHeight();
                                    }
                                    child2 = child2->nextSibling();
                                }
                            }
                        }
                        else if((0 <= item->depth()) && item->isOpen())
                        {
                            // horizontal line without button
                            p->setPen(cg.shadow());
                            p->drawLine(bx - v->treeStepSize() + 3, linebot - 2,
                                        bx + 8, linebot - 2);
                        }

                        y += child->totalHeight();
                    }
                    child = child->nextSibling();
                }
            }

            break;
        }
#endif //QT_NO_LISTVIEW

#ifndef QT_NO_COMBOBOX
        case CC_ComboBox:
        {
            if(sub & SC_ComboBoxFrame)
            {
                qDrawWarpPanel(p, r, cg, -1);
                p->setPen(cg.dark());
                int x = r.right() - 2 - 16;
                p->drawLine(x, r.top() + 2, x, r.bottom() - 2);
            }

            if(sub & SC_ComboBoxArrow)
            {
                QRect ar = visualRect(querySubControlMetrics(CC_ComboBox, widget,
                                                             SC_ComboBoxArrow),
                                      widget);
                QBrush fill = cg.brush(QColorGroup::Button);
                bool sunken = subActive == SC_ComboBoxArrow;
                qDrawWarpFilledRect(p, ar, cg.dark(), cg.light(), &fill, 2, sunken);
                p->setPen(cg.button());
                p->drawLine(ar.bottomLeft(), ar.topRight());
                QPixmap const* pm = qWarpSystemPixmaps.getPixmap(SBMP_COMBODOWN,
                                                                 QWarpPixmap::bmp);
                QPoint ppt = ar.topLeft() +
                             QPoint((ar.width() - pm->width()) / 2,
                                    (ar.height() - pm->height()) / 2);
                p->drawPixmap(ppt, *pm);
            }

            if(sub & SC_ComboBoxEditField)
            {
                QComboBox const* cb = (QComboBox const*)widget;
                QRect re = QStyle::visualRect(querySubControlMetrics(CC_ComboBox, widget,
                                                                     SC_ComboBoxEditField),
                                              widget);
                p->fillRect(re, cg.brush(QColorGroup::Base));

                if(cb->hasFocus())
                {
                    p->setPen(cg.highlightedText());
                    p->setBackgroundColor(cg.highlight());
                }
                else
                {
                    p->setPen(cg.text());
                    p->setBackgroundColor(cg.background());
                }

                if(cb->hasFocus() && !cb->editable())
                {
                    re = QStyle::visualRect(subRect(SR_ComboBoxFocusRect, cb), widget);
                    p->fillRect(re, cg.brush(QColorGroup::Highlight));
                }
            }

            break;
        }
#endif  // QT_NO_COMBOBOX

#ifndef QT_NO_SLIDER
        case CC_Slider:
        {
            QSlider const* sl = (QSlider const*)widget;
            QRect groove = querySubControlMetrics(CC_Slider, widget, SC_SliderGroove, opt),
                  handle = querySubControlMetrics(CC_Slider, widget, SC_SliderHandle, opt);

            // reducing the handle length by 2 on each side
            if(sl->orientation() == Horizontal)
            {
                handle.setLeft(handle.left() + 2);
                handle.setRight(handle.right() - 2);
            }
            else
            {
                handle.setTop(handle.top() + 2);
                handle.setBottom(handle.bottom() - 2);
            }

            if((sub & SC_SliderGroove) && groove.isValid())
            {
                if(sl->orientation() == Horizontal)
                {
                    groove.setTop(groove.top() + 4);
                    groove.setBottom(groove.bottom() - 4);
                }
                else
                {
                    groove.setLeft(groove.left() + 4);
                    groove.setRight(groove.right() - 4);
                }

                qDrawWarpFilledRect(p, groove, cg.dark(), cg.light(),
                                    &cg.brush(QColorGroup::Button), 2, true);

                groove.setTop(groove.top() + 2);
                groove.setLeft(groove.left() + 2);
                if(sl->orientation() == Horizontal)
                {
                    groove.setRight(groove.right() - 2);
                    groove.setBottom(groove.bottom() - 1);
                }
                else
                {
                    groove.setRight(groove.right() - 1);
                    groove.setBottom(groove.bottom() - 2);
                }

                if(flags & Style_HasFocus)
                    p->setPen(cg.shadow());
                else
                    p->setPen(cg.dark());

                p->drawRect(groove);
            }

            if(sub & SC_SliderTickmarks)
            {
                int tickOffset = pixelMetric(PM_SliderTickmarkOffset, sl);
                int thickness = pixelMetric(PM_SliderControlThickness, sl);
                int len = pixelMetric(PM_SliderLength, sl);
                int ticks = sl->tickmarks();
                int available = pixelMetric(PM_SliderSpaceAvailable, sl);
                int interval = sl->tickInterval();

                if(interval <= 0 )
                {
                    interval = sl->lineStep();
                    if((qPositionFromValue(sl, interval, available) -
                        qPositionFromValue(sl, 0, available)) < 3)
                        interval = sl->pageStep();
                }

                int fudge = len / 2;
                int pos;

                if(ticks & QSlider::Above)
                {
                    if(sl->orientation() == Horizontal)
                        p->fillRect(0, 0, sl->width(), tickOffset,
                                    cg.brush(QColorGroup::Background));
                    else
                        p->fillRect(0, 0, tickOffset, sl->width(),
                                    cg.brush(QColorGroup::Background));
                    int v = sl->minValue();
                    if (!interval)
                        interval = 1;
                    while(v <= sl->maxValue() + 1)
                    {
                        pos = qPositionFromValue(sl, v, available) + fudge;
                        if(sl->orientation() == Horizontal)
                        {
                            p->setPen(cg.dark());
                            p->drawLine(pos - 1, 0, pos - 1, tickOffset - 1);
                            p->setPen(cg.light());
                            p->drawLine(pos, 0, pos, tickOffset - 1);
                        }
                        else
                        {
                            p->setPen(cg.dark());
                            p->drawLine(0, pos - 1, tickOffset - 1, pos - 1);
                            p->setPen(cg.light());
                            p->drawLine(0, pos, tickOffset - 1, pos);
                        }
                        v += interval;
                    }
                }

                if(ticks & QSlider::Below)
                {
                    if(sl->orientation() == Horizontal)
                        p->fillRect(0, tickOffset + thickness, sl->width(), tickOffset,
                                    cg.brush(QColorGroup::Background));
                    else
                        p->fillRect(tickOffset + thickness, 0, tickOffset, sl->height(),
                                    cg.brush(QColorGroup::Background));
                    int v = sl->minValue();
                    if(!interval)
                        interval = 1;
                    while(v <= sl->maxValue() + 1)
                    {
                        pos = qPositionFromValue(sl, v, available) + fudge;
                        if(sl->orientation() == Horizontal)
                        {
                            p->setPen(cg.dark());
                            p->drawLine(pos - 1, tickOffset + thickness + 1,
                                        pos - 1, tickOffset + thickness + 1 + available - 1);
                            p->setPen(cg.light());
                            p->drawLine(pos, tickOffset + thickness + 1,
                                        pos, tickOffset + thickness + 1 + available - 1);
                        }
                        else
                        {
                            p->setPen(cg.dark());
                            p->drawLine(tickOffset + thickness + 1, pos - 1,
                                        tickOffset + thickness + 1 + available - 1, pos - 1);
                            p->setPen(cg.light());
                            p->drawLine(tickOffset + thickness + 1, pos,
                                        tickOffset + thickness + 1 + available - 1, pos);
                        }
                        v += interval;
                    }
                }
            }

            if(sub & SC_SliderHandle)
            {
                qDrawWarpFilledRect(p, handle, cg.dark(), cg.light(),
                                    &cg.brush(QColorGroup::Button), 2, false);

                if(flags & Style_HasFocus)
                    p->setPen(cg.shadow());
                else
                    p->setPen(cg.dark());

                if(sl->orientation() == Horizontal)
                    p->drawLine(handle.topLeft() + QPoint(5, 2),
                                handle.bottomLeft() + QPoint(5, -2));
                else
                    p->drawLine(handle.topLeft() + QPoint(2, 5),
                                handle.topRight() + QPoint(-2, 5));

                p->setPen(cg.light());

                if(sl->orientation() == Horizontal)
                    p->drawLine(handle.topLeft() + QPoint(6, 2),
                                handle.bottomLeft() + QPoint(6, -2));
                else
                    p->drawLine(handle.topLeft() + QPoint(2, 6),
                                handle.topRight() + QPoint(-2, 6));
            }

            break;
        }
#endif // QT_NO_SLIDER

        default:
        {
            QCommonStyle::drawComplexControl(ctrl, p, widget, r, cg, flags, sub,
                                             subActive, opt);
            break;
        }
    }
}

/*! \reimp */
QRect QWarp4Style::querySubControlMetrics(ComplexControl control,
                                          QWidget const* widget,
                                          SubControl sc,
                                          QStyleOption const& opt) const
{
    switch(control)
    {
#ifndef QT_NO_SCROLLBAR
        case CC_ScrollBar:
        {
            QScrollBar const* scrollbar = (QScrollBar const*)widget;
            int sbextent = pixelMetric(PM_ScrollBarExtent, widget);
            int maxlen = ((scrollbar->orientation() == Qt::Horizontal) ?
                          scrollbar->width() : scrollbar->height()) -
                          (2 * 21); // 2 buttons incl. lines
            int sliderstart = scrollbar->sliderStart();

            // calculate slider length
            int sliderlen;
            if(scrollbar->maxValue() != scrollbar->minValue())
            {
                uint range = scrollbar->maxValue() - scrollbar->minValue();
                sliderlen = (scrollbar->pageStep() * maxlen) /
                            (range + scrollbar->pageStep());
                int slidermin = pixelMetric(PM_ScrollBarSliderMin, widget);
                if((sliderlen < slidermin) || (range > INT_MAX / 2))
                    sliderlen = slidermin;
                if(sliderlen > maxlen)
                    sliderlen = maxlen;
            }
            else
            {
                sliderlen = maxlen;
            }

            // calculate button width (for vertical scroll bars this is the height)
            // note: the "button" includes the lines at the extreme ends of the sb
            int buttonWidth;
            if(scrollbar->orientation() == Qt::Horizontal)
                buttonWidth = QMIN(scrollbar->width() / 2, 21);
            else
                buttonWidth = QMIN(scrollbar->height() / 2, 21);

            switch(sc)
            {
                case SC_ScrollBarSubLine:           // top/left button
                {
                    if(scrollbar->orientation() == Qt::Horizontal)
                        return QRect(0, 0, buttonWidth, sbextent);
                    else
                        return QRect(0, 0, sbextent, buttonWidth);
                }

                case SC_ScrollBarAddLine:           // bottom/right button
                {
                    if(scrollbar->orientation() == Qt::Horizontal)
                        return QRect(scrollbar->width() - buttonWidth, 0,
                                     buttonWidth, sbextent);
                    else
                        return QRect(0, scrollbar->height() - buttonWidth,
                                     sbextent, buttonWidth);
                }

                case SC_ScrollBarSubPage:           // between top/left button and slider
                {
                    if(scrollbar->orientation() == Qt::Horizontal)
                        return QRect(buttonWidth, 0,
                                     sliderstart - buttonWidth, sbextent);
                    else
                        return QRect(0, buttonWidth,
                                     sbextent, sliderstart - buttonWidth);
                }

                case SC_ScrollBarAddPage:           // between bottom/right button and slider
                {
                    if(scrollbar->orientation() == Qt::Horizontal)
                        return QRect(sliderstart + sliderlen, 0,
                                     maxlen + buttonWidth - sliderstart - sliderlen, sbextent);
                    else
                        return QRect(0, sliderstart + sliderlen,
                                     sbextent, maxlen + buttonWidth - sliderstart - sliderlen);
                }

                case SC_ScrollBarGroove:
                {
                    if(scrollbar->orientation() == Qt::Horizontal)
                        return QRect(buttonWidth, 0,
                                     scrollbar->width() - buttonWidth * 2, sbextent);
                    else
                        return QRect(0, buttonWidth,
                                     sbextent, scrollbar->height() - buttonWidth * 2);
                }

                case SC_ScrollBarSlider:
                {
                    if(scrollbar->orientation() == Qt::Horizontal)
                        return QRect(sliderstart, 0, sliderlen, sbextent);
                    else
                        return QRect(0, sliderstart, sbextent, sliderlen);
                }
            }

            break;
        }
#endif // QT_NO_SCROLLBAR

        case CC_SpinWidget:
        {
            int fw = pixelMetric(PM_SpinBoxFrameWidth, widget);
            QSize bs;
            bs.setHeight(widget->height() - 2 * fw);
            if(bs.height() < 16)
                bs.setHeight(16);
            bs.setWidth(bs.height() + 2);
            bs = bs.expandedTo(QApplication::globalStrut());
            int bx = widget->width() - bs.width() - fw,
                by = fw;

            switch(sc)
            {
                case SC_SpinWidgetUp:
                    return QRect(bx, by, bs.width(), bs.height());
                case SC_SpinWidgetDown:
                    return QRect(bx, by + bs.height(), bs.width(), 0);
                case SC_SpinWidgetButtonField:
                    return QRect(bx, by, bs.width(), widget->height() - 2 * fw);
                case SC_SpinWidgetEditField:
                    return QRect(fw, fw,
                                 widget->width() - bs.width() - 2 * fw,
                                 widget->height() - 2 * fw);
                case SC_SpinWidgetFrame:
                    return widget->rect();
                default:
                    break;
            }

            break;
        }

        case CC_ComboBox:
        {
            QRect r(widget->rect());
            int x = r.left(),
                y = r.top(),
                wi = r.width(),
                he = r.height();
            int xpos = x;
            xpos += wi - 2 - 16;

            switch(sc)
            {
                case SC_ComboBoxFrame:
                    return widget->rect();
                case SC_ComboBoxArrow:
                    return QRect(xpos, y + 2, 16, he - 4);
                case SC_ComboBoxEditField:
                    return QRect(x + 2, y + 2, wi - 5 - 16, he - 4);
                case SC_ComboBoxListBoxPopup:
                    return opt.rect();
                default:
                    break;
            }
            break;
        }

        default:
        {
            return QCommonStyle::querySubControlMetrics(control, widget, sc, opt);
        }
   }

    return QRect();
}

/*! \reimp */
int QWarp4Style::styleHint(StyleHint hint,
                           QWidget const* widget,
                           QStyleOption const& opt,
                           QStyleHintReturn *returnData) const
{
    int ret;

    switch (hint)
    {
        case SH_EtchDisabledText:
        case SH_Slider_SnapToValue:
        case SH_PrintDialog_RightAlignButtons:
        case SH_FontDialog_SelectAssociatedText:
        case SH_MenuBar_AltKeyNavigation:
        case SH_MainWindow_SpaceBelowMenuBar:
        case SH_ScrollBar_StopMouseOverSlider:
        {
            ret = 1;
            break;
        }

        case SH_MenuBar_MouseTracking:
        case SH_PopupMenu_MouseTracking:
        case SH_PopupMenu_AllowActiveAndDisabled:
        case SH_PopupMenu_SpaceActivatesItem:
        case SH_PopupMenu_Scrollable:
        case SH_PopupMenu_SloppySubMenus:
        case SH_PopupMenu_SubMenuPopupDelay:
        case SH_ComboBox_ListMouseTracking:
        case SH_ItemView_ChangeHighlightOnFocus:
        case SH_ToolBox_SelectedPageTitleBold:
        {
            ret = 0;
            break;
        }

        case SH_GUIStyle:
        {
            // This value still exists from Qt 1.0 (as it seems), but is never
            // ever used in the current code! So the function of setting it
            // here is only to avoid using one of the other GUI styles still
            // being used (i.e., WindowsStyle and MotifStyle - which in turn are
            // also due to being removed entirely with some new Qt version...)
            // Note: currently it is being used to add some space to the left of
            // the menubar - as that space (another amount...) is also added
            // for other OS styles in QMenuBar.cpp (looks like an 1.0 inheritance!)
            ret = PMStyle;
            break;
        }

        default:
        {
            ret = QCommonStyle::styleHint(hint, widget, opt, returnData);
            break;
        }
    }

    return ret;
}

/*! \reimp */
QRect QWarp4Style::subRect(SubRect r, QWidget const* widget) const
{
    QRect rect;

    switch(r)
    {
        case SR_PushButtonFocusRect:
        case SR_CheckBoxFocusRect:
        case SR_RadioButtonFocusRect:
        {
            QButton const* button = (QButton const*)widget;
            if(!button->pixmap() && button->text().isEmpty())
            {
                switch(r)
                {
                    case SR_PushButtonFocusRect:
                        rect = QRect(widget->rect().center(), QSize(0, 0));
                        break;
                    case SR_CheckBoxFocusRect:
                        rect = subRect(SR_CheckBoxIndicator, widget);
                        break;
                    case SR_RadioButtonFocusRect:
                        rect = subRect(SR_RadioButtonIndicator, widget);
                        break;
                }
                rect.addCoords(1, 1, -1, -1);
                break;
            }
            QPainter* p = new QPainter(button);
            QRect cr;
            switch(r)
            {
                case SR_PushButtonFocusRect:
                    cr = subRect(SR_PushButtonContents, widget);
                    break;
                case SR_CheckBoxFocusRect:
                    cr = subRect(SR_CheckBoxContents, widget);
                    break;
                case SR_RadioButtonFocusRect:
                    cr = subRect(SR_RadioButtonContents, widget);
                    break;
            }
            int align = ((SR_PushButtonFocusRect == r) ?
                        Qt::AlignCenter | Qt::AlignVCenter :
                        Qt::AlignLeft | Qt::AlignVCenter | Qt::ShowPrefix);
            rect = itemRect(p, cr, align,
                            button->isEnabled(), button->pixmap(), button->text());
            delete p;
            rect.addCoords(-1, -1, 1, 1);
            rect = rect.intersect(widget->rect());
            break;
        }

#ifndef QT_NO_SLIDER
        case SR_SliderFocusRect:
        {
            rect = widget->rect();
            break;
        }
#endif // QT_NO_SLIDER

#ifndef QT_NO_PROGRESSBAR
        case SR_ProgressBarGroove:
        case SR_ProgressBarContents:
        {
            QFontMetrics fm((widget ? widget->fontMetrics() :
                                      QApplication::fontMetrics()));
            QProgressBar const* progressbar = (QProgressBar const*)widget;
            int textw = 0;
            if(progressbar->percentageVisible())
                textw = fm.width("100%") + 6;

            // only change for Warp4 compared to "common": the style default
            // is "centered"
            QRect wrect(widget->rect());
            if(!progressbar->indicatorFollowsStyle() &&
               !progressbar->centerIndicator())
                rect.setCoords(wrect.left(), wrect.top(),
                               wrect.right() - textw, wrect.bottom());
            else
                rect = wrect;
            break;
        }

        case SR_ProgressBarLabel:
        {
            QFontMetrics fm((widget ? widget->fontMetrics() :
                                      QApplication::fontMetrics()));
            QProgressBar const* progressbar = (QProgressBar const*)widget;
            int textw = 0;
            if(progressbar->percentageVisible())
                textw = fm.width("100%") + 6;

            // only change for Warp4 compared to "common": the style default
            // is "centered"
            QRect wrect(widget->rect());
            if(!progressbar->indicatorFollowsStyle() &&
               !progressbar->centerIndicator())
                rect.setCoords(wrect.right() - textw, wrect.top(),
                               wrect.right(), wrect.bottom());
            else
                rect = wrect;
            break;
        }
#endif // QT_NO_PROGRESSBAR

        case SR_ToolBoxTabContents:
        {
            rect = widget->rect();
            break;
        }

        default:
        {
            rect = QCommonStyle::subRect(r, widget);
            break;
        }
    }

    return rect;
}

#include "qwarp4style.moc"

#endif
