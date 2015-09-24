/****************************************************************************
** $Id: qmime.h 114 2006-08-10 17:56:11Z dmik $
**
** Definition of mime classes
**
** Created : 981204
**
** Copyright (C) 1998-2000 Trolltech AS.  All rights reserved.
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

#ifndef QMIME_H
#define QMIME_H

#ifndef QT_H
#include "qwindowdefs.h"
#include "qmap.h"
#endif // QT_H

#ifndef QT_NO_MIME

class QImageDrag;
class QTextDrag;

class Q_EXPORT QMimeSource
{
    friend class QClipboardData;

public:
    QMimeSource();
    virtual ~QMimeSource();
    virtual const char* format( int n = 0 ) const = 0;
    virtual bool provides( const char* ) const;
    virtual QByteArray encodedData( const char* ) const = 0;
    int serialNumber() const;

private:
    int ser_no;
    enum { NoCache, Text, Graphics } cacheType;
    union
    {
	struct
	{
	    QString *str;
	    QCString *subtype;
	} txt;
	struct
	{
	    QImage *img;
	    QPixmap *pix;
	} gfx;
    } cache;
    void clearCache();

    // friends for caching
    friend class QImageDrag;
    friend class QTextDrag;

};

inline int QMimeSource::serialNumber() const
{ return ser_no; }

class QStringList;
class QMimeSourceFactoryData;

class Q_EXPORT QMimeSourceFactory {
public:
    QMimeSourceFactory();
    virtual ~QMimeSourceFactory();

    static QMimeSourceFactory* defaultFactory();
    static void setDefaultFactory( QMimeSourceFactory* );
    static QMimeSourceFactory* takeDefaultFactory();
    static void addFactory( QMimeSourceFactory *f );
    static void removeFactory( QMimeSourceFactory *f );

    virtual const QMimeSource* data(const QString& abs_name) const;
    virtual QString makeAbsolute(const QString& abs_or_rel_name, const QString& context) const;
    const QMimeSource* data(const QString& abs_or_rel_name, const QString& context) const;

    virtual void setText( const QString& abs_name, const QString& text );
    virtual void setImage( const QString& abs_name, const QImage& im );
    virtual void setPixmap( const QString& abs_name, const QPixmap& pm );
    virtual void setData( const QString& abs_name, QMimeSource* data );
    virtual void setFilePath( const QStringList& );
    virtual QStringList filePath() const;
    void addFilePath( const QString& );
    virtual void setExtensionType( const QString& ext, const char* mimetype );

private:
    QMimeSource *dataInternal(const QString& abs_name, const QMap<QString, QString> &extensions ) const;
    QMimeSourceFactoryData* d;
};

#if defined(Q_WS_WIN)

#ifndef QT_H
#include "qptrlist.h" // down here for GCC 2.7.* compatibility
#endif // QT_H

/*
  Encapsulation of conversion between MIME and Windows CLIPFORMAT.
  Not need on X11, as the underlying protocol uses the MIME standard
  directly.
*/

class Q_EXPORT QWindowsMime {
public:
    QWindowsMime();
    virtual ~QWindowsMime();

    static void initialize();

    static QPtrList<QWindowsMime> all();
    static QWindowsMime* convertor( const char* mime, int cf );
    static const char* cfToMime(int cf);

    static int registerMimeType(const char *mime);

    virtual const char* convertorName()=0;
    virtual int countCf()=0;
    virtual int cf(int index)=0;
    virtual bool canConvert( const char* mime, int cf )=0;
    virtual const char* mimeFor(int cf)=0;
    virtual int cfFor(const char* )=0;
    virtual QByteArray convertToMime( QByteArray data, const char* mime, int cf )=0;
    virtual QByteArray convertFromMime( QByteArray data, const char* mime, int cf )=0;
};

#endif // Q_WS_WIN

#if defined(Q_WS_PM)

#ifndef QT_H
#include "qptrlist.h"
#include "qstrlist.h"
#endif // QT_H

