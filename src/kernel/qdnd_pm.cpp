/****************************************************************************
** $Id$
**
** Implementation of the PM direct manipulation protocol (drag and drop) for Qt.
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

#include "qapplication.h"

#ifndef QT_NO_DRAGANDDROP

#include "qapplication_p.h"
#include "qeventloop.h"
#include "qpainter.h"
#include "qwidget.h"
#include "qdragobject.h"
#include "qimage.h"
#include "qbuffer.h"
#include "qdatastream.h"
#include "qbitmap.h"
#include "qptrlist.h"
#include "qt_os2.h"

#if defined(QT_ACCESSIBILITY_SUPPORT)
#include "qaccessible.h"
#endif

// if you enable this, it makes sense to enable it in qmime_pm.cpp as well
//#define QT_DEBUG_DND

extern void qt_pmMouseButtonUp(); // defined in qapplication_pm.cpp

bool QDragManager::eventFilter( QObject *, QEvent *)
{
    // not used in the PM implementation
    return FALSE;
}

void QDragManager::timerEvent( QTimerEvent* )
{
    // not used in the PM implementation
}

void QDragManager::cancel( bool deleteSource /* = TRUE */ )
{
    // Note: currently, this method is called only by
    // QDragObject::~QDragObject() with deleteSource = FALSE 
    
    Q_ASSERT( object && !beingCancelled );
    if ( !object || beingCancelled )
        return;
    
    beingCancelled = TRUE;
    
    dragSource = 0;
    
    if ( !deleteSource ) {
        object = 0;
    } else {
        QDragObject *obj = object;
        object = 0;
        delete obj;
    }

#ifndef QT_NO_CURSOR
    if ( restoreCursor ) {
	QApplication::restoreOverrideCursor();
	restoreCursor = FALSE;
    }
#endif
#if defined(QT_ACCESSIBILITY_SUPPORT)
    QAccessible::updateAccessibility( this, 0, QAccessible::DragDropEnd );
#endif
}

void QDragManager::move( const QPoint & )
{
    // not used in the PM implementation
}


void QDragManager::drop()
{
    // not used in the PM implementation
}

/** \internal
 *  Data for QDragEnterEvent/QDragMoveEvent/QPMDropEvent.
 */
class QPMDragData
{
public:    
    QPMDragData()
        : initialized( FALSE ), dropped( FALSE )
        , gotWorkers( FALSE ), di( NULL )
    {}

    ~QPMDragData() { reset(); }
    
    void initialize( DRAGINFO *di );
    void reset( bool isAccepted, bool isActAccepted );
    void reset() { reset( FALSE, FALSE ); }
        
    void setDropped( bool d ) { dropped = d; }
    bool isDropped() const { return dropped; }
    
    DRAGINFO *info() const { return di; }
    
    bool provides( const char *format );
    const char *format( int fn );
    QByteArray encodedData( const char *format );
    
private:        

    void initWorkers();

    bool initialized : 1;
    bool dropped : 1;
    bool gotWorkers : 1;

    DRAGINFO *di;
    QPtrList<QPMMime::DropWorker> workers;
};

void QPMDragData::initialize( DRAGINFO *info )
{
    Q_ASSERT( info );
    if ( initialized || !info )
        return;

    initialized = TRUE;
    di = info;
}

void QPMDragData::initWorkers()
{
    Q_ASSERT( initialized );
    if ( !initialized || gotWorkers )
        return;

    gotWorkers = TRUE;

    // go through all convertors and collect DropWorkers to use    
    QPtrList<QPMMime> mimes = QPMMime::all();
    for ( QPMMime *mime = mimes.first(); mime; mime = mimes.next() ) {
        QPMMime::DropWorker *wrk = mime->dropWorkerFor( di );
        if ( wrk ) {
            if ( wrk->isExclusive() ) {
                // ignore all other workers if some of them identified itself
                // as exclusive
                workers.clear();
                workers.append( wrk );
                break;
            }
            // ensure there are no duplicates
            if ( !workers.containsRef( wrk ) )
                workers.append( wrk );
        }
    }

#if defined(QT_DEBUG_DND)                        
    qDebug( "QPMDragData: %d drop workers for DRAGINFO %p",
            workers.count(), di );
#endif

    // init all workers
    for ( QPMMime::DropWorker *w = workers.first(); w; w = workers.next() ) {
        w->nfo = di;
        w->init();
    }
}

void QPMDragData::reset( bool isAccepted, bool isActAccepted )
{
    if ( !initialized )
        return;
    
    // cleanup all workers
    for ( QPMMime::DropWorker *w = workers.first(); w; w = workers.next() ) {
        w->cleanup( isAccepted, isActAccepted );
        w->nfo = NULL;
    }

    workers.clear();
    di = NULL; 
    initialized = dropped = gotWorkers = FALSE;
}

bool QPMDragData::provides( const char *format )
{
    if ( !gotWorkers )
        initWorkers();

    bool result = FALSE;

    for ( QPMMime::DropWorker *w = workers.first();
          w && !result; w = workers.next() )
        result = w->provides( format );

    return result;
}

