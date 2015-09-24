/****************************************************************************
** $Id: qdir_pm.cpp 187 2008-11-09 21:47:20Z dmik $
**
** Implementation of QDir class
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

#include "qdir.h"
#include "qdir_p.h"
#include "qnamespace.h"
#include "qfileinfo.h"
#include "qfiledefs_p.h"
#include "qregexp.h"
#include "qstringlist.h"

#ifdef QT_THREAD_SUPPORT
#  include <private/qmutexpool_p.h>
#endif // QT_THREAD_SUPPORT

#include "qt_os2.h"

#include <stdlib.h>

void QDir::slashify( QString& n )
{
    if ( n.isNull() )
	return;
    for ( int i=0; i<(int)n.length(); i++ ) {
	if ( n[i] ==  '\\' )
	    n[i] = '/';
    }
}

QString QDir::homeDirPath()
{
    QString d;
    d = QFile::decodeName( getenv("HOME") );
    if ( d.isEmpty() || !QFile::exists( d ) ) {
        d = QFile::decodeName( getenv("HOMEDRIVE") ) + QFile::decodeName( getenv("HOMEPATH") );
        if ( d.isEmpty() || !QFile::exists( d ) )
            d = rootDirPath();
    }
    slashify( d );
    return d;
}

/*!
    Returns the canonical path, i.e. a path without symbolic links or
    redundant "." or ".." elements.

    On systems that do not have symbolic links this function will
    always return the same string that absPath() returns. If the
    canonical path does not exist (normally due to dangling symbolic
    links) canonicalPath() returns QString::null.

    \sa path(), absPath(), exists(), cleanDirPath(), dirName(),
	absFilePath(), QString::isNull()
*/

QString QDir::canonicalPath() const
{
    QString r;

    char tmp[PATH_MAX];

#if defined (__INNOTEK_LIBC__)
    if ( realpath( QFile::encodeName( dPath ), tmp ) != NULL )
        r = QFile::decodeName( tmp );
#else
    if ( DosQueryPathInfo( QFile::encodeName( dPath ),
                           FIL_QUERYFULLNAME, tmp, sizeof(tmp) ) == 0 )
        r = QFile::decodeName( tmp );
#endif

    slashify( r );
    return r;
}

/*!
    Creates a directory.

    If \a acceptAbsPath is TRUE a path starting with a separator ('/')
    will create the absolute directory; if \a acceptAbsPath is FALSE
    any number of separators at the beginning of \a dirName will be
    removed.

    Returns TRUE if successful; otherwise returns FALSE.

    \sa rmdir()
*/

bool QDir::mkdir( const QString &dirName, bool acceptAbsPath ) const
{
    return QT_MKDIR( QFile::encodeName( filePath( dirName, acceptAbsPath ) ) ) == 0;
}

/*!
    Removes a directory.

    If \a acceptAbsPath is TRUE a path starting with a separator ('/')
    will remove the absolute directory; if \a acceptAbsPath is FALSE
    any number of separators at the beginning of \a dirName will be
    removed.

    The directory must be empty for rmdir() to succeed.

    Returns TRUE if successful; otherwise returns FALSE.

    \sa mkdir()
*/

bool QDir::rmdir( const QString &dirName, bool acceptAbsPath ) const
{
    return QT_RMDIR( QFile::encodeName( filePath( dirName, acceptAbsPath ) ) ) == 0;
}


/*!
    Returns TRUE if the directory is readable \e and we can open files
    by name; otherwise returns FALSE.

    \warning A FALSE value from this function is not a guarantee that
    files in the directory are not accessible.

    \sa QFileInfo::isReadable()
*/

bool QDir::isReadable() const
{
    return QT_ACCESS( QFile::encodeName( dPath ), R_OK ) == 0;
}

/*!
    Returns TRUE if the directory is the root directory; otherwise
    returns FALSE.

    Note: If the directory is a symbolic link to the root directory
    this function returns FALSE. If you want to test for this use
    canonicalPath(), e.g.
    \code
    QDir d( "/tmp/root_link" );
    d = d.canonicalPath();
    if ( d.isRoot() )
	qWarning( "It is a root link" );
    \endcode

    \sa root(), rootDirPath()
*/

bool QDir::isRoot() const
{
    return dPath == "/" || dPath == "//" ||
	(dPath[0].isLetter() && dPath.mid(1,dPath.length()) == ":/");
}

