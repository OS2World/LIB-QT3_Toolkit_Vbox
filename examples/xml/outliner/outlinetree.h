/****************************************************************************
** $Id: outlinetree.h 2 2005-11-16 15:49:26Z dmik $
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#ifndef OUTLINETREE_H
#define OUTLINETREE_H

#include <qlistview.h>
#include <qdom.h>

class OutlineTree : public QListView
{
    Q_OBJECT

public:
    OutlineTree( const QString fileName, QWidget *parent = 0, const char *name = 0 );
    ~OutlineTree();

private:
    QDomDocument domTree;
    void getHeaderInformation( const QDomElement &header );
    void buildTree( QListViewItem *parentItem, const QDomElement &parentElement );
};

#endif // OUTLINETREE_H
