/****************************************************************************
** $Id: qfont_pm.cpp 174 2007-11-06 22:27:57Z dmik $
**
** Implementation of QFont, QFontMetrics and QFontInfo classes for OS/2
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

#include "qfont.h"
#include "qfontdata_p.h"
#include "qfontengine_p.h"
#include "qfontmetrics.h"
#include "qfontinfo.h"

#include "qwidget.h"
#include "qpainter.h"
#include "qdict.h"
#include "qcache.h"
#include <limits.h>
#include "qt_os2.h"
#include "qapplication_p.h"
#include "qapplication.h"
#include "qpaintdevicemetrics.h"
#include <private/qunicodetables_p.h>
#include <qfontdatabase.h>


QString qt_pm_default_family( uint styleHint )
{
    switch( styleHint ) {
        case QFont::Times:
            return QString::fromLatin1( "Times New Roman" );
        case QFont::Courier:
            return QString::fromLatin1( "Courier" );
        case QFont::Decorative:
            return QString::fromLatin1( "Times New Roman" );
        case QFont::Helvetica:
            return QString::fromLatin1( "Helvetica" );
        case QFont::System: {
#ifndef QT_PM_NO_DEFAULTFONT_OVERRIDE
            static ULONG ver[2] = {0};
            if ( ver[0] == ver[1] == 0 ) {
                DosQuerySysInfo( QSV_VERSION_MAJOR, QSV_VERSION_MINOR,
                                 &ver, sizeof(ver) );
            }
            if ( ver[0] == 20 && ver[1] >= 40 )
                return QString::fromLatin1( "WarpSans" );
            else
#endif
            return QString::fromLatin1( "System Proportional" );
        }
        default:
            // the family to be used when no StyleHint is set (in accordance
            // with the font matching algorithm stated in QFont docs)
            return QString::fromLatin1( "Helvetica" );
    }
//@@TODO (dmik): should we also add default font substitutions to
//  initFontSubst()?
}

/*****************************************************************************
  QFont member functions
 *****************************************************************************/

QFont::Script QFontPrivate::defaultScript = QFont::UnknownScript;

void QFont::initialize()
{
    if ( QFontCache::instance )
	return;
    new QFontCache();

    // #########
    QFontPrivate::defaultScript = QFont::Latin;
}

void QFont::cleanup()
{
    delete QFontCache::instance;
}

void QFontPrivate::load( QFont::Script script )
{
    // NOTE: the OS/2 implementation of this function is similar to
    // X11 and Windows implementations (which are identical). if they
    // change, probably, changes should come here as well...

#ifdef QT_CHECK_STATE
    // sanity checks
    if (!QFontCache::instance)
        qWarning("Must construct a QApplication before a QFont");
    Q_ASSERT( script >= 0 && script < QFont::LastPrivateScript );
#endif // QT_CHECK_STATE

    QFontDef req = request;
    int dpi;
    if ( paintdevice ) {
        dpi = QPaintDeviceMetrics( paintdevice ).logicalDpiY();
    } else {
        DevQueryCaps( GpiQueryDevice( qt_display_ps() ),
                      CAPS_VERTICAL_FONT_RES, 1, (PLONG) &dpi );
    }
    if (req.pointSize != -1)
        req.pixelSize = (req.pointSize * dpi + 360) / 720;
    else // assuming (req.pixelSize != -1)
        req.pointSize = 0;

    if ( ! engineData ) {
        QFontCache::Key key( req, QFont::NoScript, (int)paintdevice );

        // look for the requested font in the engine data cache
        engineData = QFontCache::instance->findEngineData( key );

        if ( ! engineData) {
            // create a new one
            engineData = new QFontEngineData;
            QFontCache::instance->insertEngineData( key, engineData );
        } else {
            engineData->ref();
        }
    }

    // the cached engineData could have already loaded the engine we want
//@@TODO (dmik): remove?
//    if ( engineData->engines[script] ) return;
    if ( engineData->engine ) return;

    // load the font
    QFontEngine *engine = 0;
    //    double scale = 1.0; // ### TODO: fix the scale calculations

    // list of families to try
    QStringList family_list;

    if (!req.family.isEmpty()) {
        family_list = QStringList::split( ',', req.family );

        // append the substitute list for each family in family_list
        QStringList subs_list;
        QStringList::ConstIterator it = family_list.begin(), end = family_list.end();
        for ( ; it != end; ++it )
            subs_list += QFont::substitutes( *it );
        family_list += subs_list;

//@@TODO (dmik): need something here?
        // append the default fallback font for the specified script
        // family_list << ... ; ###########

        // add the family corresponding to font's styleHint
        QString hintFamily = qt_pm_default_family( request.styleHint );
        if ( ! family_list.contains( hintFamily ) )
            family_list << hintFamily;

//@@TODO (dmik): should we also add QFont::lastResortFamily() and
//  QFont::lastResortFont() to the list as stated in QFont docs about the
//  font matching algorithm? Also it's not clear why QApplication font is
//  below added to the list.

        // add the default family
        QString defaultFamily = QApplication::font().family();
        if ( ! family_list.contains( defaultFamily ) )
            family_list << defaultFamily;

        // add QFont::defaultFamily() to the list, for compatibility with
        // previous versions
        family_list << QApplication::font().defaultFamily();
    }

    // null family means find the first font matching the specified script
    family_list << QString::null;

    QStringList::ConstIterator it = family_list.begin(), end = family_list.end();
    for ( ; ! engine && it != end; ++it ) {
        req.family = *it;

        engine = QFontDatabase::findFont( script, this, req );
        if ( engine ) {
            if ( engine->type() != QFontEngine::Box )
                break;

            if ( ! req.family.isEmpty() )
                engine = 0;

            continue;
        }
    }

    engine->ref();
//@@TODO (dmik): remove?
//    engineData->engines[script] = engine;
    engineData->engine = engine;
}

