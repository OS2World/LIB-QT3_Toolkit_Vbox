/****************************************************************************
** $Id: qmime_pm.cpp 114 2006-08-10 17:56:11Z dmik $
**
** Implementation of OS/2 PM MIME <-> clipboard converters
**
** Copyright (C) 1992-2002 Trolltech AS.  All rights reserved.
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

#include "qmime.h"

#ifndef QT_NO_MIME

#include <stdlib.h>
#include <time.h>

#include "qstrlist.h"
#include "qimage.h"
#include "qdatastream.h"
#include "qdragobject.h"
#include "qbuffer.h"
#include "qapplication.h"
#include "qapplication_p.h"
#include "qeventloop.h"
#include "qtextcodec.h"
#include "qdir.h"
#include "qfile.h"
#include "qurl.h"
#include "qvaluelist.h"
#include "qintdict.h"
#include "qptrlist.h"
#include "qt_os2.h"

// if you enable this, it makes sense to enable it in qdnd_pm.cpp as well
//#define QT_DEBUG_DND

#if !defined(QT_NO_DRAGANDDROP)

#define DRT_URL "UniformResourceLocator"

#define DRM_OS2FILE "DRM_OS2FILE"
#define DRM_SHAREDMEM "DRM_SHAREDMEM"

#define DRF_UNKNOWN "DRF_UNKNOWN"
#define DRF_TEXT "DRF_TEXT"
#define DRF_POINTERDATA "DRF_POINTERDATA"

#define MIME_TEXT_PLAIN "text/plain"
#define MIME_TEXT_PLAIN_CHARSET_SYSTEM "text/plain;charset=system"
#define MIME_TEXT_URI_LIST "text/uri-list"

/*! \internal
  According to my tests, DrgFreeDragtransfer() appears to be bogus: when the
  drag source attempts to free the DRAGTRANSFER structure passed to it in
  DM_RENDERPREPARE/DM_RENDER by another process, the shared memory object is not
  actually released until DrgFreeDragtransfer() is called for the second time.
  This method tries to fix this problem.
  
  \note The problem (and the solution) was not tested on platforms other than
  eCS 1.2.1 GA!
*/
void qt_DrgFreeDragtransfer( DRAGTRANSFER *xfer )
{
    Q_ASSERT( xfer );
    if ( xfer ) {
        BOOL ok = DrgFreeDragtransfer( xfer );
        Q_ASSERT( ok );
        if ( ok ) {
            ULONG size = ~0, flags = 0;
            APIRET rc = DosQueryMem( xfer, &size, &flags );
            Q_ASSERT( rc == 0 );
            if ( rc == 0 && !(flags & PAG_FREE) ) {
                PID pid;
                TID tid;
                ok = WinQueryWindowProcess( xfer->hwndClient, &pid, &tid );
                Q_ASSERT( ok );
                if ( ok ) {
                    PPIB ppib = NULL;
                    DosGetInfoBlocks( NULL, &ppib );
                    if ( ppib->pib_ulpid != pid ) {
#if defined(QT_DEBUG_DND)                        
                        qDebug( "qt_DrgFreeDragtransfer: Will free xfer %p "
                                "TWICE (other process)!", xfer );
#endif                        
                        DrgFreeDragtransfer( xfer );
                    }
                }
            }
        }
    }
}

#define SEA_TYPE ".TYPE"

/*! \internal
  Sets a single .TYPE EA vaule on a given fle.
*/ 
static void qt_SetFileTypeEA( const char *name, const char *type)
{
    #pragma pack(1)
    
    struct MY_FEA2 {
        ULONG   oNextEntryOffset;  /*  Offset to next entry. */
        BYTE    fEA;               /*  Extended attributes flag. */
        BYTE    cbName;            /*  Length of szName, not including NULL. */
        USHORT  cbValue;           /*  Value length. */
        CHAR    szName[0];         /*  Extended attribute name. */
        /* EA value follows here */
    };
    
    struct MY_FEA2LIST {
        ULONG   cbList;            /*  Total bytes of structure including full list. */
        MY_FEA2 list[0];           /*  Variable-length FEA2 structures. */
    };
    
    struct MY_FEA2_VAL {
        USHORT  usEAType;          /* EA value type (one of EAT_* constants) */
        USHORT  usValueLen;        /* Length of the value data following */
        CHAR    aValueData[0];     /* value data */
    };
    
    struct MY_FEA2_MVMT {
        USHORT      usEAType;      /* Always EAT_MVMT */
        USHORT      usCodePage;    /* 0 - default */
        USHORT      cbNumEntries;  /* Number of MYFEA2_VAL structs following */
        MY_FEA2_VAL aValues[0];    /* MYFEA2_VAL value structures */
    };
    
    #pragma pack()
    
    uint typeLen = qstrlen( type );
    uint valLen = sizeof( MY_FEA2_VAL ) + typeLen;
    uint mvmtLen = sizeof( MY_FEA2_MVMT ) + valLen;
    uint fea2Len = sizeof( MY_FEA2 ) + sizeof( SEA_TYPE );
    uint fullLen = sizeof( MY_FEA2LIST ) + fea2Len + mvmtLen;
    
    uchar *eaData = new uchar[ fullLen ];

    MY_FEA2LIST *fea2List = (MY_FEA2LIST *) eaData;
    fea2List->cbList = fullLen;
    
    MY_FEA2 *fea2 = fea2List->list;
    fea2->oNextEntryOffset = 0;
    fea2->fEA = 0;
    fea2->cbName = sizeof( SEA_TYPE ) - 1;
    fea2->cbValue = mvmtLen;
    strcpy( fea2->szName, SEA_TYPE );
    
    MY_FEA2_MVMT *mvmt = (MY_FEA2_MVMT *) (fea2->szName + sizeof( SEA_TYPE ));
    mvmt->usEAType = EAT_MVMT;
    mvmt->usCodePage = 0;
    mvmt->cbNumEntries = 1;
    
    MY_FEA2_VAL *val = mvmt->aValues;
    val->usEAType = EAT_ASCII;
    val->usValueLen = typeLen;
    memcpy( val->aValueData, type, typeLen );

    EAOP2 eaop2;
    eaop2.fpGEA2List = 0;
    eaop2.fpFEA2List = (FEA2LIST *) fea2List;
    eaop2.oError = 0;

    APIRET rc = DosSetPathInfo( name, FIL_QUERYEASIZE,
                                &eaop2, sizeof( eaop2 ), 0 );
#ifndef QT_NO_DEBUG
    if ( rc ) 
        qSystemWarning( "Could not set file EAs", rc );
#endif    

    delete[] eaData;   
}

static int hex_to_int( uchar c )
{
    if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
    if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
    if ( c >= '0' && c <= '9' ) return c - '0';
    return -1;
}

static inline int hex_to_int( char c )
{
    return hex_to_int( (uchar) c );
}

// -----------------------------------------------------------------------------

struct QPMMime::DefaultDragWorker::Data
{
    Data( bool excl ) : exclusive( excl ) {} 
    
    struct Request
    {
        Request( ULONG i, Provider *p, const char *m, const char *f )
            : index( i ), provider( p ), drm( m ), drf( f )
            , xfer( NULL ), rendered( FALSE ), sharedMem( NULL ) {}

        ~Request()
        {
            // free memory allocated for the target that requested DRM_SHAREDMEM
            if ( sharedMem )
                DosFreeMem( sharedMem );
            Q_ASSERT( !xfer );
        }
        
        ULONG index;
        Provider *provider;
        QCString drm;
        QCString drf;
        DRAGTRANSFER *xfer;
        bool rendered;
        PVOID sharedMem;
    };
    
    inline bool isInitialized() { return providers.count() != 0; } 
    void cleanupRequests();
    
    struct DrfProvider
    {
        DrfProvider() : prov( NULL ) {}
        DrfProvider( const char *d, Provider *p ) : drf( d ), prov( p ) {}
        const QCString drf;
        Provider * const prov;
    };

    typedef QValueList<DrfProvider> DrfProviderList;
    
    Provider *providerFor( const char *drf )
    {
        for ( DrfProviderList::const_iterator it = providers.begin();
              it != providers.end(); ++it )
            if ( qstrcmp( (*it).drf, drf ) == 0 )
                return (*it).prov;
        return NULL;
    }
    
    const bool exclusive : 1;
    DrfProviderList providers;
    
    ULONG itemCnt;
    QIntDict<Request> requests;
    bool renderOk : 1;
};

void QPMMime::DefaultDragWorker::Data::cleanupRequests()
{
    if ( requests.count() ) {
#if defined(QT_CHECK_STATE)
        qWarning( "In the previous DnD session, the drop target sent "
                  "DM_RENDERPREPARE/DM_RENDER\n"
                  "for some drag item but didn't send DM_ENDCONVERSATION!" );
#endif        
        requests.clear();
    }
}

QPMMime::DefaultDragWorker::DefaultDragWorker( bool exclusive )
    : d( new Data( exclusive ) )
{
    d->itemCnt = 0;
    d->requests.setAutoDelete( TRUE );
    d->renderOk = TRUE;
}

QPMMime::DefaultDragWorker::~DefaultDragWorker()
{
    d->cleanupRequests();
    delete d;
}

bool QPMMime::DefaultDragWorker::cleanup( bool isCancelled )
{
    // the return value is: TRUE if the source-side Move for the given
    // drag object should be *dis*allowed, FALSE otherwise (including cases
    // when this DragWorker didn't participate to the conversation at all)
    
    // sanity check
    Q_ASSERT( d->isInitialized() );
    if ( !d->isInitialized() )
        return TRUE;
    
    bool moveDisallowed = FALSE;

#if defined(QT_DEBUG_DND)                        
    qDebug( "DefaultDragWorker: Session ended (cancelled=%d, requests.left=%d)",
            isCancelled, d->requests.count() );
#endif

    if ( d->requests.count() ) {
        // always disallow Move if not all requests got DM_ENDCONVERSATION
        moveDisallowed = TRUE;
    } else {
        // disallow Move if rendering of some item failed
        moveDisallowed = !d->renderOk;
    }

#if defined(QT_DEBUG_DND)                        
    qDebug( "DefaultDragWorker: moveDisallowed=%d", moveDisallowed );
#endif
    
    // Note: remaining requests will be lazily deleted by cleanupRequests()
    // when a new DND session is started
    
    d->renderOk = TRUE;
    d->itemCnt = 0;
        
    // Indicate we're cleaned up (i.e. the DND session is finished)
    d->providers.clear();

    return moveDisallowed;
}

bool QPMMime::DefaultDragWorker::isExclusive() const
{
    return d->exclusive;
}       

ULONG QPMMime::DefaultDragWorker::itemCount() const
{
    return d->itemCnt;
}

QCString QPMMime::DefaultDragWorker::composeFormatString()
{
    QCString formats;

    // sanity checks
    Q_ASSERT( d->isInitialized() );
    if ( !d->isInitialized() )
        return formats;

    bool first = TRUE;
    for ( Data::DrfProviderList::const_iterator it = d->providers.begin();
          it != d->providers.end(); ++it ) {
        if ( first )
            first = FALSE;
        else
            formats += ",";
        formats += (*it).drf;
    }

    Q_ASSERT( !formats.isNull() );
    if ( formats.isNull() )
        return formats;
    
    // DRM_SHAREDMEM comes first to prevent native DRM_OS2FILE
    // rendering on the target side w/o involving the source.
    // Also, we add <DRM_SHAREDMEM,DRF_POINTERDATA> just like WPS does it
    // (however, it doesn't help when dropping objects to it -- WPS still
    // chooses DRM_OS2FILE).
    formats = "("DRM_SHAREDMEM","DRM_OS2FILE")x(" + formats + "),"
              "<"DRM_SHAREDMEM","DRF_POINTERDATA">"; 

#if defined(QT_DEBUG_DND)                        
    qDebug( "DefaultDragWorker: formats=%s, itemCnt=%ld",
            formats.data(), d->itemCnt );
#endif

    return formats;
}

