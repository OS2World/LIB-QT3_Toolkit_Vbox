/****************************************************************************
** $Id: main.cpp 160 2006-12-11 20:15:57Z dmik $
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include "statistics.h"
#include <qapplication.h>
int main( int argc, char **argv )
{
    QApplication a(argc,argv);			

    Table t;
    a.setMainWidget( &t );
    t.show();
    return a.exec();
}
