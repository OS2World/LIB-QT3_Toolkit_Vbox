/****************************************************************************
** $Id: qclipboard_pm.cpp 96 2006-07-06 21:11:46Z dmik $
**
** Implementation of QClipboard class for OS/2
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

#include "qclipboard.h"

#ifndef QT_NO_CLIPBOARD

#include "qapplication.h"
#include "qapplication_p.h"
#include "qeventloop.h"
#include "qpixmap.h"
#include "qdatetime.h"
#include "qimage.h"
#include "qmime.h"
#include "qt_os2.h"


/*****************************************************************************
  Internal QClipboard functions for OS/2.
 *****************************************************************************/

static HWND prevClipboardViewer = 0;
static QWidget *qt_cb_owner = 0;

static QWidget *clipboardOwner()
{
    if ( !qt_cb_owner )
	qt_cb_owner = new QWidget( 0, "internal clipboard owner" );
    return qt_cb_owner;
}


/*****************************************************************************
  QClipboard member functions for OS/2.
 *****************************************************************************/

bool QClipboard::supportsSelection() const
{
    return FALSE;
}


bool QClipboard::ownsSelection() const
{
    return FALSE;
}


bool QClipboard::ownsClipboard() const
{
    return qt_cb_owner && WinQueryClipbrdOwner( 0 ) == qt_cb_owner->winId();
}


void QClipboard::setSelectionMode(bool)
{
}


bool QClipboard::selectionModeEnabled() const
{
    return FALSE;
}


void QClipboard::ownerDestroyed()
{
    QWidget *owner = (QWidget *)sender();
    if ( owner == qt_cb_owner ) {
        if ( owner->winId() == WinQueryClipbrdViewer( 0 ) )
                WinSetClipbrdViewer( 0, prevClipboardViewer );
        prevClipboardViewer = 0;
    }
}


void QClipboard::connectNotify( const char *signal )
{
    if ( qstrcmp( signal, SIGNAL( dataChanged() ) ) == 0 ) {
	QWidget *owner = clipboardOwner();
        HWND clipboardViewer = WinQueryClipbrdViewer( 0 );
        if ( owner->winId() != clipboardViewer ) {
            prevClipboardViewer = clipboardViewer;
            BOOL ok = WinSetClipbrdViewer( 0, owner->winId() );
            Q_UNUSED( ok );
#ifndef QT_NO_DEBUG
            if ( !ok )
                qSystemWarning( "QClipboard: Failed to set clipboard viewer" );
#endif
            connect( owner, SIGNAL( destroyed() ), SLOT( ownerDestroyed() ) );
        }
    }
}

class QClipboardWatcher : public QMimeSource {
public:
    QClipboardWatcher()
    {
    }

    bool provides( const char* mime ) const
    {
	bool r = FALSE;
	if ( WinOpenClipbrd( 0 ) ) {
	    ulong cf = 0;
	    while (!r && (cf = WinEnumClipbrdFmts( 0, cf ))) {
		if ( QPMMime::convertor( mime, cf ) )
		    r = TRUE;
	    }
#ifndef QT_NO_DEBUG
	    if ( !WinCloseClipbrd( 0 ) )
		qSystemWarning( "QClipboard: Failed to close clipboard" );
#else
	    WinCloseClipbrd( 0 );
#endif
	}
#ifndef QT_NO_DEBUG
	else
	    qSystemWarning( "QClipboardWatcher: Failed to open clipboard" );
#endif
	return r;
    }

    const char* format( int n ) const
    {
	const char* mime = 0;

	if ( n >= 0 ) {
	    if ( WinOpenClipbrd( 0 ) ) {
		ulong cf = 0;
		while ( (cf = WinEnumClipbrdFmts( 0, cf )) ) {
                    mime = QPMMime::cfToMime( cf );
                    if ( mime ) {
                        if ( !n )
                            break;
                        n--;
                        mime = 0;
                    }
		}
		WinCloseClipbrd( 0 );
	    }
	}
	if ( !n )
	    return mime;
	return 0;
    }

