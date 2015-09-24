/****************************************************************************
** $Id: qsettings_pm.cpp 8 2005-11-16 19:36:46Z dmik $
**
** Implementation of QSettings class
**
** Copyright (C) 1992-2002 Trolltech AS.  All rights reserved.
** Copyright (C) 2004 Norman ASA.  Initial OS/2 Port.
** Copyright (C) 2005 netlabs.org.  Further OS/2 Development.
**
** This file is part of the tools module of the Qt GUI Toolkit.
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

#include "qsettings.h"
#include <private/qsettings_p.h>
#include "qregexp.h"

#ifndef QT_NO_SETTINGS

/* Define some Win32 stuff to use Open32 Reg* functions from REGISTRY.DLL */

extern "C" {

//@@TODO (dmik): no UNICODE support at the moment
#define QT_WA( uni, ansi ) ansi

typedef wchar_t WCHAR;
typedef WCHAR TCHAR;

// These definitions come from os2wdef.h from the OS/2 Toolkit

#define APIENTRY    _System

typedef char                CHAR ,   *PCHAR ,  *LPCHAR;
typedef int                 INT,     *PINT,    *LPINT;
typedef long                LONG,    *PLONG,   *LPLONG;
typedef unsigned char       UCHAR ,  *PUCHAR,  *LPUCHAR;
typedef unsigned long       ULONG ,  *PULONG , *LPULONG;
typedef void                VOID,    *PVOID,   *LPVOID;

typedef UCHAR               BYTE,    *PBYTE,   *LPBYTE;
typedef ULONG               DWORD,   *PDWORD,  *LPDWORD;

typedef INT                 BOOL,    *PBOOL,   *LPBOOL;

typedef WCHAR               *LPWSTR, *PWSTR;
typedef const WCHAR         *LPCWSTR, *PCWSTR;

typedef CHAR                *LPSTR,  *PSTR;
typedef const CHAR          *LPCSTR, *PCSTR;

#define CONST       const

// These definitions come from os2win.h from the OS/2 Toolkit

typedef DWORD       ACCESS_MASK;
typedef ACCESS_MASK REGSAM;

typedef DWORD       HKEY, *PHKEY;

typedef struct _tagSECURITY_ATTRIBUTES {
    DWORD                            nLength;
    LPVOID                           lpSecurityDescriptor;
    BOOL                             bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _tagFILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

#define HKEY_LOCAL_MACHINE      0xFFFFFFEFL
#define HKEY_CURRENT_USER       0xFFFFFFEEL
#define HKEY_USERS              0xFFFFFFEDL
#define HKEY_CLASSES_ROOT       0xFFFFFFECL

//#define DELETE                       0x00010000
#define READ_CONTROL                 0x00020000
//#define WRITE_DAC                    0x00040000
//#define WRITE_OWNER                  0x00080000
//#define SYNCHRONIZE                  0x00100000

//#define SPECIFIC_RIGHTS_ALL          0x0000FFFF
//#define STANDARD_RIGHTS_READ         (READ_CONTROL)
//#define STANDARD_RIGHTS_WRITE        (READ_CONTROL)
//#define STANDARD_RIGHTS_EXECUTE      (READ_CONTROL)
//#define STANDARD_RIGHTS_REQUIRED     0x000F0000
#define STANDARD_RIGHTS_ALL          0x001F0000

#define KEY_QUERY_VALUE         0x0001
#define KEY_SET_VALUE           0x0002
#define KEY_CREATE_SUB_KEY      0x0004
#define KEY_ENUMERATE_SUB_KEYS  0x0008
#define KEY_NOTIFY              0x0010
#define KEY_CREATE_LINK         0x0020

#define REG_OPTION_NON_VOLATILE 0x00000000L
#define REG_OPTION_VOLATILE     0x00000001L
#define REG_CREATED_NEW_KEY     0x00000001L
#define REG_OPENED_EXISTING_KEY 0x00000002L

#define KEY_READ         READ_CONTROL | KEY_QUERY_VALUE |\
                           KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY

#define KEY_WRITE        READ_CONTROL | KEY_SET_VALUE | KEY_CREATE_SUB_KEY

#define KEY_EXECUTE      KEY_READ

#define KEY_ALL_ACCESS   STANDARD_RIGHTS_ALL | KEY_QUERY_VALUE |\
                           KEY_SET_VALUE | KEY_CREATE_SUB_KEY |\
                           KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY |\
                           KEY_CREATE_LINK

#define REG_NONE                  0
#define REG_SZ                    1
#define REG_EXPAND_SZ             2
#define REG_BINARY                3
#define REG_DWORD                 4
#define REG_DWORD_LITTLE_ENDIAN   4
#define REG_DWORD_BIG_ENDIAN      5
#define REG_LINK                  6
#define REG_MULTI_SZ              7
#define REG_RESOURCE_LIST         8

// These definitions come from winreg.h from MSVC 6.0
LONG APIENTRY RegCloseKey( HKEY hKey );
LONG APIENTRY RegCreateKeyExA( HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition );
LONG APIENTRY RegCreateKeyExW( HKEY hKey,LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition );
LONG APIENTRY RegDeleteKeyA( HKEY hKey, LPCSTR lpSubKey);
LONG APIENTRY RegDeleteKeyW( HKEY hKey, LPCWSTR lpSubKey);
LONG APIENTRY RegDeleteValueA( HKEY hKey, LPCSTR lpValueName );
LONG APIENTRY RegDeleteValueW( HKEY hKey, LPCWSTR lpValueName );
LONG APIENTRY RegEnumKeyExA( HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcbName, LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcbClass, PFILETIME lpftLastWriteTime );
LONG APIENTRY RegEnumKeyExW( HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcbName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcbClass, PFILETIME lpftLastWriteTime);
LONG APIENTRY RegEnumValueA( HKEY hKey, DWORD dwIndex, LPSTR lpValueName, LPDWORD lpcbValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData );
LONG APIENTRY RegEnumValueW( HKEY hKey, DWORD dwIndex, LPWSTR lpValueName, LPDWORD lpcbValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData );
LONG APIENTRY RegFlushKey( HKEY hKey );
LONG APIENTRY RegOpenKeyExA( HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult );
LONG APIENTRY RegOpenKeyExW( HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult );
LONG APIENTRY RegQueryInfoKeyA( HKEY hKey, LPSTR lpClass, LPDWORD lpcbClass, LPDWORD lpReserved, LPDWORD lpcSubKeys, LPDWORD lpcbMaxSubKeyLen, LPDWORD lpcbMaxClassLen, LPDWORD lpcValues, LPDWORD lpcbMaxValueNameLen, LPDWORD lpcbMaxValueLen, LPDWORD lpcbSecurityDescriptor, PFILETIME lpftLastWriteTime );
LONG APIENTRY RegQueryInfoKeyW( HKEY hKey, LPWSTR lpClass, LPDWORD lpcbClass, LPDWORD lpReserved, LPDWORD lpcSubKeys, LPDWORD lpcbMaxSubKeyLen, LPDWORD lpcbMaxClassLen, LPDWORD lpcValues, LPDWORD lpcbMaxValueNameLen, LPDWORD lpcbMaxValueLen, LPDWORD lpcbSecurityDescriptor, PFILETIME lpftLastWriteTime );
LONG APIENTRY RegQueryValueExA( HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData );
LONG APIENTRY RegQueryValueExW( HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData );
LONG APIENTRY RegSetValueExA( HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, CONST BYTE* lpData, DWORD cbData );
LONG APIENTRY RegSetValueExW( HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType, CONST BYTE* lpData, DWORD cbData );

// These definitions come from winerror.h from MSVC 6.0
#define ERROR_SUCCESS                    0L
#define ERROR_FILE_NOT_FOUND             2L
#define ERROR_ACCESS_DENIED              5L
#define ERROR_NO_MORE_ITEMS              259L

} // extern "C"

/* End of Win32 definitions */


static bool settingsTryUser = TRUE;
static bool settingsTryLocal = TRUE;
static QString *settingsBasePath = 0;

void Q_EXPORT qt_setSettingsTryUser( bool tryUser )
{
    settingsTryUser = tryUser;
}

void Q_EXPORT qt_setSettingsTryLocal( bool tryLocal )
{
    settingsTryLocal = tryLocal;
}

void Q_EXPORT qt_setSettingsBasePath( const QString &base )
{
    if ( settingsBasePath ) {
	qWarning( "qt_setSettingsBasePath has to be called without any settings object being instantiated!" );
	return;
    }
    settingsBasePath = new QString( base );
}

class QSettingsSysPrivate
{
public:
    QSettingsSysPrivate( QSettingsPrivate *priv );
    ~QSettingsSysPrivate();

    HKEY user;
    HKEY local;

    QString folder( const QString& );
    QString entry( const QString& );

    QString validateKey( const QString &key );

    bool writeKey( const QString &key, const QByteArray &value, ulong type );
    QByteArray readKey( const QString &key, ulong &type );
    HKEY readKeyHelper( HKEY root, const QString &folder, const QString &entry, ulong &size );

    HKEY openKey( const QString &key, bool write, bool remove = FALSE );

    QStringList paths;

private:
    QSettingsPrivate *d;

    static uint refCount;
};

uint QSettingsSysPrivate::refCount = 0;

QSettingsSysPrivate::QSettingsSysPrivate( QSettingsPrivate *priv )
    : d( priv )
{
    paths.append( "" );
    if ( !settingsBasePath ) {
	settingsBasePath = new QString("Software");
    }
    refCount++;
    local = 0;
    user  = 0 ;

    LONG res = ERROR_SUCCESS;
    if ( settingsTryLocal ) {
	QT_WA( {
	    res = RegOpenKeyExW( HKEY_LOCAL_MACHINE, NULL, 0, KEY_ALL_ACCESS, &local );
	} , {
	    res = RegOpenKeyExA( HKEY_LOCAL_MACHINE, NULL, 0, KEY_ALL_ACCESS, &local );
	} );

	if ( res != ERROR_SUCCESS ) {
	    QT_WA( {
		res = RegOpenKeyExW( HKEY_LOCAL_MACHINE, NULL, 0, KEY_READ, &local );
	    } , {
		res = RegOpenKeyExA( HKEY_LOCAL_MACHINE, NULL, 0, KEY_READ, &local );
	    } );
	    if ( res != ERROR_SUCCESS ) {
		local = NULL;
	    }
	}
    }

    if ( settingsTryUser ) {
	QT_WA( {
	    res = RegOpenKeyExW( HKEY_CURRENT_USER, NULL, 0, KEY_ALL_ACCESS, &user );
	} , {
	    res = RegOpenKeyExA( HKEY_CURRENT_USER, NULL, 0, KEY_ALL_ACCESS, &user );
	} );

	if ( res != ERROR_SUCCESS ) {
	    QT_WA( {
		res = RegOpenKeyExW( HKEY_CURRENT_USER, NULL, 0, KEY_READ, &user );
	    } , {
		res = RegOpenKeyExA( HKEY_CURRENT_USER, NULL, 0, KEY_READ, &user );
	    } );
	    if ( res != ERROR_SUCCESS ) {
		user = NULL;
	    }
	}
    }

#if defined(QT_CHECK_STATE)
    if ( !local && !user )
	qSystemWarning( "Error opening registry!", res );
#endif
}

QSettingsSysPrivate::~QSettingsSysPrivate()
{
    LONG res;
    if ( local ) {
	res = RegCloseKey( local );
#if defined(QT_CHECK_STATE)
	if ( res != ERROR_SUCCESS )
	    qSystemWarning( "Error closing local machine!", res );
#endif
    }
    if ( user ) {
	res = RegCloseKey( user );
#if defined(QT_CHECK_STATE)
	if ( res != ERROR_SUCCESS )
	    qSystemWarning( "Error closing current user!", res );
#endif
    }

    // Make sure that we only delete the base path if no one else is using it anymore
    if (refCount > 0) {
	refCount--;

	if (refCount == 0) {
	    delete settingsBasePath;
	    settingsBasePath = 0;
	}
    }
}

QString QSettingsSysPrivate::validateKey( const QString &key )
{
    if ( key.isEmpty() )
	return key;

    QString newKey = key;
    newKey = newKey.replace( QRegExp( "[/]+" ), "\\" );

    if ( newKey[0] != '\\' )
	newKey = "\\" + newKey;
    if ( newKey[(int)newKey.length() - 1] == '\\' )
	newKey = newKey.left( newKey.length() - 1 );

    return newKey;
}

QString QSettingsSysPrivate::folder( const QString &key )
{
    QString k = validateKey( key );
    Q_ASSERT(settingsBasePath);
    return *settingsBasePath + k.left( k.findRev( "\\" ) );
}

QString QSettingsSysPrivate::entry( const QString &key )
{
    QString k = validateKey( key );
    return k.right( k.length() - k.findRev( "\\" ) - 1 );
}

HKEY QSettingsSysPrivate::openKey( const QString &key, bool write, bool remove )
{
    QString f = folder( key );

    HKEY handle = 0;
    LONG res = ERROR_FILE_NOT_FOUND;

    // if we write and there is a user specific setting, overwrite that
    if ( (write||remove) && user ) {
	QT_WA( {
	    if ( remove )
		res = RegOpenKeyExW( user, (TCHAR*)f.ucs2(), 0, KEY_ALL_ACCESS, &handle );
	    else
		res = RegOpenKeyExW( user, (TCHAR*)f.ucs2(), 0, KEY_WRITE, &handle );
	} , {
	    if ( remove )
		res = RegOpenKeyExA( user, f.local8Bit(), 0, KEY_ALL_ACCESS, &handle );
	    else
		res = RegOpenKeyExA( user, f.local8Bit(), 0, KEY_WRITE, &handle );
	} );
    }

    if ( res != ERROR_SUCCESS && local && d->globalScope ) {
	QT_WA( {
	    if ( write && !remove )
		res = RegCreateKeyExW( local, (TCHAR*)f.ucs2(), 0, empty_t, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &handle, NULL );
	    else if ( !write && !remove )
		res = RegOpenKeyExW( local, (TCHAR*)f.ucs2(), 0, KEY_READ, &handle );
	    else
		res = RegOpenKeyExW( local, (TCHAR*)f.ucs2(), 0, KEY_ALL_ACCESS, &handle );
	} , {
	    if ( write && !remove )
		res = RegCreateKeyExA( local, f.local8Bit(), 0, "", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &handle, NULL );
	    else if ( !write && !remove )
		res = RegOpenKeyExA( local, f.local8Bit(), 0, KEY_READ, &handle );
	    else
		res = RegOpenKeyExA( local, f.local8Bit(), 0, KEY_ALL_ACCESS, &handle );
	} );
    }
    if ( !handle && user ) {
	QT_WA( {
	    if ( write && !remove )
		res = RegCreateKeyExW( user, (TCHAR*)f.ucs2(), 0, empty_t, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &handle, NULL );
	    else if ( !write && !remove )
		res = RegOpenKeyExW( user, (TCHAR*)f.ucs2(), 0, KEY_READ, &handle );
	    else
		res = RegOpenKeyExW( user, (TCHAR*)f.ucs2(), 0, KEY_ALL_ACCESS, &handle );
	} , {
	    if ( write && !remove )
		res = RegCreateKeyExA( user, f.local8Bit(), 0, "", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &handle, NULL );
	    else if ( !write && !remove )
		res = RegOpenKeyExA( user, f.local8Bit(), 0, KEY_READ, &handle );
	    else
		res = RegOpenKeyExA( user, f.local8Bit(), 0, KEY_ALL_ACCESS, &handle );
	} );
    }
    return handle;
}

bool QSettingsSysPrivate::writeKey( const QString &key, const QByteArray &value, ulong type )
{
    QString e;
    LONG res = ERROR_ACCESS_DENIED;

    HKEY handle = 0;
    for ( QStringList::Iterator it = paths.fromLast(); it != paths.end(); --it ) {
	QString k = *it + "/" + key;
	e = entry( k );
	handle = openKey( k, TRUE );
	if ( handle )
	    break;
    }
    if ( !handle )
	return FALSE;

    if (e == "Default" || e == "." )
	e = "";

    if ( value.size() ) {
	QT_WA( {
	    res = RegSetValueExW( handle, e.isEmpty() ? 0 : (TCHAR*)e.ucs2(), 0, type, (const uchar*)value.data(), value.size() );
	} , {
	    res = RegSetValueExA( handle, e.isEmpty() ? (const char*)0 : (const char*)e.local8Bit(), 0, type, (const uchar*)value.data(), value.size() );
	} );

	if ( res != ERROR_SUCCESS ) {
#if defined(QT_CHECK_STATE)
	    qSystemWarning( "Couldn't write value " + key, res );
#endif
	    return FALSE;
	}
    }

    RegCloseKey( handle );
    return TRUE;
}

HKEY QSettingsSysPrivate::readKeyHelper( HKEY root, const QString &folder, const QString &entry, ulong &size )
{
    HKEY handle;
    LONG res = ERROR_ACCESS_DENIED;
    QT_WA( {
	res = RegOpenKeyExW( root, (TCHAR*)folder.ucs2(), 0, KEY_READ, &handle );
    } , {
	res = RegOpenKeyExA( root, folder.local8Bit(), 0, KEY_READ, &handle );
    } );

    if ( res == ERROR_SUCCESS ) {
	QT_WA( {
	    res = RegQueryValueExW( handle, entry.isEmpty() ? 0 : (TCHAR*)entry.ucs2(), NULL, NULL, NULL, &size );
	} , {
	    res = RegQueryValueExA( handle, entry.isEmpty() ? (const char*)0 : (const char*)entry.local8Bit(), NULL, NULL, NULL, &size );
	} );
    }
    if ( res != ERROR_SUCCESS ) {
	RegCloseKey( handle );
	size = 0;
	handle = 0;
    }

    return handle;
}

QByteArray QSettingsSysPrivate::readKey( const QString &key, ulong &type )
{
    HKEY handle = 0;
    ulong size = 0;
    QString e;
    type = REG_NONE;

    if ( user ) {
	for ( QStringList::Iterator it = paths.fromLast(); it != paths.end(); --it ) {
	    if ( handle ) {
		RegCloseKey( handle );
		handle = 0;
	    }
	    QString k = *it + "/" + key;
	    QString f = folder( k );
	    e = entry( k );
	    if ( e == "Default" || e == "." )
		e = "";

	    handle = readKeyHelper( user, f, e, size );

	    if ( !handle )
		size = 0;
	    else if ( size )
		break;
	}
    }

    if ( !size && local ) {
	for ( QStringList::Iterator it = paths.fromLast(); it != paths.end(); --it ) {
	    if ( handle ) {
		RegCloseKey( handle );
		handle = 0;
	    }

	    QString k = *it + "/" + key;
	    QString f = folder( k );
	    e = entry( k );
	    if ( e == "Default" || e == "." )
		e = "";

	    handle = readKeyHelper( local, f, e, size );

	    if ( !handle )
		size = 0;
	    else if ( size )
		break;
	}
    }

    if ( !size ) {
	if ( handle )
	    RegCloseKey( handle );
	return QByteArray();
    }

    uchar* data = new uchar[ size ];
    QT_WA( {
	RegQueryValueExW( handle, e.isEmpty() ? 0 : (TCHAR*)e.ucs2(), NULL, &type, data, &size );
    } , {
	RegQueryValueExA( handle, e.isEmpty() ? (const char*)0 : (const char*)e.local8Bit(), NULL, &type, data, &size );
    } );

    QByteArray result;
    result.setRawData( (const char*)data, size );
    RegCloseKey( handle );

    return result;
}

void QSettingsPrivate::sysInit()
{
    sysd = new QSettingsSysPrivate( this );
}

void QSettingsPrivate::sysClear()
{
    delete sysd;
}

bool QSettingsPrivate::sysSync()
{
    if ( sysd->local )
	RegFlushKey( sysd->local );
    if ( sysd->user )
	RegFlushKey( sysd->user );
    return TRUE;
}

bool QSettingsPrivate::sysReadBoolEntry(const QString &key, bool def, bool *ok ) const
{
    return sysReadNumEntry( key, def, ok );
}

double QSettingsPrivate::sysReadDoubleEntry( const QString &key, double def, bool *ok ) const
{
    Q_ASSERT(sysd);

    if ( ok )
	*ok = FALSE;
    ulong type;
    QByteArray array = sysd->readKey( key, type );
    if ( type != REG_BINARY ) {
	char *data = array.data();
	array.resetRawData( data, array.size() );
	delete[] data;
	return def;
    }
    if ( array.size() != sizeof(double) )
	return def;

    if ( ok )
	*ok = TRUE;

    double res = 0;
    char* data = (char*)&res;
    for ( unsigned int i = 0; i < sizeof(double); ++i )
	data[i] = array[ i ];

    char *adata = array.data();
    array.resetRawData( adata, array.size() );
    delete[] adata;
    return res;
}

int QSettingsPrivate::sysReadNumEntry(const QString &key, int def, bool *ok ) const
{
    Q_ASSERT(sysd);

    if ( ok )
	*ok = FALSE;
    ulong type;
    QByteArray array = sysd->readKey( key, type );
    if ( type != REG_DWORD ) {
	char *data = array.data();
	array.resetRawData( data, array.size() );
	delete[] data;

	return def;
    }

    if ( array.size() != sizeof(int) )
	return def;

    if ( ok )
	*ok = TRUE;

    int res = 0;
    char* data = (char*)&res;
    for ( unsigned int i = 0; i < sizeof(int); ++i )
	data[i] = array[ i ];

    char *adata = array.data();
    array.resetRawData( adata, array.size() );
    delete[] adata;
    return res;
}

QString QSettingsPrivate::sysReadEntry(const QString &key, const QString &def, bool *ok ) const
{
    if ( ok ) // no, everything is not ok
	*ok = FALSE;

    ulong type;
    QByteArray array = sysd->readKey( key, type );
    if ( type != REG_SZ ) {
	char *data = array.data();
	array.resetRawData( data, array.size() );
	delete[] data;
	return def;
    }

    if ( ok )
	*ok = TRUE;
    QString result = QString::null;

    QT_WA( {
	int s = array.size();
	for ( int i = 0; i < s; i+=2 ) {
	    QChar c( array[ i ], array[ i+1 ] );
	    if( !c.isNull() )
		result+=c;
	}
    } , {
	result = QString::fromLocal8Bit( array );
    } );

    if ( array.size() == 2 && result.isNull() )
	result = "";

    char *data = array.data();
    array.resetRawData( data, array.size() );
    delete[] data;

    return result;
}

bool QSettingsPrivate::sysWriteEntry( const QString &key, bool value )
{
    return sysWriteEntry( key, (int)value );
}

bool QSettingsPrivate::sysWriteEntry( const QString &key, double value )
{
    Q_ASSERT(sysd);

    QByteArray array( sizeof(double) );
    const char *data = (char*)&value;

    for ( unsigned int i = 0; i < sizeof(double); ++i )
	array[i] = data[i];

    return sysd->writeKey( key, array, REG_BINARY );
}

bool QSettingsPrivate::sysWriteEntry( const QString &key, int value )
{
    Q_ASSERT(sysd);

    QByteArray array( sizeof(int) );
    const char* data = (char*)&value;
    for ( unsigned int i = 0; i < sizeof(int ); ++i )
	array[i] = data[i];

    return sysd->writeKey( key, array, REG_DWORD );
}

bool QSettingsPrivate::sysWriteEntry( const QString &key, const QString &value )
{
    Q_ASSERT(sysd);

    QByteArray array( 0 );
    QT_WA( {
	array.resize( value.length() * 2 + 2 );
	const QChar *data = value.unicode();
	int i;
	for ( i = 0; i < (int)value.length(); ++i ) {
	    array[ 2*i ] = data[ i ].cell();
	    array[ (2*i)+1 ] = data[ i ].row();
	}

	array[ (2*i) ] = 0;
	array[ (2*i)+1 ] = 0;
    } , {
	array.resize( value.length() );
	array = value.local8Bit();
    } );

    return sysd->writeKey( key, array, REG_SZ );
}

bool QSettingsPrivate::sysRemoveEntry( const QString &key )
{
    Q_ASSERT(sysd);

    QString e;
    LONG res;

    HKEY handle = 0;
    for ( QStringList::Iterator it = sysd->paths.fromLast(); it != sysd->paths.end(); --it ) {
	QString k = (*it).isEmpty() ? key : *it + "/" + key;
	handle = sysd->openKey( k, FALSE, TRUE );
	e = sysd->entry( k );
	if ( handle )
	    break;
    }
    if ( !handle )
	return TRUE;
    if ( e == "Default" || e == "." )
	e = "";
    QT_WA( {
	res = RegDeleteValueW( handle, (TCHAR*)e.ucs2() );
    } , {
	res = RegDeleteValueA( handle, e.local8Bit() );
    } );

    if ( res != ERROR_SUCCESS && res != ERROR_FILE_NOT_FOUND ) {
#if defined(QT_CHECK_STATE)
	qSystemWarning( "Error deleting value " + key, res );
#endif
	return FALSE;
    }
    char vname[2];
    DWORD vnamesz = 1;
    FILETIME lastWrite;
#ifdef Q_OS_TEMP
    LONG res2 = RegEnumValue( handle, 0, (LPWSTR)vname, &vnamesz, NULL, NULL, NULL, NULL );
    LONG res3 = RegEnumKeyEx( handle, 0, (LPWSTR)vname, &vnamesz, NULL, NULL, NULL, &lastWrite );
#else
    LONG res2 = RegEnumValueA( handle, 0, vname, &vnamesz, NULL, NULL, NULL, NULL );
    LONG res3 = RegEnumKeyExA( handle, 0, vname, &vnamesz, NULL, NULL, NULL, &lastWrite );
#endif
    if ( res2 == ERROR_NO_MORE_ITEMS && res3 == ERROR_NO_MORE_ITEMS )
#ifdef Q_OS_TEMP
	RegDeleteKeyW( handle, L"" );
#else
    RegDeleteKeyA( handle, "" );
#endif
    else
	RegCloseKey( handle );
    return TRUE;
}

QStringList QSettingsPrivate::sysEntryList( const QString &key ) const
{
    Q_ASSERT(sysd);

    QStringList result;

    HKEY handle = 0;
    for ( QStringList::Iterator it = sysd->paths.fromLast(); it != sysd->paths.end(); --it ) {
	QString k = (*it).isEmpty() ? key + "/fake" : *it + "/" + key + "/fake";
	handle = sysd->openKey( k, FALSE );
	if ( handle )
	    break;
    }
    if ( !handle )
	return result;

    DWORD count;
    DWORD maxlen;

    QT_WA( {
	RegQueryInfoKeyW( handle, NULL, NULL, NULL, NULL, NULL, NULL, &count, &maxlen, NULL, NULL, NULL );
    } , {
	RegQueryInfoKeyA( handle, NULL, NULL, NULL, NULL, NULL, NULL, &count, &maxlen, NULL, NULL, NULL );
    } );
    maxlen++;
    DWORD index = 0;

    TCHAR *vnameT = new TCHAR[ maxlen ];
    char *vnameA = new char[ maxlen ];
    QString qname;

    DWORD vnamesz = 0;
    LONG res = ERROR_SUCCESS;

    while ( res != ERROR_NO_MORE_ITEMS ) {
	vnamesz = maxlen;
	QT_WA( {
	    res = RegEnumValueW( handle, index, vnameT, &vnamesz, NULL, NULL, NULL, NULL );
	    if ( res == ERROR_SUCCESS )
		qname = QString::fromUcs2( (ushort*)vnameT );
	} , {
	    res = RegEnumValueA( handle, index, vnameA, &vnamesz, NULL, NULL, NULL, NULL );
	    if ( res == ERROR_SUCCESS )
		qname = vnameA;
	} );
	if ( res == ERROR_NO_MORE_ITEMS )
	    break;
	if ( qname.isEmpty() )
	    result.append( "Default" );
	else
	    result.append( qname );
	++index;
    }

    delete [] vnameA;
    delete [] vnameT;

    RegCloseKey( handle );
    return result;
}

QStringList QSettingsPrivate::sysSubkeyList( const QString &key ) const
{
    Q_ASSERT(sysd);

    QStringList result;

    HKEY handle = 0;
    for ( QStringList::Iterator it = sysd->paths.fromLast(); it != sysd->paths.end(); --it ) {
	QString k = (*it).isEmpty() ? key + "/fake" : *it + "/" + key + "/fake";
	handle = sysd->openKey( k, FALSE );
	if ( handle )
	    break;
    }
    if ( !handle )
	return result;

    DWORD count;
    DWORD maxlen;
    QT_WA( {
	RegQueryInfoKeyW( handle, NULL, NULL, NULL, &count, &maxlen, NULL, NULL, NULL, NULL, NULL, NULL );
	maxlen++;
    } , {
	RegQueryInfoKeyA( handle, NULL, NULL, NULL, &count, &maxlen, NULL, NULL, NULL, NULL, NULL, NULL );
    } );

    DWORD index = 0;
    FILETIME lastWrite;

    TCHAR *vnameT = new TCHAR[ maxlen ];
    char *vnameA = new char[ maxlen ];
    QString qname;

    DWORD vnamesz = 0;
    LONG res = ERROR_SUCCESS;

    while ( res != ERROR_NO_MORE_ITEMS ) {
	vnamesz = maxlen;
	QT_WA( {
	    res = RegEnumKeyExW( handle, index, vnameT, &vnamesz, NULL, NULL, NULL, &lastWrite );
	    if ( res == ERROR_SUCCESS )
		qname = QString::fromUcs2( (ushort*)vnameT );
	} , {
	    res = RegEnumKeyExA( handle, index, vnameA, &vnamesz, NULL, NULL, NULL, &lastWrite );
	    if ( res == ERROR_SUCCESS )
		qname = vnameA;
	} );

	if ( res == ERROR_NO_MORE_ITEMS )
	    break;
	result.append( qname );
	++index;
    }

    delete [] vnameA;
    delete [] vnameT;

    RegCloseKey( handle );
    return result;
}

void QSettingsPrivate::sysInsertSearchPath( QSettings::System s, const QString &path )
{
    Q_ASSERT(sysd);

    if ( s != QSettings::Windows || path.isEmpty() )
	return;
    QString p = path;
    if ( p[0] != '/' )
	p = "/" + p;
    sysd->paths.append( p );
}

void QSettingsPrivate::sysRemoveSearchPath( QSettings::System s, const QString &path )
{
    Q_ASSERT(sysd);

    if ( s != QSettings::Windows || path.isEmpty() )
	return;
    QString p = path;
    if ( p[0] != '/' )
	p = "/" + p;
    sysd->paths.remove( p );
}

#endif //QT_NO_SETTINGS
