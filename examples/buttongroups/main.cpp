/****************************************************************************
** $Id: main.cpp 2 2005-11-16 15:49:26Z dmik $
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include "buttongroups.h"
#include <qapplication.h>

int main( int argc, char **argv )
{
    QApplication a( argc, argv );

    ButtonsGroups buttonsgroups;
    buttonsgroups.resize( 500, 250 );
    buttonsgroups.setCaption( "Qt Example - Buttongroups" );
    a.setMainWidget( &buttonsgroups );
    buttonsgroups.show();

    return a.exec();
}