/*
  Encapsulation of conversion between MIME and OS/2 PM clipboard formats
  and between MIME and Direct Manipulation (DND) mechanisms and formats.
*/

class Q_EXPORT QPMMime
{
public:

#if !defined(QT_NO_DRAGANDDROP)

    class DragWorker
    {
    public:
        DragWorker() : src( NULL ) {}
        virtual ~DragWorker() {}
    
        const QMimeSource *source() const { return src; }
        
        virtual void init() {}
        // methods always implemented
        virtual bool cleanup( bool isCancelled ) = 0;
        virtual bool isExclusive() const = 0;
        virtual ULONG itemCount() const = 0;
        virtual HWND hwnd() const = 0;
        // methods implemented if isExclusive() == TRUE and itemCount() == 0
        virtual DRAGINFO *createDragInfo( const char *name, USHORT supportedOps )
                                        { return NULL; }
        // methods implemented if itemCount() >= 0
        virtual QCString composeFormatString() { return QCString(); }
        virtual bool prepare( const char *drm, const char *drf, DRAGITEM *item,
                              ULONG itemIndex ) { return FALSE; }
        virtual void defaultFileType( const char *&type, const char *&ext ) {};
        
    private:
        const QMimeSource *src;
        friend class QPMCoopDragWorker;
        friend class QDragManager;
    };
    
    class DefaultDragWorker : public DragWorker, public QPMObjectWindow
    {
    private:
        DefaultDragWorker( bool exclusive ); 
    public:
        virtual ~DefaultDragWorker();
    
        // DragpWorker interface
        bool cleanup( bool isCancelled );
        bool isExclusive() const;
        ULONG itemCount() const;
        HWND hwnd() const { return QPMObjectWindow::hwnd(); }
        QCString composeFormatString();
        bool prepare( const char *drm, const char *drf, DRAGITEM *item,
                      ULONG itemIndex );
        void defaultFileType( const char *&type, const char *&ext );
        
        // QPMObjectWindow interface
        MRESULT message( ULONG msg, MPARAM mp1, MPARAM mp2 );

        class Provider
        {
        public:
            virtual const char *format( const char *drf ) const = 0;
            virtual bool provide( const char *drf, const QByteArray &allData,
                                  ULONG itemIndex, QByteArray &itemData ) = 0;
            virtual void fileType( const char *drf, const char *&type,
                                   const char *&ext ) {};
        };
        
        bool addProvider( const char *drf, Provider *provider,
                          ULONG itemCount = 1 );
        
        static bool canRender( const char *drm );
        
    private:
        struct Data;
        Data *d;
        friend class QPMMime;
    };
    
    class DropWorker
    {
    public:
        DropWorker() : nfo( NULL ) {}
        virtual ~DropWorker() {}
        
        DRAGINFO *info() const { return nfo; }

        virtual void init() {}
        virtual void cleanup( bool isAccepted, bool isActAccepted ) {}
        virtual bool isExclusive() const = 0;
        virtual bool provides( const char *format ) const = 0;
        virtual int formatCount() const = 0;
        virtual const char *format( int fn ) const = 0;
        virtual QByteArray encodedData( const char *format ) const = 0;
        
    private:
        DRAGINFO *nfo;
        friend class QPMDragData;
    };

    class DefaultDropWorker : public DropWorker, public QPMObjectWindow
    {
    private:
        DefaultDropWorker(); 
    public:
        virtual ~DefaultDropWorker(); 
    
        // DropWorker interface
        void cleanup( bool isAccepted, bool isActAccepted );
        bool isExclusive() const;
        bool provides( const char *format ) const;
        int formatCount() const;
        const char *format( int fn ) const;
        QByteArray encodedData( const char *format ) const;
        
        // QPMObjectWindow interface
        MRESULT message( ULONG msg, MPARAM mp1, MPARAM mp2 );
    
        class Provider
        {
        public:
            virtual const char *drf( const char *format ) const = 0;
            virtual bool provide( const char *format, ULONG itemIndex,
                                  const QByteArray &itemData,
                                  QByteArray &allData ) = 0;
        };
        