bool QPMMime::DefaultDragWorker::prepare( const char *drm, const char *drf,
                                          DRAGITEM *item, ULONG itemIndex )
{
    // sanity checks
    Q_ASSERT( d->isInitialized() );
    if ( !d->isInitialized() )
        return FALSE;

    Q_ASSERT( item && itemIndex < d->itemCnt );
    if ( !item || itemIndex >= d->itemCnt )
        return FALSE;
    
#if defined(QT_DEBUG_DND)                        
    qDebug( "DefaultDragWorker: Preparing item %ld (id=%ld) for <%s,%s>",
            itemIndex, item->ulItemID, drm, drf );
#endif

    Provider *p = d->providerFor( drf );
    
    if ( !canRender( drm ) || p == NULL ) {
#if defined(QT_DEBUG_DND)                        
        qDebug( "DefaultDragWorker: Cannot render the given RMF" );
#endif
        return FALSE;
    }

    Data::Request *req = d->requests[ item->ulItemID ];

    if ( req ) {
        // this item has been already prepared, ensure it has also been
        // rendered already 
        Q_ASSERT( req->index == itemIndex );
        Q_ASSERT( req->rendered );
        if ( req->index != itemIndex || !req->rendered )
            return FALSE;
        // remove the old request to free resources
        d->requests.remove( item->ulItemID );
    }

    // store the request
    req = new Data::Request( itemIndex, p, drm, drf );
    d->requests.insert( item->ulItemID, req );
    
    return TRUE;
}

void QPMMime::DefaultDragWorker::defaultFileType( const char *&type,
                                                  const char *&ext )
{
    Q_ASSERT( d->providers.count() );
    if ( d->providers.count() ) {
        Provider *p = d->providers.front().prov;
        Q_ASSERT( p );
        if ( p )
            p->fileType( d->providers.front().drf, type, ext );
    }
}

MRESULT QPMMime::DefaultDragWorker::message( ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    if ( msg == DM_RENDER ) {
        DRAGTRANSFER *xfer = (DRAGTRANSFER *) mp1;

        // sanity checks
        Q_ASSERT( d->isInitialized() );
        Q_ASSERT( xfer );
        if ( !d->isInitialized() || !xfer )
            return (MRESULT) FALSE;

        Q_ASSERT( xfer->hwndClient && xfer->pditem );
        if ( !xfer->hwndClient || !xfer->pditem )
            return (MRESULT) FALSE;

        Data::Request *req = d->requests[ xfer->pditem->ulItemID ];

        // check that this item has been prepared (should always be the case
        // because the target would never know our hwnd() otherwise)
        Q_ASSERT( req ); // prepared
        Q_ASSERT( !req->xfer ); // no DM_RENDER requested
        if ( !req || req->xfer )
            return (MRESULT) FALSE;

#if defined(QT_DEBUG_DND)                        
        qDebug( "DefaultDragWorker: Got DM_RENDER to %s for item %ld (id=%ld)",
                queryHSTR( xfer->hstrSelectedRMF ).data(),
                req->index, xfer->pditem->ulItemID );
#endif

        QCString drm, drf;
        if ( !parseRMF( xfer->hstrSelectedRMF, drm, drf ) )
            Q_ASSERT( /* parseRMF() = */ FALSE );
        
        if ( req->drm != drm || req->drf != drf ) { 
            xfer->fsReply = DMFL_RENDERRETRY;
            return (MRESULT) FALSE;
        }
        
        // indicate that DM_RENDER was requested
        req->xfer = xfer;
        
#if defined(QT_DEBUG_DND)                        
        qDebug( "DefaultDragWorker: Will render from [%s] using provider %p",
                req->provider->format( drf ), req->provider );
#endif

        // We would lile to post WM_USER to ourselves to do actual rendering
        // after we return from DM_RENDER. But we are inside DrgDrag() at this
        // point (our DND implementation is fully synchronous by design), so
        // PM will not  deliver this message to us until we return from
        // DrgDrag(). Thus, we have to send it.
        
        WinSendMsg( hwnd(), WM_USER, MPFROMLONG( xfer->pditem->ulItemID ),
                                     MPFROMP( req ) );
        
        return (MRESULT) TRUE;
    }

    if ( msg == WM_USER ) {
        // sanity checks
        Q_ASSERT( d->isInitialized() );
        if ( !d->isInitialized() )
            return (MRESULT) FALSE;

        ULONG itemId = LONGFROMMP( mp1 );
        
        // sanity checks
        Data::Request *req = d->requests[ itemId ];
        Q_ASSERT( req ); // prepared
        Q_ASSERT( req->xfer != NULL ); // DM_RENDER requested
        Q_ASSERT( !req->rendered ); // not yet rendered
        Q_ASSERT( (Data::Request *) PVOIDFROMMP( mp2 ) == req );
        if ( !req || req->xfer == NULL || req->rendered ||
             (Data::Request *) PVOIDFROMMP( mp2 ) != req )
            return (MRESULT) FALSE;

        Q_ASSERT( source() && req->provider && req->index < d->itemCnt );
        if ( !source() || !req->provider || req->index >= d->itemCnt )
            return (MRESULT) FALSE;
            
#if defined(QT_DEBUG_DND)                        
        qDebug( "DefaultDragWorker: Got DO_RENDER for item %ld (id=%ld), "
                "provider=%p, drm=%s, drf=%s",
                req->index, req->xfer->pditem->ulItemID,
                req->provider, req->drm.data(), req->drf.data() );
#endif

        bool renderOk = FALSE;
        QByteArray allData =
            source()->encodedData( req->provider->format( req->drf ) );
        QByteArray itemData;

        renderOk = req->provider->provide( req->drf, allData,
                                           req->index, itemData );

        if ( renderOk ) {
            enum DRM { OS2File, SharedMem } drmType;
            if ( qstrcmp( req->drm, DRM_SHAREDMEM ) == 0 ) drmType = SharedMem;
            else drmType = OS2File;
            
            if ( drmType == OS2File ) {
                QCString renderToName = queryHSTR( req->xfer->hstrRenderToName );
                Q_ASSERT( !renderToName.isEmpty() );
                renderOk = !renderToName.isEmpty();
                if ( renderOk ) {
#if defined(QT_DEBUG_DND)                        
                    qDebug( "DefaultDragWorker: Will write to \"%s\"",
                            renderToName.data() );
#endif
                    QFile file( QFile::decodeName( renderToName ) );
                    renderOk = file.open( IO_WriteOnly );
                    if ( renderOk ) {
                        Q_LONG written =
                            file.writeBlock( itemData.data(), itemData.size() );
                        renderOk = written == ( Q_LONG ) itemData.size() &&
                                   file.status() == IO_Ok;
                        file.close();
                        if ( renderOk && req->xfer->pditem->hstrType ) {
                            // since WPS ignores hstrType, write it manually
                            // to the .TYPE EA of the created file
                            qt_SetFileTypeEA( renderToName,
                                queryHSTR( req->xfer->pditem->hstrType ) ); 
                        }
                    }
                }
            } else {
                PID pid;
                TID tid;
                bool isSameProcess = FALSE;
                renderOk = WinQueryWindowProcess( req->xfer->hwndClient,
                                                  &pid, &tid );
                if ( renderOk ) {
                    PPIB ppib = NULL;
                    DosGetInfoBlocks( NULL, &ppib );
                    isSameProcess = ppib->pib_ulpid == pid;
                    
                    ULONG sz = itemData.size() + sizeof (ULONG);
                    char *ptr = NULL;
                    APIRET rc = isSameProcess ?
                                DosAllocMem( (PPVOID) &ptr, sz,
                                              PAG_COMMIT | PAG_READ | PAG_WRITE ) :                
                                DosAllocSharedMem( (PPVOID) &ptr, NULL, sz,
                                                    OBJ_GIVEABLE | PAG_COMMIT |
                                                    PAG_READ | PAG_WRITE );                
                    renderOk = rc == 0;
                    if ( renderOk && !isSameProcess ) {
                        rc = DosGiveSharedMem( ptr, pid, PAG_READ );
                        renderOk = rc == 0;
                    }
                    if ( renderOk ) {
                        *(ULONG *) ptr = itemData.size();
                        memcpy( ptr + sizeof (ULONG), itemData.data(),
                                itemData.size() );
                        req->xfer->hstrRenderToName = (HSTR) ptr;
                        req->sharedMem = ptr;
#if defined(QT_DEBUG_DND)                        
                        qDebug( "DefaultDragWorker: Created shared memory object %p",
                                ptr );
#endif
#ifndef QT_NO_DEBUG
                    } else {
                        qSystemWarning( "DosAllocSharedMem/DosGiveSharedMem "
                                        "failed", rc );
#endif                        
                    }
#ifndef QT_NO_DEBUG
                } else {
                    qSystemWarning( "WinQueryWindowProcess failed" );
#endif                    
                }
            }
        }

        req->rendered = TRUE;
        // cumulative render result
        d->renderOk &= renderOk;

#if defined(QT_DEBUG_DND)                        
        qDebug( "DefaultDragWorker: renderOk=%d, overall.renderOk=%d",
                renderOk, d->renderOk );
#endif

        // note that we don't allow the target to retry
        USHORT reply = renderOk ? DMFL_RENDEROK : DMFL_RENDERFAIL;
        DrgPostTransferMsg( req->xfer->hwndClient, DM_RENDERCOMPLETE,
                            req->xfer, reply, 0, FALSE );

        // DRAGTRANSFER is no more necessary, free it early
        qt_DrgFreeDragtransfer( req->xfer );
#if defined(QT_DEBUG_DND)                        
        {
            ULONG size = ~0, flags = 0;
            DosQueryMem( req->xfer, &size, &flags );
            qDebug( "DefaultDragWorker: Freed DRAGTRANSFER: "
                    "req->xfer=%p, size=%lu(0x%08lX), flags=0x%08lX",
                    req->xfer, size, size, flags );
        }
#endif        
        req->xfer = NULL;
        
        return (MRESULT) FALSE;
    }
    
    if ( msg == DM_ENDCONVERSATION ) {
        // we don't check that isInitialized() is TRUE here, because WPS
        // (and probably some other apps) may send this message after
        // cleanup() is called up on return from DrgDrag
        
        ULONG itemId = LONGFROMMP( mp1 );
        ULONG flags = LONGFROMMP( mp2 );
        
        // sanity check (don't assert, see above)
        Data::Request *req = d->requests[ itemId ];
        Q_ASSERT( req );
        if ( !req )
            return (MRESULT) FALSE;

#if defined(QT_DEBUG_DND)                        
        qDebug( "DefaultDragWorker: Got DM_ENDCONVERSATION for item %ld (id=%ld), "
                "provider=%p, drm=%s, drf=%s, rendered=%d, outdated=%d",
                req->index, itemId, req->provider,
                req->drm.data(), req->drf.data(), req->rendered,
                !d->isInitialized() );
#endif

        // proceed further only if it's not an outdated request
        // from the previous DND session
        if ( d->isInitialized() ) {
            if ( !req->rendered ) {
                // we treat cancelling the render request (for any reason)
                // as a failure
                d->renderOk = FALSE;
            } else {
                // the overall success is TRUE only if target says Okay
                d->renderOk &= flags == DMFL_TARGETSUCCESSFUL;
            }
        }
        
        // delete the request
        d->requests.remove( itemId );
        
        return (MRESULT) FALSE;
    }
    
    return (MRESULT) FALSE;
}

bool QPMMime::DefaultDragWorker::addProvider( const char *drf, Provider *provider,
                                              ULONG itemCnt /* = 1 */ )
{
    // make sure remaining requests from the previous DND session are deleted
    d->cleanupRequests();

    Q_ASSERT( drf && provider && itemCnt >= 1 );
    if ( drf && provider && itemCnt >= 1 ) {
        if ( d->providers.count() == 0 ) {
            // first provider
            d->itemCnt = itemCnt;
            d->providers.push_back( Data::DrfProvider( drf, provider ) );
            return TRUE;
        }
        // next provider, must not be exclusive and itemCnt must match
        if ( !d->exclusive && d->itemCnt == itemCnt ) {
            // ensure there are no dups (several providers for the same drf)
            if ( !d->providerFor( drf ) )
                d->providers.push_back( Data::DrfProvider( drf, provider ) );
            return TRUE;
        }
    }
    return FALSE;
}

// static
bool QPMMime::DefaultDragWorker::canRender( const char *drm )
{
    return qstrcmp( drm, DRM_SHAREDMEM ) == 0 ||
           qstrcmp( drm, DRM_OS2FILE ) == 0;
}

//------------------------------------------------------------------------------

struct QPMMime::DefaultDropWorker::Data
{
    struct MimeProvider
    {
        MimeProvider() : prov( NULL ) {}
        MimeProvider( const char *m, Provider *p ) : mime( m ), prov( p ) {}
        const QCString mime;
        Provider * const prov;
    };

    typedef QValueList<MimeProvider> MimeProviderList;
    
