/****************************************************************************
** $Id: qstatusbar.h 2 2005-11-16 15:49:26Z dmik $
**
** Definition of QStatusBar class
**
** Created : 980316
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of the widgets module of the Qt GUI Toolkit.
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

#ifndef QSTATUSBAR_H
#define QSTATUSBAR_H

#ifndef QT_H
#include "qwidget.h"
#endif // QT_H

#ifndef QT_NO_STATUSBAR


class QStatusBarPrivate;


class Q_EXPORT QStatusBar: public QWidget
{
    Q_OBJECT
    Q_PROPERTY( bool sizeGripEnabled READ isSizeGripEnabled WRITE setSizeGripEnabled )

public:
    QStatusBar( QWidget* parent=0, const char* name=0 );
    virtual ~QStatusBar();

    virtual void addWidget( QWidget *, int stretch = 0, bool = FALSE );
    virtual void removeWidget( QWidget * );

    void setSizeGripEnabled(bool);
    bool isSizeGripEnabled() const;

public slots:
    void message( const QString &);
    void message( const QString &, int );
    void clear();

signals:
    void messageChanged( const QString &text );

protected:
    void paintEvent( QPaintEvent * );
    void resizeEvent( QResizeEvent * );

    void reformat();
    void hideOrShow();
    bool event( QEvent *);

private:
    QStatusBarPrivate * d;
private:	// Disabled copy constructor and operator=
#if defined(Q_DISABLE_COPY)
    QStatusBar( const QStatusBar & );
    QStatusBar& operator=( const QStatusBar & );
#endif
};

#endif // QT_NO_STATUSBAR

#endif // QSTATUSBAR_H