const char *QPMDragData::format( int fn )
{
    if ( !gotWorkers )
        initWorkers();

    for ( QPMMime::DropWorker *w = workers.first();
          w; w = workers.next() ) {
        int count = w->formatCount();
        if ( fn < count )
            return w->format( fn );
        fn = fn - count;   
    }
    
    return NULL;    
}

QByteArray QPMDragData::encodedData( const char *format )
{
    if ( !gotWorkers )
        initWorkers();

    QByteArray result;

    for ( QPMMime::DropWorker *w = workers.first();
          w && result.isNull(); w = workers.next() )
        result = w->encodedData( format );

    return result;
}

class QPMDropEvent : public QDropEvent
{
public:
    QPMDropEvent( const QPoint &pos, QPMDragData *data )
        : QDropEvent( pos ) { d = data; }
       
    inline bool isTrulyAccepted() const { return accpt; }
    inline bool isActionTrulyAccepted() const { return accptact; }
};

class QPMDragEnterEvent : public QDragEnterEvent
{
public:
    QPMDragEnterEvent( const QPoint &pos, QPMDragData *data )
        : QDragEnterEvent( pos ) { d = data; }

    inline bool isTrulyAccepted() const { return accpt; }
    inline bool isActionTrulyAccepted() const { return accptact; }
};

class QPMDragMoveEvent : public QDragMoveEvent
{
public:
    QPMDragMoveEvent( const QPoint &pos, QPMDragData *data )
        : QDragMoveEvent( pos ) { d = data; }

    inline bool isTrulyAccepted() const { return accpt; }
    inline bool isActionTrulyAccepted() const { return accptact; }
};

bool QDropEvent::provides( const char* mimeType ) const
{
    QPMDragData *data = static_cast<QPMDragData *> (d);
    Q_ASSERT( data );
    if ( !data )
        return FALSE;
    
    return data->provides( mimeType );
}

const char *QDropEvent::format( int fn ) const
{
    QPMDragData *data = static_cast<QPMDragData *> (d);
    Q_ASSERT( data );
    if ( !data )
        return NULL;
    
    return data->format( fn );
}

QByteArray QDropEvent::encodedData( const char *format ) const
{
    QByteArray array;

    QPMDragData *data = static_cast<QPMDragData *> (d);
    Q_ASSERT( data );
    
    if ( !data || !data->isDropped() ) {
        // This is a QDragEnterEvent/QDragMoveEvent subclass; we cannot provide
        // any MIME contents yet because it's impossible to do data transfers
        // before DM_DROP is sent. Return shortly.
        return array;
    }

    array = data->encodedData( format );
    return array;
}

/*!
 *  \internal
 *  Direct manipulation (Drag & Drop) event handler 
 */