    Provider *providerFor( const char *mime )
    {
        for ( MimeProviderList::const_iterator it = providers.begin();
              it != providers.end(); ++it )
            if ( qstrcmp( (*it).mime, mime ) == 0 )
                return (*it).prov;
        return NULL;
    }
    
    bool exclusive : 1;
    MimeProviderList providers;

    bool sending_DM_RENDER : 1;
    bool got_DM_RENDERCOMPLETE : 1;
    USHORT flags_DM_RENDERCOMPLETE;
    int waiting_DM_RENDERCOMPLETE;
};

QPMMime::DefaultDropWorker::DefaultDropWorker() : d( new Data() )
{
    d->exclusive = FALSE;
    d->sending_DM_RENDER = d->got_DM_RENDERCOMPLETE = FALSE;
    d->flags_DM_RENDERCOMPLETE = 0;
    d->waiting_DM_RENDERCOMPLETE = 0;
}

QPMMime::DefaultDropWorker::~DefaultDropWorker()
{
    delete d;
}

void QPMMime::DefaultDropWorker::cleanup( bool isAccepted, bool isActAccepted )
{
    if ( d->waiting_DM_RENDERCOMPLETE != 0 ) {
#if defined(QT_CHECK_STATE)
        qWarning( "The previous drag source didn't post DM_RENDERCOMPLETE!\n"
                  "Contact the drag source developer." );
#endif        
        qApp->eventLoop()->exitLoop();
    }

    d->providers.clear();
    d->exclusive = FALSE;
    d->sending_DM_RENDER = d->got_DM_RENDERCOMPLETE = FALSE;
    d->flags_DM_RENDERCOMPLETE = 0;
    d->waiting_DM_RENDERCOMPLETE = 0;
}

bool QPMMime::DefaultDropWorker::isExclusive() const
{
    return d->exclusive;
}

bool QPMMime::DefaultDropWorker::provides( const char *format ) const
{
    return d->providerFor( format ) != NULL;
}

int QPMMime::DefaultDropWorker::formatCount() const
{
    return d->providers.count();
}

const char *QPMMime::DefaultDropWorker::format( int fn ) const
{
    if ( fn >= 0 && (uint) fn < d->providers.count() )
        return d->providers[ fn ].mime;
    return NULL;    
}

static QCString composeTempFileName()
{
    static char defTmpDir[ 3 ] = { 0 };
    const char *tmpDir = getenv( "TEMP" );
    if ( !tmpDir ) tmpDir = getenv( "TMP" );
    if ( !tmpDir || !QFile::exists( QFile::decodeName( tmpDir ) ) ) {
        if ( !defTmpDir[ 0 ] ) { 
            ULONG bootDrive = 0;
            DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                             &bootDrive, sizeof (bootDrive) );
            defTmpDir[ 0 ] = bootDrive;
            defTmpDir[ 1 ] = ':';
        }
        tmpDir = defTmpDir;
    }

    static bool srandDone = FALSE;
    if ( !srandDone ) {
        srand( time( NULL ) );
        srandDone = TRUE;
    }
    
    ULONG num = rand();
    enum { Attempts = 100 };
    int attempts = Attempts;
    
    QCString tmpName;
    do {
        tmpName.sprintf( "%s\\%08lX.tmp", tmpDir, num );
        if ( !QFile::exists( QFile::decodeName( tmpName ) ) )
            break;
        num = rand();
    } while ( --attempts > 0 );
    
    Q_ASSERT( attempts > 0 );
    if ( attempts <= 0 )
        tmpName.resize( 0 );
    
    return tmpName;
}

QByteArray QPMMime::DefaultDropWorker::encodedData( const char *format ) const
{
#if defined(QT_DEBUG_DND)                        
    qDebug( "DefaultDropWorker::encodedData(%s)", format );
#endif

    QByteArray data;

    Q_ASSERT( info() );
    if ( !info() )
        return data;
    
    ULONG itemCount = DrgQueryDragitemCount( info() );
    Q_ASSERT( itemCount );
    if ( !itemCount )
        return data;
    
    Provider *provider = d->providerFor( format );
    if ( !provider )
        return data;

    const char *drf = provider->drf( format );
    Q_ASSERT( drf );
    if ( !drf )
        return data;

    // Note: Allocating and freeing DRAGTRANSFER structures is a real mess. It's
    // absolutely unclear how they can be reused for multiple items and/or render
    // requests. My practice shows, that they cannot be reused at all, especially
    // when the source and the target are the same process: if we have multiple
    // items and use the same DRAGTRANSFER for all of them, the source will call
    // DrgFreeDragtransfer() every time that will eventually destroy the memory
    // object before the target finishes to work with it, so that the next
    // DrgFreeDragtransfer() will generate a segfault in PMCTLS. Also note that
    // using a number > 1 as an argument to DrgAllocDragtransfer() won't help
    // because that will still allocate a single memory object. Thus, we will
    // always allocate a new struct per every item. It seems to work.

    QCString renderToName = composeTempFileName();
    HSTR hstrRenderToName = DrgAddStrHandle( renderToName );
    
    HSTR rmfOS2File =
        DrgAddStrHandle( QCString().sprintf( "<"DRM_OS2FILE",%s>", drf ) );
    HSTR rmfSharedMem =
        DrgAddStrHandle( QCString().sprintf( "<"DRM_SHAREDMEM",%s>", drf ) );
    
    MRESULT mrc;
    bool renderOk = FALSE;
    
    DRAGTRANSFER *xfer = NULL;
    QCString srcFileName;

    QByteArray allData, itemData;
    
    for ( ULONG i = 0; i < itemCount; ++i ) {
        DRAGITEM *item = DrgQueryDragitemPtr( info(), i );
        Q_ASSERT( item );
        if ( !item ) {
            renderOk = FALSE;
            break;
        }

        enum { None, OS2File, SharedMem } drm = None;
        bool needToTalk = TRUE;
        
        // determine the mechanism to use (prefer DRM_SHAREDMEM)
        
        if ( DrgVerifyRMF( item, DRM_SHAREDMEM, drf ) &&
             DrgVerifyRMF( item, DRM_SHAREDMEM, DRF_POINTERDATA ) )
            drm = SharedMem;
        if ( DrgVerifyRMF( item, DRM_OS2FILE, drf ) ) {
            srcFileName = querySourceNameFull( item );
            // If the source provides the full file name, we prefer DRM_OS2FILE
            // even if there is also DRM_SHAREDMEM available because we don't
            // need to do any communication in this case at all. This will help
            // with some native drag sources (such as DragText) that cannot send
            // DM_RENDERCOMPLETE synchronously (before we return from DM_DROP)
            // and would hang otherwise.
            if ( !srcFileName.isEmpty() ) {
                needToTalk = FALSE;
                drm = OS2File;
            } else if ( drm == None ) {
                srcFileName = renderToName;
                drm = OS2File;
            }
        }
        Q_ASSERT( drm != None );
        if ( drm == None ) {
            renderOk = FALSE;
            break;
        }
        
        if ( needToTalk ) {
            // need to perform a conversation with the source,
            // allocate a new DRAGTRANSFER structure for each item
            xfer = DrgAllocDragtransfer( 1 );
            Q_ASSERT( xfer );
            if ( !xfer ) {
                renderOk = FALSE;
                break;
            }
    
            xfer->cb = sizeof( DRAGTRANSFER );
            xfer->hwndClient = hwnd();
            xfer->ulTargetInfo = (ULONG) info();
            xfer->usOperation = info()->usOperation;
            
            xfer->pditem = item;
            if ( drm == OS2File ) {
                xfer->hstrSelectedRMF = rmfOS2File;
                xfer->hstrRenderToName = hstrRenderToName;
            } else {
                xfer->hstrSelectedRMF = rmfSharedMem;
                xfer->hstrRenderToName = 0;
            }
    
#if defined(QT_DEBUG_DND)                        
            qDebug( "DefaultDropWorker: Will use %s to render item %p",
                    queryHSTR( xfer->hstrSelectedRMF ).data(), item );
#endif

            mrc = (MRESULT) TRUE;
            if ( (item->fsControl & DC_PREPARE) |
                 (item->fsControl & DC_PREPAREITEM) ) {
#if defined(QT_DEBUG_DND)                        
                qDebug( "DefaultDropWorker: Sending DM_RENDERPREPARE to 0x%08lX...",
                        info()->hwndSource );
#endif                
                mrc = DrgSendTransferMsg( info()->hwndSource, DM_RENDERPREPARE,
                                          MPFROMP (xfer), 0 );
#if defined(QT_DEBUG_DND)                        
                qDebug( "DefaultDropWorker: Finisned sending DM_RENDERPREPARE\n"
                        " mrc=%p, xfer->fsReply=0x%08hX", mrc, xfer->fsReply );
#endif                
                renderOk = (BOOL) mrc;
            }
    
            if ( (BOOL) mrc ) {
#if defined(QT_DEBUG_DND)                        
                qDebug( "DefaultDropWorker: Sending DM_RENDER to 0x%08lX...",
                        item->hwndItem );
#endif
                d->sending_DM_RENDER = TRUE;
                mrc = DrgSendTransferMsg( item->hwndItem, DM_RENDER,
                                          MPFROMP (xfer), 0 );
                d->sending_DM_RENDER = FALSE;
#if defined(QT_DEBUG_DND)                        
                qDebug( "DefaultDropWorker: Finisned Sending DM_RENDER\n"
                        " mrc=%p, xfer->fsReply=0x%hX, got_DM_RENDERCOMPLETE=%d",
                        mrc, xfer->fsReply, d->got_DM_RENDERCOMPLETE );
#endif

                if ( !(BOOL) mrc || d->got_DM_RENDERCOMPLETE ) {
                    if ( d->got_DM_RENDERCOMPLETE )
                        renderOk = (d->flags_DM_RENDERCOMPLETE & DMFL_RENDEROK);
                    else
                        renderOk = FALSE;
                } else {
                    // synchronously wait for DM_RENDERCOMPLETE
                    d->waiting_DM_RENDERCOMPLETE = qApp->loopLevel() + 1;
#if defined(QT_DEBUG_DND)                        
                    qDebug( "DefaultDropWorker: Waiting for DM_RENDERCOMPLETE..." );
                    int level =
#endif
                    qApp->eventLoop()->enterLoop();
#if defined(QT_DEBUG_DND)                        
                    qDebug( "DefaultDropWorker: Finished waiting for "
                            "DM_RENDERCOMPLETE (%d)\n"
                            " got_DM_RENDERCOMPLETE=%d, usFS=0x%hX",
                            level, d->got_DM_RENDERCOMPLETE, d->flags_DM_RENDERCOMPLETE );
#endif
                    // JFTR: at this point, cleanup() might have been called,
                    // as a result of either program exit or getting another        
                    // DM_DRAGOVER (if the source app has crashed) before getting 
                    // DM_RENDERCOMPLETE from the source. Use data members with
                    // care!
                    d->waiting_DM_RENDERCOMPLETE = 0;
                    renderOk = d->got_DM_RENDERCOMPLETE &&
                               (d->flags_DM_RENDERCOMPLETE & DMFL_RENDEROK);
                }
    
                d->got_DM_RENDERCOMPLETE = FALSE;
            }
        } else {
#if defined(QT_DEBUG_DND)                        
            qDebug( "DefaultDropWorker: Source supports <"DRM_OS2FILE",%s> and "
                    "provides a file \"%s\" for item %p (no need to render)",
                    drf, srcFileName.data(), item );
#endif
            renderOk = TRUE;
        }

        if ( renderOk ) {
            if ( drm == OS2File ) {
#if defined(QT_DEBUG_DND)                        
                qDebug( "DefaultDragWorker: Will read from \"%s\"",
                        srcFileName.data() );
#endif
                QFile file( QFile::decodeName( srcFileName ) );
                renderOk = file.open( IO_ReadOnly );
                if ( renderOk ) {
                    itemData = file.readAll();
                    renderOk = file.status() == IO_Ok;
                    file.close();
                }
                bool ok = file.remove();
                Q_ASSERT( (ok = ok) );
            } else {
                Q_ASSERT( xfer->hstrRenderToName );
                renderOk = xfer->hstrRenderToName != 0;
                if ( renderOk ) {
                    const char *ptr = (const char *) xfer->hstrRenderToName;
                    ULONG size = ~0;
                    ULONG flags = 0;
                    APIRET rc = DosQueryMem( (PVOID) ptr, &size, &flags );
                    renderOk = rc == 0;
                    if ( renderOk ) {
#if defined(QT_DEBUG_DND)                        
                        qDebug( "DefaultDropWorker: Got shared data=%p, size=%lu "
                                "(0x%08lX), flags=0x%08lX", ptr, size, size, flags );
#endif
                        Q_ASSERT( flags & (PAG_COMMIT | PAG_READ | PAG_BASE) ==
                                  (PAG_COMMIT | PAG_READ | PAG_BASE) ); 
                        renderOk = flags & (PAG_COMMIT | PAG_READ | PAG_BASE) ==
                                   (PAG_COMMIT | PAG_READ | PAG_BASE);
#ifndef QT_NO_DEBUG
                    } else {
                        qSystemWarning( "DosQueryMem failed", rc );
#endif                        
                    }
                    if ( renderOk ) {
                        ULONG realSize = *(ULONG *) ptr;
#if defined(QT_DEBUG_DND)                        
                        qDebug( "DefaultDropWorker: realSize=%lu", realSize );
#endif
                        Q_ASSERT( realSize <= size );
                        renderOk = realSize <= size;
                        if ( renderOk ) {
                            itemData.resize( realSize );
                            memcpy( itemData.data(), ptr + sizeof (ULONG), realSize );
                        }
                    }
                    // free memory only if it is given by another process,
                    // otherwise DefaultDragWorker will free it
                    if ( flags & PAG_SHARED )
                        DosFreeMem( (PVOID) xfer->hstrRenderToName );
                }
            }
        }

        if ( renderOk )
            renderOk = provider->provide( format, i, itemData, allData );

        if ( needToTalk ) {
            // free the DRAGTRANSFER structure
            DrgFreeDragtransfer( xfer );
#if defined(QT_DEBUG_DND)                        
            {
                ULONG size = ~0, flags = 0;
                DosQueryMem( xfer, &size, &flags );
                qDebug( "DefaultDropWorker: Freed DRAGTRANSFER: "
                        "xfer=%p, size=%lu(0x%08lX), flags=0x%08lX",
                        xfer, size, size, flags );
            }
#endif            
            xfer = NULL;
        }
        
        if ( !renderOk )
            break;
    }

#if defined(QT_DEBUG_DND)                        
    qDebug( "DefaultDropWorker: renderOk=%d", renderOk );
#endif

    DrgDeleteStrHandle( rmfSharedMem );
    DrgDeleteStrHandle( rmfOS2File );
    DrgDeleteStrHandle( hstrRenderToName );

    if ( renderOk )
        data = allData;

    return data;
}

