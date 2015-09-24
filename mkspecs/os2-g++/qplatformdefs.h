#ifndef QPLATFORMDEFS_H
#define QPLATFORMDEFS_H

// Get Qt defines/settings

#include "qglobal.h"

#include <io.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

#define Q_FS_FAT
/// @todo (dmik) remove?
//#ifdef QT_LARGEFILE_SUPPORT
//#define QT_STATBUF		struct _stati64		// non-ANSI defs
//#define QT_STATBUF4TSTAT	struct _stati64		// non-ANSI defs
//#define QT_STAT			::_stati64
//#define QT_FSTAT		::_fstati64
//#else
#define QT_STATBUF		struct stat
#define QT_STATBUF4TSTAT	struct stat
#define QT_STAT			::stat
#define QT_FSTAT		::fstat
//#endif
#define QT_STAT_REG		S_IFREG
#define QT_STAT_DIR		S_IFDIR
#define QT_STAT_MASK		S_IFMT
#if defined(S_IFLNK)
#  define QT_STAT_LNK		S_IFLNK
#endif
#define QT_FILENO		fileno
#define QT_OPEN			::open
#define QT_CLOSE		::close
/// @todo (dmik) remove?
//#ifdef QT_LARGEFILE_SUPPORT
//#define QT_LSEEK		::lseeki64
//#define QT_TSTAT		::tstati64
//#else
#define QT_LSEEK		::lseek
#define QT_TSTAT		::tstat
//#endif
#define QT_READ			::read
#define QT_WRITE		::write
#define QT_ACCESS		::access
// QT_GETCWD must return the drive letter with the path
#define QT_GETCWD		::_getcwd2
// QT_CHDIR should change the current drive if present in the path
#define QT_CHDIR		::_chdir2
#define QT_MKDIR(name)		::mkdir(name,0)
#define QT_RMDIR		::rmdir
#define QT_OPEN_RDONLY		O_RDONLY
#define QT_OPEN_WRONLY		O_WRONLY
#define QT_OPEN_RDWR		O_RDWR
#define QT_OPEN_CREAT		O_CREAT
#define QT_OPEN_TRUNC		O_TRUNC
#define QT_OPEN_APPEND		O_APPEND
#if defined(O_TEXT)
# define QT_OPEN_TEXT		O_TEXT
# define QT_OPEN_BINARY		O_BINARY
#endif

#define QT_SIGNAL_ARGS		int
/// @todo (dmik) find where these are used
//#define QT_SIGNAL_RETTYPE   void
//#define QT_SIGNAL_IGNORE    SIG_IGN

#define QT_VSNPRINTF		::vsnprintf
#define QT_SNPRINTF		::snprintf

#endif // QPLATFORMDEFS_H