MRESULT qt_dispatchDragAndDrop( QWidget *widget, const QMSG &qmsg )
{
    static HWND lastDragOverHwnd = 0; // last target window
    static USHORT lastDragOverOp = 0; // last DM_DRAGOVER operation
    static USHORT lastRealOp = 0; // last real op (never DO_DEFAULT or DO_UNKNOWN)
    static USHORT defaultOp = 0; // default op for DO_DEFAULT or DO_UNKNOWN

    static USHORT supportedOps = 0; // operations supported by all items
    static bool sourceAllowsOp = FALSE; // does source allow requested operation

    static bool lastAccept = FALSE; // last reply from the target
    static bool lastAcceptAction = FALSE; // last action reply from the target
    
    static QPMDragData dragData;
    
    Q_ASSERT( widget );
    
    BOOL ok = FALSE;

    switch( qmsg.msg ) {
        case DM_DRAGOVER: {
            bool first = lastDragOverHwnd != qmsg.hwnd;
            if ( first ) {
                // the first DM_DRAGOVER message
                lastDragOverHwnd = qmsg.hwnd;
                lastAccept = lastAcceptAction = FALSE;
                supportedOps = DO_COPYABLE | DO_MOVEABLE | DO_LINKABLE;
                // ensure drag data is reset (just in case of a wrong msg flow...)
                dragData.reset();
            }

            Q_ASSERT( first || widget->acceptDrops() );
            if ( !widget->acceptDrops() ) {
                if ( !first ) {
                    // Odin32 apps are dramatically bogus, they continue to send
                    // DM_DRAGOVER even if we reply DOR_NEVERDROP. Simulate
                    // DM_DRAGLEAVE
                    lastDragOverHwnd = 0;
                    dragData.reset();
                }
                return MRFROM2SHORT( DOR_NEVERDROP, 0 );
            }
            
            DRAGINFO *info = (DRAGINFO *) qmsg.mp1;
            ok = DrgAccessDraginfo( info );
            Q_ASSERT( ok && info );
            if ( !ok || !info )
                return MRFROM2SHORT( DOR_NEVERDROP, 0 );

            USHORT dropReply = DOR_DROP;

            if ( first  ) {
                // determine the set of operations supported by *all* items
                // (this implies that DRAGITEM::fsSupportedOps is a bit field) 
                ULONG itemCount = DrgQueryDragitemCount( info );
                for ( ULONG i = 0; i < itemCount; ++ i ) {
                    PDRAGITEM item = DrgQueryDragitemPtr( info, i );
                    Q_ASSERT( item );
                    if ( !item ) {
                        dropReply = DOR_NEVERDROP;
                        break;
                    }
                    supportedOps &= item->fsSupportedOps;
                }
                if ( dropReply != DOR_NEVERDROP ) {
                    Q_ASSERT( itemCount );
                    if ( !itemCount || !supportedOps ) {
                        // items don't have even a single common operation...
                        dropReply = DOR_NEVERDROP;
                    } else {
                        // determine the default operation
                        // (in order MOVE, COPY, LINK, for compatibility with Win32)
                        if ( supportedOps & DO_MOVEABLE ) defaultOp = DO_MOVE;
                        else if ( supportedOps & DO_COPYABLE ) defaultOp = DO_COPY;
                        else if ( supportedOps & DO_LINKABLE ) defaultOp = DO_LINK;
                        else Q_ASSERT( FALSE ); // should never happen
                    }
                }
            }
            
            if ( dropReply != DOR_NEVERDROP ) {
                
                if ( first || lastDragOverOp != info->usOperation ) {
                    // the current drop operation was changed by a modifier key
                    lastDragOverOp = info->usOperation;
                    lastRealOp = info->usOperation;
                    if ( lastRealOp == DO_DEFAULT || lastRealOp == DO_UNKNOWN ) {
                        // the default operation is requested
                        lastRealOp = defaultOp;
                    }
                    sourceAllowsOp =
                        ((supportedOps & DO_MOVEABLE) && lastRealOp == DO_MOVE) ||
                        ((supportedOps & DO_COPYABLE) && lastRealOp == DO_COPY) ||
                        ((supportedOps & DO_LINKABLE) && lastRealOp == DO_LINK);
                }

                // Note that if sourceAllowsOp = false here, we have to deliver
                // events anyway (stealing them from Qt would be confusing), but
                // we will silently ignore any accept commands and always reject
                // the drop. Other platforms seem to do the same.
                
                // convert the operation to an Action
                QDropEvent::Action action = QDropEvent::Copy; 
                if ( lastRealOp == DO_COPY ) action = QDropEvent::Copy;
                else if ( lastRealOp == DO_MOVE ) action = QDropEvent::Move;
                else if ( lastRealOp == DO_LINK ) action = QDropEvent::Link;
                else Q_ASSERT( FALSE ); // should never happen
            
                // flip y coordinate
                QPoint pnt( info->xDrop, info->yDrop );
                pnt.setY( QApplication::desktop()->height() - (pnt.y() + 1) );
                pnt = widget->mapFromGlobal( pnt );

                // initialize drag data used in QPMDrag.../QPMDrop... events
                if ( first )
                    dragData.initialize( info ); 
                
                QDropEvent *de = NULL;
                QPMDragEnterEvent *dee = NULL;
                QPMDragMoveEvent *dme = NULL;
                
                if ( first )
                    de = dee = new QPMDragEnterEvent( pnt, &dragData );
                else
                    de = dme = new QPMDragMoveEvent( pnt, &dragData );
                
                de->setAction( action );
                de->accept( lastAccept );
                de->acceptAction( lastAcceptAction );
                
                QApplication::sendEvent( widget, de );

                // if not allowed or not accepted, always reply DOR_NODROP
                // to have DM_DRAGOVER delivered to us again in any case
                
                dropReply = sourceAllowsOp && de->isAccepted() ?
                            DOR_DROP : DOR_NODROP;
                
                lastAccept = dee ? dee->isTrulyAccepted() :
                                   dme->isTrulyAccepted();
                lastAcceptAction = dee ? dee->isActionTrulyAccepted() :
                                         dme->isActionTrulyAccepted();

                delete de;
            }
            
            DrgFreeDraginfo( info );
            
            return MRFROM2SHORT( dropReply, lastRealOp );
        }
        case DM_DRAGLEAVE: {
            // Odin32 apps produce incorrect message flow, ignore
            Q_ASSERT( lastDragOverHwnd != 0 );
            if ( lastDragOverHwnd == 0 )
                return 0;

            lastDragOverHwnd = 0;
            dragData.reset();
            
            if ( !widget->acceptDrops() )
                return 0;

            QDragLeaveEvent de;
            QApplication::sendEvent( widget, &de );
            return 0;
        }
        case DM_DROP: {
            // Odin32 apps produce incorrect message flow, ignore
            Q_ASSERT( lastDragOverHwnd != 0 );
            if ( lastDragOverHwnd == 0 )
                return 0;

            // Odin32 apps send DM_DROP even if we replied DOR_NEVERDROP or
            // DOR_NODROP, simulate DM_DRAGLEAVE
            Q_ASSERT( lastAccept || lastAcceptAction );
            if ( !lastAccept && !lastAcceptAction ) {
                WinSendMsg( qmsg.hwnd, DM_DRAGLEAVE, 0, 0 );
                return 0;
            }

            lastDragOverHwnd = 0;
            
            Q_ASSERT( widget->acceptDrops() );
            if ( !widget->acceptDrops() )
                return 0;
            
            DRAGINFO *info = (DRAGINFO *) qmsg.mp1;
            ok = DrgAccessDraginfo( info );
            Q_ASSERT( ok && info );
            if ( !ok || !info )
                return MRFROM2SHORT( DOR_NEVERDROP, 0 );
            
            Q_ASSERT( lastRealOp == info->usOperation );
            
            // convert the operation to an Action
            QDropEvent::Action action = QDropEvent::Copy; 
            if ( lastRealOp == DO_COPY ) action = QDropEvent::Copy;
            else if ( lastRealOp == DO_MOVE ) action = QDropEvent::Move;
            else if ( lastRealOp == DO_LINK ) action = QDropEvent::Link;
            else Q_ASSERT( FALSE ); // should never happen
            
            // flip y coordinate
            QPoint pnt( info->xDrop, info->yDrop );
            pnt.setY( QApplication::desktop()->height() - (pnt.y() + 1) );

            dragData.setDropped( TRUE );
            
            QPMDropEvent de( widget->mapFromGlobal( pnt ), &dragData );
            de.setAction( action );
            de.accept( lastAccept );
            de.acceptAction( lastAcceptAction );
            
            QApplication::sendEvent( widget, &de );

            dragData.reset( de.isTrulyAccepted(), de.isActionTrulyAccepted() );
            
            // If the target has accepted the particular Drop action (using
            // acceptAction() rather than just accept()), it means that it will
            // perform the necessary operation on its own (for example, will
            // delete the source if the accepted action is Move). In this case,
            // we always send DMFL_TARGETFAIL to the source to prevent it from
            // doing the same on its side.
            
            ULONG targetReply =
                de.isTrulyAccepted() && !de.isActionTrulyAccepted() ?
                DMFL_TARGETSUCCESSFUL : DMFL_TARGETFAIL;
         
            // send DM_ENDCONVERSATION for every item
            ULONG itemCount = DrgQueryDragitemCount( info );
            for ( ULONG i = 0; i < itemCount; ++ i ) {
                PDRAGITEM item = DrgQueryDragitemPtr( info, i );
                Q_ASSERT( item );
                if ( !item )
                    continue;
                // it is possible that this item required DM_RENDERPREPARE but
                // returned FALSE in reply to it (so hwndItem may be NULL)
                if ( !item->hwndItem )
                    continue;
                DrgSendTransferMsg( item->hwndItem, DM_ENDCONVERSATION,
                                    MPFROMLONG( item->ulItemID ),
                                    MPFROMLONG( targetReply ) );
            }
            
            DrgDeleteDraginfoStrHandles( info );
            DrgFreeDraginfo( info );
            
            return 0;
        }
        default:
            break;
    }
    
    return WinDefWindowProc( qmsg.hwnd, qmsg.msg, qmsg.mp1, qmsg.mp2 );
}