    QByteArray encodedData( const char* mime ) const
    {
	QByteArray r;
	if ( WinOpenClipbrd( 0 ) ) {
	    QPtrList<QPMMime> all = QPMMime::all();
	    for ( QPMMime* c = all.first(); c; c = all.next() ) {
		ulong cf = c->cfFor( mime );
		if ( cf ) {
                    ULONG fl = 0;
                    BOOL ok = WinQueryClipbrdFmtInfo( 0, cf, &fl );
                    if ( ok  && (fl & QPMMime::CFI_Storage) ==
                                (c->flFor( cf ) & QPMMime::CFI_Storage) ) {
                        ULONG data = WinQueryClipbrdData ( 0, cf );
                        if ( (ok = (data != 0)) ) {
                            char *dataPtr = 0;
                            uint dataSize = 0;
                            if ( fl & CFI_POINTER ) {
                                ULONG sz = ~0, flags = 0;
                                // get data size rounded to the page boundary (4K)
                                APIRET rc = DosQueryMem( (PVOID) data, &sz, &flags );
                                if ( rc ) {
#ifndef QT_NO_DEBUG
                                    qSystemWarning( "QClipboard: Failed to query memory info", rc );
#endif
                                    continue;
                                }
                                // look at the last dword of the last page
                                dataSize = *(((ULONG *)(data + sz)) - 1);
                                // If this dword seems to contain the exact data size,
                                // use it. Otherwise use the rounded size hoping
                                // that a data reader will correctly determine
                                // the exact size according to the mime type and
                                // data header
                                if ( !dataSize || dataSize > (sz - sizeof( ULONG )) )
                                    dataSize = sz;
                                dataPtr = (char *) data;
                            }
                            else if ( fl & CFI_HANDLE ) {
                                // represent the handle as first 4 bytes of
                                // the data array
                                dataSize = sizeof( ULONG );
                                dataPtr =  (char *) &data;
                            }
                            else {
#ifdef QT_CHECK_RANGE
                                qWarning( "QClipboard: wrong format flags: %08lX", fl );
#endif
                                continue;
                            }
                            r.setRawData( dataPtr, dataSize );
                            QByteArray tr = c->convertToMime( r, mime, cf );
                            tr.detach();
                            r.resetRawData( dataPtr, dataSize );
                            r = tr;
                            break;
                        }
		    }
#ifndef QT_NO_DEBUG
		    if ( !ok )
			qSystemWarning( "QClipboard: Failed to read clipboard data" );
#endif
		}
	    }
	    WinCloseClipbrd( 0 );
	}
#ifndef QT_NO_DEBUG
	else
	    qSystemWarning( "QClipboard: Failed to open Clipboard" );
#endif
	return r;
    }
};



class QClipboardData
{
public:
    QClipboardData();
   ~QClipboardData();

    void setSource(QMimeSource* s)
    {
	delete src;
	src = s;
	if ( src )
	    src->clearCache();
    }
    QMimeSource* source()
    {
	if ( src )
	    src->clearCache();
	return src;
    }
    QMimeSource* provider()
    {
	if (!prov)
	    prov = new QClipboardWatcher();
	prov->clearCache();
	return prov;
    }

private:
    QMimeSource* src;
    QMimeSource* prov;
};

QClipboardData::QClipboardData()
{
    src = 0;
    prov = 0;
}

QClipboardData::~QClipboardData()
{
    delete src;
    delete prov;
}

static QClipboardData *internalCbData = 0;

static void cleanupClipboardData()
{
    delete internalCbData;
    internalCbData = 0;
}

static QClipboardData *clipboardData()
{
    if ( internalCbData == 0 ) {
	internalCbData = new QClipboardData;
	Q_CHECK_PTR( internalCbData );
	qAddPostRoutine( cleanupClipboardData );
    }
    return internalCbData;
}


//#define QT_DEBUG_CB