        bool addProvider( const char *format, Provider *provider );
        bool addExclusiveProvider( const char *format, Provider *provider );
        
        static bool canRender( DRAGITEM *item, const char *drf );
        static bool getSupportedRMFs( DRAGITEM *item, QPtrList<QStrList> &list );
        
    private:
        struct Data;
        Data *d;
        friend class QPMMime;
    };

#endif // !QT_NO_DRAGANDDROP

    enum { CFI_Storage = CFI_POINTER & CFI_HANDLE };
    
    QPMMime();
    virtual ~QPMMime();

    static void initialize();

    static QPtrList<QPMMime> all();
    static QPMMime* convertor( const char* mime, ulong cf );
    static const char* cfToMime( ulong cf );

#if !defined(QT_NO_DRAGANDDROP)
    
    static QCString queryHSTR( HSTR hstr );
    static QCString querySourceNameFull( DRAGITEM *item );
    static bool canTargetRenderAsOS2File( DRAGITEM *item, QCString *fullName = NULL );
    static bool parseRMFs( HSTR rmfs, QPtrList<QStrList> &list );
    static bool parseRMF( HSTR rmf, QCString &mechanism, QCString &format );
    
    static DefaultDragWorker *defaultCoopDragWorker();
    static DefaultDragWorker *defaultExclDragWorker();
    static DefaultDropWorker *defaultDropWorker();

#endif // !QT_NO_DRAGANDDROP

    // Clipboard format convertor interface
    
    virtual const char* convertorName() = 0;
    virtual int countCf() = 0;
    virtual ulong cf( int index ) = 0;
    virtual ulong flFor( ulong cf ) = 0;
    virtual ulong cfFor( const char* ) = 0;
    virtual const char* mimeFor( ulong cf ) = 0;
    virtual bool canConvert( const char* mime, ulong cf ) = 0;
    virtual QByteArray convertToMime( QByteArray data, const char *mime, ulong cf ) = 0;
    virtual QByteArray convertFromMime( QByteArray data, const char *mime, ulong cf ) = 0;
    
#if !defined(QT_NO_DRAGANDDROP)

    // Direct Manipulation (DND) convertor interface

    virtual DragWorker *dragWorkerFor( const char *mime, QMimeSource *src ) { return 0; }
    virtual DropWorker *dropWorkerFor( DRAGINFO *info ) { return 0; }
    
#endif // !QT_NO_DRAGANDDROP
};

#endif // Q_WS_PM

#if defined(Q_WS_MAC)

#ifndef QT_H
#include "qptrlist.h" // down here for GCC 2.7.* compatibility
#endif // QT_H

/*
  Encapsulation of conversion between MIME and Mac flavor.
  Not need on X11, as the underlying protocol uses the MIME standard
  directly.
*/

class Q_EXPORT QMacMime {
    char type;
public:
    enum QMacMimeType { MIME_DND=0x01, MIME_CLIP=0x02, MIME_QT_CONVERTOR=0x04, MIME_ALL=MIME_DND|MIME_CLIP };
    QMacMime(char);
    virtual ~QMacMime();

    static void initialize();

    static QPtrList<QMacMime> all(QMacMimeType);
    static QMacMime* convertor(QMacMimeType, const char* mime, int flav);
    static const char* flavorToMime(QMacMimeType, int flav);

    virtual const char* convertorName()=0;
    virtual int countFlavors()=0;
    virtual int flavor(int index)=0;
    virtual bool canConvert(const char* mime, int flav)=0;
    virtual const char* mimeFor(int flav)=0;
    virtual int flavorFor(const char*)=0;
    virtual QByteArray convertToMime(QValueList<QByteArray> data, const char* mime, int flav)=0;
    virtual QValueList<QByteArray> convertFromMime(QByteArray data, const char* mime, int flav)=0;
};

#endif // Q_WS_MAC

#endif // QT_NO_MIME

#endif // QMIME_H