/*!
    Renames a file or directory.

    If \a acceptAbsPaths is TRUE a path starting with a separator
    ('/') will rename the file with the absolute path; if \a
    acceptAbsPaths is FALSE any number of separators at the beginning
    of the names will be removed.

    Returns TRUE if successful; otherwise returns FALSE.

    On most file systems, rename() fails only if \a oldName does not
    exist or if \a newName and \a oldName are not on the same
    partition. On Windows, rename() will fail if \a newName already
    exists. However, there are also other reasons why rename() can
    fail. For example, on at least one file system rename() fails if
    \a newName points to an open file.
*/

bool QDir::rename( const QString &oldName, const QString &newName,
		   bool acceptAbsPaths	)
{
    if ( oldName.isEmpty() || newName.isEmpty() ) {
#if defined(QT_CHECK_NULL)
	qWarning( "QDir::rename: Empty or null file name" );
#endif
	return FALSE;
    }
    QString fn1 = filePath( oldName, acceptAbsPaths );
    QString fn2 = filePath( newName, acceptAbsPaths );
    return ::rename( QFile::encodeName( fn1 ), QFile::encodeName( fn2 ) ) == 0;
}
/*!
    Sets the application's current working directory to \a path.
    Returns TRUE if the directory was successfully changed; otherwise
    returns FALSE.
*/


bool QDir::setCurrent( const QString &path )
{
    return QT_CHDIR( QFile::encodeName( path ) ) == 0;
}

/*!
    Returns the absolute path of the application's current directory.

    \sa current()
*/

QString QDir::currentDirPath()
{
    QString result;

    char currentName[PATH_MAX];
    if ( QT_GETCWD(currentName,PATH_MAX) != 0 ) {
        result = QFile::decodeName( currentName );
    }
    slashify( result );
    return result;
}

/*!
    Returns the absolute path for the root directory.

    For UNIX operating systems this returns "/". For Windows file
    systems this normally returns "c:/". For OS/2 this returns "x:/"
    where \c x is the system boot drive letter.

    \sa root() drives()
*/


QString QDir::rootDirPath()
{
    ULONG bootDrive = 0;
    DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, (PVOID) &bootDrive, sizeof(bootDrive) );
    QString d = QChar( (char)(bootDrive + 'A' - 1) );
    d += ":/";
    return d;
}

/*!
    Returns TRUE if \a path is relative; returns FALSE if it is
    absolute.

    \sa isRelative()
*/

bool QDir::isRelativePath( const QString &path )
{
    return !(path[0] == '/' || path[0] == '\\' ||
	     (path[0].isLetter() && path[1] == ':'));		// drive, e.g. a: or network drive
}

/*!
  \internal
  Reads directory entries.
*/

