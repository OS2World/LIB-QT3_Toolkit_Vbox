/****************************************************************************
** QWarp4Style meta object code from reading C++ file 'qwarp4style.h'
**
** Created: Sun Nov 2 20:31:27 2014
**      by: The Qt MOC ($Id: moc_yacc.cpp 2 2005-11-16 15:49:26Z dmik $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../../include/qwarp4style.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *QWarp4Style::className() const
{
    return "QWarp4Style";
}

QMetaObject *QWarp4Style::metaObj = 0;
static QMetaObjectCleanUp cleanUp_QWarp4Style( "QWarp4Style", &QWarp4Style::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString QWarp4Style::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "QWarp4Style", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString QWarp4Style::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "QWarp4Style", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* QWarp4Style::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QCommonStyle::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"QWarp4Style", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_QWarp4Style.setMetaObject( metaObj );
    return metaObj;
}

void* QWarp4Style::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "QWarp4Style" ) )
	return this;
    return QCommonStyle::qt_cast( clname );
}

bool QWarp4Style::qt_invoke( int _id, QUObject* _o )
{
    return QCommonStyle::qt_invoke(_id,_o);
}

bool QWarp4Style::qt_emit( int _id, QUObject* _o )
{
    return QCommonStyle::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool QWarp4Style::qt_property( int id, int f, QVariant* v)
{
    return QCommonStyle::qt_property( id, f, v);
}

bool QWarp4Style::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
