/****************************************************************************
** SizeHandle meta object code from reading C++ file 'sizehandle.h'
**
** Created: Sun Nov 2 20:36:27 2014
**      by: The Qt MOC ($Id: moc_yacc.cpp 2 2005-11-16 15:49:26Z dmik $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../sizehandle.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *SizeHandle::className() const
{
    return "SizeHandle";
}

QMetaObject *SizeHandle::metaObj = 0;
static QMetaObjectCleanUp cleanUp_SizeHandle( "SizeHandle", &SizeHandle::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString SizeHandle::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "SizeHandle", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString SizeHandle::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "SizeHandle", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* SizeHandle::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"SizeHandle", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_SizeHandle.setMetaObject( metaObj );
    return metaObj;
}

void* SizeHandle::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "SizeHandle" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool SizeHandle::qt_invoke( int _id, QUObject* _o )
{
    return QWidget::qt_invoke(_id,_o);
}

bool SizeHandle::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool SizeHandle::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool SizeHandle::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES