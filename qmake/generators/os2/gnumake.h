/****************************************************************************
** $Id: gnumake.h 8 2005-11-16 19:36:46Z dmik $
**
** Definition of GNUMakefileGenerator class.
**
** Copyright (C) 1992-2003 Trolltech AS.  All rights reserved.
** Copyright (C) 2004 Norman ASA.  Initial OS/2 Port.
** Copyright (C) 2005 netlabs.org.  Further OS/2 Development.
**
** This file is part of qmake.
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

#ifndef __GNUMAKE_H__
#define __GNUMAKE_H__

#include "winmakefile.h"

class GNUMakefileGenerator : public Win32MakefileGenerator
{
    bool init_flag;
    void writeGNUParts(QTextStream &);
    void writeSubDirs(QTextStream &t) ;

    bool writeMakefile(QTextStream &);
    void init();
    
    virtual bool findLibraries();

public:
    GNUMakefileGenerator(QMakeProject *p);
    ~GNUMakefileGenerator();

};

inline GNUMakefileGenerator::~GNUMakefileGenerator()
{ }

#endif /* __GNUMAKE_H__ */
