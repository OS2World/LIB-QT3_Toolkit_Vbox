/****************************************************************************
** $Id: main.cpp 2 2005-11-16 15:49:26Z dmik $
**
** Copyright (C) 1992-2002 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include <qapplication.h>
#include "ftpmainwindow.h"

int main( int argc, char **argv )
{
    QApplication a( argc, argv );

    FtpMainWindow m;
    a.setMainWidget( &m );
    m.show();
    a.processEvents();
    m.connectToHost();
    return a.exec();
}
