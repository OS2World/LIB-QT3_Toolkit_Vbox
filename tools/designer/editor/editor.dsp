# Microsoft Developer Studio Project File - Name="editor" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=editor - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "editor.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "editor.mak" CFG="editor - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "editor - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "editor - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "editor - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "../../../lib"
# PROP BASE Intermediate_Dir "tmp/obj/release-shared-mt"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../../../lib"
# PROP Intermediate_Dir "tmp/obj/release-shared-mt"
# PROP Target_Dir ""
# ADD CPP -MD /W3 /I "..\interfaces" /I "$(QTDIR)\include" /I "." /I "tmp\moc\release-shared-mt" /I "R:\Qt\3.3.x-p8\mkspecs\win32-msvc" /D "WIN32" /D "NDEBUG" /D "_LIB" /D UNICODE /D QT_DLL /D QT_THREAD_SUPPORT /D QT_ACCESSIBILITY_SUPPORT /D QT_TABLET_SUPPORT /D "QT_NO_DEBUG" /FD /c -nologo -Zm200 -GX -GX -GR -O1 
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD LIB32 /nologo /out:"..\..\..\lib\editor.lib" 


!ELSEIF  "$(CFG)" == "editor - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD CPP -MDd /W3 /GZ /ZI /Od /I "..\interfaces" /I "$(QTDIR)\include" /I "." /I "tmp\moc\release-shared-mt" /I "R:\Qt\3.3.x-p8\mkspecs\win32-msvc" /D "WIN32" /D "_DEBUG" /D "_LIB" /D UNICODE /D QT_DLL /D QT_THREAD_SUPPORT /D QT_ACCESSIBILITY_SUPPORT /D QT_TABLET_SUPPORT /FD /c -nologo -Zm200 -GX -GX -GR  -Zi  
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD LIB32 /nologo /out:"..\..\..\lib\editor.lib" 


!ENDIF 

# Begin Target

# Name "editor - Win32 Release"
# Name "editor - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=editor.cpp
# End Source File
# Begin Source File

SOURCE=parenmatcher.cpp
# End Source File
# Begin Source File

SOURCE=completion.cpp
# End Source File
# Begin Source File

SOURCE=viewmanager.cpp
# End Source File
# Begin Source File

SOURCE=markerwidget.cpp
# End Source File
# Begin Source File

SOURCE=conf.cpp
# End Source File
# Begin Source File

SOURCE=browser.cpp
# End Source File
# Begin Source File

SOURCE=arghintwidget.cpp
# End Source File
# Begin Source File

SOURCE=cindent.cpp
# End Source File
# Begin Source File

SOURCE=yyindent.cpp
# End Source File
# Begin Source File

SOURCE=preferences.ui.h
# End Source File

# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=editor.h

USERDEP_editor=""$(QTDIR)\bin\moc.exe""

!IF  "$(CFG)" == "editor - Win32 Release"

# Begin Custom Build - Moc'ing editor.h...
InputPath=.\editor.h


BuildCmds= \
	$(QTDIR)\bin\moc editor.h -o tmp\moc\release-shared-mt\moc_editor.cpp \

"tmp\moc\release-shared-mt\moc_editor.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ELSEIF  "$(CFG)" == "editor - Win32 Debug"

# Begin Custom Build - Moc'ing editor.h...
InputPath=.\editor.h


BuildCmds= \
	$(QTDIR)\bin\moc editor.h -o tmp\moc\release-shared-mt\moc_editor.cpp \

"tmp\moc\release-shared-mt\moc_editor.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=parenmatcher.h

# End Source File
# Begin Source File

SOURCE=completion.h

USERDEP_completion=""$(QTDIR)\bin\moc.exe""

!IF  "$(CFG)" == "editor - Win32 Release"

# Begin Custom Build - Moc'ing completion.h...
InputPath=.\completion.h


BuildCmds= \
	$(QTDIR)\bin\moc completion.h -o tmp\moc\release-shared-mt\moc_completion.cpp \

"tmp\moc\release-shared-mt\moc_completion.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ELSEIF  "$(CFG)" == "editor - Win32 Debug"