MRESULT QPMMime::DefaultDropWorker::message( ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch ( msg ) {
        case DM_RENDERCOMPLETE: {
            // sanity check
            Q_ASSERT( info() );
            if ( !info() )
                return (MRESULT) FALSE;

#if defined(QT_DEBUG_DND)                        
            qDebug( "DefaultDropWorker: Got DM_RENDERCOMPLETE" );
#endif            
            d->got_DM_RENDERCOMPLETE = TRUE;
            d->flags_DM_RENDERCOMPLETE = SHORT1FROMMP( mp2 );

            if (d->sending_DM_RENDER)
            {
                DRAGTRANSFER *xfer = (DRAGTRANSFER *) mp1;
#if defined(QT_CHECK_STATE)
                qWarning( "Drag item 0x%08lX sent DM_RENDERCOMPLETE w/o first "
                          "replying to DM_RENDER!\n"
                          "Contact the drag source developer.",
                          xfer->pditem->hwndItem );
#endif                
                return (MRESULT) FALSE;
            }
            
            // stop synchronous waiting for DM_RENDERCOMPLETE
            if ( d->waiting_DM_RENDERCOMPLETE != 0 )
                qApp->eventLoop()->exitLoop();
            return (MRESULT) FALSE;
        }
        default:
            break;
    }
    
    return (MRESULT) FALSE;
}

bool QPMMime::DefaultDropWorker::addProvider( const char *format,
                                              Provider *provider )
{
    Q_ASSERT( format && provider );
    if ( format && provider && !d->exclusive ) {
        // ensure there are no dups (several providers for the same mime)
        if ( !d->providerFor( format ) )
            d->providers.push_back( Data::MimeProvider( format, provider ) );
        return TRUE;
    }
    return FALSE;
}

bool QPMMime::DefaultDropWorker::addExclusiveProvider( const char *format,
                                                       Provider *provider )
{
    Q_ASSERT( format && provider );
    if ( format && provider && !d->exclusive ) {
        d->exclusive = TRUE;
        d->providers.clear();
        d->providers.push_back( Data::MimeProvider( format, provider ) );
        return TRUE;
    }
    return FALSE;
}

// static
bool QPMMime::DefaultDropWorker::canRender( DRAGITEM *item, const char *drf )
{
    return DrgVerifyRMF( item, DRM_OS2FILE, drf ) ||
           (DrgVerifyRMF( item, DRM_SHAREDMEM, drf ) &&
            DrgVerifyRMF( item, DRM_SHAREDMEM, DRF_POINTERDATA ));
}

// static
/*! \internal
  Parses the rendering mechanism/format specification of the given \a item
  and stores only those mechanism branches in the given \a list that represent
  mechanisms supported by this worker. Returns FALSE if fails to parse the
  RMF specification. Note that if no supported mechanisms are found, TRUE is
  returned but the \a list will simply contain zero items.  
  \note The method clears the given \a list variable before proceeding and sets
  auto-deletion to TRUE.
  \sa canRender(), PMMime::parseRMFs()
*/
bool QPMMime::DefaultDropWorker::getSupportedRMFs( DRAGITEM *item, 
                                                   QPtrList<QStrList> &list )
{
    if ( !parseRMFs( item->hstrRMF, list ) )
        return FALSE;
    
    for ( QStrList *mech = list.first(); mech; ) {
        const char *drm = mech->first();
        if ( qstrcmp( drm, DRM_OS2FILE ) == 0 ) {
            mech = list.next();
            continue;
        }
        if ( qstrcmp( drm, DRM_SHAREDMEM ) == 0 ) {
            const char *drf = mech->next();
            // accept DRM_SHAREDMEM only if there is DRF_POINTERDATA
            for( ; drf; drf = mech->next() ) {
                if ( qstrcmp( drf, DRF_POINTERDATA ) == 0 )
                    break;
            }
            if ( drf ) {
                mech = list.next();
                continue;
            }
        }
        // remove the unsupported mechanism branch from the list
        bool wasLast = list.getLast() == mech;
        list.removeRef( mech );
        // after deleting the last item, the current one will be set to the new
        // last item which was already analyzed earlier, so set to 0 to stop 
        mech = wasLast ? 0 : list.current();
    }
    
    return TRUE;
}

#endif // !QT_NO_DRAGANDDROP

//------------------------------------------------------------------------------

static QPtrList<QPMMime> mimes;

/*!
  \class QPMMime qmime.h
  \brief The QPMMime class maps open-standard MIME to OS/2 PM Clipboard formats.
  \ingroup io
  \ingroup draganddrop
  \ingroup misc

  Qt's drag-and-drop and clipboard facilities use the MIME standard.
  On X11, this maps trivially to the Xdnd protocol, but on OS/2
  although some applications use MIME types to describe clipboard
  formats, others use arbitrary non-standardized naming conventions,
  or unnamed built-in formats of OS/2.

  By instantiating subclasses of QPMMime that provide conversions
  between OS/2 PM Clipboard and MIME formats, you can convert
  proprietary clipboard formats to MIME formats.

  Qt has predefined support for the following OS/2 PM Clipboard formats:
  \list
    \i CF_TEXT - converted to "text/plain;charset=system" or "text/plain"
	    and supported by QTextDrag.
    \i CF_BITMAP - converted to "image/fmt", where fmt is
		a \link QImage::outputFormats() Qt image format\endlink,
	    and supported by QImageDrag.
  \endlist

  An example use of this class would be to map the OS/2 Metafile
  clipboard format (CF_METAFILE) to and from the MIME type "image/x-metafile".
  This conversion might simply be adding or removing a header, or even
  just passing on the data.  See the
  \link dnd.html Drag-and-Drop documentation\endlink for more information
  on choosing and definition MIME types.

  You can check if a MIME type is convertible using canConvert() and
  can perform conversions with convertToMime() and convertFromMime().
*/

/*!
  Constructs a new conversion object, adding it to the globally accessed
  list of available convertors.
*/
QPMMime::QPMMime()
{
    // we prepend the convertor to let it override other convertors
    mimes.prepend( this );
}

/*!
  Destroys a conversion object, removing it from the global
  list of available convertors.
*/
QPMMime::~QPMMime()
{
    mimes.remove( this );
}



struct QPMRegisteredMimeType {
    QPMRegisteredMimeType( ulong c, const char *m ) :
	cf( c ), mime( m ) {}
    ulong cf;
    QCString mime;
#if !defined(QT_NO_DRAGANDDROP)
    QCString drf;
#endif // !QT_NO_DRAGANDDROP
};


class QPMMimeAnyMime : public QPMMime {
public:
    QPMMimeAnyMime();
    ~QPMMimeAnyMime();
    
    const char *convertorName();
    int countCf();
    ulong cf( int index );
    ulong flFor( ulong cf );
    ulong cfFor( const char* mime );
    const char *mimeFor( ulong cf );
    bool canConvert( const char* mime, ulong cf );
    QByteArray convertToMime( QByteArray data, const char *, ulong );
    QByteArray convertFromMime( QByteArray data, const char *, ulong );

#if !defined(QT_NO_DRAGANDDROP)
    
    DragWorker *dragWorkerFor( const char *mime, QMimeSource *src );
    DropWorker *dropWorkerFor( DRAGINFO *info );

    class AnyDragProvider : public DefaultDragWorker::Provider
    {
    public:
        AnyDragProvider( QPMMimeAnyMime *am ) : anyMime( am ) {}
        // Provider interface
        const char *format( const char *drf ) const;
        bool provide( const char *drf, const QByteArray &allData,
                      ULONG itemIndex, QByteArray &itemData );
        void fileType( const char *drf, const char *&type, const char *&ext );
    private:
        QPMMimeAnyMime *anyMime;
    };

    class AnyDropProvider : public DefaultDropWorker::Provider
    {
    public:
        AnyDropProvider( QPMMimeAnyMime *am ) : anyMime( am ) {}
        // Provider interface
        const char *drf( const char *format ) const;
        bool provide( const char *format, ULONG itemIndex,
                      const QByteArray &itemData, QByteArray &allData );
    private:
        QPMMimeAnyMime *anyMime;
    };

#endif // !QT_NO_DRAGANDDROP
    
private:
    ulong registerMimeType( const char *mime );
    const char *lookupMimeType( ulong cf );

    QPtrList<QPMRegisteredMimeType> mimetypes;
    
#if !defined(QT_NO_DRAGANDDROP)

    AnyDragProvider anyDragProvider;
    AnyDropProvider anyDropProvider;

    friend class AnyDragProvider;
    friend class AnyDropProvider;
    
#endif // !QT_NO_DRAGANDDROP
};

QPMMimeAnyMime::QPMMimeAnyMime()
#if !defined(QT_NO_DRAGANDDROP)
    : anyDragProvider( AnyDragProvider( this ) )
    , anyDropProvider( AnyDropProvider( this ) )
#endif // !QT_NO_DRAGANDDROP
{
    mimetypes.setAutoDelete( TRUE );
}

QPMMimeAnyMime::~QPMMimeAnyMime()
{
    // dereference all atoms we've referenced
    HATOMTBL tbl = WinQuerySystemAtomTable();
    for ( QPMRegisteredMimeType *mt = mimetypes.first();
          mt; mt = mimetypes.next() ) {
        WinDeleteAtom( tbl, 0xFFFF0000 | mt->cf );
    }
}

const char* QPMMimeAnyMime::convertorName()
{
    return "Any-Mime";
}

int QPMMimeAnyMime::countCf()
{
    return mimetypes.count();
}

ulong QPMMimeAnyMime::cf( int index )
{
    return mimetypes.at( index )->cf;
}

ulong QPMMimeAnyMime::flFor( ulong cf )
{
    // all formats in this converter assume CFI_POINTER data storage type
    return CFI_POINTER;
}

ulong QPMMimeAnyMime::cfFor( const char* mime )
{
    QPMRegisteredMimeType *mt = mimetypes.current();
    if ( mt ) // quick check with most-recent
	if ( qstricmp( mt->mime, mime ) == 0 )
            return mt->cf;
    for ( mt = mimetypes.first(); mt; mt = mimetypes.next() )
	if ( qstricmp( mt->mime, mime ) == 0 )
	    return mt->cf;
    // try to register the mime type
    return registerMimeType( mime );
}

