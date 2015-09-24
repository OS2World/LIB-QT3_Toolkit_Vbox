/****************************************************************************
** QSizeGrip meta object code from reading C++ file 'qsizegrip.h'
**
** Created: Sun Nov 2 20:29:56 2014
**      by: The Qt MOC ($Id: moc_yacc.cpp 2 2005-11-16 15:49:26Z dmik $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../../include/qsizegrip.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *QSizeGrip::className() const
{
    return "QSizeGrip";
}

QMetaObject *QSizeGrip::metaObj = 0;
static QMetaObjectCleanUp cleanUp_QSizeGrip( "QSizeGrip", &QSizeGrip::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString QSizeGrip::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "QSizeGrip", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString QSizeGrip::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "QSizeGrip", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* QSizeGrip::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"QSizeGrip", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_QSizeGrip.setMetaObject( metaObj );
    return metaObj;
}

void* QSizeGrip::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "QSizeGrip" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool QSizeGrip::qt_invoke( int _id, QUObject* _o )
{
    return QWidget::qt_invoke(_id,_o);
}

bool QSizeGrip::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool QSizeGrip::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool QSizeGrip::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
