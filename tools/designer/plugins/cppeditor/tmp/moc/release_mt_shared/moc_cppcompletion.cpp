/****************************************************************************
** CppEditorCompletion meta object code from reading C++ file 'cppcompletion.h'
**
** Created: Sun Nov 2 20:38:40 2014
**      by: The Qt MOC ($Id: moc_yacc.cpp 2 2005-11-16 15:49:26Z dmik $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../cppcompletion.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *CppEditorCompletion::className() const
{
    return "CppEditorCompletion";
}

QMetaObject *CppEditorCompletion::metaObj = 0;
static QMetaObjectCleanUp cleanUp_CppEditorCompletion( "CppEditorCompletion", &CppEditorCompletion::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString CppEditorCompletion::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "CppEditorCompletion", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString CppEditorCompletion::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "CppEditorCompletion", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* CppEditorCompletion::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = EditorCompletion::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"CppEditorCompletion", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_CppEditorCompletion.setMetaObject( metaObj );
    return metaObj;
}

void* CppEditorCompletion::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "CppEditorCompletion" ) )
	return this;
    return EditorCompletion::qt_cast( clname );
}

bool CppEditorCompletion::qt_invoke( int _id, QUObject* _o )
{
    return EditorCompletion::qt_invoke(_id,_o);
}

bool CppEditorCompletion::qt_emit( int _id, QUObject* _o )
{
    return EditorCompletion::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool CppEditorCompletion::qt_property( int id, int f, QVariant* v)
{
    return EditorCompletion::qt_property( id, f, v);
}

bool CppEditorCompletion::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
