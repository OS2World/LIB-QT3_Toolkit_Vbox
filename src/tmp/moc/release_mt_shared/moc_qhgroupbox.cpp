/****************************************************************************
** QHGroupBox meta object code from reading C++ file 'qhgroupbox.h'
**
** Created: Sun Nov 2 20:30:22 2014
**      by: The Qt MOC ($Id: moc_yacc.cpp 2 2005-11-16 15:49:26Z dmik $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../../include/qhgroupbox.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *QHGroupBox::className() const
{
    return "QHGroupBox";
}

QMetaObject *QHGroupBox::metaObj = 0;
static QMetaObjectCleanUp cleanUp_QHGroupBox( "QHGroupBox", &QHGroupBox::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString QHGroupBox::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "QHGroupBox", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString QHGroupBox::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "QHGroupBox", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* QHGroupBox::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QGroupBox::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"QHGroupBox", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_QHGroupBox.setMetaObject( metaObj );
    return metaObj;
}

void* QHGroupBox::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "QHGroupBox" ) )
	return this;
    return QGroupBox::qt_cast( clname );
}

bool QHGroupBox::qt_invoke( int _id, QUObject* _o )
{
    return QGroupBox::qt_invoke(_id,_o);
}

bool QHGroupBox::qt_emit( int _id, QUObject* _o )
{
    return QGroupBox::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool QHGroupBox::qt_property( int id, int f, QVariant* v)
{
    return QGroupBox::qt_property( id, f, v);
}

bool QHGroupBox::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