class QPMCoopDragWorker : public QPMMime::DragWorker, public QPMObjectWindow
{
public:
    QPMCoopDragWorker() : info( NULL ) {}
    bool collectWorkers( QDragObject *o );

    // DragpWorker interface
    void init();
    bool cleanup( bool isCancelled );
    bool isExclusive() const { return TRUE; }
    ULONG itemCount() const { return 0; }
    HWND hwnd() const;
    DRAGINFO *createDragInfo( const char *name, USHORT supportedOps );

    // QPMObjectWindow interface
    MRESULT message( ULONG msg, MPARAM mp1, MPARAM mp2 );
    
private:
    QPtrList<DragWorker> workers;
    DRAGINFO *info;
};

bool QPMCoopDragWorker::collectWorkers( QDragObject *o )
{
    Q_ASSERT( o );
    if ( !o )
        return FALSE;

    bool gotExcl = FALSE; // got isExclusive() worker?
    bool skipExcl = FALSE; // skip isExclusive() or itemCount() > 1 workers?
    ULONG coopLevel = 0; // itemCount() level for !isExclusive() workers 

    bool gotExclForMime = FALSE;

    // go through all formats and all convertors to collect DragWorkers
    const char *fmt = NULL;    
    for ( int i = 0; (fmt = o->format( i )); ++i ) {
#if defined(QT_DEBUG_DND)            
        qDebug( "QPMCoopDragWorker: Searching for worker for mime [%s]...", fmt );
#endif        
        QPtrList<QPMMime> mimes = QPMMime::all();
        for ( QPMMime *mime = mimes.first(); mime; mime = mimes.next() ) {
            QPMMime::DragWorker *wrk = mime->dragWorkerFor( fmt, o );
            if ( !wrk )
                continue;
#if defined(QT_DEBUG_DND)            
            qDebug( "QPMCoopDragWorker: Got worker %p (isExclusive()=%d, "
                    "itemCount()=%ld) from convertor '%s' (gotExclForMime=%d, "
                    "gotExcl=%d, skipExcl=%d, coopLevel=%ld)",
                    wrk, wrk->isExclusive(), wrk->itemCount(),
                    mime->convertorName(), gotExclForMime, gotExcl, skipExcl,
                    coopLevel );
#endif            
            if ( wrk->isExclusive() ) {
                if ( !skipExcl && !gotExclForMime ) {
                    gotExclForMime = TRUE;
                    if ( !gotExcl ) {
                        gotExcl = TRUE;
                        workers.append( wrk );
                    } else {
                        // skip everything exclusive unless it's exactly the
                        // same worker
                        skipExcl = !workers.containsRef( wrk );
                    }
                }
                // continue to search for a fall-back cooperative 1-item worker
                // (like QPMMimeAnyMime) for the case if this worker quits
                // the game
                continue;
            }
            ULONG itemCnt = wrk->itemCount();
            if ( itemCnt == 0 ) {
#if defined(QT_CHECK_RANGE)
                qWarning( "Cooperative DragWorker %p for mime [%s] has "
                          "itemCount() = 0!", wrk, fmt );
#endif                
                continue;
            }
            if ( itemCnt > 1 ) {
                // coop workers with item count > 1 are also considered exclusive
                // here, because may not co-exist with 1-item workers that should
                // always be able to contribute
                if ( !gotExcl && !skipExcl && !gotExclForMime ) {
                    gotExclForMime = TRUE;
                    workers.append( wrk );
                    if ( !coopLevel )
                        coopLevel = itemCnt;
                    // only those for the same number of items can proceed
                    if ( itemCnt != coopLevel )
                        skipExcl = TRUE;
                }
                // continue to search for a fall-back cooperative 1-item worker
                // (like QPMMimeAnyMime) for the case if this worker quits
                // the game
                continue;
            }
            workers.append( wrk );
            // Don't search for other workrers for the same mime type --
            // we've already got a drag worker for it and adding another
            // one would just introduce mime type duplicates on the drop
            // target's side (where the first encountered drop worker
            // for the given RMF would be used for actual data transfer
            // anyway). See also QClipboard::setData(). 
            break;
        }
        if ( gotExclForMime ) {
            // ensure we have a fall-back coop (should be the last added item)
            DragWorker *w = workers.getLast();
            if ( w->isExclusive() || w->itemCount() > 1 ) {
#if defined(QT_CHECK_STATE)
                qWarning( "DragWorker %p for [%s] (isExclusive()=%d, itemCount()"
                          "=%ld) has no fall-back cooperative 1-item worker!",
                          w, fmt, w->isExclusive(), w->itemCount() );
#endif                
                workers.removeLast();
            }
            gotExclForMime = FALSE;
        } else {
            // got a regular (non-fall-back) 1-item coop, skip evreything else
            skipExcl = TRUE;
        }
    }

    // remove either all exclusive workers or all their fall-back workers
    // (depending on skipExcl) and remove duplicates 
    for ( DragWorker *wrk = workers.first(); wrk; ) {
        bool excl = wrk->isExclusive() || wrk->itemCount() > 1;
        if ( skipExcl == excl || workers.containsRef( wrk ) > 1 ) {
            workers.remove();
            wrk = workers.current();
        } else { 
            wrk = workers.next();
        }
    }
    
#if defined(QT_DEBUG_DND)            
    for ( DragWorker *wrk = workers.first(); wrk; wrk = workers.next() ) {
        qDebug( "QPMCoopDragWorker: Will use worker %p (isExclusive()=%d, "
                "itemCount()=%ld)", wrk, wrk->isExclusive(), wrk->itemCount() ); 
    }
#endif

    Q_ASSERT( workers.count() > 0 );
    return workers.count() > 0;
}

