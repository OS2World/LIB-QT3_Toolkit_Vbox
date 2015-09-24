#
# specific stuff for gnu make
#
CXX	    =	g++
CFLAGS	    =	-c -o$@ -O -s -Zomf \
		-I. -Igenerators -Igenerators\unix -Igenerators\win32 -Igenerators\os2 -Igenerators\mac -I..\include -I..\src\tools \
		-I..\mkspecs\os2-g++ \
		-DQT_NO_TEXTCODEC -DQT_LITE_COMPONENT -DQT_NODLL -DQT_NO_STL -DQT_NO_COMPRESS -DHAVE_QCONFIG_CPP
CXXFLAGS    =   $(CFLAGS)
LFLAGS	    =   -s -Zomf -Zstack 0x2000 -Zlinker /PM:VIO -Zlinker /EXEPACK:2
LIBS	    =   -lregistry.dll
LINKQMAKE   =	g++ $(LFLAGS) -o qmake.exe $(OBJS) $(QTOBJS) $(LIBS)
ADDCLEAN    =


#qmake code
OBJS=project.obj main.obj makefile.obj unixmake.obj unixmake2.obj borland_bmake.obj mingw_make.obj gnumake.obj \
    msvc_nmake.obj msvc_dsp.obj msvc_vcproj.obj option.obj winmakefile.obj projectgenerator.obj \
    metrowerks_xml.obj pbuilder_pbx.obj msvc_objectmodel.obj property.obj meta.obj qtmd5.obj

#qt code
QTOBJS=qstring.obj qlocale.obj quuid.obj qtextstream.obj qiodevice.obj qglobal.obj qgdict.obj qcstring.obj qdatastream.obj \
    qgarray.obj qbuffer.obj qglist.obj qptrcollection.obj qfile.obj qfile_pm.obj qregexp.obj qgvector.obj \
    qgcache.obj qbitarray.obj qdir.obj qfileinfo_pm.obj qdir_pm.obj qfileinfo.obj qdatetime.obj \
    qstringlist.obj qmap.obj qlibrary.obj qlibrary_pm.obj qconfig.obj qsettings.obj qsettings_pm.obj qunicodetables.obj



..\bin\qmake.exe: qmake.exe
	copy qmake.exe ..\bin\qmake.exe
	-del qmake.exe >nul 2>&1

qmake.exe: $(OBJS) $(QTOBJS)
	$(LINKQMAKE)

clean::
	-del $(OBJS) $(QTOBJS) $(ADDCLEAN) >nul 2>&1

distclean:: clean
	-del qmake.exe >nul 2>&1

.c.obj:
	$(CXX) $(CFLAGS) $<

.cpp.obj:
	$(CXX) $(CXXFLAGS) $<

qconfig.obj: ..\src\tools\qconfig.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qconfig.cpp

qlibrary.obj: ..\src\tools\qlibrary.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qlibrary.cpp

qlibrary_pm.obj: ..\src\tools\qlibrary_pm.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qlibrary_pm.cpp

qtextstream.obj: ..\src\tools\qtextstream.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qtextstream.cpp

qiodevice.obj: ..\src\tools\qiodevice.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qiodevice.cpp

qglobal.obj: ..\src\tools\qglobal.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qglobal.cpp

qgdict.obj: ..\src\tools\qgdict.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qgdict.cpp

qcstring.obj: ..\src\tools\qcstring.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qcstring.cpp

qstring.obj: ..\src\tools\qstring.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qstring.cpp

qlocale.obj: ..\src\tools\qlocale.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qlocale.cpp

quuid.obj: ..\src\tools\quuid.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\quuid.cpp

qdatastream.obj: ..\src\tools\qdatastream.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qdatastream.cpp

qgarray.obj: ..\src\tools\qgarray.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qgarray.cpp

qbuffer.obj: ..\src\tools\qbuffer.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qbuffer.cpp

qglist.obj: ..\src\tools\qglist.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qglist.cpp

qptrcollection.obj: ..\src\tools\qptrcollection.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qptrcollection.cpp

qfile.obj: ..\src\tools\qfile.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qfile.cpp

qfile_pm.obj: ..\src\tools\qfile_pm.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qfile_pm.cpp