const char* QPMMimeAnyMime::mimeFor( ulong cf )
{
    QPMRegisteredMimeType *mt = mimetypes.current();
    if ( mt ) // quick check with most-recent
	if ( mt->cf == cf )
	    return mt->mime;
    for ( mt = mimetypes.first(); mt; mt = mimetypes.next() )
	if ( mt->cf == cf )
	    return mt->mime;
    // try to determine the mime type from the clipboard format ID
    return lookupMimeType( cf );
}

bool QPMMimeAnyMime::canConvert( const char* mime, ulong cf )
{
    return mime && cf && qstricmp( mimeFor( cf ), mime ) == 0;
}

QByteArray QPMMimeAnyMime::convertToMime( QByteArray data, const char *, ulong )
{
    return data;
}

QByteArray QPMMimeAnyMime::convertFromMime( QByteArray data, const char *, ulong )
{
    return data;
}

#if !defined(QT_NO_DRAGANDDROP)

const char *QPMMimeAnyMime::AnyDragProvider::format( const char *drf ) const
{
    QPMRegisteredMimeType *mt = anyMime->mimetypes.current();
    if ( mt ) // quick check with most-recent
	if ( qstricmp( mt->drf, drf ) == 0 )
            return mt->mime;
    for ( mt = anyMime->mimetypes.first(); mt; mt = anyMime->mimetypes.next() )
	if ( qstricmp( mt->drf, drf ) == 0 )
	    return mt->mime;
        
    Q_ASSERT( /* mt->mime != */ NULL );
    return NULL;
}

bool QPMMimeAnyMime::AnyDragProvider::provide( const char *drf,
                                               const QByteArray &allData,
                                               ULONG itemIndex,
                                               QByteArray &itemData )
{
    // always straight through coversion
    itemData = allData;
    return TRUE;
}

void QPMMimeAnyMime::AnyDragProvider::fileType( const char *drf, const char *&type,
                                                const char *&ext )
{
    // file type = mime
    type = format( drf );
    Q_ASSERT( type );
    
    // no way to determine the extension
    ext = NULL;

    /// @todo (dmik) later, we can add a hack method to qmime.cpp that returns
    //  the extension->mime map of the current QMimeSourceFactory and use it
    //  to determine the extension.
};

const char *QPMMimeAnyMime::AnyDropProvider::drf( const char *format ) const
{
    ulong cf = anyMime->cfFor( format );
    if ( cf ) {
        // current is what was found by cfFor()
        Q_ASSERT( anyMime->mimetypes.current()->drf );
        return anyMime->mimetypes.current()->drf;
    }
        
    Q_ASSERT( FALSE );
    return NULL;
}

bool QPMMimeAnyMime::AnyDropProvider::provide( const char *format,
                                             ULONG /* itemIndex */,
                                             const QByteArray &itemData,
                                             QByteArray &allData )
{
    // always straight through coversion
    allData = itemData;
    return TRUE;
}

QPMMime::DragWorker *QPMMimeAnyMime::dragWorkerFor( const char *mime, QMimeSource *src )
{
    ulong cf = cfFor( mime );
    if ( cf ) {
        DefaultDragWorker *defWorker = defaultCoopDragWorker();
        QCString drf;
        drf.sprintf( "CF_%04lX", cf ); 
        // current is what was found by cfFor()
        mimetypes.current()->drf = drf;
        // add a cooperative provider
        defWorker->addProvider( drf, &anyDragProvider );
        return defWorker;
    }
    
    return NULL;
}

QPMMime::DropWorker *QPMMimeAnyMime::dropWorkerFor( DRAGINFO *info )
{
    ULONG itemCount = DrgQueryDragitemCount( info );
    Q_ASSERT( itemCount );
    if ( !itemCount )
        return NULL;

    if ( itemCount == 1 ) {
        DRAGITEM *item = DrgQueryDragitemPtr( info, 0 );
        Q_ASSERT( item );
        if ( !item )
            return NULL;

        DefaultDropWorker *defWorker = defaultDropWorker();
        bool atLeastOneSupported = FALSE;
        
        // check that we support one of DRMs and the format is MIME_hhhh
        QPtrList<QStrList> list;
        defWorker->getSupportedRMFs( item, list );
        for( QStrList *mech = list.first(); mech; mech = list.next() ) {
#if defined(QT_DEBUG_DND)                        
            qDebug( "QPMMimeAnyMime: Supported drm: %s", mech->getFirst() );            
#endif
            for( const char *drf = mech->at( 1 ); drf; drf = mech->next() ) {
                if ( qstrncmp( drf, "CF_", 3 ) == 0 ) {
                    // get the atom ID
                    ulong cf = 0; 
                    uint len = qstrlen ( drf );
                    for ( uint i = 3; i < len; ++i ) {
                        int hex = hex_to_int( drf[ i ] );
                        if ( hex < 0 ) { cf = 0; break; }
                        cf = (cf << 4) | (uint) hex;
                    }
                    Q_ASSERT( cf );
                    if ( !cf )
                        continue;
#if defined(QT_DEBUG_DND)                        
                    qDebug( "QPMMimeAnyMime: drf %s is atom 0x%08lX", drf, cf );
#endif                    
                    const char *mime = mimeFor( cf );
                    if ( mime ) {
#if defined(QT_DEBUG_DND)                        
                        qDebug( "QPMMimeAnyMime: Will provide [%s] for drf %s",
                                mime, drf );
#endif                        
                        // current is what was found by mimeFor()
                        mimetypes.current()->drf = drf;
                        // add a cooperative provider (can coexist with others)
                        defWorker->addProvider( mime, &anyDropProvider );
                        atLeastOneSupported = TRUE;
                    }
                }
            }
        }
        
        if ( atLeastOneSupported )
            return defWorker;
    }
    
    return NULL;
}

#endif // !QT_NO_DRAGANDDROP

ulong QPMMimeAnyMime::registerMimeType( const char *mime )
{
    QCString atom;
    atom.sprintf( "mime:%s", mime );
    
    ulong f = WinAddAtom( WinQuerySystemAtomTable(), atom );
    if ( !f ) {
#ifndef QT_NO_DEBUG
	qSystemWarning( "QPMMime: Failed to register the clipboard format" );
#endif
        return 0;
    }

    mimetypes.append( new QPMRegisteredMimeType ( f, mime ) );
    Q_ASSERT( mimetypes.current()->cf == f ); 
    return mimetypes.current()->cf == f ? f : 0;
}

const char *QPMMimeAnyMime::lookupMimeType( ulong cf )
{
    HATOMTBL tbl = WinQuerySystemAtomTable();
    ULONG len = WinQueryAtomLength( tbl, cf );
    QCString atom( len + 1 );
    WinQueryAtomName( tbl, cf, atom.data(), atom.size() );

    // see if this atom represents a mime type    
    if ( qstrncmp( atom, "mime:", 5 ) == 0 ) {
        atom = atom.right( atom.length() - 5 );
        if ( !atom.isEmpty() ) {
            ulong f = WinAddAtom( tbl, (PSZ) (0xFFFF0000 | cf) );
            if ( !f ) {
#ifndef QT_NO_DEBUG
	        qSystemWarning( "QPMMime: Failed to reference the clipboard format" );
#endif
                return 0;
            }
            mimetypes.append( new QPMRegisteredMimeType ( cf, atom ) );
            Q_ASSERT( mimetypes.current()->cf == cf ); 
            return mimetypes.current()->cf == cf ? mimetypes.current()->mime : NULL;
        }
    }
    
    return NULL;
}

class QPMMimeText : public QPMMime {
public:
    QPMMimeText();
    ~QPMMimeText();
    const char* convertorName();
    int countCf();
    ulong cf( int index );
    ulong flFor( ulong cf );
    ulong cfFor( const char* mime );
    const char* mimeFor( ulong cf );
    bool canConvert( const char* mime, ulong cf );
    QByteArray convertToMime( QByteArray data, const char *, ulong cf );
    QByteArray convertFromMime( QByteArray data, const char *, ulong cf );

#if !defined(QT_NO_DRAGANDDROP)
    
    DragWorker *dragWorkerFor( const char *mime, QMimeSource *src );
    DropWorker *dropWorkerFor( DRAGINFO *info );

    class NativeFileDrag : public DragWorker, public QPMObjectWindow
    {
    public:
        // DragWorker interface
        bool cleanup( bool isCancelled ) { return TRUE; } // always disallow Move
        bool isExclusive() const { return TRUE; }
        ULONG itemCount() const { return 0; } // super exclusive
        HWND hwnd() const { return QPMObjectWindow::hwnd(); }
        DRAGINFO *createDragInfo( const char *name, USHORT supportedOps );
        // QPMObjectWindow interface (dummy implementation, we don't need to interact)
        MRESULT message( ULONG msg, MPARAM mp1, MPARAM mp2 ) { return 0; }
    };
    
    class NativeFileDrop : public DropWorker
    {
    public:
        // DropWorker interface
        bool isExclusive() const { return TRUE; }
        bool provides( const char *format ) const;
        int formatCount() const;
        const char *format( int fn ) const;
        QByteArray encodedData( const char *format ) const;
    };
    
    class TextDragProvider : public DefaultDragWorker::Provider
    {
    public:
        TextDragProvider() : exclusive( FALSE ) {}
        bool exclusive;
        // Provider interface
        const char *format( const char *drf ) const;
        bool provide( const char *drf, const QByteArray &allData,
                      ULONG itemIndex, QByteArray &itemData );
        void fileType( const char *drf, const char *&type, const char *&ext );
    };

    class TextDropProvider : public DefaultDropWorker::Provider
    {
    public:
        // Provider interface
        const char *drf( const char *format ) const;
        bool provide( const char *format, ULONG itemIndex,
                      const QByteArray &itemData, QByteArray &allData );
    };
    
#endif // !QT_NO_DRAGANDDROP
    
private:
    const ulong CF_TextUnicode;
#if !defined(QT_NO_DRAGANDDROP)
    NativeFileDrag nativeFileDrag;
    NativeFileDrop nativeFileDrop;
    TextDragProvider textDragProvider;
    TextDropProvider textDropProvider;
#endif // !QT_NO_DRAGANDDROP
};

QPMMimeText::QPMMimeText() :
    // register a clipboard format for unicode text
    // ("text/unicode" is what Mozilla uses to for unicode, so Qt apps will
    // be able to interchange unicode text with Mozilla apps)
    CF_TextUnicode ( WinAddAtom( WinQuerySystemAtomTable(), "text/unicode" ) )
{
}

QPMMimeText::~QPMMimeText()
{
    WinDeleteAtom( WinQuerySystemAtomTable(), 0xFFFF0000 | CF_TextUnicode );
}

const char* QPMMimeText::convertorName()
{
    return "Text";
}

int QPMMimeText::countCf()
{
    return 2;
}

ulong QPMMimeText::cf( int index )
{
    if ( index == 0 )
        return CF_TextUnicode;
    else if ( index == 1 )
        return CF_TEXT;
    return 0;
}

ulong QPMMimeText::flFor( ulong cf )
{
    // both CF_TEXT and CF_TextUnicode use CFI_POINTER
    return cf == CF_TEXT || cf == CF_TextUnicode ? CFI_POINTER : 0;
}

ulong QPMMimeText::cfFor( const char* mime )
{
    if ( qstrnicmp( mime, MIME_TEXT_PLAIN, qstrlen( MIME_TEXT_PLAIN ) ) != 0 ||
         (mime[10] != 0 && mime[10] != ' ' && mime[10] != ';') )
        return 0;

    QCString m( mime );
    int i = m.find( "charset=" );
    if ( i >= 0 ) {
	QCString cs( m.data() + i + 8 );
	i = cs.find( ";" );
	if ( i >= 0 )
	    cs = cs.left( i );
	if ( cs == "system" )
	    return CF_TEXT;
	if ( cs == "ISO-10646-UCS-2" )
	    return CF_TextUnicode;
        // any other 'charset' spec 
        return 0;
    }
    
    // no 'charset' spec
    return CF_TEXT;
}

const char* QPMMimeText::mimeFor( ulong cf )
{
    if ( cf == CF_TEXT )
	return MIME_TEXT_PLAIN_CHARSET_SYSTEM;
    else if ( cf == CF_TextUnicode )
        return "text/plain;charset=ISO-10646-UCS-2";
    return 0;
}

