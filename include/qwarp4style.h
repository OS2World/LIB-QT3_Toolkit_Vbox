/****************************************************************************
** $Id: qwarp4style.h 175 2007-11-06 23:17:04Z dmik $
**
** Definition of OS/2 Warp-like style class
**
** Copyright (C) 1992-2003 Trolltech AS.  All rights reserved.
** Copyright (C) 2007-2007 netlabs.org. OS/2 Development.
**
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

#ifndef QWARP4STYLE_H
#define QWARP4STYLE_H

#ifndef QT_H
#include "qcommonstyle.h"
#endif // QT_H

#if !defined(QT_NO_STYLE_WARP4) || defined(QT_PLUGIN)

#if defined(QT_PLUGIN)
#define Q_EXPORT_STYLE_WARP4
#else
#define Q_EXPORT_STYLE_WARP4 Q_EXPORT
#endif

#ifndef QT_NO_ICONSET
class QIconFactory;
#endif

class Q_EXPORT_STYLE_WARP4 QWarp4Style : public QCommonStyle
{
    Q_OBJECT
public:
    QWarp4Style();
    ~QWarp4Style();

    void polish( QApplication * );
    void unPolish( QApplication * );

    void polish( QWidget * );
    void unPolish( QWidget * );

    void polish( QPalette & );

    virtual void polishPopupMenu( QPopupMenu* );

    // new stuff
    void drawPrimitive( PrimitiveElement pe,
                        QPainter *p,
                        const QRect &r,
                        const QColorGroup &cg,
                        SFlags flags = Style_Default,
                        const QStyleOption& = QStyleOption::Default ) const;

    void drawControl( ControlElement element,
                      QPainter *p,
                      const QWidget *widget,
                      const QRect &r,
                      const QColorGroup &cg,
                      SFlags flags = Style_Default,
                      const QStyleOption& = QStyleOption::Default ) const;

    void drawComplexControl( ComplexControl control,
                             QPainter* p,
                             const QWidget* widget,
                             const QRect& r,
                             const QColorGroup& cg,
                             SFlags flags = Style_Default,
                             SCFlags sub = (uint)SC_All,
                             SCFlags subActive = SC_None,
                             const QStyleOption& = QStyleOption::Default ) const;

    QRect querySubControlMetrics( ComplexControl control,
                                  const QWidget *widget,
                                  SubControl sc,
                                  const QStyleOption& = QStyleOption::Default ) const;
    
    int pixelMetric( PixelMetric metric,
                     const QWidget *widget = 0 ) const;

    QSize sizeFromContents( ContentsType contents,
                            const QWidget *widget,
                            const QSize &contentsSize,
                            const QStyleOption& = QStyleOption::Default ) const;

    int styleHint( StyleHint sh, const QWidget *,
                   const QStyleOption & = QStyleOption::Default,
                   QStyleHintReturn* = 0) const;

    QPixmap stylePixmap( StylePixmap stylepixmap,
                         const QWidget *widget = 0,
                         const QStyleOption& = QStyleOption::Default ) const;

    QRect subRect( SubRect r, const QWidget *widget ) const;

private:

    // Disabled copy constructor and operator=
#if defined(Q_DISABLE_COPY)
    QWarp4Style( const QWarp4Style & );
    QWarp4Style& operator=( const QWarp4Style & );
#endif

#ifndef QT_NO_ICONSET
    QIconFactory *oldDefIconFactory;
    QIconFactory *pmDefIconFactory;
#endif
};

#endif // QT_NO_STYLE_WARP4

#endif // QWARP4STYLE_H