# Begin Custom Build - Moc'ing completion.h...
InputPath=.\completion.h


BuildCmds= \
	$(QTDIR)\bin\moc completion.h -o tmp\moc\release-shared-mt\moc_completion.cpp \

"tmp\moc\release-shared-mt\moc_completion.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=viewmanager.h

USERDEP_viewmanager=""$(QTDIR)\bin\moc.exe""

!IF  "$(CFG)" == "editor - Win32 Release"

# Begin Custom Build - Moc'ing viewmanager.h...
InputPath=.\viewmanager.h


BuildCmds= \
	$(QTDIR)\bin\moc viewmanager.h -o tmp\moc\release-shared-mt\moc_viewmanager.cpp \

"tmp\moc\release-shared-mt\moc_viewmanager.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ELSEIF  "$(CFG)" == "editor - Win32 Debug"

# Begin Custom Build - Moc'ing viewmanager.h...
InputPath=.\viewmanager.h


BuildCmds= \
	$(QTDIR)\bin\moc viewmanager.h -o tmp\moc\release-shared-mt\moc_viewmanager.cpp \

"tmp\moc\release-shared-mt\moc_viewmanager.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=markerwidget.h

USERDEP_markerwidget=""$(QTDIR)\bin\moc.exe""

!IF  "$(CFG)" == "editor - Win32 Release"

# Begin Custom Build - Moc'ing markerwidget.h...
InputPath=.\markerwidget.h


BuildCmds= \
	$(QTDIR)\bin\moc markerwidget.h -o tmp\moc\release-shared-mt\moc_markerwidget.cpp \

"tmp\moc\release-shared-mt\moc_markerwidget.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ELSEIF  "$(CFG)" == "editor - Win32 Debug"

# Begin Custom Build - Moc'ing markerwidget.h...
InputPath=.\markerwidget.h


BuildCmds= \
	$(QTDIR)\bin\moc markerwidget.h -o tmp\moc\release-shared-mt\moc_markerwidget.cpp \

"tmp\moc\release-shared-mt\moc_markerwidget.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=conf.h

# End Source File
# Begin Source File

SOURCE=browser.h

USERDEP_browser=""$(QTDIR)\bin\moc.exe""

!IF  "$(CFG)" == "editor - Win32 Release"

# Begin Custom Build - Moc'ing browser.h...
InputPath=.\browser.h


BuildCmds= \
	$(QTDIR)\bin\moc browser.h -o tmp\moc\release-shared-mt\moc_browser.cpp \

"tmp\moc\release-shared-mt\moc_browser.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ELSEIF  "$(CFG)" == "editor - Win32 Debug"

# Begin Custom Build - Moc'ing browser.h...
InputPath=.\browser.h


BuildCmds= \
	$(QTDIR)\bin\moc browser.h -o tmp\moc\release-shared-mt\moc_browser.cpp \

"tmp\moc\release-shared-mt\moc_browser.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=arghintwidget.h

USERDEP_arghintwidget=""$(QTDIR)\bin\moc.exe""

!IF  "$(CFG)" == "editor - Win32 Release"

# Begin Custom Build - Moc'ing arghintwidget.h...
InputPath=.\arghintwidget.h


BuildCmds= \
	$(QTDIR)\bin\moc arghintwidget.h -o tmp\moc\release-shared-mt\moc_arghintwidget.cpp \

"tmp\moc\release-shared-mt\moc_arghintwidget.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ELSEIF  "$(CFG)" == "editor - Win32 Debug"

# Begin Custom Build - Moc'ing arghintwidget.h...
InputPath=.\arghintwidget.h


BuildCmds= \
	$(QTDIR)\bin\moc arghintwidget.h -o tmp\moc\release-shared-mt\moc_arghintwidget.cpp \

"tmp\moc\release-shared-mt\moc_arghintwidget.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=cindent.h

# End Source File

# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group

# Begin Group "Forms"
# Prop Default_Filter "ui"
# Begin Source File

SOURCE=preferences.ui
USERDEP_preferences.ui="$(QTDIR)\bin\moc.exe" "$(QTDIR)\bin\uic.exe"