bool QPMMimeText::canConvert( const char* mime, ulong cf )
{
    if ( !cf )
        return FALSE;

    return cfFor( mime ) == cf;
}

/*
    text/plain is defined as using CRLF, but so many programs don't,
    and programmers just look for '\n' in strings.
    OS/2 really needs CRLF, so we ensure it here.
*/

QByteArray QPMMimeText::convertToMime( QByteArray data, const char* , ulong cf )
{
    if ( cf == CF_TEXT ) {
        const char* d = data.data();
        const int s = qstrlen( d );
        QByteArray r( data.size() + 1 );
        char* o = r.data();
        int j = 0;
        for ( int i = 0; i < s; i++ ) {
            char c = d[i];
            if ( c != '\r' )
                o[j++] = c;
        }
        o[j] = 0;
        return r;
    } else if ( cf == CF_TextUnicode ) {
        // Mozilla uses un-marked little-endian nul-terminated Unicode
        // for "text/unicode"
        int sz = data.size();
        int len = 0;
        // Find NUL
        for ( ; len < sz - 1 && (data[len+0] || data[len+1]); len += 2 )
            ;

        QByteArray r( len + 2 );
        r[0] = uchar( 0xff ); // BOM
        r[1] = uchar( 0xfe );
        memcpy( r.data() + 2, data.data(), len );
        return r;
    }

    return QByteArray();
}

extern QTextCodec* qt_findcharset( const QCString& mimetype );

QByteArray QPMMimeText::convertFromMime( QByteArray data, const char *mime, ulong cf )
{
    if ( cf == CF_TEXT ) {
        // Anticipate required space for CRLFs at 1/40
        int maxsize = data.size() + data.size() / 40 + 3;
        QByteArray r( maxsize );
        char* o = r.data();
        const char* d = data.data();
        const int s = data.size();
        bool cr = FALSE;
        int j = 0;
        for ( int i = 0; i < s; i++ ) {
            char c = d[i];
            if ( c == '\r' )
                cr = TRUE;
            else {
                if ( c == '\n' ) {
                    if ( !cr )
                        o[j++]='\r';
                }
                cr = FALSE;
            }
            o[j++] = c;
            if ( j + 3 >= maxsize ) {
                maxsize += maxsize / 4;
                r.resize( maxsize );
                o = r.data();
            }
        }
        o[j] = 0;
        return r;
    } else if ( cf == CF_TextUnicode ) {
        QTextCodec *codec = qt_findcharset( mime );
        QString str = codec->toUnicode( data );
        const QChar *u = str.unicode();
        QString res;
        const int s = str.length();
        int maxsize = s + s / 40 + 3;
        res.setLength( maxsize );
        int ri = 0;
        bool cr = FALSE;
        for ( int i = 0; i < s; ++ i ) {
            if ( *u == '\r' )
                cr = TRUE;
            else {
                if ( *u == '\n' && !cr )
                    res[ri++] = QChar('\r');
                cr = FALSE;
            }
            res[ri++] = *u;
            if ( ri + 3 >= maxsize ) {
                maxsize += maxsize / 4;
                res.setLength( maxsize );
            }
            ++u;
        }
        res.truncate( ri );
        const int byteLength = res.length() * 2;
        QByteArray r( byteLength + 2 );
        memcpy( r.data(), res.unicode(), byteLength );
        r[byteLength] = 0;
        r[byteLength+1] = 0;
        return r;
    }

    return QByteArray();
}

#if !defined(QT_NO_DRAGANDDROP)

DRAGINFO *QPMMimeText::NativeFileDrag::createDragInfo( const char *name,
                                                       USHORT supportedOps )
{
    Q_ASSERT( source() );
    if ( !source() )
        return NULL;
    
    // obtain the list of files
    QStringList list;
    QUriDrag::decodeLocalFiles( source(), list );
    ULONG itemCnt = list.count();
    Q_ASSERT( itemCnt );
    if ( !itemCnt )
        return NULL;
    
#if defined(QT_DEBUG_DND)                        
    qDebug( "QPMMimeText::NativeFileDrag: itemCnt=%ld", itemCnt );
#endif
    
    DRAGINFO *info = DrgAllocDraginfo( itemCnt );
    Q_ASSERT( info );
    if ( !info )
        return NULL;
    
    bool ok = TRUE;
    QStringList::Iterator it = list.begin();
    for ( ULONG i = 0; i < itemCnt; ++i, ++it ) {
        DRAGITEM *item = DrgQueryDragitemPtr( info, i );
        Q_ASSERT( item );
        if ( !item ) {
            ok = FALSE;
            break;
        }

        QCString fileName = QDir::convertSeparators( *it ).local8Bit();

        int sep = fileName.findRev( '\\' );
        Q_ASSERT( sep > 0 && sep < int( fileName.length() ) - 1 );
        if ( sep <= 0 || sep >= int( fileName.length() ) - 1 ) {
            ok = FALSE;
            break;
        }

        item->hstrSourceName = DrgAddStrHandle( fileName.data() + sep + 1 );
        fileName.truncate( sep + 1 );
        item->hstrContainerName = DrgAddStrHandle( fileName );

#if defined(QT_DEBUG_DND)                        
        qDebug( "QPMMimeText::NativeFileDrag: item %ld: dir=\"%s\", name=\"%s\"",
                i, queryHSTR( item->hstrContainerName ).data(),
                   queryHSTR( item->hstrSourceName ).data() );
#endif
        
        item->hwndItem = hwnd();
        item->ulItemID = 0;
        item->hstrType = DrgAddStrHandle( DRT_UNKNOWN );
        item->hstrRMF = DrgAddStrHandle( "<"DRM_OS2FILE","DRF_UNKNOWN">" );
        item->hstrTargetName = 0;
        item->cxOffset = 0;
        item->cyOffset = 0;
        item->fsControl = 0;
        item->fsSupportedOps = supportedOps;
    }    

    if ( !ok ) {
        DrgFreeDraginfo( info );
        info = NULL;
    }
    
    return info;
}

bool QPMMimeText::NativeFileDrop::provides( const char *format ) const
{
    return qstricmp( format, MIME_TEXT_URI_LIST ) == 0;
}

int QPMMimeText::NativeFileDrop::formatCount() const
{
    return 1;
}

const char *QPMMimeText::NativeFileDrop::format( int fn ) const
{
    return fn == 0 ? MIME_TEXT_URI_LIST : NULL;
}

QByteArray QPMMimeText::NativeFileDrop::encodedData( const char *format ) const
{
    QByteArray data;

    Q_ASSERT( info() );
    if ( !info() )
        return data;
    
    ULONG itemCount = DrgQueryDragitemCount( info() );
    Q_ASSERT( itemCount );
    if ( !itemCount )
        return data;
    
    // sanity check
    if ( qstricmp( format, MIME_TEXT_URI_LIST ) != 0 )
        return data;
    
    QCString texturi;

    for ( ULONG i = 0; i < itemCount; ++i ) {
        DRAGITEM *item = DrgQueryDragitemPtr( info(), i );
        Q_ASSERT( item );
        QCString fullName;
        if ( !item || !canTargetRenderAsOS2File( item, &fullName ) )
            return data;
        QString fn = QFile::decodeName( fullName );
        texturi += QUriDrag::localFileToUri( fn );
        texturi += "\r\n";
    }
    
    data = texturi;
    return data;
}

const char *QPMMimeText::TextDragProvider::format( const char *drf ) const
{
    if ( qstrcmp( drf, DRF_TEXT ) == 0 ) {
        if ( exclusive )
            return MIME_TEXT_URI_LIST;
        else
            return MIME_TEXT_PLAIN_CHARSET_SYSTEM;
    }
    return NULL;
}

bool QPMMimeText::TextDragProvider::provide( const char *drf,
                                             const QByteArray &allData,
                                             ULONG itemIndex,
                                             QByteArray &itemData )
{
    if ( qstrcmp( drf, DRF_TEXT ) == 0 ) {
        if ( exclusive ) {
            // locate the required item
            int dataSize = allData.size();
            if ( !dataSize )
                return FALSE;
            int begin = 0, end = 0, next = 0;
            do {
                begin = next;
                end = allData.find( '\r', begin );
                if ( end >= 0 ) {
                    next = end + 1;
                    if ( next < dataSize && allData[ next ] == '\n' )
                        ++ next;
                } else {
                    end = allData.find( '\n', begin );
                    if ( end >= 0 )
                        next = end + 1;
                }
            } while ( itemIndex-- && end >= 0 && next < dataSize );
            int urlLen = end - begin;
            if ( urlLen <= 0 )
                return FALSE;
            QCString urlStr( urlLen + 1 );
            memcpy( urlStr.data(), allData.data() + begin, urlLen );
            // decode from UTF-8
            urlStr = QString::fromUtf8( urlStr ).local8Bit(); 
            // encode special/national chars to escaped %XX within urlStr
            // (see QUrl::encode(), but special chars a bit differ here: for
            // example, Mozilla doesn't like when ':' is encoded after 'http').
            const QCString special( "<>#@\"&%$,;?={}|^~[]\'`\\ \n\t\r" );
            QCString itemStr( urlLen * 3 + 1 );
            int itemLen = 0;
            for ( int i = 0; i < urlLen; ++i ) {
                uchar ch = urlStr[ i ];
                if ( ch >= 128 || special.contains( ch ) ) {
                    itemStr[ itemLen++ ] = '%';
                    uchar c = ch / 16;
                    c += c > 9 ? 'A' - 10 : '0';
                    itemStr[ itemLen++ ] = c;
                    c = ch % 16;
                    c += c > 9 ? 'A' - 10 : '0';
                    itemStr[ itemLen++ ] = c;
                } else {
                    itemStr[ itemLen++ ] = ch;
                }
            }
            itemData = itemStr;
            // resize on QByteArray to ensure the trailing \0 is stripped
            itemData.resize( itemLen );
        } else {
            itemData = allData;
        }
        return TRUE;
    }
    return FALSE;
}

void QPMMimeText::TextDragProvider::fileType( const char *drf, const char *&type,
                                              const char *&ext )
{
    if ( qstrcmp( drf, DRF_TEXT ) == 0 ) {
        if ( exclusive ) {
            type = DRT_URL;
            // no extension for URLs
        } else {
            type = DRT_TEXT;
            ext = "txt";
        }
    }
};

const char *QPMMimeText::TextDropProvider::drf( const char *format ) const
{
    // sanity check
    if ( qstricmp( format, MIME_TEXT_PLAIN_CHARSET_SYSTEM ) == 0 ||
         qstricmp( format, MIME_TEXT_URI_LIST ) == 0 )
        return DRF_TEXT;
    return NULL;
}

bool QPMMimeText::TextDropProvider::provide( const char *format,
                                             ULONG /* itemIndex */,
                                             const QByteArray &itemData,
                                             QByteArray &allData )
{
    if ( qstricmp( format, MIME_TEXT_PLAIN_CHARSET_SYSTEM ) == 0 ) {
        allData = itemData;
        return TRUE;
    }

    if ( qstricmp( format, MIME_TEXT_URI_LIST ) == 0 ) {
        // decode escaped %XX within itemData (see QUrl::decode())
        size_t dataSize = itemData.size();
        size_t strLen = 0;
        QCString itemStr( dataSize + 1 );
        for ( size_t i = 0; i < dataSize; ) {
            uchar ch = itemData[ i++ ];
            if ( ch == '%' && (i + 2 <= dataSize) ) {
                int hi = hex_to_int( itemData[ i ] );
                if ( hi >= 0 ) {
                    int lo = hex_to_int( itemData[ i + 1 ] );
                    if ( lo >= 0 ) {
                        ch = (hi << 4) + lo;
                        i += 2;
                    }
                }
            }
            itemStr[ strLen++ ] = ch; 
        }
        itemStr.truncate( strLen );
        // check that itemData is a valid URL
        QUrl url( QString::fromLocal8Bit( itemStr ) );
        if ( !url.isValid() )
            return FALSE;
        // oops, QUrl incorrectly handles the 'file:' protocol, do it for it
        QString urlStr;
        if ( url.isLocalFile() )
            urlStr = url.protocol() + ":///" + url.path();
        else
            urlStr = url.toString();
        // append the URL to the list
        QCString str ( allData );
        str += QUriDrag::unicodeUriToUri( urlStr );
        str += "\r\n";
        allData = str;
        return TRUE;
    }
    
    return FALSE;
}