HWND QPMCoopDragWorker::hwnd() const
{
    DragWorker *firstWorker = workers.getFirst();
    Q_ASSERT( firstWorker );
    if ( !firstWorker )
        return 0;
    
    if ( firstWorker->isExclusive() && firstWorker->itemCount() == 0 ) {
        // this is a super exclusive worker that will do everything on its own
        return firstWorker->hwnd();
    }
    
    return QPMObjectWindow::hwnd();
}

void QPMCoopDragWorker::init()
{
    Q_ASSERT( source() );
    for ( DragWorker *wrk = workers.first(); wrk; wrk = workers.next() ) {
        wrk->src = source();
        wrk->init();
    }
}

bool QPMCoopDragWorker::cleanup( bool isCancelled )
{
    bool moveDisallowed = FALSE;
    
    for ( DragWorker *wrk = workers.first(); wrk; wrk = workers.next() ) {
        // disallow the Move operation if at least one worker asked so
        moveDisallowed |= wrk->cleanup( isCancelled );
        wrk->src = NULL;
    }
    workers.clear();
    info = NULL;
    return moveDisallowed;
}

DRAGINFO *QPMCoopDragWorker::createDragInfo( const char *name, USHORT supportedOps )
{
    Q_ASSERT( !info );
    if ( info )
        return NULL;

    DragWorker *firstWorker = workers.getFirst();
    Q_ASSERT( firstWorker );
    if ( !firstWorker )
        return NULL;

    ULONG itemCnt = firstWorker->itemCount();

    if ( firstWorker->isExclusive() && itemCnt == 0 ) {
        // this is a super exclusive worker that will do everything on its own
#if defined(QT_DEBUG_DND)                        
        qDebug( "QPMCoopDragWorker: Will redirect to super worker %p",
                firstWorker );
#endif        
        return firstWorker->createDragInfo( name, supportedOps );
    }
    
    // note that all workers at this place require the same amount of items
    // (guaranteed by collectWorkers())
    
#if defined(QT_DEBUG_DND)                        
    qDebug( "QPMCoopDragWorker: itemCnt=%ld", itemCnt );
#endif

    info = DrgAllocDraginfo( itemCnt );
    Q_ASSERT( info );
    if ( !info )
        return NULL;
    
    // collect all mechanism/format pairs
    QCString allFormats;
    for ( DragWorker *wrk = workers.first(); wrk; wrk = workers.next() ) {
        QCString formats = wrk->composeFormatString();
        Q_ASSERT( !formats.isNull() );
        if ( !formats.isNull() ) {
            if ( allFormats.isNull() )
                allFormats = formats;
            else {
                allFormats += ",";
                allFormats += formats;
            }
        }
    }

#if defined(QT_DEBUG_DND)                        
    qDebug( "QPMCoopDragWorker: allFormats=%s", allFormats.data() );
#endif

    static ULONG itemID = 0;
    
    const char *type = NULL;
    const char *ext = NULL;
    firstWorker->defaultFileType( type, ext );

    bool ok = TRUE;
    for ( ULONG i = 0; i < itemCnt; ++i ) {
        DRAGITEM *item = DrgQueryDragitemPtr( info, i );
        Q_ASSERT( item );
        if ( !item ) {
            ok = FALSE;
            break;
        }

        QCString targetName;
        if ( itemCnt == 1 ) targetName = name;
        else targetName.sprintf( "%s %ld", name, i + 1 );
        
        if ( ext ) {
            targetName += ".";
            targetName += ext;
        }

#if defined(QT_DEBUG_DND)                        
        qDebug( "QPMCoopDragWorker: item %ld: type=[%s], name=\"%s\"",
                i, type, targetName.data() );
#endif

        // Note 1: DRAGITEM::hstrType is actually ignored by WPS,
        // only the target extension matters.
        
        // Note 2: We're not required to fill in the hwndItem field because we
        // use the DC_PREPARE flag (to indicate it will be filled later, after
        // DM_RENDERPREPARE); however, Mozilla refuses to render if hwndItem
        // is initially 0. Set it to our HWND instead (we'll issue a warning if
        // DM_RENDER or DM_ENDCONVERSATION is erroneously sent to us) 

        item->hwndItem = hwnd();
        item->ulItemID = itemID ++;
        item->hstrType = DrgAddStrHandle( type ? type : DRT_UNKNOWN );
        item->hstrRMF = DrgAddStrHandle( allFormats );
        item->hstrContainerName = 0;
        item->hstrSourceName = 0;
        item->hstrTargetName = DrgAddStrHandle( targetName );
        item->cxOffset = 0;
        item->cyOffset = 0;
        item->fsControl = DC_PREPARE; // require DM_RENDERPREPARE from target
        item->fsSupportedOps = supportedOps;
    }
   
    if ( !ok ) {
        DrgFreeDraginfo( info );
        info = NULL;
    }
    
    return info;
}