static void setClipboardData( ulong cf, const char *mime, QPMMime *c, QMimeSource *s )
{
    QByteArray md = s->encodedData( mime );
#if defined(QT_DEBUG_CB)
    qDebug( "source is %d bytes of %s", md.size(), mime );
#endif

    md = c->convertFromMime( md, mime, cf );
    ulong fl = c->flFor( cf );
    int len = md.size();
#if defined(QT_DEBUG_CB)
    qDebug( "rendered %d bytes of CF 0x%08lX (0x%08lX) by %s",
            len, cf, fl, c->convertorName());
#endif

    ULONG data = 0;
    if ( fl & CFI_POINTER ) {
        // allocate memory for the array + dword for its size
        APIRET rc = DosAllocSharedMem( (PVOID *) &data, NULL, len + 4,
                                        PAG_WRITE  | PAG_COMMIT | OBJ_GIVEABLE );
        if ( rc ) {
#ifndef QT_NO_DEBUG
            qSystemWarning( "QClipboard: Failed to allocate shared memory", rc );
#endif
            return;
        }
        // get the size rounded to the page boundary (4K)
        ULONG sz = ~0, flags = 0;
        rc = DosQueryMem( (PVOID) (data + len - 1), &sz, &flags );
        if ( !rc ) {
            sz += (len - 1);
            // store the exact size to the last dword of the last page
            *(((ULONG *)(data + sz)) - 1) = len;
        }
#ifndef QT_NO_DEBUG
        else
            qSystemWarning( "QClipboard: Failed to query memory info", rc );
#endif
        memcpy( (void *) data, md.data(), len );
    }
    else if ( fl & CFI_HANDLE ) {
        if ( len == sizeof(ULONG) ) {
            // treat first 4 bytes of the array as a handle
            data = *(ULONG *) md.data();
        } else {
#ifdef QT_CHECK_RANGE
            qWarning( "QClipboard: wrong CFI_HANDLE data len: %d", len );
#endif
            return;
        }
    }
    else {
#ifdef QT_CHECK_RANGE
        qWarning( "QClipboard: wrong format flags: %08lX", fl );
#endif
        return;
    }
    BOOL ok = WinSetClipbrdData( 0, data, cf, fl );
#ifndef QT_NO_DEBUG
    if ( !ok )
	qSystemWarning( "QClipboard: Failed to write data" );
#else
    Q_UNUSED( ok );
#endif
}

static void renderFormat( ulong cf )
{
#if defined(QT_DEBUG_CB)
    qDebug( "renderFormat(0x%08lX)", cf );
#endif
    // sanity checks
    if ( !internalCbData )
	return;
    QMimeSource *s = internalCbData->source();
    if ( !s )
	return;

    const char* mime;
    for ( int i = 0; (mime = s->format( i )); i++ ) {
	QPMMime* c = QPMMime::convertor( mime, cf );
	if ( c ) {
	    setClipboardData( cf, mime, c, s );
	    return;
	}
    }
}

static bool ignore_WM_DESTROYCLIPBOARD = FALSE;

static void renderAllFormats()
{
#if defined(QT_DEBUG_CB)
    qDebug( "renderAllFormats" );
#endif
    // sanity checks
    if ( !internalCbData )
	return;
    QMimeSource *s = internalCbData->source();
    if ( !s )
	return;
    if ( !qt_cb_owner )
	return;

    if ( !WinOpenClipbrd( 0 ) ) {
#if defined(QT_CHECK_STATE)
	qSystemWarning( "renderAllFormats: couldn't open clipboard" );
#endif
	return;
    }

    ignore_WM_DESTROYCLIPBOARD = TRUE;
    WinEmptyClipbrd( 0 );
    ignore_WM_DESTROYCLIPBOARD = FALSE;

    const char* mime;
    QPtrList<QPMMime> all = QPMMime::all();
    for ( int i = 0; (mime = s->format( i )); i++ ) {
	for ( QPMMime* c = all.first(); c; c = all.next() ) {
	    if ( c->cfFor( mime ) ) {
		for ( int j = 0; j < c->countCf(); j++ ) {
		    ulong cf = c->cf(j);
		    if ( c->canConvert( mime, cf ) ) {
                        ulong fl = 0;
                        if ( WinQueryClipbrdFmtInfo( 0, cf, &fl ) ) {
                            // the format is already rendered by some other
                            // convertor, skip it
                            continue;
                        }
			setClipboardData( cf, mime, c, s );
		    }
		}
	    }
	}
    }

    WinCloseClipbrd( 0 );
}

QClipboard::~QClipboard()
{
    renderAllFormats();
    delete qt_cb_owner;
    qt_cb_owner = 0;
}

