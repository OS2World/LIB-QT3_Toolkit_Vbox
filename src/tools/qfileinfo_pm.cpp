/****************************************************************************
** $Id: qfileinfo_pm.cpp 8 2005-11-16 19:36:46Z dmik $
**
** Implementation of QFileInfo class
**
** Copyright (C) 1992-2003 Trolltech AS.  All rights reserved.
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

#include "qplatformdefs.h"

#include "qlibrary.h"
#include "qfileinfo.h"
#include "qfiledefs_p.h"
#include "qdatetime.h"
#include "qdir.h"

#ifdef QT_THREAD_SUPPORT
#  include <private/qmutexpool_p.h>
#endif // QT_THREAD_SUPPORT

#include "qt_os2.h"
#include <ctype.h>

extern bool qt_file_access( const QString& fn, int t );

static QString currentDirOfDrive( char ch )
{
    QString result;

#if defined(Q_CC_GNU)
    char currentName[PATH_MAX];
    if ( _getcwd1( currentName, toupper( (uchar) ch ) ) == 0 ) {
        result = QFile::decodeName( currentName );
    }
#else
    char currentName[PATH_MAX];
    if ( _getdcwd( toupper( (uchar) ch ) - 'A' + 1, currentName, PATH_MAX ) != 0 ) {
        result = QFile::decodeName( currentName );
    }
#endif // Q_CC_GNU
    return result;
}


void QFileInfo::slashify( QString &s )
{
    for (int i=0; i<(int)s.length(); i++) {
	if ( s[i] == '\\' )
	    s[i] = '/';
    }
    if ( s[ (int)s.length() - 1 ] == '/' && s.length() > 3 )
	s.remove( (int)s.length() - 1, 1 );
}

void QFileInfo::makeAbs( QString &s )
{
    if ( s[ 1 ] != ':' && s[ 1 ] != '/' ) {
	s.prepend( ":" );
#if defined(Q_CC_GNU)
	s.prepend( _getdrive() );
#else
	s.prepend( (char)_getdrive() + 'A' - 1 );
#endif
    }
    if ( s[ 1 ] == ':' && s.length() > 3 && s[ 2 ] != '/' ) {
	QString d = currentDirOfDrive( (char)s[ 0 ].latin1() );
        slashify( d );
	s = d + "/" + s.mid( 2, 0xFFFFFF );
    }
}

bool QFileInfo::isFile() const
{
    if ( !fic || !cache )
	doStat();
    return fic ? (fic->st.st_mode & QT_STAT_MASK) == QT_STAT_REG : FALSE;
}

bool QFileInfo::isDir() const
{
    if ( !fic || !cache )
	doStat();
    return fic ? (fic->st.st_mode & QT_STAT_MASK) == QT_STAT_DIR : FALSE;
}

bool QFileInfo::isSymLink() const
{
    return FALSE;
}

QString QFileInfo::readLink() const
{
    return QString();
}

QString QFileInfo::owner() const
{
    return QString::null;
}

static const uint nobodyID = (uint) -2;

uint QFileInfo::ownerId() const
{
    return nobodyID;
}

QString QFileInfo::group() const
{
    return QString::null;
}

uint QFileInfo::groupId() const
{
    return nobodyID;
}

bool QFileInfo::permission( int p ) const
{
    //@@TODO (dmik): do checks for permissions using file ACLs when possible...

    // just check if it's ReadOnly

    if ( p & ( WriteOwner | WriteUser | WriteGroup | WriteOther ) ) {
        return qt_file_access( fn, W_OK );
    }
    return TRUE;
}

void QFileInfo::doStat() const
{
    if ( fn.isEmpty() )
	return;

    QFileInfo *that = ((QFileInfo*)this);	// mutable function
    if ( !that->fic )
	that->fic = new QFileInfoCache;
    QT_STATBUF *b = &that->fic->st;

    int r = QT_STAT( QFile::encodeName( fn ), b );
    if ( r != 0 ) {
        // special handling for situations when stat() returns error but
        // we can guess whether it is a directory name or not.
	bool is_dir = FALSE;
	if (
            (fn[0] == '/' && fn[1] == '/') ||
            (fn[0] == '\\' && fn[1] == '\\')
        ) {
	    // UNC
	    int s = fn.find(fn[0],2);
	    if ( s > 0 ) {
		// "\\server\..."
		s = fn.find(fn[0],s+1);
		if ( s > 0 ) {
		    // "\\server\share\..."
		    if ( fn[s+1] ) {
			// "\\server\share\notfound"
		    } else {
			// "\\server\share\"
			is_dir = TRUE;
		    }
		} else {
		    // "\\server\share"
		    is_dir = TRUE;
		}
	    } else {
		// "\\server"
		is_dir = TRUE;
	    }
	} else if (
            (fn.length() == 2 && fn[1] == ':') ||
            (fn.length() == 3 && fn[1] == ':' && (fn[2] == '/' || fn[2] == '\\'))
        ) {
            if ( fn[0].isLetter() )
                // "x:", "x:\" or "x:/" can be only a dir
		is_dir = TRUE;
        }
	if ( is_dir ) {
	    // looks like a dir, is a dir.
	    memset( b, 0, sizeof(*b) );
	    b->st_mode = QT_STAT_DIR;
	    b->st_nlink = 1;
	    r = 0;
	}
    }

    if ( r != 0 ) {
	delete that->fic;
	that->fic = 0;
    }
}

QString QFileInfo::dirPath( bool absPath ) const
{
    QString s;
    if ( absPath )
	s = absFilePath();
    else
	s = fn;
    int pos = s.findRev( '/' );
    if ( pos == -1 ) {
	if ( s[ 2 ] == '/' )
	    return s.left( 3 );
	if ( s[ 1 ] == ':' ) {
	    if ( absPath )
		return s.left( 2 ) + "/";
	    return s.left( 2 );
	}
	return QString::fromLatin1(".");
    } else {
	if ( pos == 0 )
	    return QString::fromLatin1( "/" );
	if ( pos == 2 && s[ 1 ] == ':'  && s[ 2 ] == '/')
	    pos++;
	return s.left( pos );
    }
}

QString QFileInfo::fileName() const
{
    int p = fn.findRev( '/' );
    if ( p == -1 ) {
	int p = fn.findRev( ':' );
	if ( p != -1 )
	    return fn.mid( p + 1 );
	return fn;
    } else {
	return fn.mid( p + 1 );
    }
}

/*!
    Returns TRUE if the file is hidden; otherwise returns FALSE.

    On Unix-like operating systems, including Mac OS X, a file is
    hidden if its name begins with ".". On Windows and OS/2 a file is hidden if
    its hidden attribute is set.
*/
bool QFileInfo::isHidden() const
{
    if ( !fic || !cache )
	doStat();
    return fic ? (fic->st.st_attr & FILE_HIDDEN) == FILE_HIDDEN : FALSE;
}
