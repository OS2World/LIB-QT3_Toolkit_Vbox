/****************************************************************************
** $Id: main.cpp 2 2005-11-16 15:49:26Z dmik $
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include <qapplication.h>
#include "tooltip.h"

int main( int argc, char ** argv ) 
{
    QApplication a( argc, argv );

    TellMe mw;
    mw.setCaption( "Qt Example - Dynamic Tool Tips" );
    a.setMainWidget( &mw );
    mw.show();
    
    return a.exec();
}