bool QClipboard::event( QEvent *e )
{
    if ( e->type() != QEvent::Clipboard )
	return QObject::event( e );

    QMSG *msg = (QMSG *)((QCustomEvent*)e)->data();
    if ( !msg ) {
	renderAllFormats();
	return TRUE;
    }

#if defined(QT_DEBUG_CB)
    qDebug( "QClipboard::event: msg=%08lX mp1=%p mp2=%p",
            msg->msg, msg->mp1, msg->mp2 );
#endif

    switch ( msg->msg ) {

	case WM_DRAWCLIPBOARD:
	    emit dataChanged();
            // PM doesn't inform the previous clipboard viewer when another
            // app changes it (nor does it support viewer chains in some other
            // way). The best we can do is to propagate the message to the
            // previous clipboard viewer ourselves (though there is no guarantee
            // that all non-Qt apps will do the same).
            if ( prevClipboardViewer ) {
                // propagate the message to the previous clipboard viewer
                BOOL ok = WinPostMsg ( prevClipboardViewer,
                                       msg->msg, msg->mp1, msg->mp2 );
                if ( !ok )
                    prevClipboardViewer = 0;
            }
	    break;

	case WM_DESTROYCLIPBOARD:
	    if ( !ignore_WM_DESTROYCLIPBOARD )
		cleanupClipboardData();
	    break;

	case WM_RENDERFMT:
	    renderFormat( (ULONG) msg->mp1 );
	    break;
	case WM_RENDERALLFMTS:
	    renderAllFormats();
	    break;
    }

    return TRUE;
}


void QClipboard::clear( Mode mode )
{
    if ( mode != Clipboard ) return;

    if ( WinOpenClipbrd( 0 ) ) {
	WinEmptyClipbrd( 0 );
	WinCloseClipbrd( 0 );
    }
}


QMimeSource* QClipboard::data( Mode mode ) const
{
    if ( mode != Clipboard ) return 0;

    QClipboardData *d = clipboardData();
    return d->provider();
}


void QClipboard::setData( QMimeSource* src, Mode mode )
{
    if ( mode != Clipboard ) return;

    if ( !WinOpenClipbrd( 0 ) ) {
#ifndef QT_NO_DEBUG
	qSystemWarning( "QClipboard: Failed to open clipboard" );
#endif
	return;
    }

    QClipboardData *d = clipboardData();
    d->setSource( src );

    ignore_WM_DESTROYCLIPBOARD = TRUE;
    BOOL ok = WinEmptyClipbrd( 0 );
    ignore_WM_DESTROYCLIPBOARD = FALSE;
#ifndef QT_NO_DEBUG
    if ( !ok )
	qSystemWarning( "QClipboard: Failed to empty clipboard" );
#else
    Q_UNUSED( ok );
#endif

    // Register all the formats of src that we can render.
    const char* mime;
    QPtrList<QPMMime> all = QPMMime::all();
    for ( int i = 0; (mime = src->format( i )); i++ ) {
#if defined(QT_DEBUG_CB)
        qDebug( "setData: src mime[%i] = %s", i, mime );
#endif
	for ( QPMMime* c = all.first(); c; c = all.next() ) {
#if defined(QT_DEBUG_CB)
        qDebug( "setData: trying convertor %s", c->convertorName() );
#endif
	    if ( c->cfFor( mime ) ) {
		for ( int j = 0; j < c->countCf(); j++ ) {
		    ulong cf = c->cf( j );
		    if ( c->canConvert( mime, cf ) ) {
                        ulong fl = 0;
                        if ( WinQueryClipbrdFmtInfo( 0, cf, &fl ) ) {
                            // the format is already rendered by some other
                            // convertor, skip it
                            continue;
                        }

                        fl = c->flFor( cf );
#if defined(QT_DEBUG_CB)
                        qDebug( "setData: converting to CF 0x%08lX (0x%08lX)",
                                cf, fl );
#endif
			if ( qApp && qApp->eventLoop()->loopLevel() ) {
                            // setup delayed rendering of clipboard data
                            WinSetClipbrdOwner( 0, clipboardOwner()->winId() );
			    WinSetClipbrdData( 0, 0, cf, fl );
                        }
			else {
                            // write now if we can't process data requests
			    setClipboardData( cf, mime, c, src );
                        }
		    }
		}
                // Don't search for other formats for the same mime type --
                // we've already represented it in the clipboard and adding a
                // new format would just introduce mime type duplicates on the
                // clipboard reader's side (where the first encountered format
                // would be used for actual data transfer anyway).
                // See also QDragManager::drag().
                break;
	    }
	}
    }

    WinCloseClipbrd( 0 );
}

#endif // QT_NO_CLIPBOARD