!IF  "$(CFG)" == "editor - Win32 Release"

# Begin Custom Build - Uic'ing preferences.ui...
InputPath=.\preferences.ui

BuildCmds= \
	$(QTDIR)\bin\uic preferences.ui -o preferences.h \
	$(QTDIR)\bin\uic preferences.ui -i preferences.h -o preferences.cpp \
	$(QTDIR)\bin\moc  preferences.h -o tmp\moc\release-shared-mt\moc_preferences.cpp \

"preferences.h" : "$(SOURCE)" "$(INTDIR)" "$(OUTDIR)"
	$(BuildCmds)

"preferences.cpp" : "$(SOURCE)" "$(INTDIR)" "$(OUTDIR)"
	$(BuildCmds)

"tmp\moc\release-shared-mt\moc_preferences.cpp" : "$(SOURCE)" "$(INTDIR)" "$(OUTDIR)"
	$(BuildCmds)

# End Custom Build

!ELSEIF  "$(CFG)" == "editor - Win32 Debug"

# Begin Custom Build - Uic'ing preferences.ui...
InputPath=.\preferences.ui

BuildCmds= \
	$(QTDIR)\bin\uic preferences.ui -o preferences.h \
	$(QTDIR)\bin\uic preferences.ui -i preferences.h -o preferences.cpp \
	$(QTDIR)\bin\moc  preferences.h -o tmp\moc\release-shared-mt\moc_preferences.cpp \

"preferences.h" : "$(SOURCE)" "$(INTDIR)" "$(OUTDIR)"
	$(BuildCmds)

"preferences.cpp" : "$(SOURCE)" "$(INTDIR)" "$(OUTDIR)"
	$(BuildCmds)

"tmp\moc\release-shared-mt\moc_preferences.cpp" : "$(SOURCE)" "$(INTDIR)" "$(OUTDIR)"
	$(BuildCmds)

# End Custom Build

!ENDIF 

# End Source File

# End Group






# Begin Group "Generated"
# Begin Source File

SOURCE=tmp\moc\release-shared-mt\moc_editor.cpp
# End Source File
# Begin Source File

SOURCE=tmp\moc\release-shared-mt\moc_completion.cpp
# End Source File
# Begin Source File

SOURCE=tmp\moc\release-shared-mt\moc_viewmanager.cpp
# End Source File
# Begin Source File

SOURCE=tmp\moc\release-shared-mt\moc_markerwidget.cpp
# End Source File
# Begin Source File

SOURCE=tmp\moc\release-shared-mt\moc_browser.cpp
# End Source File
# Begin Source File

SOURCE=tmp\moc\release-shared-mt\moc_arghintwidget.cpp
# End Source File
# Begin Source File

SOURCE=tmp\moc\release-shared-mt\moc_preferences.cpp
# End Source File
# Begin Source File

SOURCE=tmp\moc\release-shared-mt\arghintwidget.moc
USERDEP_tmp_moc_release_shared_mt_arghintwidget=".\arghintwidget.cpp" "$(QTDIR)\bin\moc.exe"

!IF  "$(CFG)" == "editor - Win32 Release"

# Begin Custom Build - Moc'ing arghintwidget.cpp...
InputPath=.\tmp\moc\release-shared-mt\arghintwidget.moc

"tmp\moc\release-shared-mt\arghintwidget.moc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(QTDIR)\bin\moc arghintwidget.cpp -o tmp\moc\release-shared-mt\arghintwidget.moc

# End Custom Build

!ELSEIF  "$(CFG)" == "editor - Win32 Debug"

# Begin Custom Build - Moc'ing arghintwidget.cpp...
InputPath=.\tmp\moc\release-shared-mt\arghintwidget.moc

"tmp\moc\release-shared-mt\arghintwidget.moc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(QTDIR)\bin\moc arghintwidget.cpp -o tmp\moc\release-shared-mt\arghintwidget.moc

# End Custom Build

!ENDIF 

# End Source File

# Begin Source File

SOURCE=preferences.cpp
# End Source File

# Begin Source File

SOURCE=preferences.h
# End Source File



# Prop Default_Filter "moc"
# End Group
# End Target
# End Project