bool QDir::readDirEntries( const QString &nameFilter,
			   int filterSpec, int sortSpec )
{
    int i;

    QValueList<QRegExp> filters = qt_makeFilterList( nameFilter );

    bool doDirs	    = (filterSpec & Dirs)	!= 0;
    bool doFiles    = (filterSpec & Files)	!= 0;
    bool noSymLinks = (filterSpec & NoSymLinks) != 0;
    bool doReadable = (filterSpec & Readable)	!= 0;
    bool doWritable = (filterSpec & Writable)	!= 0;
    bool doExecable = (filterSpec & Executable) != 0;

    // FilterSpec::Drives is mentioned in the docs but never used in Qt/Win32.
    // So do we for compatibility reasons.

    bool      first = TRUE;
    QString   p = dPath.copy();
    int	      plen = p.length();
    HDIR      ff = (HDIR) HDIR_CREATE;
    FILEFINDBUF3 finfo = {0};
    ULONG     fcount = 1;
    APIRET    rc = 0;
    ULONG     attribute = FILE_READONLY | FILE_ARCHIVED | FILE_DIRECTORY;
    QFileInfo fi;

    if (filterSpec & Modified) attribute |= MUST_HAVE_ARCHIVED;
    if (filterSpec & Hidden) attribute |= FILE_HIDDEN;
    if (filterSpec & System) attribute |= FILE_SYSTEM;

    if ( plen == 0 ) {
#if defined(QT_CHECK_NULL)
	qWarning( "QDir::readDirEntries: No directory name specified" );
#endif
	return FALSE;
    }
    if ( p.at(plen-1) != '/' && p.at(plen-1) != '\\' )
	p += '/';
    p += QString::fromLatin1("*.*");

    rc = DosFindFirst( QFile::encodeName( p ), &ff, attribute,
        &finfo, sizeof(finfo), &fcount, FIL_STANDARD );

    if ( !fList ) {
	fList  = new QStringList;
	Q_CHECK_PTR( fList );
    } else {
	fList->clear();
    }

    if ( rc != 0 ) {
	// if it is a floppy disk drive, it might just not have a file on it
	if ( plen > 1 && p[1] == ':' &&
		( p[0]=='A' || p[0]=='a' || p[0]=='B' || p[0]=='b' ) ) {
	    if ( !fiList ) {
		fiList = new QFileInfoList;
		Q_CHECK_PTR( fiList );
		fiList->setAutoDelete( TRUE );
	    } else {
		fiList->clear();
	    }
	    return TRUE;
	}
#if defined(QT_CHECK_RANGE)
	qWarning( "QDir::readDirEntries: Cannot read the directory: %s (UTF8)",
		  dPath.utf8().data() );
#endif
	return FALSE;
    }

    if ( !fiList ) {
	fiList = new QFileInfoList;
	Q_CHECK_PTR( fiList );
	fiList->setAutoDelete( TRUE );
    } else {
	fiList->clear();
    }

    for ( ;; ) {
	if ( first )
	    first = FALSE;
	else {
            if ( DosFindNext( ff, &finfo, sizeof(finfo), &fcount) != 0 )
                break;
	}
        int  attrib = finfo.attrFile;
	bool isDir	= (attrib & FILE_DIRECTORY) != 0;
	bool isFile	= !isDir;
	bool isSymLink	= FALSE;
	bool isReadable = TRUE;
	bool isWritable = (attrib & FILE_READONLY) == 0;
	bool isExecable = FALSE;

	QString fname;
        fname = QFile::decodeName( (const char*)finfo.achName );

	if ( !qt_matchFilterList(filters, fname) && !(allDirs && isDir) )
	    continue;

	if  ( (doDirs && isDir) || (doFiles && isFile) ) {
	    QString name = fname;
	    slashify(name);
	    if ( doExecable && isFile ) {
		QString ext = name.right(4).lower();
		if ( ext == ".exe" || ext == ".com" || ext == ".bat" ||
		     ext == ".cmd" )
		    isExecable = TRUE;
	    }

	    if ( noSymLinks && isSymLink )
		continue;
	    if ( (filterSpec & RWEMask) != 0 )
		if ( (doReadable && !isReadable) ||
		     (doWritable && !isWritable) ||
		     (doExecable && !isExecable) )
		    continue;
	    fi.setFile( *this, name );
	    fiList->append( new QFileInfo( fi ) );
	}
    }
    DosFindClose( ff );

    // Sort...
    QDirSortItem* si= new QDirSortItem[fiList->count()];
    QFileInfo* itm;
    i=0;
    for (itm = fiList->first(); itm; itm = fiList->next())
	si[i++].item = itm;
    qt_cmp_si_sortSpec = sortSpec;
    qsort( si, i, sizeof(si[0]), qt_cmp_si );
    // put them back in the list
    fiList->setAutoDelete( FALSE );
    fiList->clear();
    int j;
    for ( j=0; j<i; j++ ) {
	fiList->append( si[j].item );
	fList->append( si[j].item->fileName() );
    }
    delete [] si;
    fiList->setAutoDelete( TRUE );

    if ( filterSpec == (FilterSpec)filtS && sortSpec == (SortSpec)sortS &&
	 nameFilter == nameFilt )
	dirty = FALSE;
    else
	dirty = TRUE;
    return TRUE;
}


/*!
    Returns a list of the root directories on this system. On Windows
    this returns a number of QFileInfo objects containing "C:/", "D:/"
    etc. On other operating systems, it returns a list containing just
    one root directory (e.g. "/").

    The returned pointer is owned by Qt. Callers should \e not delete
    or modify it.
*/

const QFileInfoList * QDir::drives()
{
    // at most one instance of QFileInfoList is leaked, and this variable
    // points to that list
    static QFileInfoList * knownMemoryLeak = 0;

#ifdef QT_THREAD_SUPPORT
    QMutexLocker locker( qt_global_mutexpool ?
			 qt_global_mutexpool->get( &knownMemoryLeak ) : 0 );
#endif // QT_THREAD_SUPPORT

    if ( !knownMemoryLeak ) {
	knownMemoryLeak = new QFileInfoList;
	knownMemoryLeak->setAutoDelete( TRUE );
    }

    if ( !knownMemoryLeak->count() ) {
	ULONG driveBits, dummy;
	DosQueryCurrentDisk( &dummy, &driveBits );
	driveBits &= 0x3ffffff;

	char driveName[4];
	qstrcpy( driveName, "A:/" );

	while( driveBits ) {
	    if ( driveBits & 1 )
		knownMemoryLeak->append( new QFileInfo( QString::fromLatin1(driveName).upper() ) );
	    driveName[0]++;
	    driveBits = driveBits >> 1;
	}
    }

    return knownMemoryLeak;
}