qtextcodec.obj: ..\src\codecs\qtextcodec.cpp
	$(CXX) $(CXXFLAGS) ..\src\codecs\qtextcodec.cpp

qregexp.obj: ..\src\tools\qregexp.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qregexp.cpp

qgvector.obj: ..\src\tools\qgvector.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qgvector.cpp

qgcache.obj: ..\src\tools\qgcache.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qgcache.cpp

qbitarray.obj: ..\src\tools\qbitarray.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qbitarray.cpp

qdir.obj: ..\src\tools\qdir.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qdir.cpp

qsettings.obj: ..\src\tools\qsettings.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qsettings.cpp

qsettings_pm.obj: ..\src\tools\qsettings_pm.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qsettings_pm.cpp

qfileinfo_pm.obj: ..\src\tools\qfileinfo_pm.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qfileinfo_pm.cpp

qdir_pm.obj: ..\src\tools\qdir_pm.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qdir_pm.cpp

qfileinfo.obj: ..\src\tools\qfileinfo.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qfileinfo.cpp

qdatetime.obj: ..\src\tools\qdatetime.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qdatetime.cpp

qstringlist.obj: ..\src\tools\qstringlist.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qstringlist.cpp

qmap.obj: ..\src\tools\qmap.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qmap.cpp

qunicodetables.obj: ..\src\tools\qunicodetables.cpp
	$(CXX) $(CXXFLAGS) ..\src\tools\qunicodetables.cpp

makefile.obj: generators\makefile.cpp
	$(CXX) $(CXXFLAGS) generators\makefile.cpp

unixmake.obj: generators\unix\unixmake.cpp
	$(CXX) $(CXXFLAGS) generators\unix\unixmake.cpp

unixmake2.obj: generators\unix\unixmake2.cpp
	$(CXX) $(CXXFLAGS) generators\unix\unixmake2.cpp

winmakefile.obj: generators/win32/winmakefile.cpp
	$(CXX) $(CXXFLAGS) generators/win32/winmakefile.cpp

borland_bmake.obj: generators/win32/borland_bmake.cpp
	$(CXX) $(CXXFLAGS) generators/win32/borland_bmake.cpp

mingw_make.obj: generators/win32/mingw_make.cpp
	$(CXX) $(CXXFLAGS) generators/win32/mingw_make.cpp

gnumake.obj: generators/os2/gnumake.cpp
	$(CXX) $(CXXFLAGS) generators/os2/gnumake.cpp

msvc_nmake.obj: generators/win32/msvc_nmake.cpp
	$(CXX) $(CXXFLAGS) generators/win32/msvc_nmake.cpp

msvc_dsp.obj: generators/win32/msvc_dsp.cpp
	$(CXX) $(CXXFLAGS) generators/win32/msvc_dsp.cpp

msvc_vcproj.obj: generators/win32/msvc_vcproj.cpp
	$(CXX) $(CXXFLAGS) generators/win32/msvc_vcproj.cpp

msvc_objectmodel.obj: generators/win32/msvc_objectmodel.cpp
	$(CXX) $(CXXFLAGS) generators/win32/msvc_objectmodel.cpp

qtmd5.obj: qtmd5.cpp
	$(CXX) $(CXXFLAGS) qtmd5.cpp

project.obj: project.cpp project.h option.h
	$(CXX) $(CXXFLAGS) project.cpp

meta.obj: meta.cpp project.h option.h
	$(CXX) $(CXXFLAGS) meta.cpp

property.obj: property.cpp property.h option.h
	$(CXX) $(CXXFLAGS) property.cpp

main.obj: main.cpp project.h
	$(CXX) $(CXXFLAGS) main.cpp

option.obj: option.cpp option.h
	$(CXX) $(CXXFLAGS) option.cpp

projectgenerator.obj: generators/projectgenerator.cpp
	$(CXX) $(CXXFLAGS) generators/projectgenerator.cpp

metrowerks_xml.obj: generators/mac/metrowerks_xml.cpp
	$(CXX) $(CXXFLAGS) generators/mac/metrowerks_xml.cpp

pbuilder_pbx.obj: generators/mac/pbuilder_pbx.cpp
	$(CXX) $(CXXFLAGS) generators/mac/pbuilder_pbx.cpp