//@@TODO (dmik): do we actually need any handle?
//PFATTRS QFont::handle() const
//{
//    QFontEngine *engine = d->engineForScript( QFont::NoScript );
//    return &engine->fa;
//}

void QFont::selectTo( HPS hps ) const
{
    QFontEngine *engine = d->engineForScript( QFont::NoScript );
#ifdef QT_CHECK_STATE
    Q_ASSERT( engine != 0 );
#endif // QT_CHECK_STATE
    engine->selectTo( hps );
}

QString QFont::rawName() const
{
//@@TODO (dmik): use szFacename here?
    return family();
}

void QFont::setRawName( const QString &name )
{
    setFamily( name );
}


bool QFont::dirty() const
{
    return !d->engineData;
}


QString QFont::defaultFamily() const
{
    return qt_pm_default_family( d->request.styleHint );
}

QString QFont::lastResortFamily() const
{
    return qt_pm_default_family( QFont::AnyStyle );
}

QString QFont::lastResortFont() const
{
    return QString::fromLatin1( "System Proportional" );
}



/*****************************************************************************
  QFontMetrics member functions
 *****************************************************************************/

//@@TODO (dmik): do we need a separate (other than in qfont.cpp)
//  implementation of this func?
//int QFontMetrics::leftBearing(QChar ch) const
//{
//#ifdef Q_OS_TEMP
//    return 0;
//#else
//    QFontEngine *engine = d->engineForScript( (QFont::Script) fscript );
//#ifdef QT_CHECK_STATE
//    Q_ASSERT( engine != 0 );
//#endif // QT_CHECK_STATE
//
//    if ( IS_TRUETYPE ) {
//	ABC abc;
//	QT_WA( {
//	    uint ch16 = ch.unicode();
//	    GetCharABCWidths(engine->dc(),ch16,ch16,&abc);
//	} , {
//	    uint ch8;
//	    if ( ch.row() || ch.cell() > 127 ) {
//		QCString w = QString(ch).local8Bit();
//		if ( w.length() != 1 )
//		    return 0;
//		ch8 = (uchar)w[0];
//	    } else {
//		ch8 = ch.cell();
//	    }
//	    GetCharABCWidthsA(engine->dc(),ch8,ch8,&abc);
//	} );
//	return abc.abcA;
//    } else {
//	QT_WA( {
//	    uint ch16 = ch.unicode();
//	    ABCFLOAT abc;
//	    GetCharABCWidthsFloat(engine->dc(),ch16,ch16,&abc);
//	    return int(abc.abcfA);
//	} , {
//	    return 0;
//	} );
//    }
//    return 0;
//#endif
//}


//@@TODO (dmik): do we need a separate (other than in qfont.cpp)
//  implementation of this func?
//int QFontMetrics::rightBearing(QChar ch) const
//{
//    return 0;
//#ifdef Q_OS_TEMP
//	return 0;
//#else
//    QFontEngine *engine = d->engineForScript( (QFont::Script) fscript );
//#ifdef QT_CHECK_STATE
//    Q_ASSERT( engine != 0 );
//#endif // QT_CHECK_STATE
//
//    if ( IS_TRUETYPE ) {
//	ABC abc;
//	QT_WA( {
//	    uint ch16 = ch.unicode();
//	    GetCharABCWidths(engine->dc(),ch16,ch16,&abc);
//	    return abc.abcC;
//	} , {
//	    uint ch8;
//	    if ( ch.row() || ch.cell() > 127 ) {
//		QCString w = QString(ch).local8Bit();
//		if ( w.length() != 1 )
//		    return 0;
//		ch8 = (uchar)w[0];
//	    } else {
//		ch8 = ch.cell();
//	    }
//	    GetCharABCWidthsA(engine->dc(),ch8,ch8,&abc);
//	} );
//	return abc.abcC;
//    } else {
//	QT_WA( {
//	    uint ch16 = ch.unicode();
//	    ABCFLOAT abc;
//	    GetCharABCWidthsFloat(engine->dc(),ch16,ch16,&abc);
//	    return int(abc.abcfC);
//	} , {
//	    return -TMW.tmOverhang;
//	} );
//    }
//
//    return 0;
//#endif
//}


int QFontMetrics::width( QChar ch ) const
{
    if ( ::category( ch ) == QChar::Mark_NonSpacing )
	return 0;

    QFont::Script script;
    SCRIPT_FOR_CHAR( script, ch );

    QFontEngine *engine = d->engineForScript( script );
#ifdef QT_CHECK_STATE
    Q_ASSERT( engine != 0 );
#endif // QT_CHECK_STATE

    if ( painter )
	painter->setNativeXForm( FALSE /* assumeYNegation */);

    glyph_t glyphs[8];
    advance_t advances[8];

    int nglyphs = 7;
    engine->stringToCMap( &ch, 1, glyphs, advances, &nglyphs, FALSE );

    if ( painter )
	painter->clearNativeXForm();

    return advances[0];
}


int QFontMetrics::charWidth( const QString &str, int pos ) const
{
    if ( pos < 0 || pos > (int)str.length() )
	return 0;

    if ( painter )
	painter->setNativeXForm( FALSE /* assumeYNegation */);

    QTextEngine layout( str,  d );
    layout.itemize( QTextEngine::WidthOnly );
    int w = layout.width( pos, 1 );

    if ( painter )
	painter->clearNativeXForm();

    return w;
}