extern void qt_DrgFreeDragtransfer( DRAGTRANSFER *xfer ); // defined in qmime_pm.cpp

MRESULT QPMCoopDragWorker::message( ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    if ( msg == DM_RENDERPREPARE ) {
        if ( !info ) {
#if defined(QT_CHECK_STATE)
            qWarning( "Drop target sent DM_RENDERPREPARE after the DnD session "
                      "is over!" );
#endif            
            // free the given DRAGTRANSFER structure to avoud memory leak 
            DRAGTRANSFER *xfer = (DRAGTRANSFER *) mp1;
            if ( xfer )
                qt_DrgFreeDragtransfer( xfer );
            return (MRESULT) FALSE;
        }
    
        DRAGTRANSFER *xfer = (DRAGTRANSFER *) mp1;
        Q_ASSERT( xfer && xfer->pditem );
        if ( !xfer || !xfer->pditem )
            return (MRESULT) FALSE;
        
        // find the item's index (ordinal number)
        ULONG itemCnt = DrgQueryDragitemCount( info );
        ULONG index = 0;
        for ( ; index <  itemCnt; ++index )
            if ( DrgQueryDragitemPtr( info, index ) == xfer->pditem )
                break;
        
        Q_ASSERT( index < itemCnt );
        if ( index >= itemCnt )
            return (MRESULT) FALSE;

#if defined(QT_DEBUG_DND)                        
        qDebug( "QPMCoopDragWorker: Got DM_RENDERPREPARE to %s for item %ld "
                "(id=%ld)",
                QPMMime::queryHSTR( xfer->hstrSelectedRMF ).data(),
                index, xfer->pditem->ulItemID );
#endif
        
        QCString drm, drf;
        if ( !QPMMime::parseRMF( xfer->hstrSelectedRMF, drm, drf ) ) {
            Q_ASSERT( FALSE );
            return (MRESULT) FALSE;
        }

        DragWorker *wrk = NULL;
        for ( wrk = workers.first(); wrk; wrk = workers.next() )
            if ( wrk->prepare( drm, drf, xfer->pditem, index ) )
                break;
        if ( !wrk ) {
#if defined(QT_DEBUG_DND)                        
            qDebug( "QPMCoopDragWorker: No suitable worker found" );
#endif            
            return (MRESULT) FALSE;
        }

        xfer->pditem->hwndItem = wrk->hwnd();
        Q_ASSERT( xfer->pditem->hwndItem );
        return (MRESULT) ( xfer->pditem->hwndItem ? TRUE : FALSE );
    }
    
    if ( msg == DM_RENDER || msg == DM_ENDCONVERSATION ) {
#if defined(QT_CHECK_STATE)
        qWarning( "Drop target sent DM_RENDER or DM_ENDCONVERSATION to the "
                  "drag source window instead of the drag item window!" );
#endif        
        if ( msg == DM_RENDER ) {
            // free the given DRAGTRANSFER structure to avoud memory leak 
            DRAGTRANSFER *xfer = (DRAGTRANSFER *) mp1;
            if ( xfer )
                qt_DrgFreeDragtransfer( xfer );
        }
    }
    
    return (MRESULT) FALSE;
}