QPMMime::DragWorker *QPMMimeText::dragWorkerFor( const char *mime, QMimeSource *src )
{
    if ( cfFor( mime ) == CF_TEXT ) {
        // add a cooperative provider
        textDragProvider.exclusive = FALSE;
        DefaultDragWorker *defWorker = defaultCoopDragWorker();
        defWorker->addProvider( DRF_TEXT, &textDragProvider );
        return defWorker;
    }
    
    if ( qstricmp( mime, MIME_TEXT_URI_LIST ) == 0 ) {
        // see what kind of items text/uri-list represents
        QStrList uriList;
        QStringList fileList;
        QUriDrag::decode( src, uriList );
        QUriDrag::decodeLocalFiles( src, fileList );
        if ( fileList.count() && fileList.count() == uriList.count() ) {
            // all items are local files, return an exclusive file drag worker
            return &nativeFileDrag;
        }
        if ( uriList.count() && !fileList.count() ) {
            // all items are non-files, add an exclusive provider for the
            // specified item count
            textDragProvider.exclusive = TRUE;
            DefaultDragWorker *defWorker = defaultExclDragWorker();
            bool ok = defWorker->addProvider( DRF_TEXT, &textDragProvider,
                                              uriList.count() );
            return ok ? defWorker : NULL;
        }
        // if items are mixed, we return NULL to fallback to QPMMimeAnyMime 
    }
    
    return NULL;
}

QPMMime::DropWorker *QPMMimeText::dropWorkerFor( DRAGINFO *info )
{
    ULONG itemCount = DrgQueryDragitemCount( info );
    Q_ASSERT( itemCount );
    if ( !itemCount )
        return NULL;
    
    if ( itemCount == 1 ) {
        DRAGITEM *item = DrgQueryDragitemPtr( info, 0 );
        Q_ASSERT( item );
        if ( !item )
            return NULL;
        // proceed only if the target cannot render DRM_OS2FILE on its own
        // and if the item type is not "UniformResourceLocator" (which will be
        // processed below) 
        if ( !canTargetRenderAsOS2File( item ) &&
             !DrgVerifyType( item, DRT_URL ) ) {
            DefaultDropWorker *defWorker = defaultDropWorker();
            // check that we support one of DRMs and the format is DRF_TEXT
            if ( defWorker->canRender( item, DRF_TEXT ) ) {
                // add a cooperative provider (can coexist with others)
                defWorker->addProvider( MIME_TEXT_PLAIN_CHARSET_SYSTEM,
                                        &textDropProvider );
                return defWorker;
            }
            return NULL;
        }
    }

    // Either the target can render DRM_OS2FILE on its own (so it's a valid
    // file/directory name), or it's an "UniformResourceLocator", or there is
    // more than one drag item. Check that all items are of either one type
    // or another. If so, we can represent them as 'text/uri-list'.
    bool allAreFiles = TRUE;
    bool allAreURLs = TRUE; 
    DefaultDropWorker *defWorker = defaultDropWorker();
    for ( ULONG i = 0; i < itemCount; ++i ) {
        DRAGITEM *item = DrgQueryDragitemPtr( info, i );
        Q_ASSERT( item );
        if ( !item )
            return NULL;
        if (allAreFiles)
            allAreFiles &= canTargetRenderAsOS2File( item );
        if (allAreURLs)
            allAreURLs &= DrgVerifyType( item, DRT_URL ) &&
                          defWorker->canRender( item, DRF_TEXT );
        if (!allAreFiles && !allAreURLs)
            return NULL;
    }

    if (allAreFiles) {
        // return an exclusive drop worker
        return &nativeFileDrop;
    }
    
    // add an exclusive provider (can neither coexist with other workers
    // or providers)
    bool ok = defWorker->addExclusiveProvider( MIME_TEXT_URI_LIST,
                                               &textDropProvider );
    return ok ? defWorker : NULL;
}

#endif // !QT_NO_DRAGANDDROP


class QPMMimeImage : public QPMMime {
public:
    const char* convertorName();
    int countCf();
    ulong cf( int index );
    ulong flFor( ulong cf );
    ulong cfFor( const char* mime );
    const char* mimeFor( ulong cf );
    bool canConvert( const char* mime, ulong cf );
    QByteArray convertToMime( QByteArray data, const char *mime, ulong cf );
    QByteArray convertFromMime( QByteArray data, const char *mime, ulong cf );
};

const char* QPMMimeImage::convertorName()
{
    return "Image";
}

int QPMMimeImage::countCf()
{
    return 1;
}

ulong QPMMimeImage::cf( int index )
{
    return index == 0 ? CF_BITMAP : 0;
}

ulong QPMMimeImage::flFor( ulong cf )
{
    // CF_BITMAP uses CFI_HANDLE
    return cf == CF_BITMAP ? CFI_HANDLE : 0;
}

ulong QPMMimeImage::cfFor( const char *mime )
{
    if ( qstrnicmp( mime, "image/", 6 ) == 0 ) {
	QStrList ofmts = QImage::outputFormats();
	for ( const char *fmt = ofmts.first(); fmt; fmt = ofmts.next() )
	    if ( qstricmp( fmt, mime + 6 ) == 0 )
                return CF_BITMAP;
    }
    return 0;
}

const char* QPMMimeImage::mimeFor( ulong cf )
{
    if ( cf == CF_BITMAP )
        return "image/bmp";
    return 0;
}

bool QPMMimeImage::canConvert( const char* mime, ulong cf )
{
    if ( cf == CF_BITMAP && qstrnicmp( mime, "image/", 6 ) == 0 ) {
        QStrList ofmts = QImage::outputFormats();
        for ( const char* fmt = ofmts.first(); fmt; fmt = ofmts.next() )
            if ( qstricmp( fmt, mime + 6 ) == 0 )
                return TRUE;
    }
    return FALSE;
}

QByteArray QPMMimeImage::convertToMime( QByteArray data, const char *mime, ulong cf )
{
    if ( qstrnicmp( mime, "image/", 6 ) != 0 || cf != CF_BITMAP )  // Sanity
	return QByteArray();

    HBITMAP hbm = (HBITMAP) *(ULONG *) data.data();

    QPixmap pm;
    pm.attachHandle( hbm );
    QImage img = pm.convertToImage();
    pm.detachHandle(); // prevent hbm from being deleted

    QCString ofmt = mime + 6;
    QByteArray ba;
    QBuffer iod( ba );
    iod.open( IO_WriteOnly );
    QImageIO iio( &iod, ofmt.upper() );
    iio.setImage( img );
    if ( iio.write() ) {
        iod.close();
        return ba;
    }

    // Failed
    return QByteArray();
}

QByteArray QPMMimeImage::convertFromMime( QByteArray data, const char *mime, ulong cf )
{
    if ( qstrnicmp( mime, "image/", 6 ) != 0 || cf != CF_BITMAP )  // Sanity
	return QByteArray();

    QImage img;
    img.loadFromData( (unsigned char *) data.data(), data.size() );
    if ( !img.isNull() ) {
        HBITMAP hbm = QPixmap( img ).detachHandle();
        if ( hbm ) {
            QByteArray ba ( sizeof(HBITMAP) );
            *(HBITMAP *) ba.data() = hbm;
            return ba;
        }
    }

    // Failed
    return QByteArray();
}


static
void cleanup_mimes()
{
    QPMMime* wm;
    while ( (wm = mimes.first()) ) {
	delete wm;
    }
}

/*!
  This is an internal function.
*/
void QPMMime::initialize()
{
    if ( mimes.isEmpty() ) {
        // add standard convertors so that QPMMimeAnyMime is the last in the list
        new QPMMimeAnyMime;
	new QPMMimeImage;
	new QPMMimeText;
	qAddPostRoutine( cleanup_mimes );
    }
}

/*!
  Returns the most-recently created QPMMime that can convert
  between the \a mime and \a cf formats.  Returns 0 if no such convertor
  exists.
*/
QPMMime *
QPMMime::convertor( const char *mime, ulong cf )
{
    // return nothing for illegal requests
    if ( !cf )
	return 0;

    QPMMime* wm;
    for ( wm = mimes.first(); wm; wm = mimes.next() ) {
	if ( wm->canConvert( mime, cf ) ) {
	    return wm;
	}
    }
    return 0;
}


/*!
  Returns a MIME type for \a cf, or 0 if none exists.
*/
const char* QPMMime::cfToMime( ulong cf )
{
    const char* m = 0;
    QPMMime* wm;
    for ( wm = mimes.first(); wm && !m; wm = mimes.next() ) {
	m = wm->mimeFor( cf );
    }
    return m;
}

/*!
  Returns a list of all currently defined QPMMime objects.
*/
QPtrList<QPMMime> QPMMime::all()
{
    return mimes;
}

#if !defined(QT_NO_DRAGANDDROP)

/*!
  Returns a string represented by \a hstr.
*/
QCString QPMMime::queryHSTR( HSTR hstr )
{
    QCString str;
    ULONG len = DrgQueryStrNameLen( hstr );
    if ( len ) {
        str.resize( len + 1 );
        DrgQueryStrName( hstr, str.size(), str.data() );
    }
    return str;
}

/*!
  Returns a string that is a concatenation of \c hstrContainerName and
  \c hstrSourceName fileds of the given \a item structure.
*/
QCString QPMMime::querySourceNameFull( DRAGITEM *item )
{
    QCString fullName;
    if ( !item )
        return fullName;

    ULONG pathLen = DrgQueryStrNameLen( item->hstrContainerName );
    ULONG nameLen = DrgQueryStrNameLen( item->hstrSourceName );
    if ( !pathLen || !nameLen )
        return fullName;
    
    fullName.resize( pathLen + nameLen + 1 );
    DrgQueryStrName( item->hstrContainerName, pathLen + 1, fullName.data() );
    DrgQueryStrName( item->hstrSourceName, nameLen + 1, fullName.data() + pathLen );
    
    return fullName;
}

/*! \internal
  Checks that the given drag \a item supports the DRM_OS2FILE rendering
  mechanism and can be rendered by a target w/o involving the source (i.e.,
  DRM_OS2FILE is the first supported format and a valid file name with full
  path is provided). If the function returns TRUE, \a fullName (if not NULL)
  will be assigned the item's full source file name (composed from
  \c hstrContainerName and \c hstrSourceName fields).
 */
bool QPMMime::canTargetRenderAsOS2File( DRAGITEM *item, QCString *fullName /* = NULL */ )
{
    if ( !item )
        return FALSE;
    
    if ( item->fsControl & (DC_PREPARE | DC_PREPAREITEM) )
        return FALSE;

    {
        // DrgVerifyNativeRMF doesn't work on my system (ECS 1.2.1 GA):
        // it always returns FALSE regardless of arguments. Use simplified
        // hstrRMF parsing to determine whether DRM_OS2FILE is the native
        // mechanism or not (i.e. "^\s*[\(<]\s*DRM_OS2FILE\s*,.*").
            
        QCString rmf = queryHSTR( item->hstrRMF );
        bool ok = FALSE;
        int i = rmf.find( DRM_OS2FILE );
        if ( i >= 1 ) {
            for( int j = i - 1; j >= 0; --j ) {
                char ch = rmf.data()[j];
                if ( ch == ' ' ) continue;
                if ( ch == '<' || ch == '(' ) {
                    if ( ok ) return FALSE;
                    ok = TRUE;
                } else {
                    return FALSE;
                }
            }
        }
        if ( ok ) {
            ok = FALSE;
            int drmLen = strlen( DRM_OS2FILE );
            for( int j = i + drmLen; j < (int) rmf.length(); ++j ) {
                char ch = rmf.data()[j];
                if ( ch == ' ' ) continue;
                if ( ch == ',' ) {
                    ok = TRUE;
                    break;
                }
                return FALSE;
            }
        }
        if ( !ok )
            return FALSE;
    }

    QCString srcFullName = querySourceNameFull( item );
    if ( srcFullName.isEmpty() )
        return FALSE;
    
    QCString srcFullName2( srcFullName.length() + 1 );
    APIRET rc = DosQueryPathInfo( srcFullName, FIL_QUERYFULLNAME,
                                  srcFullName2.data(), srcFullName2.size() );
    if ( rc != 0 )
        return FALSE;
    
    QString s1 = QFile::decodeName( srcFullName.data() );
    QString s2 = QFile::decodeName( srcFullName2.data() );
    
    if ( s1.lower() != s2.lower() )
        return FALSE;

    if ( fullName )
        *fullName = srcFullName;
    return TRUE;
}

