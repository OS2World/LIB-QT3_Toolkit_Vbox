/****************************************************************************
** $Id: rot13.h 2 2005-11-16 15:49:26Z dmik $
**
** Definition of something or other
**
** Created : 979899
**
** Copyright (C) 1997 by Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#ifndef ROT13_H
#define ROT13_H

#include <qwidget.h>

class QMultiLineEdit;

class Rot13: public QWidget {
    Q_OBJECT
public:
    Rot13();

    QString rot13( const QString & ) const;

private slots:
    void changeLeft();
    void changeRight();

private:
    QMultiLineEdit * left, * right;
};

#endif