bool QDragManager::drag( QDragObject *o, QDragObject::DragMode mode )
{
    // return shortly if drag is already in progress or being cancelled
    if ( object || beingCancelled )
        return FALSE;

    QDragObject::setTarget( 0 );
    
    // detect a mouse button to end dragging
    LONG vkTerminate = 0;
    {
        ULONG msg = WinQuerySysValue( HWND_DESKTOP, SV_BEGINDRAG ) & 0xFFFF;
        switch( msg ) {
            case WM_BUTTON1MOTIONSTART: vkTerminate = VK_BUTTON1; break; 
            case WM_BUTTON2MOTIONSTART: vkTerminate = VK_BUTTON2; break; 
            case WM_BUTTON3MOTIONSTART: vkTerminate = VK_BUTTON3; break; 
        }
        
        if ( WinGetKeyState( HWND_DESKTOP, vkTerminate ) & 0x8000 ) {
            // prefer the default button if it is pressed
        } else if ( WinGetKeyState( HWND_DESKTOP, VK_BUTTON2 ) & 0x8000 ) {
            vkTerminate = VK_BUTTON2;
        } else if ( WinGetKeyState( HWND_DESKTOP, VK_BUTTON1 ) & 0x8000 ) {
            vkTerminate = VK_BUTTON1;
        } else if ( WinGetKeyState( HWND_DESKTOP, VK_BUTTON3 ) & 0x8000 ) {
            vkTerminate = VK_BUTTON3;
        } else {
            vkTerminate = 0;
        }
    }
    
    if ( !vkTerminate ) {
#if defined(QT_CHECK_STATE)
        qWarning( "QDragManager: No valid mouse button pressed, "
                  "dragging cancelled!" );
#endif        
        delete o;
        return FALSE;
    }

    USHORT supportedOps = 0;
    
    switch ( mode ) {
        case QDragObject::DragDefault:
            supportedOps = DO_MOVEABLE | DO_COPYABLE | DO_LINKABLE;
            break;
        case QDragObject::DragMove:
            supportedOps = DO_MOVEABLE;
            break;
        case QDragObject::DragCopy:
            supportedOps = DO_COPYABLE;
            break;
        case QDragObject::DragCopyOrMove:
            supportedOps = DO_MOVEABLE | DO_COPYABLE;
            break;
        case QDragObject::DragLink:
            supportedOps = DO_LINKABLE;
            break;
    }
    
    static QPMCoopDragWorker dragWorker;
    
    bool ok = dragWorker.collectWorkers( o );
    Q_ASSERT( ok );
    Q_ASSERT( dragWorker.hwnd() );
    if ( !ok || ! dragWorker.hwnd() ) {
        delete o;
        return FALSE;
    }
    
    object = o;
    dragSource = (QWidget *) (object->parent());
    
    dragWorker.src = o;
    dragWorker.init();
    DRAGINFO *info = dragWorker.createDragInfo( o->name(), supportedOps );

    Q_ASSERT( info );
    if ( !info ) {
        dragWorker.cleanup( TRUE /* isCancelled */);
        dragWorker.src = NULL;
        dragSource = 0;
        object = 0;
        delete o;
        return FALSE;
    }

#if defined(QT_ACCESSIBILITY_SUPPORT)
    QAccessible::updateAccessibility( this, 0, QAccessible::DragDropStart );
#endif
    
/// @todo (dmik)
//    updatePixmap();

    DRAGIMAGE img;
    img.cb = sizeof (DRAGIMAGE);
    img.hImage = WinQuerySysPointer( HWND_DESKTOP, SPTR_FILE, FALSE );
    img.fl = DRG_ICON;
    img.cxOffset = 0;
    img.cyOffset = 0;

    // the mouse is most likely captured by Qt at this point, uncapture it
    // or DrgDrag() will definitely fail
    WinSetCapture( HWND_DESKTOP, 0 );
    
    HWND target = DrgDrag( dragWorker.hwnd(), info, &img, 1, vkTerminate,
                           (PVOID) 0x80000000L ); // don't lock the desktop PS

#if defined(QT_DEBUG_DND)                        
    qDebug( "QDragManager: DrgDrag() returned %08lX (err=0x%08lX)",
            target, WinGetLastError( 0 ) );
#endif    
    
    // we won't get any mouse release event, so manually adjust qApp state
    qt_pmMouseButtonUp();

    bool moveDisallowed = dragWorker.cleanup( beingCancelled || target == 0 );
    dragWorker.src = NULL;
    
    moveDisallowed |= beingCancelled || target == 0 ||
                      info->usOperation != DO_MOVE;

#if defined(QT_DEBUG_DND)                        
    qDebug( "QDragManager: moveDisallowed=%d", moveDisallowed );
#endif

    if ( target == 0 )
        DrgDeleteDraginfoStrHandles( info );
    DrgFreeDraginfo( info );

    if ( !beingCancelled ) {
        QDragObject::setTarget( QWidget::find( target ) );
        cancel(); // this will delete o (object)
    }
    
    beingCancelled = FALSE;

/// @todo (dmik)
//    updatePixmap();
    
    return !moveDisallowed;
}