// static
/*! \internal
  Parses the given \a rmfs list (full rendering mechanism/format specification)
  and builds a \a list of mechanism branches. Each mechanism branch is also a
  list, where the first item is the mechahism name and all subsequent items are
  formats supported by this mechanism. Returns FALSE if fails to parse \a rmf.
  \note The method clears the given \a list variable before proceeding and sets
  auto-deletion to TRUE.
*/
bool QPMMime::parseRMFs( HSTR rmfs, QPtrList<QStrList> &list )
{
    // The format of the RMF list is "elem {,elem,elem...}"
    // where elem is "(mechanism{,mechanism...}) x (format{,format...})"
    // or "<mechanism,format>".
    // We use a simple FSM to parse it. In terms of FSM, the format is:
    //
    // STRT ( BCM m CMCH echanism CMCH , NCM m CMCH echanism CMCH ) ECM x
    //     SCMF ( BCF f CFCH ormat CFCH , NCF f CFCH ormat CFCH ) ECF , STRT
    // STRT < BP m PMCH echanism PMCH , SPMF f PFCH ormat PFCH > EP , STRT

    QCString str = queryHSTR( rmfs );
    uint len = str.length();

    enum {
        // states
        STRT = 0, BCM, CMCH, NCM, ECM, SCMF, BCF, CFCH, NCF, ECF,
        BP, PMCH, SPMF, PFCH, EP,
        STATES_COUNT,
        // pseudo states
        Err, Skip,
        // inputs
        obr = 0, cbr, xx, lt, gt, cm, any, ws,
        INPUTS_COUNT,
    };

    static const char Chars[] =  { '(', ')', 'x', 'X', '<', '>', ',', ' ', 0 };
    static const char Inputs[] = { obr, cbr, xx,  xx,  lt,  gt,  cm,  ws };
    static const uchar Fsm [STATES_COUNT] [INPUTS_COUNT] = {
             /* 0 obr  1 cbr  2 xx   3 lt   4 gt   5 cm   6 any  7 ws */
/* STRT 0  */ { BCM,   Err,   Err,   BP,    Err,   Err,   Err,   Skip },
/* BCM  1  */ { Err,   Err,   Err,   Err,   Err,   Err,   CMCH,  Skip },
/* CMCH 2  */ { Err,   ECM,   CMCH,  Err,   Err,   NCM,   CMCH,  CMCH },
/* NCM  3  */ { Err,   Err,   Err,   Err,   Err,   Err,   CMCH,  Skip },
/* ECM  4  */ { Err,   Err,   SCMF,  Err,   Err,   Err,   Err,   Skip },
/* SCMF 5  */ { BCF,   Err,   Err,   Err,   Err,   Err,   Err,   Skip },  
/* BCF  6  */ { Err,   Err,   Err,   Err,   Err,   Err,   CFCH,  Skip },
/* CFCH 7  */ { Err,   ECF,   CFCH,  Err,   Err,   NCF,   CFCH,  CFCH },
/* NCF  8  */ { Err,   Err,   Err,   Err,   Err,   Err,   CFCH,  Skip },
/* ECF  9  */ { Err,   Err,   Err,   Err,   Err,   STRT,  Err,   Skip },
/* BP   10 */ { Err,   Err,   Err,   Err,   Err,   Err,   PMCH,  Skip },
/* PMCH 11 */ { Err,   Err,   PMCH,  Err,   Err,   SPMF,  PMCH,  PMCH  },
/* SPMF 12 */ { Err,   Err,   Err,   Err,   Err,   Err,   PFCH,  Skip },
/* PFCH 13 */ { Err,   Err,   PFCH,  Err,   EP,    Err,   PFCH,  PFCH },
/* EP   14 */ { Err,   Err,   Err,   Err,   Err,   STRT,  Err,   Skip }
    };
    
    list.clear();
    list.setAutoDelete( TRUE );
    
    QPtrList<QStrList> mech;
    QCString buf;
    QStrList *m = NULL;
    
    uint state = STRT;
    uint start = 0, end = 0, space = 0;
    
    for ( uint i = 0; i < len && state != Err ; ++i ) {
        char ch = str[i];
        char *p = strchr( Chars, ch );
        uint input = p ? Inputs[ p - Chars ] : any;
        uint new_state = Fsm[ state ][ input ];
        switch ( new_state ) {
            case Skip:
                continue;
            case CMCH:
            case CFCH:
            case PMCH:
            case PFCH:
                if ( state != new_state )
                    start = end = i;
                ++end;
                // accumulate trailing space for truncation
                if ( input == ws ) ++space;
                else space = 0;
                break;
            case NCM:
            case ECM:
            case SPMF:
                buf = QCString( str.data() + start, end - start - space + 1 );
                // find the mechanism branch in the output list
                for ( m = list.first(); m; m = list.next() ) {
                    if ( qstrcmp( m->getFirst(), buf ) == 0 )
                        break;
                }
                if ( !m ) {
                    // append to the output list if not found
                    m = new QStrList();
                    m->append( buf );
                    list.append( m );
                }
                // store in the current list for making a cross product
                mech.append( m );
                start = end = 0;
                break;
            case NCF:
            case ECF:
            case EP:
                buf = QCString( str.data() + start, end - start - space + 1 );
                // make a cross product with all current mechanisms
                for ( m = mech.first(); m; m = mech.next() )
                    m->append( buf );
                if ( new_state != NCF )
                    mech.clear();
                start = end = 0;
                break;
            default:
                break;
        }
        state = new_state;
    }
    
    return state == ECF || state == EP;
}

// static
/*! \internal
  Splits the given \a rmf (rendering mechanism/format pair) to a \a mechanism
  and a \a format string. Returns FALSE if fails to parse \a rmf.  
 */
bool QPMMime::parseRMF( HSTR rmf, QCString &mechanism, QCString &format )
{
    QPtrList<QStrList> list;
    if ( !parseRMFs( rmf, list ) )
        return FALSE;
    
    if ( list.count() != 1 || list.getFirst()->count() != 2 )
        return FALSE;
    
    QStrList *m = list.getFirst();
    mechanism = m->first();
    format = m->next();
    
    return TRUE;
}

// static
/*! \internal */
QPMMime::DefaultDragWorker *QPMMime::defaultCoopDragWorker()
{
    static DefaultDragWorker defCoopDragWorker( FALSE /* exclusive */ );
    return &defCoopDragWorker;
}

// static
/*! \internal */
QPMMime::DefaultDragWorker *QPMMime::defaultExclDragWorker()
{
    static DefaultDragWorker defExclDragWorker( TRUE /* exclusive */ );
    return &defExclDragWorker;
}

// static
/*! \internal */
QPMMime::DefaultDropWorker *QPMMime::defaultDropWorker()
{
    static DefaultDropWorker defaultDropWorker;
    return &defaultDropWorker;
}

#endif // !QT_NO_DRAGANDDROP

/*!
  \fn const char* QPMMime::convertorName()

  Returns a name for the convertor.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn int QPMMime::countCf()

  Returns the number of OS/2 PM Clipboard formats supported by this
  convertor.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn ulong QPMMime::cf(int index)

  Returns the OS/2 PM Clipboard format supported by this convertor
  that is ordinarily at position \a index. This means that cf(0)
  returns the first OS/2 PM Clipboard format supported, and
  cf(countCf()-1) returns the last. If \a index is out of range the
  return value is undefined.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn ulong QPMMime::flFor(ulong cf)

  Returns the data storage flag for the OS/2 PM Clipboard type \a cf
  (either \c CFI_POINTER or \c CFI_HANDLE) used by this convertor,
  or 0 if invalid \a cf is specified.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn bool QPMMime::canConvert( const char* mime, ulong cf )

  Returns TRUE if the convertor can convert (both ways) between
  \a mime and \a cf; otherwise returns FALSE.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn const char* QPMMime::mimeFor(ulong cf)

  Returns the MIME type used for OS/2 PM Clipboard format \a cf, or
  0 if this convertor does not support \a cf.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn ulong QPMMime::cfFor(const char* mime)

  Returns the OS/2 PM Clipboard type used for MIME type \a mime, or
  0 if this convertor does not support \a mime.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn QByteArray QPMMime::convertToMime( QByteArray data, const char* mime, ulong cf )

  Returns \a data converted from OS/2 PM Clipboard format \a cf
  to MIME type \a mime.

  Note that OS/2 PM Clipboard formats must all be self-terminating.  The
  input \a data may contain trailing data. If flFor(ulong) for the given \a cf
  has the \c CFI_HANDLE flag set, then first 4 bytes of \a data represent a
  valid OS/2 handle of the appropriate type, otherwise \a data contains data
  itself.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn QByteArray QPMMime::convertFromMime( QByteArray data, const char* mime, ulong cf )

  Returns \a data converted from MIME type \a mime
  to OS/2 PM Clipboard format \a cf.

  Note that OS/2 PM Clipboard formats must all be self-terminating.  The
  return value may contain trailing data. If flFor(ulong) for the given \a cf
  has the \c CFI_HANDLE flag set, then first 4 bytes of the returned array
  must represent a valid OS/2 handle of the appropriate type, otherwise the array
  contains data itself.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \class QPMMime::DragWorker
  
  \internal
  This class is responsible for the sources's part of the Direct
  Manipulation conversation after the drop event occurs on a target.
  
  Drag workers can be super exclusive (solely responsible for converting the
  given mime type to a set of DRAGITEM structures), exclusive (cannot coexist
  with other workers but don't manage the DRAGINFO/DRAGITEM creation), or 
  or cooperative (can coexist with other drag and share the same set of DRAGITEM
  structures in order to represent different mime data types). As opposed to
  super exclusive workers (identified by isExclusive() returning TRUE and by
  itemCount() returning zero), exclusive and cooperative workers do not
  create DRAGINFO/DRAGITEM structures on their own, they implement a subset
  of methods that is used by the drag manager to fill drag structures it creates.

  If a super exlusive or an exclusive worker is encoundered when starting the
  object drag, it will be used only if there are no any other workers found for
  \b other mime types of the object being dragged. If a cooperative worker
  with item count greater than one is encountered, it will be used only if all
  other found workers are also cooperative and require the same number of items.
  In both cases, if the above conditions are broken, the respective workers
  are discarded (ignored). Instead, a special fall-back cooperative worker
  (that requires a single DRAGITEM, supports any mime type and can coexist with
  other one-item cooperative workers) will be used for the given mime type.

  \note Subclasses must NOT free the DRAGINFO structure they allocated and
  returned by createDragInfo().
  \note Every exclusive drag worker must implement createDragInfo() and must not
  implement composeFormatSting()/canRender()/prepare()/defaultFileType(). And
  vice versa, every cooperative drag worker must implement the latter three
  functions but not the former two.
  \note The return value of cleanup() is whether the Move operation is
  disallowed by this worker or not (if the worker doesn't participate in the
  DND session, it should return FALSE, to let other workers allow Move).
  \note If you need more details, please contact the developers of Qt for OS/2.
*/

/*!
  \class QPMMime::DefaultDragWorker
  
  \internal
  This class is a Drag Worker that supports standard DRM_SHAREDMEM and
  DRM_OS2FILE and rendering mechanisms. It uses
  QPMMime::DefaultDragWorker::Provider subclasses to map mime types of the
  object being dragged to rendering formats and apply preprocessing of data
  before rendering.
  
  \note If you need more details, please contact the developers of Qt for OS/2.
*/

/*!
  \class QPMMime::DropWorker
  
  \internal
  This class is responsible for the target's part of the Direct
  Manipulation conversation after the drop event occurs on a target.
  
  Drop workers can be exclusive (solely responsible for converting the given
  set of DRAGITEM structures) or cooperative (can coexist with other drop
  workers in order to produce different mime data types from the same set of
  DRAGITEM structures). If an exclusive drop worker is encountered when
  processing the drop event, all other workers are silently ignored.
  
  \note Subclasses must NOT free the DRAGINFO structure pointed to by info().
  \note Subclasses must NOT send DM_ENDCONVERSATION to the source.
  \note If you need more details, please contact the developers of Qt for OS/2.
*/

/*!
  \class QPMMime::DefaultDropWorker
  
  \internal
  This class is a Drop Worker that supports standard DRM_SHAREDMEM and
  DRM_OS2FILE and rendering mechanisms. It uses
  QPMMime::DefaultDropWorker::Provider subclasses to map various rendering
  formats to particular mime types and apply postprocessing of data after
  rendering.
  
  \note If you need more details, please contact the developers of Qt for OS/2.
*/

#endif // QT_NO_MIME
