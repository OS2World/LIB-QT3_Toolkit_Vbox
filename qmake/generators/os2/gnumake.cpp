/****************************************************************************
** $Id: gnumake.cpp 152 2006-11-10 00:30:20Z dmik $
**
** Implementation of GNUMakefileGenerator class.
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

#include "gnumake.h"
#include "option.h"
#include <qregexp.h>
#include <qdir.h>
#include <stdlib.h>
#include <time.h>


GNUMakefileGenerator::GNUMakefileGenerator(QMakeProject *p) : Win32MakefileGenerator(p), init_flag(FALSE)
{
}

bool
GNUMakefileGenerator::findLibraries() // todo - pascal
{
    return TRUE;
}

bool
GNUMakefileGenerator::writeMakefile(QTextStream &t)
{
    writeHeader(t);
    if(!project->variables()["QMAKE_FAILED_REQUIREMENTS"].isEmpty()) {
	t << "all clean:" << "\n\t"
	  << "@echo \"Some of the required modules ("
	  << var("QMAKE_FAILED_REQUIREMENTS") << ") are not available.\"" << "\n\t"
	  << "@echo \"Skipped.\"" << endl << endl;
	writeMakeQmake(t);
	return TRUE;
    }

    if(project->first("TEMPLATE") == "app" ||
       project->first("TEMPLATE") == "lib") {
	writeGNUParts(t);
	return MakefileGenerator::writeMakefile(t);
    }
    else if(project->first("TEMPLATE") == "subdirs") {
	writeSubDirs(t);
	return TRUE;
    }
    return FALSE;
 }

void
GNUMakefileGenerator::writeGNUParts(QTextStream &t)
{
    t << "QMAKESPECDIR	=	" << specdir() << endl << endl;
    
    t << "####### Compiler, tools and options" << endl << endl;
    t << "CC		=	" << var("QMAKE_CC") << endl;
    t << "CXX		=	" << var("QMAKE_CXX") << endl;
    t << "LEX		= " << var("QMAKE_LEX") << endl;
    t << "YACC		= " << var("QMAKE_YACC") << endl;
    t << "CFLAGS	=	" << var("QMAKE_CFLAGS") << " "
      << varGlue("PRL_EXPORT_DEFINES","-D"," -D","") << " "
      <<  varGlue("DEFINES","-D"," -D","") << endl;
    t << "CXXFLAGS	=	" << var("QMAKE_CXXFLAGS") << " "
      << varGlue("PRL_EXPORT_DEFINES","-D"," -D","") << " "
      << varGlue("DEFINES","-D"," -D","") << endl;
    t << "LEXFLAGS	=" << var("QMAKE_LEXFLAGS") << endl;
    t << "YACCFLAGS	=" << var("QMAKE_YACCFLAGS") << endl;

    t << "INCPATH	=	";
    {
        QStringList &incs = project->variables()["INCLUDEPATH"];
        QString incsSemicolon;
        for(QStringList::Iterator incit = incs.begin(); incit != incs.end(); ++incit) {
            QString inc = (*incit);
            inc.replace(QRegExp("\\\\$"), "");
            inc.replace(QRegExp("\""), "");
            if ( inc.contains( QRegExp("[ +&;$%()]") ) )
                inc = "\"" + inc + "\"";
            t << " " << var ("QMAKE_CFLAGS_INCDIR") << inc;
            incsSemicolon += inc + ";";
        }
        t << " " << var ("QMAKE_CFLAGS_INCDIR") << "\"$(QMAKESPECDIR)\"" << endl;
        incsSemicolon += "\"$(QMAKESPECDIR)\"";
        t << "INCLUDEPATH = " << incsSemicolon << endl;
    }
    if(!project->variables()["QMAKE_APP_OR_DLL"].isEmpty()) {
	t << "LINK	=	" << var("QMAKE_LINK") << endl;
	t << "LFLAGS	=	" << var("QMAKE_LFLAGS") << endl;
	t << "LIBS	=	";
        QString flag = var ("QMAKE_LFLAGS_LIBDIR") + "\""; 
	if ( !project->variables()["QMAKE_LIBDIR"].isEmpty() )
	    t << varGlue ("QMAKE_LIBDIR", flag, "\" " + flag, "\"") << " ";
        flag = var ("QMAKE_LFLAGS_LIB");
        t << varGlue ("QMAKE_LIBS", flag, " " + flag, "") << endl;
    }
    else {
	t << "LIB	=	" << var("QMAKE_LIB") << endl;
    }
    t << "MOC		=	" << (project->isEmpty("QMAKE_MOC") ? QString("moc") :
			      Option::fixPathToTargetOS(var("QMAKE_MOC"), FALSE)) << endl;
    t << "UIC		=	" << (project->isEmpty("QMAKE_UIC") ? QString("uic") :
			      Option::fixPathToTargetOS(var("QMAKE_UIC"), FALSE)) << endl;
    t << "QMAKE		=	" << (project->isEmpty("QMAKE_QMAKE") ? QString("qmake") :
			      Option::fixPathToTargetOS(var("QMAKE_QMAKE"), FALSE)) << endl;
    t << "IDC		=	" << (project->isEmpty("QMAKE_IDC") ? QString("idc") :
			      Option::fixPathToTargetOS(var("QMAKE_IDC"), FALSE)) << endl;
    t << "IDL		=	" << (project->isEmpty("QMAKE_IDL") ? QString("") :
			      Option::fixPathToTargetOS(var("QMAKE_IDL"), FALSE)) << endl;
    t << "RC		=	" << (project->isEmpty("QMAKE_RC") ? QString("rc") :
			      Option::fixPathToTargetOS(var("QMAKE_RC"), FALSE)) << endl;
    t << "ZIP		=	" << var("QMAKE_ZIP") << endl;
    t << "COPY_FILE	=       " << var("QMAKE_COPY") << endl;
    t << "COPY_DIR	=       " << var("QMAKE_COPY") << endl;
    t << "DEL_FILE	=       " << var("QMAKE_DEL_FILE") << endl;
    t << "DEL_DIR	=       " << var("QMAKE_DEL_DIR") << endl;
    t << "MOVE		=       " << var("QMAKE_MOVE") << endl;
    t << "IF_FILE_EXISTS =	" << var("QMAKE_IF_FILE_EXISTS") << endl;
    t << "CHK_DIR_EXISTS =	" << var("QMAKE_CHK_DIR_EXISTS") << endl;
    t << "MKDIR		=	" << var("QMAKE_MKDIR") << endl;
    t << endl;

    t << "####### Output directory" << endl << endl;
    if (! project->variables()["OBJECTS_DIR"].isEmpty())
	t << "OBJECTS_DIR = " << var("OBJECTS_DIR").replace(QRegExp("\\\\$"),"") << endl;
    else
	t << "OBJECTS_DIR = ." << endl;
    if (! project->variables()["MOC_DIR"].isEmpty())
	t << "MOC_DIR = " << var("MOC_DIR").replace(QRegExp("\\\\$"),"") << endl;
    else
	t << "MOC_DIR = ." << endl;
    t << endl;

    t << "####### Files" << endl << endl;
    t << "HEADERS =	" << varList("HEADERS") << endl;
    t << "SOURCES =	" << varList("SOURCES") << endl;
    t << "OBJECTS =	" << varList("OBJECTS") << endl;
    t << "FORMS =	" << varList("FORMS") << endl;
    t << "UICDECLS =	" << varList("UICDECLS") << endl;
    t << "UICIMPLS =	" << varList("UICIMPLS") << endl;
    t << "SRCMOC	=	" << varList("SRCMOC") << endl;
    t << "OBJMOC	=	" << varList("OBJMOC") << endl;
    
    QString extraCompilerDeps;
    if(!project->isEmpty("QMAKE_EXTRA_COMPILERS")) {
	t << "OBJCOMP = " << varList("OBJCOMP") << endl;
	extraCompilerDeps += " $(OBJCOMP) ";
	
	QStringList &comps = project->variables()["QMAKE_EXTRA_COMPILERS"];
	for(QStringList::Iterator compit = comps.begin(); compit != comps.end(); ++compit) {
	    QStringList &vars = project->variables()[(*compit) + ".variables"];
	    for(QStringList::Iterator varit = vars.begin(); varit != vars.end(); ++varit) {
		QStringList vals = project->variables()[(*varit)];
		if(!vals.isEmpty())
		    t << "QMAKE_COMP_" << (*varit) << " = " << valList(vals) << endl;
	    }
	}
    }
    
    if ( project->isEmpty( "DEF_FILE" ) ) {
        if ( project->isActiveConfig("dll") ) {
            t << "DEF_FILE	=	$(basename $(TARGET)).def" << endl;
            project->variables()["QMAKE_CLEAN"].append( "$(DEF_FILE)" );
            if ( !project->isEmpty( "DEF_FILE_TEMPLATE" ) ) {
                t << "DEF_FILE_TEMPLATE = " << var( "DEF_FILE_TEMPLATE" ) << endl;
                project->variables()["QMAKE_GENDEF_DEPS"] += "$(DEF_FILE_TEMPLATE)";
            }
            if ( !project->isEmpty( "DEF_FILE_MAP" ) ) {
                t << "DEF_FILE_MAP = " << var( "DEF_FILE_MAP" ) << endl;
                project->variables()["QMAKE_GENDEF_DEPS"] += "$(DEF_FILE_MAP)";
            }
        }
    } else {
        if ( !project->isEmpty( "DEF_FILE_TEMPLATE" ) ) {
            fprintf( stderr, "Both DEF_FILE and DEF_FILE_TEMPLATE are specified.\n" );
            fprintf( stderr, "Please specify one of them, not both." );
            exit( 666 );
        }
        t << "DEF_FILE	=	" << var( "DEF_FILE" ) << endl;
    }

    if ( !project->isEmpty( "RC_FILE" ) ) {
        t << "RC_FILE	=	" << var("RC_FILE") << endl;
    }
    if ( !project->isEmpty( "RES_FILE" ) ) {
        t << "RES_FILE	=	" << var("RES_FILE") << endl;
    }
    
    if ( project->isActiveConfig("dll") && !project->isEmpty( "QMAKE_RUN_IMPLIB" ) ) {
        t << "TARGET_IMPLIB	=	$(basename $(TARGET)).lib" << endl;
        project->variables()["QMAKE_CLEAN"].append( "$(TARGET_IMPLIB)" );
    }

    t << "DIST	=	" << varList("DISTFILES") << endl;
    t << "TARGET	=	";
    if( !project->variables()[ "DESTDIR" ].isEmpty() )
	t << varGlue("TARGET",project->first("DESTDIR"),"",project->first("TARGET_EXT"));
    else
	t << project->variables()[ "TARGET" ].first() << project->variables()[ "TARGET_EXT" ].first();
    t << endl;

    if ( !project->isEmpty( "VERSION" ) ) {
        t << "VERSION	=	" << var("VERSION") << endl;
    }
    t << endl;

    t << "####### Implicit rules" << endl << endl;
    t << ".SUFFIXES: .c " << Option::cpp_ext.join (" ") << endl << endl;
    t << ".c" + Option::obj_ext + ":\n\t" << var("QMAKE_RUN_CC_IMP") << endl << endl;
    for (QStringList::Iterator it = Option::cpp_ext.begin(); it != Option::cpp_ext.end(); ++it) {
        t << (*it) + Option::obj_ext + ":\n\t" << var("QMAKE_RUN_CXX_IMP") << endl << endl;
    }

    t << "####### Build rules" << endl << endl;
    t << "all: " << "$(OBJECTS_DIR) " << "$(MOC_DIR) " << varGlue("ALL_DEPS",""," "," ") << "$(TARGET)" << endl << endl;
    t << "$(TARGET): " << var("PRE_TARGETDEPS") << " $(UICDECLS) $(OBJECTS) $(OBJMOC) ";
    if(!project->variables()["QMAKE_APP_OR_DLL"].isEmpty()) {
        t << "$(DEF_FILE) " << extraCompilerDeps << var("POST_TARGETDEPS");
        if ( project->isEmpty( "QMAKE_RUN_LINK" ) )
            t << "\n\t" << "$(LINK) $(LFLAGS) -o $(TARGET) $(DEF_FILE) $(OBJECTS) $(OBJMOC) $(LIBS)";
        else
            t << "\n\t" << var( "QMAKE_RUN_LINK" );
        if ( !project->isEmpty( "RES_FILE" ) && !project->isEmpty( "QMAKE_RUN_RC_EXE" ) )
            t << "\n\t" << var("QMAKE_RUN_RC_EXE");
        if ( project->isActiveConfig("dll") && !project->isEmpty( "QMAKE_RUN_IMPLIB" ) )
            t << "\n\t" << var( "QMAKE_RUN_IMPLIB" );
    } else {
        t << extraCompilerDeps << var("POST_TARGETDEPS");
        if ( project->isEmpty( "QMAKE_RUN_LIB" ) )
            t << "\n\t" << "$(LIB) $(TARGET) $(OBJECTS) $(OBJMOC)";
        else
            t << "\n\t" << var( "QMAKE_RUN_LIB" );
    }
    t << extraCompilerDeps;
    if(project->isActiveConfig("dll") && !project->variables()["DLLDESTDIR"].isEmpty()) {
	QStringList dlldirs = project->variables()["DLLDESTDIR"];
	for ( QStringList::Iterator dlldir = dlldirs.begin(); dlldir != dlldirs.end(); ++dlldir ) {
	    t << "\n\t" << "$(COPY_FILE) $(TARGET) " << *dlldir;
	}
    }
    t << endl << endl;

    if ( project->isEmpty( "DEF_FILE" ) && project->isActiveConfig("dll") ) {
        t << "$(DEF_FILE): " << var( "QMAKE_GENDEF_DEPS" );
        t << valGlue (QStringList::split (" ;; ", var ("QMAKE_RUN_GENDEF")),
                "\n\t", "\n\t", "") << endl << endl;
    }
    
    if ( !project->isEmpty( "RC_FILE" ) && !project->isEmpty( "QMAKE_RUN_RC_RES" ) ) {
        t << "$(RES_FILE): $(RC_FILE)\n\t";
        t << var("QMAKE_RUN_RC_RES") << endl << endl;
    }

    t << "mocables: $(SRCMOC)" << endl << endl;

    if ( !project->isEmpty( "OBJECTS_DIR" ) ) 
        t << "$(OBJECTS_DIR):" << "\n\t"
          << "@$(CHK_DIR_EXISTS) $(OBJECTS_DIR) $(MKDIR) $(OBJECTS_DIR)" << endl << endl;

    if ( !project->isEmpty( "MOC_DIR" ) ) 
        t << "$(MOC_DIR):" << "\n\t"
          << "@$(CHK_DIR_EXISTS) $(MOC_DIR) $(MKDIR) $(MOC_DIR)" << endl << endl;

    writeMakeQmake(t);

    QStringList dist_files;
    for(
        QStringList::Iterator it = Option::mkfile::project_files.begin();
        it != Option::mkfile::project_files.end(); ++it
    ) {
        dist_files << fileFixify(*it);
    }
/// @todo (dmik) guess we don't need to zip this    
//    if(!project->isEmpty("QMAKE_INTERNAL_INCLUDED_FILES"))
//	dist_files += project->variables()["QMAKE_INTERNAL_INCLUDED_FILES"];
    if(!project->isEmpty("TRANSLATIONS"))
	dist_files << var("TRANSLATIONS");
    if(!project->isEmpty("FORMS")) {
	QStringList &forms = project->variables()["FORMS"];
	for(QStringList::Iterator formit = forms.begin(); formit != forms.end(); ++formit) {
	    QString ui_h = fileFixify((*formit) + Option::h_ext.first());
	    if(QFile::exists(ui_h) )
		dist_files << ui_h;
	}
    }
    t << "dist:" << "\n\t"
      << "$(ZIP) " << var("QMAKE_ORIG_TARGET") << ".zip " << "$(SOURCES) $(HEADERS) $(DIST) $(FORMS) "
      << dist_files.join(" ") << " " << var("TRANSLATIONS") << " " << var("IMAGES") << endl << endl;

    t << "clean:"
      << varGlue("OBJECTS","\n\t-$(DEL_FILE) ","\n\t-$(DEL_FILE) ","")
      << varGlue("SRCMOC" ,"\n\t-$(DEL_FILE) ","\n\t-$(DEL_FILE) ","")
      << varGlue("OBJMOC" ,"\n\t-$(DEL_FILE) ","\n\t-$(DEL_FILE) ","")
      << varGlue("UICDECLS" ,"\n\t-$(DEL_FILE) ","\n\t-$(DEL_FILE) ","")
      << varGlue("UICIMPLS" ,"\n\t-$(DEL_FILE) ","\n\t-$(DEL_FILE) ","")
      << "\n\t-$(DEL_FILE) $(TARGET)"
      << varGlue("QMAKE_CLEAN","\n\t-$(DEL_FILE) ","\n\t-$(DEL_FILE) ","")
      << varGlue("CLEAN_FILES","\n\t-$(DEL_FILE) ","\n\t-$(DEL_FILE) ","");
    if(!project->isEmpty("IMAGES"))
	t << varGlue("QMAKE_IMAGE_COLLECTION", "\n\t-$(DEL_FILE) ", "\n\t-$(DEL_FILE) ", "");

    if ( !project->isEmpty( "QMAKE_QUIET_CLEAN" ) ) {
        QStringList &delf = project->variables()["QMAKE_QUIET_CLEAN"];
        for( QStringList::Iterator it = delf.begin(); it != delf.end(); ++it ) {
            t << "\n\t-@$(IF_FILE_EXISTS) " << (*it) << " $(DEL_FILE) " << (*it);
        }
    }
    
    // user defined targets
    QStringList::Iterator it;
    QStringList &qut = project->variables()["QMAKE_EXTRA_TARGETS"];

    for(it = qut.begin(); it != qut.end(); ++it) {
	QString targ = var((*it) + ".target"),
		 cmd = var((*it) + ".commands"),
           extradeps = var((*it) + ".extradeps"),
                deps;
	if(targ.isEmpty())
	    targ = (*it);
	QStringList &deplist = project->variables()[(*it) + ".depends"];
	for(QStringList::Iterator dep_it = deplist.begin(); dep_it != deplist.end(); ++dep_it) {
	    QString dep = var((*dep_it) + ".target");
	    if(dep.isEmpty())
		dep = (*dep_it);
	    deps += " " + dep;
	}
	t << "\n\n";
        if ( !extradeps.isEmpty() ) {
            t << valGlue( QStringList::split (" ;; ", extradeps),
                           targ + ": ", "\n" + targ + ": ", "\n" );
        }
	t << targ << ":" << deps << "\n\t"
	  << valGlue( QStringList::split (" ;; ", cmd), "", "\n\t", "" );
    }

    t << endl << endl;
   
    QStringList &quc = project->variables()["QMAKE_EXTRA_COMPILERS"];
    for(it = quc.begin(); it != quc.end(); ++it) {
	QString tmp_out = project->variables()[(*it) + ".output"].first();
	QString tmp_cmd = project->variables()[(*it) + ".commands"].join(" ");
	QString tmp_dep = project->variables()[(*it) + ".depends"].join(" ");
	QStringList &vars = project->variables()[(*it) + ".variables"];
	if(tmp_out.isEmpty() || tmp_cmd.isEmpty())
	    continue;
	QStringList &tmp = project->variables()[(*it) + ".input"];
	for(QStringList::Iterator it2 = tmp.begin(); it2 != tmp.end(); ++it2) {
	    QStringList &inputs = project->variables()[(*it2)];
	    for(QStringList::Iterator input = inputs.begin(); input != inputs.end(); ++input) {
		QFileInfo fi(Option::fixPathToLocalOS((*input)));
		QString in = Option::fixPathToTargetOS((*input), FALSE),
		       out = tmp_out, cmd = tmp_cmd, deps;
		out.replace("${QMAKE_FILE_BASE}", fi.baseName());
		out.replace("${QMAKE_FILE_NAME}", fi.fileName());
		cmd.replace("${QMAKE_FILE_BASE}", fi.baseName());
		cmd.replace("${QMAKE_FILE_OUT}", out);
		cmd.replace("${QMAKE_FILE_NAME}", fi.fileName());
		for(QStringList::Iterator it3 = vars.begin(); it3 != vars.end(); ++it3)
		    cmd.replace("$(" + (*it3) + ")", "$(QMAKE_COMP_" + (*it3)+")");
		if(!tmp_dep.isEmpty()) {
		    char buff[256];
		    QString dep_cmd = tmp_dep;
		    dep_cmd.replace("${QMAKE_FILE_NAME}", fi.fileName());
		    if(FILE *proc = QT_POPEN(dep_cmd.latin1(), "r")) {
			while(!feof(proc)) {
			    int read_in = fread(buff, 1, 255, proc);
			    if(!read_in)
				break;
			    int l = 0;
			    for(int i = 0; i < read_in; i++) {
				if(buff[i] == '\n' || buff[i] == ' ') {
				    deps += " " + QCString(buff+l, (i - l) + 1);
				    l = i;
				}
			    }
			}
			fclose(proc);
		    }
		}
		t << out << ": " << in << deps << "\n\t"
		  << valGlue( QStringList::split( " ;; ", cmd ), "", "\n\t", "" )
                  << endl << endl;
	    }
	}
    }
    t << endl;
}


void
GNUMakefileGenerator::init()
{
    if(init_flag)
	return;
    init_flag = TRUE;

    /* this should probably not be here, but I'm using it to wrap the .t files */
    if(project->first("TEMPLATE") == "app")
	project->variables()["QMAKE_APP_FLAG"].append("1");
    else if(project->first("TEMPLATE") == "lib")
	project->variables()["QMAKE_LIB_FLAG"].append("1");
    else if(project->first("TEMPLATE") == "subdirs") {
	MakefileGenerator::init();
	if(project->variables()["MAKEFILE"].isEmpty())
	    project->variables()["MAKEFILE"].append("Makefile");
	if(project->variables()["QMAKE"].isEmpty())
	    project->variables()["QMAKE"].append("qmake");
	return;
    }

    bool is_qt = project->isActiveConfig( "build_qt" );
    project->variables()["QMAKE_ORIG_TARGET"] = project->variables()["TARGET"];

    // LIBS defined in Profile comes first for gcc
    project->variables()["QMAKE_LIBS"] += project->variables()["LIBS"];
    
    QString targetfilename = project->variables()["TARGET"].first();
    QStringList &configs = project->variables()["CONFIG"];
    
    if (project->isActiveConfig("qt") && project->isActiveConfig("shared"))
	project->variables()["DEFINES"].append("QT_DLL");

    if (!project->variables()["QMAKE_DEFINES_QT"].isEmpty() &&
        (is_qt || project->isActiveConfig("qt") || project->isActiveConfig("qtinc"))) {
        QStringList &defs = project->variables()["DEFINES"];
        QStringList &qtdefs = project->variables()["QMAKE_DEFINES_QT"];
        QStringList::ConstIterator it = qtdefs.end();
        while (it != qtdefs.begin()) {
            --it;
            if (!defs.contains(*it))
                defs.prepend(*it);
        }
    }
    
    if (project->isActiveConfig("qt_dll"))
	if (configs.findIndex("qt") == -1)
	    configs.append("qt");
    
    if ( project->isActiveConfig("qt") ) {
	if ( project->isActiveConfig( "plugin" ) ) {
	    project->variables()["CONFIG"].append("dll");
	    if(project->isActiveConfig("qt"))
		project->variables()["DEFINES"].append("QT_PLUGIN");
	}
	if ( (project->variables()["DEFINES"].findIndex("QT_NODLL") == -1) &&
         ((project->variables()["DEFINES"].findIndex("QT_MAKEDLL") != -1 ||
           project->variables()["DEFINES"].findIndex("QT_DLL") != -1) ||
          (getenv("QT_DLL") && !getenv("QT_NODLL"))) ) {
	    project->variables()["QMAKE_QT_DLL"].append("1");
	    if ( is_qt && !project->variables()["QMAKE_LIB_FLAG"].isEmpty() )
		project->variables()["CONFIG"].append("dll");
	}
	if ( project->isActiveConfig("thread") )
	    project->variables()[is_qt ? "PRL_EXPORT_DEFINES" : "DEFINES"].append("QT_THREAD_SUPPORT");
	if ( project->isActiveConfig("accessibility" ) )
	    project->variables()[is_qt ? "PRL_EXPORT_DEFINES" : "DEFINES"].append("QT_ACCESSIBILITY_SUPPORT");
	if ( project->isActiveConfig("tablet") )
	    project->variables()[is_qt ? "PRL_EXPORT_DEFINES" : "DEFINES"].append("QT_TABLET_SUPPORT");
    }
    
    if ( project->isActiveConfig("dll") || !project->variables()["QMAKE_APP_FLAG"].isEmpty() ) {
	project->variables()["CONFIG"].remove("staticlib");
	project->variables()["QMAKE_APP_OR_DLL"].append("1");
    } else {
	project->variables()["CONFIG"].append("staticlib");
    }
    
    if ( project->isActiveConfig("warn_off") ) {
	project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_WARN_OFF"];
	project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_WARN_OFF"];
    } else if ( project->isActiveConfig("warn_on") ) {
	project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_WARN_ON"];
	project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_WARN_ON"];
    }
    
    if ( project->isActiveConfig("debug") ) {
        if ( project->isActiveConfig("thread") ) {
	    // use the DLL RT even here
	    if ( project->variables()["DEFINES"].contains("QT_DLL") ) {
		project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_MT_DLLDBG"];
		project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_MT_DLLDBG"];
	    } else {
		project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_MT_DBG"];
		project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_MT_DBG"];
	    }
	}
	project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_DEBUG"];
	project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_DEBUG"];
	project->variables()["QMAKE_LFLAGS"] += project->variables()["QMAKE_LFLAGS_DEBUG"];
    } else {
	if ( project->isActiveConfig("thread") ) {
	    if ( project->variables()["DEFINES"].contains("QT_DLL") ) {
		project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_MT_DLL"];
		project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_MT_DLL"];
	    } else {
		project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_MT"];
		project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_MT"];
	    }
	}
	project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_RELEASE"];
	project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_RELEASE"];
	project->variables()["QMAKE_LFLAGS"] += project->variables()["QMAKE_LFLAGS_RELEASE"];
    }
    
    if ( !project->variables()["QMAKE_INCDIR"].isEmpty())
	project->variables()["INCLUDEPATH"] += project->variables()["QMAKE_INCDIR"];
    
    if ( project->isActiveConfig("qt") || project->isActiveConfig("opengl") )
	project->variables()["CONFIG"].append("windows");
    
    if ( project->isActiveConfig("qt") ) {
	project->variables()["CONFIG"].append("moc");
	project->variables()["INCLUDEPATH"] +=	project->variables()["QMAKE_INCDIR_QT"];
	if ( !project->isActiveConfig("debug") ) {
            project->variables()["QMAKE_LIBDIR"] += project->variables()["QMAKE_LIBDIR_QT"];
            project->variables()["QMAKE_LIBDIR"] += project->variables()["QMAKE_LIBDIR_QT_DEBUG"];
	    project->variables()[is_qt ? "PRL_EXPORT_DEFINES" : "DEFINES"].append("QT_NO_DEBUG");
        } else {
            project->variables()["QMAKE_LIBDIR"] += project->variables()["QMAKE_LIBDIR_QT_DEBUG"];
            project->variables()["QMAKE_LIBDIR"] += project->variables()["QMAKE_LIBDIR_QT"];
        }
	if ( is_qt && !project->variables()["QMAKE_LIB_FLAG"].isEmpty() ) {
	    if ( !project->variables()["QMAKE_QT_DLL"].isEmpty()) {
		project->variables()["DEFINES"].append("QT_MAKEDLL");
		project->variables()["QMAKE_LFLAGS"] += project->variables()["QMAKE_LFLAGS_QT_DLL"];
	    }
	} else {

	    if ( project->variables()["QMAKE_QT_DLL"].isEmpty() ) {
                if(project->isActiveConfig("thread"))
                    project->variables()["QMAKE_LIBS"] += project->variables()["QMAKE_LIBS_QT_THREAD"];
                else
                    project->variables()["QMAKE_LIBS"] += project->variables()["QMAKE_LIBS_QT"];
            } else {
                if(project->isActiveConfig("thread"))
                    project->variables()["QMAKE_LIBS"] += project->variables()["QMAKE_LIBS_QT_THREAD_DLL"];
                else
                    project->variables()["QMAKE_LIBS"] += project->variables()["QMAKE_LIBS_QT_DLL"];
	    }
            if ( !project->isEmpty( "QMAKE_LIBS_QT_ENTRY" ) ) { 
                if ( !project->isActiveConfig("dll") && !project->isActiveConfig("plugin") ) {
                    project->variables()["QMAKE_LIBS"] +=project->variables()["QMAKE_LIBS_QT_ENTRY"];
                }
                // QMAKE_LIBS_QT_ENTRY should be first on the link line as it needs qt
                project->variables()["QMAKE_LIBS"].remove(project->variables()["QMAKE_LIBS_QT_ENTRY"].first());
                project->variables()["QMAKE_LIBS"].prepend(project->variables()["QMAKE_LIBS_QT_ENTRY"].first());
            }
	}
    }
    
    if ( project->isActiveConfig("opengl") ) {
	project->variables()["QMAKE_LIBS"] += project->variables()["QMAKE_LIBS_OPENGL"];
	project->variables()["QMAKE_LFLAGS"] += project->variables()["QMAKE_LFLAGS_OPENGL"];
    }
    
    if ( project->isActiveConfig("dll") ) {
	project->variables()["QMAKE_CFLAGS_CONSOLE_ANY"] = project->variables()["QMAKE_CFLAGS_CONSOLE_DLL"];
	project->variables()["QMAKE_CXXFLAGS_CONSOLE_ANY"] = project->variables()["QMAKE_CXXFLAGS_CONSOLE_DLL"];
	project->variables()["QMAKE_LFLAGS_CONSOLE_ANY"] = project->variables()["QMAKE_LFLAGS_CONSOLE_DLL"];
	project->variables()["QMAKE_LFLAGS_WINDOWS_ANY"] = project->variables()["QMAKE_LFLAGS_WINDOWS_DLL"];
        project->variables()["TARGET_EXT"].append(".dll");
    } else {
	project->variables()["QMAKE_CFLAGS_CONSOLE_ANY"] = project->variables()["QMAKE_CFLAGS_CONSOLE"];
	project->variables()["QMAKE_CXXFLAGS_CONSOLE_ANY"] = project->variables()["QMAKE_CXXFLAGS_CONSOLE"];
	project->variables()["QMAKE_LFLAGS_CONSOLE_ANY"] = project->variables()["QMAKE_LFLAGS_CONSOLE"];
	project->variables()["QMAKE_LFLAGS_WINDOWS_ANY"] = project->variables()["QMAKE_LFLAGS_WINDOWS"];
	if ( !project->variables()["QMAKE_APP_FLAG"].isEmpty()) {
	    project->variables()["TARGET_EXT"].append(".exe");
	} else {
	    project->variables()["TARGET_EXT"].append(".lib");
	}
    }
    
    if ( project->isActiveConfig("windows") ) {
	if ( project->isActiveConfig("console") ) {
	    project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_CONSOLE_ANY"];
	    project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_CONSOLE_ANY"];
	    project->variables()["QMAKE_LFLAGS"] += project->variables()["QMAKE_LFLAGS_CONSOLE_ANY"];
	    project->variables()["QMAKE_LIBS"] += project->variables()["QMAKE_LIBS_CONSOLE"];
	} else {
	    project->variables()["QMAKE_LFLAGS"] += project->variables()["QMAKE_LFLAGS_WINDOWS_ANY"];
	}
	project->variables()["QMAKE_LIBS"] += project->variables()["QMAKE_LIBS_WINDOWS"];
    } else {
	project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_CONSOLE_ANY"];
	project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_CONSOLE_ANY"];
	project->variables()["QMAKE_LFLAGS"] += project->variables()["QMAKE_LFLAGS_CONSOLE_ANY"];
	project->variables()["QMAKE_LIBS"] += project->variables()["QMAKE_LIBS_CONSOLE"];
    }
    
    if ( project->isActiveConfig("exceptions") ) {
	project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_EXCEPTIONS_ON"];
	project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_EXCEPTIONS_ON"];
    } else {
	project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_EXCEPTIONS_OFF"];
	project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_EXCEPTIONS_OFF"];
    }
    
    if ( project->isActiveConfig("rtti") ) {
	project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_RTTI_ON"];
	project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_RTTI_ON"];
    } else {
	project->variables()["QMAKE_CFLAGS"] += project->variables()["QMAKE_CFLAGS_RTTI_OFF"];
	project->variables()["QMAKE_CXXFLAGS"] += project->variables()["QMAKE_CXXFLAGS_RTTI_OFF"];
    }

    if ( project->isActiveConfig("moc") )
	setMocAware(TRUE);
    
    // move -L<path> constructs to libdir and remove -l from the beginning of
    // all other libraries (the correct linker switch will be set when
    // generating the LIBS variable for the makefile in writeGNUParts())
    QStringList &libs = project->variables()["QMAKE_LIBS"];
    for ( QStringList::Iterator libit = libs.begin(); libit != libs.end();  ) {
	if ( (*libit).startsWith( "-L" ) ) {
	    project->variables()["QMAKE_LIBDIR"] += (*libit).mid(2);
	    libit = libs.remove( libit );
	} else {
            if ( (*libit).startsWith( "-l" ) )
                *libit = (*libit).mid(2);
	    ++libit;
	}
    }

    project->variables()["QMAKE_FILETAGS"] += QStringList::split(' ',
	"HEADERS SOURCES DEF_FILE DEF_FILE_TEMPLATE DEF_FILE_MAP RC_FILE "
        "RES_FILE TARGET QMAKE_LIBS DESTDIR DLLDESTDIR INCLUDEPATH");
    QStringList &l = project->variables()["QMAKE_FILETAGS"];
    QStringList::Iterator it;
    for(it = l.begin(); it != l.end(); ++it) {
	QStringList &gdmf = project->variables()[(*it)];
	for(QStringList::Iterator inner = gdmf.begin(); inner != gdmf.end(); ++inner)
	    (*inner) = Option::fixPathToTargetOS((*inner), FALSE);
    }

    if ( !project->variables()["RC_FILE"].isEmpty()) {
	if ( !project->variables()["RES_FILE"].isEmpty()) {
	    fprintf( stderr, "Both RC_FILE and RES_FILE are specified.\n" );
	    fprintf( stderr, "Please specify one of them, not both.\n" );
	    exit( 666 );
	}
	project->variables()["RES_FILE"] =
            Option::fixPathToLocalOS( var( "OBJECTS_DIR" ) + "/"
                                      + QFileInfo( var( "RC_FILE" ) ).baseName( TRUE )
                                      + ".res",
                                      FALSE ); 
	project->variables()["CLEAN_FILES"] += "$(RES_FILE)";
    }

    if ( !project->variables()["RES_FILE"].isEmpty())
	project->variables()["POST_TARGETDEPS"] += "$(RES_FILE)";
    
    MakefileGenerator::init();
    
    if ( !project->variables()["VERSION"].isEmpty()) {
	QStringList l = QStringList::split('.', project->first("VERSION"));
	project->variables()["VER_MAJ"].append(l[0]);
	project->variables()["VER_MIN"].append(l[1]);
    }

    QStringList &quc = project->variables()["QMAKE_EXTRA_COMPILERS"];
    for(it = quc.begin(); it != quc.end(); ++it) {
	QString tmp_out = project->variables()[(*it) + ".output"].first();
	if(tmp_out.isEmpty())
	    continue;
	QStringList &tmp = project->variables()[(*it) + ".input"];
	for(QStringList::Iterator it2 = tmp.begin(); it2 != tmp.end(); ++it2) {
	    QStringList &inputs = project->variables()[(*it2)];
	    for(QStringList::Iterator input = inputs.begin(); input != inputs.end(); ++input) {
		QFileInfo fi(Option::fixPathToLocalOS((*input)));
		QString in = Option::fixPathToTargetOS((*input), FALSE),
		       out = tmp_out;
		out.replace("${QMAKE_FILE_BASE}", fi.baseName());
		out.replace("${QMAKE_FILE_NAME}", fi.fileName());
		if(project->variables()[(*it) + ".CONFIG"].findIndex("no_link") == -1)
		    project->variables()["OBJCOMP"] += out;
	    }
	}
    }
}

void
GNUMakefileGenerator::writeSubDirs(QTextStream &t)
{
    QString qs ;
    QTextStream ts (&qs, IO_WriteOnly) ;
    Win32MakefileGenerator::writeSubDirs( ts ) ;
    QRegExp rx("(\\n\\tcd [^\\n\\t]+)(\\n\\t.+)\\n\\t@cd ..") ;
    rx.setMinimal(TRUE);
    int pos = 0 ;
    while ( -1 != (pos = rx.search( qs, pos)))
    {
	QString qsMatch = rx.cap(2);
	qsMatch.replace("\n\t"," && \\\n\t");
	qs.replace(pos+rx.cap(1).length(), rx.cap(2).length(), qsMatch );
	pos += (rx.cap(1).length()+qsMatch.length());
    }
    t << qs ;
}