void QDragManager::updatePixmap()
{
    /// @todo (dmik) later
  
/// @todo (dmik) remove    
//    if ( object ) {
//	if ( cursor ) {
//#ifndef Q_OS_TEMP
//	    for ( int i=0; i<n_cursor; i++ ) {
//		DestroyCursor(cursor[i]);
//	    }
//#endif
//	    delete [] cursor;
//	    cursor = 0;
//	}
//
//	QPixmap pm = object->pixmap();
//	if ( pm.isNull() ) {
//	    // None.
//	} else {
//	    cursor = new HCURSOR[n_cursor];
//	    QPoint pm_hot = object->pixmapHotSpot();
//	    for (int cnum=0; cnum<n_cursor; cnum++) {
//		QPixmap cpm = pm_cursor[cnum];
//
//		int x1 = QMIN(-pm_hot.x(),0);
//		int x2 = QMAX(pm.width()-pm_hot.x(),cpm.width());
//		int y1 = QMIN(-pm_hot.y(),0);
//		int y2 = QMAX(pm.height()-pm_hot.y(),cpm.height());
//
//		int w = x2-x1+1;
//		int h = y2-y1+1;
//
//#ifndef Q_OS_TEMP
//		if ( qWinVersion() & WV_DOS_based ) {
//		    // Limited cursor size
//		    int reqw = GetSystemMetrics(SM_CXCURSOR);
//		    int reqh = GetSystemMetrics(SM_CYCURSOR);
//		    if ( reqw < w ) {
//			// Not wide enough - move objectpm right
//			pm_hot.setX(pm_hot.x()-w+reqw);
//		    }
//		    if ( reqh < h ) {
//			// Not tall enough - move objectpm down
//			pm_hot.setY(pm_hot.y()-h+reqh);
//		    }
//		    // Always use system cursor size
//		    w = reqw;
//		    h = reqh;
//		}
//#endif
//
//		QPixmap colorbits(w,h,-1,QPixmap::NormalOptim);
//		{
//		    QPainter p(&colorbits);
//		    p.fillRect(0,0,w,h,color1);
//		    p.drawPixmap(QMAX(0,-pm_hot.x()),QMAX(0,-pm_hot.y()),pm);
//		    p.drawPixmap(QMAX(0,pm_hot.x()),QMAX(0,pm_hot.y()),cpm);
//		}
//
//		QBitmap maskbits(w,h,TRUE,QPixmap::NormalOptim);
//		{
//		    QPainter p(&maskbits);
//		    if ( pm.mask() ) {
//			QBitmap m(*pm.mask());
//			m.setMask(m);
//			p.drawPixmap(QMAX(0,-pm_hot.x()),QMAX(0,-pm_hot.y()),m);
//		    } else {
//			p.fillRect(QMAX(0,-pm_hot.x()),QMAX(0,-pm_hot.y()),
//			    pm.width(),pm.height(),color1);
//		    }
//		    if ( cpm.mask() ) {
//			QBitmap m(*cpm.mask());
//			m.setMask(m);
//			p.drawPixmap(QMAX(0,pm_hot.x()),QMAX(0,pm_hot.y()),m);
//		    } else {
//			p.fillRect(QMAX(0,pm_hot.x()),QMAX(0,pm_hot.y()),
//			    cpm.width(),cpm.height(),
//			    color1);
//		    }
//		}
//
//		HBITMAP im = qt_createIconMask(maskbits);
//		ICONINFO ii;
//		ii.fIcon     = FALSE;
//		ii.xHotspot  = QMAX(0,pm_hot.x());
//		ii.yHotspot  = QMAX(0,pm_hot.y());
//		ii.hbmMask   = im;
//		ii.hbmColor  = colorbits.hbm();
//		cursor[cnum] = CreateIconIndirect(&ii);
//		DeleteObject( im );
//	    }
//	}
//    }
}

#endif // QT_NO_DRAGANDDROP
