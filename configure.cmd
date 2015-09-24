/* REXX */
/*
 */ G.!Id = '$Id: configure.cmd 207 2011-06-26 12:28:45Z rudi $' /*
 *
 *  :mode=netrexx:tabSize=4:indentSize=4:noTabs=true:
 *  :folding=explicit:collapseFolds=1:
 *
 *  ----------------------------------------------------------------------------
 *
 *	Qt Toolkit Version 3 for OS/2 Configuration Script
 *
 *  Author: Dmitry A. Kuminov
 */

signal on syntax
signal on halt
signal on novalue
trace off
numeric digits 12
'@echo off'

/* globals
 ******************************************************************************/

G.!Title        = 'Qt Toolkit Version 3 for OS/2 Configuration Script',
                  '${revision} [${revision.date} ${revision.time}]'

G.!TAB          = '09'x
G.!EOL          = '0D0A'x

G.!ScreenWidth  = -1
G.!ScreenHeight = -1

/* path to GCC */
Opt.!GCCPath            = ''
Opt.!GCCPath.0          = 0
Opt.!GCCPath.!choice    = ''

/* path to ILINK */
Opt.!ILINKPath          = ''
Opt.!ILINKPath.0        = 0
Opt.!ILINKPath.!choice  = ''

/* path to GNU Make */
Opt.!GNUMakePath            = ''
Opt.!GNUMakePath.0          = 0
Opt.!GNUMakePath.!choice    = ''

/* linker selection */
Opt.!UseWlink   = 1

/* library version */
Opt.!RELEASE    = 1
Opt.!DLL        = 1
Opt.!CustomDLLName = 'myqt'
/* optional module configuration */
Opt.!NETWORK    = 1
Opt.!WORKSPACE  = 1
Opt.!ICONVIEW   = 1
Opt.!TABLE      = 1
Opt.!CANVAS     = 1
Opt.!XML        = 1
Opt.!SQL        = 0
/* third party features */
Opt.!ZLIB       = 1
Opt.!JPEG       = 1
Opt.!PNG        = 1
Opt.!MNG        = 1
Opt.!GIF        = 1

/* extra OS/2 specific features */
/* @todo (r=dmik) commented out until there is a GCC compatible FT2 support lib
Opt.!Extra.!FT2LIB              = ''
Opt.!Extra.!FT2LIB.0            = 0
Opt.!Extra.!FT2LIB.!choice      = ''
Opt.!Extra.!FT2LIB.!enabled     = ''
*/

Opt.!QTCustomDefines  = ''

/*
 *  Defines to disable various Qt features that haven't been implemented
 *  yet on OS/2. NOTE: don't touch this variable!
 */
G.!QTMandatoryDefines  = 'QT_NO_PRINTER QT_NO_IPV6'

/* Configuration cache file */
G.!ConfigCache      = '.configure.cache'
/* Configure Compiler targets */
G.!QtCmd            = 'qt.cmd'
/* Configure Qt targets */
G.!QMakeCache       = '.qmake.cache'
G.!QtOS2Config      = '.qtos2config'
G.!QtBuild          = '.qtbuild'
G.!QModulesH        = 'include\qmodules.h'
G.!QConfigH         = 'include\qconfig.h'
G.!QConfigCPP       = 'src\tools\qconfig.cpp'
G.!QMakeMakefile    = 'qmake\Makefile'
G.!Makefile         = 'Makefile'

/* all globals to be exposed in procedures */
Globals = 'G. Opt. Static.'

/* init rexx lib
 ******************************************************************************/

if (RxFuncQuery('SysLoadFuncs')) then do
    call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
    call SysLoadFuncs
end

/* startup code
 ******************************************************************************/

parse value SysTextScreenSize() with G.!ScreenHeight G.!ScreenWidth
if (G.!ScreenHeight < 25 | G.!ScreenWidth < 80) then do
    address 'cmd' 'mode co80,25'
    parse value SysTextScreenSize() with G.!ScreenHeight G.!ScreenWidth
    if (G.!ScreenHeight < 25 | G.!ScreenWidth < 80) then do
        call SayErr 'WARNING: Cannot set screen size to 80 x 25!'
        call SayErr 'Some messages can be unreadable.'
        say
        call WaitForAnyKey
    end
end

call MagicCmdHook arg(1)
call MagicLogHook arg(1)

parse source . . G.!ScriptFile
G.!ScriptDir = FixDir(filespec('D', G.!ScriptFile) || filespec('P', G.!ScriptFile))

parse value strip(G.!Id,,'$') with,
    'Id: '.' 'G.!Revision' 'G.!RevisionDate' 'G.!RevisionTime' '.
G.!Title = Replace(G.!Title, '${revision}', G.!Revision)
G.!Title = Replace(G.!Title, '${revision.date}', G.!RevisionDate)
G.!Title = Replace(G.!Title, '${revision.time}', G.!RevisionTime)

call Main

/* termination code
 ******************************************************************************/

call Done 0

/* functions
 ******************************************************************************/

/**
 *  Just do the job.
 */
Main: procedure expose (Globals)

    say
    say
    call SaySay 'Qt Toolkit Version 3 for OS/2 Configuration Script'
    call SaySay '=================================================='
    say
    call SaySay 'This script will interactively configure your installation of the'
    call SaySay 'Qt Library for OS/2. It assumes that you have unzipped the entire'
    call SaySay 'contents of the distribution archive to the following directory:'
    say
    call SaySay ''''G.!ScriptDir''''
    say
    call SaySay 'You can cancel the script execution at any time by pressing the'
    call SaySay 'CTRL-BREAK key combination.'
    say

    /* Qt directory */
    G.!QTDir = G.!ScriptDir
    G.!QTDirP = FixDirNoSlash(G.!QTDir)

    /* Fix paths */
    G.!ConfigCache = G.!QTDirP'\'G.!ConfigCache
    G.!QtCmd = G.!QTDirP'\'G.!QtCmd
    G.!QMakeCache = G.!QTDirP'\'G.!QMakeCache
    G.!QtOS2Config = G.!QTDirP'\'G.!QtOS2Config
    G.!QtBuild = G.!QTDirP'\'G.!QtBuild
    G.!QModulesH = G.!QTDirP'\'G.!QModulesH
    G.!QConfigH = G.!QTDirP'\'G.!QConfigH
    G.!QConfigCPP = G.!QTDirP'\'G.!QConfigCPP
    G.!QMakeMakefile = G.!QTDirP'\'G.!QMakeMakefile
    G.!Makefile = G.!QTDirP'\'G.!Makefile

    /* Create the \bin subdirectory */
    if (\DirExists(G.!QTDirP'\bin')) then call MakeDir G.!QTDirP'\bin'

    /* Read the configuration cache */
    call ReadConfigCache

    call WaitForAnyKey

    G.!FirstRun = \FileExists(G.!ConfigCache)

    G.!DidConfigureCompiler = 0
    G.!DidConfigureQt = 0
    G.!DidCompileQt = 0

    if (G.!FirstRun | \FileExists(G.!QtCmd)) then call ConfigureCompiler

    if (G.!FirstRun |,
        \FileExists(G.!QMakeCache) | \FileExists(G.!QtOS2Config) |,
        \FileExists(G.!QModulesH) | \FileExists(G.!QConfigH) |,
        \FileExists(G.!QConfigCPP) |,
        \FileExists(G.!QMakeMakefile) | \FileExists(G.!Makefile)) then
        call ConfigureQt

    if (G.!FirstRun) then call CompileQt

    if (\G.!DidConfigureCompiler | \G.!DidConfigureQt | \G.!DidCompileQt) then
    do forever
        G.!Choice.0 = 4
        G.!Choice.1 = 'Configure the compiler'
        G.!Choice.2 = 'Configure the Qt Library'
        G.!Choice.3 = 'Compile the Qt Library'
        G.!Choice.4 = 'Nothing, thanks'
        choice = GetChoice('Select what to do:', 'G.!Choice')
        select
            when choice == 1 then call ConfigureCompiler
            when choice == 2 then do
                call ConfigureQt
                if (G.!DidConfigureQt) then do
                    call SaySay 'It is strongly recommended to recompile',
                                'the Qt library'
                    call SaySay 'after it has been (re)configured.'
                    if (GetYesNo('Procceed to compile?', 'Y')) then do
                        call CompileQt
                        leave
                    end
                end
            end
            when choice == 3 then do
                call CompileQt
                leave
            end
            otherwise leave
        end
    end

    call SaySay 'All done.'
    say

    if (G.!DidConfigureCompiler | G.!DidCompileQt) then do
        call SaySay 'You can now put the generated '''filespec('N', G.!QtCmd)'''',
                    'script to one'
        call SaySay 'of the directories listed in your %PATH% environment'
        call SaySay 'variable and use it as a command line wrapper to setup the'
        call SaySay 'environment necessary to work with the Qt Toolkit for OS/2:'
        call SaySay 'to rebuild the library, to run examples and tutorials, to'
        call SaySay 'compile your own projects.'
        say
        call SaySay 'The script was generated in the following directory:'
        call SaySay ''''FixDir(filespec('D', G.!QtCmd)||filespec('P', G.!QtCmd))''''
        say
    end

    return

/** Writes out the given global variable to the given file */
WriteVar: procedure expose (Globals)
    parse arg file, variable
    call lineout file, variable||' = '''||value(variable)''''
    return

/** Writes out the given global stem to the given file */
WriteStem: procedure expose (Globals)
    parse arg file, stem
    call WriteVar file, stem'.0'
    do n = 1 to value(stem'.0')
        call WriteVar file, stem'.'n
    end
    return

/**
 */
WriteConfigCache: procedure expose (Globals)

    call DeleteFile G.!ConfigCache

    call WriteStem G.!ConfigCache, 'Opt.!GCCPath'
    call WriteVar G.!ConfigCache, 'Opt.!GCCPath.!choice'

    call WriteStem G.!ConfigCache, 'Opt.!ILINKPath'
    call WriteVar G.!ConfigCache, 'Opt.!ILINKPath.!choice'

    call WriteStem G.!ConfigCache, 'Opt.!GNUMakePath'
    call WriteVar G.!ConfigCache, 'Opt.!GNUMakePath.!choice'

    call WriteVar G.!ConfigCache, 'Opt.!UseWlink'

    call WriteVar G.!ConfigCache, 'Opt.!RELEASE'
    call WriteVar G.!ConfigCache, 'Opt.!DLL'
    call WriteVar G.!ConfigCache, 'Opt.!CustomDLLName'

    call WriteVar G.!ConfigCache, 'Opt.!NETWORK'
    call WriteVar G.!ConfigCache, 'Opt.!WORKSPACE'
    call WriteVar G.!ConfigCache, 'Opt.!ICONVIEW'
    call WriteVar G.!ConfigCache, 'Opt.!TABLE'
    call WriteVar G.!ConfigCache, 'Opt.!CANVAS'
    call WriteVar G.!ConfigCache, 'Opt.!XML'
    call WriteVar G.!ConfigCache, 'Opt.!SQL'

    call WriteVar G.!ConfigCache, 'Opt.!ZLIB'
    call WriteVar G.!ConfigCache, 'Opt.!JPEG'
    call WriteVar G.!ConfigCache, 'Opt.!PNG'
    call WriteVar G.!ConfigCache, 'Opt.!MNG'
    call WriteVar G.!ConfigCache, 'Opt.!GIF'

/* @todo (r=dmik) commented out until there is a GCC compatible FT2 support lib
    call WriteStem G.!ConfigCache, 'Opt.!Extra.!FT2LIB'
    call WriteVar G.!ConfigCache, 'Opt.!Extra.!FT2LIB.!choice'
    call WriteVar G.!ConfigCache, 'Opt.!Extra.!FT2LIB.!enabled'
*/
    call WriteVar G.!ConfigCache, 'Opt.!QTCustomDefines'

    call lineout G.!ConfigCache

    return

/**
 */
ReadConfigCache: procedure expose (Globals)

    signal on syntax name ReadConfigCacheError
    signal on novalue name ReadConfigCacheError

    do while lines(G.!ConfigCache)
        line = linein(G.!ConfigCache)
        interpret line
    end

    signal on syntax
    signal on novalue

    call stream G.!ConfigCache, 'C', 'CLOSE'

    Opt.!GCCPath.!changed = 0
    Opt.!ILINKPath.!changed = 0
    Opt.!GNUMAKEPath.!changed = 0

/* @todo (r=dmik) commented out until there is a GCC compatible FT2 support lib
    Opt.!Extra.!FT2LIB.!changed = 0
*/
    return

ReadConfigCacheError:
    signal on syntax
    signal on novalue
    call SayErr 'FATAL: Error reading '''G.!ConfigCache'''!'
    call SayErr 'Please remove this file and restart the script.'
    call Done 1

/**
 */
ConfigureCompiler: procedure expose (Globals)

    call SaySay 'Configure the compiler'
    call SaySay '----------------------'
    say
    call SaySay 'Currently, Qt/OS2 supports the Innotek GCC compilers 3.2.2 and 3.3.5'
    call SaySay 'used together with Watcom WL.EXE or IBM ILINK 3.08 to link native OMF '
    call SaySay 'object files generated by GCC into OS/2 executables.'
    call SaySay 'When using the Watcom linker, the LXLite tool is needed as well.'
    say
    call SaySay 'Also, the GNU Make utility version 3.81beta1 or newer is required.'
    say

    do until (Opt.!GCCPath \= '')
        Opt.!GCCPath = MenuSelectPath(,
            'Opt.!GCCPath',,
            'Select where to search for a GCC installation:',,
            'bin\gcc.exe', 'CheckGCCPath',  'CheckGCCVersion',,
            'ERROR: Could not find a valid GCC installation'||G.!EOL||,
            'in ''%1''!'||G.!EOL||,
            'Please read the documentation.')
    end
    call WriteConfigCache

    G.!Choices.0 = 2
    G.!Choices.1 = 'Patched OpenWatcom linker from Netlabs.org'
    G.!Choices.2 = 'IBM linker from VisualAge C++ 3.08'
    choice = GetChoice('Select the linker type:', 'G.!Choices', -Opt.!UseWlink + 2)
    Opt.!UseWlink = -choice + 2

    if Opt.!UseWlink = 0 then do
        do until (Opt.!ILINKPath \= '')
            Opt.!ILINKPath = MenuSelectPath(,
                'Opt.!ILINKPath',,
                'Select where to search for an ILINK installation:',,
                'ilink.exe', 'CheckILINKPath',  'CheckILINKVersion',,
                'ERROR: Could not find a valid ILINK installation'||G.!EOL||,
                'in ''%1''!'||G.!EOL||,
                'Please read the documentation.')
        end
    end

    call WriteConfigCache

    do until (Opt.!GNUMakePath \= '')
        Opt.!GNUMakePath = MenuSelectPath(,
            'Opt.!GNUMakePath',,
            'Select where to search for the GNU Make utility:',,
            'make.exe', 'CheckGNUMakePath',  'CheckGNUMakeVersion',,
            'ERROR: Could not find a GNU Make utility'||G.!EOL||,
            'in ''%1''!'||G.!EOL||,
            'Please read the documentation.')
    end
    call WriteConfigCache

    call SaySay 'You have selected the following compiler configuration:'
    say
    call SaySay 'Compiler     : Innotek GCC ('Opt.!GCCPath')'
    if Opt.!UseWlink = 0 then
        call SaySay 'Linker       : IBM ILINK ('Opt.!ILINKPath')'
    else
        call SaySay 'Linker       : OpenWatcom linker in GCC path'
    call SaySay 'Make Utility : GNU Make ('Opt.!GNUMakePath')'
    say

    call WaitForAnyKey

    call DeleteFile G.!QtCmd

    call lineout G.!QtCmd, '/*REXX*/'
    call lineout G.!QtCmd, '/*'
    call lineout G.!QtCmd, ' *  Command line wrapper to set up the Qt environment'
    call lineout G.!QtCmd, ' *  Generated on' date() time()
    call lineout G.!QtCmd, ' */'
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, 'QMAKESPEC   = ''os2-g++'''
    call lineout G.!QtCmd, 'QTDIR       = '''QuotePath(G.!QTDir)''''
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, 'GCC_PATH        = '''QuotePath(FixDirNoSlash(Opt.!GCCPath))''''
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, 'trace off'
    call lineout G.!QtCmd, 'address ''cmd'''
    call lineout G.!QtCmd, '''@echo off'''
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, 'if (RxFuncQuery(''SysBootDrive'')) then do'
    call lineout G.!QtCmd, '    call RxFuncAdd ''SysBootDrive'', ''RexxUtil'', ''SysBootDrive'''
    call lineout G.!QtCmd, '    call RxFuncAdd ''SysSetPriority'', ''RexxUtil'', ''SysSetPriority'''
    call lineout G.!QtCmd, 'end'
    call lineout G.!QtCmd, ''
    /*
     *  determine whether to setup the environment for the debug version of Qt
     *  or for the release one
     */
    call lineout G.!QtCmd, '/* determine the default DEBUG value using .qtos2config */'
    call lineout G.!QtCmd, 'DEBUG = 0'
    call lineout G.!QtCmd, 'do while (lines(QTDIR''\.qtos2config'') > 0)'
    call lineout G.!QtCmd, '    line = strip(linein(QTDIR''\.qtos2config''))'
    call lineout G.!QtCmd, '    if (word(line, 1) == ''CONFIG'') then'
    call lineout G.!QtCmd, '        if (wordpos(''debug'', line, 3) > 0) then DEBUG = 1'
    call lineout G.!QtCmd, 'end'
    call lineout G.!QtCmd, 'call lineout QTDIR''\.qtos2config'''
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, '/* parse command line */'
    call lineout G.!QtCmd, 'parse arg arg1 args'
    call lineout G.!QtCmd, 'arg1 = translate(arg1)'
    call lineout G.!QtCmd, 'if (arg1 == ''D'' | arg1 == ''DEBUG'') then DEBUG = 1'
    call lineout G.!QtCmd, 'else if (arg1 == ''R'' | arg1 == ''RELEASE'') then DEBUG = 0'
    call lineout G.!QtCmd, 'else parse arg args'
    call lineout G.!QtCmd, ''
    /*
     *  setup GCC before everything else, to prevent it from possible reordering
     *  of some essential paths
     */
    call lineout G.!QtCmd, '/* setup the GCC environment */'
    if Opt.!UseWlink then
        call lineout G.!QtCmd, '''call'' GCC_PATH''\bin\gccenv.cmd'' GCC_PATH ''WLINK'''
    else
        call lineout G.!QtCmd, '''call'' GCC_PATH''\bin\gccenv.cmd'' GCC_PATH ''VAC308'''
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, 'call value ''QMAKESPEC'', QMAKESPEC, ''OS2ENVIRONMENT'''
    call lineout G.!QtCmd, 'call value ''QTDIR'', QTDIR, ''OS2ENVIRONMENT'''
    call lineout G.!QtCmd, ''
    /*
     *  place debug directories first in the PATH if in debug mode
     */
    call lineout G.!QtCmd, 'if (\DEBUG) then do'
    call lineout G.!QtCmd, '    /* releae versions come first */'
    call lineout G.!QtCmd, '    call AddPathEnv ''PATH'', QTDIR''\bin'', ''A'''
    call lineout G.!QtCmd, '    call AddPathEnv ''PATH'', QTDIR''\bin\debug'', ''A'''
    call lineout G.!QtCmd, '    call AddPathEnv ''BEGINLIBPATH'', QTDIR''\bin'', ''A'''
    call lineout G.!QtCmd, '    call AddPathEnv ''BEGINLIBPATH'', QTDIR''\bin\debug'', ''A'''
    call lineout G.!QtCmd, 'end'
    call lineout G.!QtCmd, 'else do'
    call lineout G.!QtCmd, '    /* debug versions come first */'
    call lineout G.!QtCmd, '    call AddPathEnv ''PATH'', QTDIR''\bin\debug'', ''A'''
    call lineout G.!QtCmd, '    call AddPathEnv ''PATH'', QTDIR''\bin'', ''A'''
    call lineout G.!QtCmd, '    call AddPathEnv ''BEGINLIBPATH'', QTDIR''\bin\debug'', ''A'''
    call lineout G.!QtCmd, '    call AddPathEnv ''BEGINLIBPATH'', QTDIR''\bin'', ''A'''
    call lineout G.!QtCmd, 'end'
    call lineout G.!QtCmd, ''
    /*
     *  protect against the "4OS2 & forward slash" problem
     */
    call lineout G.!QtCmd, 'call value ''MAKESHELL'', SysBootDrive()''\os2\cmd.exe'', ''OS2ENVIRONMENT'''
    call lineout G.!QtCmd, ''
    /*
     *  the order of variables below is important! GCCPath must come first
     *  in order to appear last in PATH (to ensure that, on the contrary, paths
     *  to the linker and to the make tool choosen by the user are always at
     *  the beginning)
     */
    if Opt.!UseWlink = 0 then do
        call ConfigureCompiler_GenAddPathEnv 'PATH',,
            Opt.!GCCPath.!path';'Opt.!ILINKPath.!path';'Opt.!GNUMAKEPath.!path
        call ConfigureCompiler_GenAddPathEnv 'BEGINLIBPATH',,
            Opt.!GCCPath.!libpath';'Opt.!ILINKPath.!libpath';'Opt.!GNUMAKEPath.!libpath
    end
    else do
        call ConfigureCompiler_GenAddPathEnv 'PATH',,
            Opt.!GCCPath.!path';'Opt.!GNUMAKEPath.!path
        call ConfigureCompiler_GenAddPathEnv 'BEGINLIBPATH',,
            Opt.!GCCPath.!libpath';'Opt.!GNUMAKEPath.!libpath
    end

    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, '/*'
    call lineout G.!QtCmd, ' * set this to allow for direct linking with DLLs'
    call lineout G.!QtCmd, ' * without first making import libraries'
    call lineout G.!QtCmd, ' */'
    call lineout G.!QtCmd, 'call AddPathEnv ''LIBRARY_PATH'', SysBootDrive()''\OS2\DLL'''
    call lineout G.!QtCmd, 'call AddPathEnv ''LIBRARY_PATH'', SysBootDrive()''\MPTN\DLL'''
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, '/*'
    call lineout G.!QtCmd, ' * set our our priority class to IDLE so prevent GCC from eating'
    call lineout G.!QtCmd, ' * 100% of CPU load on single-processor machines'
    call lineout G.!QtCmd, ' */'
    call lineout G.!QtCmd, 'call SysSetPriority 1, 0'
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, '/* pass arguments to the command shell */'
    call lineout G.!QtCmd, '''cmd /c'' args'
    call lineout G.!QtCmd, 'exit rc'
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd, '/* some useful stuff */'
    call lineout G.!QtCmd, ''

    /* copy some function definitions */
    call stream G.!ScriptFile, 'C', 'OPEN READ'
    exportLine = 0
    do while lines(G.!ScriptFile)
        line = linein(G.!ScriptFile)
        if (left(line, 6) == '/* <<<') then exportLine = 1
        else if (left(line, 6) == '/* >>>') then exportLine = 0
        else if exportLine then call lineout G.!QtCmd, line
    end
    call stream G.!ScriptFile, 'C', 'CLOSE'

    call lineout G.!QtCmd, ''
    call lineout G.!QtCmd

    call SaySay 'The compiler has been configured.'
    say

    G.!DidConfigureCompiler = 1
    return

/**
 */
ConfigureCompiler_GenAddPathEnv: procedure expose (Globals)
    parse arg env, paths
    G.!temp = ''
    /* first pass: remove dups */
    do while length(paths) > 0
        parse var paths path';'paths
        if (path \== '') then
            call AddPathVar 'G.!temp', path, 'P'
    end
    /* second pass: generate AddPathEnv calls */
    paths = G.!temp
    do while length(paths) > 0
        parse var paths path';'paths
        if (path \== '') then
            call lineout G.!QtCmd, 'call AddPathEnv '''env''','''QuotePath(path)''', ''P'''
    end
    return

/**
 */
ConfigureQt: procedure expose (Globals)

    call SaySay 'Configure Qt Library'
    call SaySay '--------------------'
    say

    /* Qt library type */

    G.!Choices.0 = 2
    G.!Choices.1 = 'Dynamic link library (DLL)'
    G.!Choices.2 = 'Static library'
    choice = GetChoice('Select the Qt library type:', 'G.!Choices', -Opt.!DLL + 2)
    Opt.!DLL = -choice + 2

    OfficialBuildConfig = ''

    if (Opt.!DLL == 1) then do
        call SaySay 'You are going to build Qt as a dynamic link library.  In order'
        call SaySay 'to prevent possible DLL naming conflicts (a.k.a. DLL hell) with'
        call SaySay 'the official binary distribution of Qt and with other custom'
        call SaySay 'builds, please personalize your version of the DLL by entering'
        call SaySay 'a unique base name for it (without extension).  Note that it'
        call SaySay 'can only consist of latin letters A to Z, numbers, a dash and'
        call SaySay 'an underscore, and its length cannot exceed 8 characters.  Also,'
        call SaySay 'this name cannot start with ''qt''.'
        say
        name = Opt.!CustomDLLName
        do forever
            name = InputLine('Enter a base name for the Qt DLL:', name)
            if (name == 'qt_official_build') then do
                /* magic CONFIG keyword to mark the Qt build as official */
                OfficialBuildConfig = 'qt_official_build '
                leave
            end
            upperName = translate(name)
            if (upperName \= '' & length(upperName) <= 8 &,
                verify(upperName,'ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-_') == 0 &,
                left(upperName, 2) \= 'QT') then do
                Opt.!CustomDLLName = name
                call DeleteFile G.!QtBuild
                leave
            end
            call SayErr 'The entered name is not valid.'
            say
        end
    end

    /* Qt library debug level */

    G.!Choices.0 = 2
    G.!Choices.1 = 'Release (no debug information)'
    G.!Choices.2 = 'Debug'
    choice = GetChoice('Select the Qt library debug level:', 'G.!Choices', -Opt.!RELEASE + 2)
    Opt.!RELEASE = -choice + 2

    /* Optional Qt modules */

    G.!Choices.0 = 7
    do forever
        G.!Choices.1 = 'Network     'GetEnabledText(Opt.!NETWORK)
        G.!Choices.2 = 'Workspace   'GetEnabledText(Opt.!WORKSPACE)
        G.!Choices.3 = 'Iconview    'GetEnabledText(Opt.!ICONVIEW)
        G.!Choices.4 = 'Table       'GetEnabledText(Opt.!TABLE)
        G.!Choices.5 = 'Canvas      'GetEnabledText(Opt.!CANVAS)
        G.!Choices.6 = 'XML         'GetEnabledText(Opt.!XML)
        G.!Choices.7 = 'SQL         'GetEnabledText(Opt.!SQL)
        call SaySay 'Enable or disable optional Qt library modules'
        choice = GetChoice('(pressing Enter will accept the current selection):',,
                           'G.!Choices',, 'E')
        select
            when choice == 1 then Opt.!NETWORK = \Opt.!NETWORK
            when choice == 2 then Opt.!WORKSPACE = \Opt.!WORKSPACE
            when choice == 3 then Opt.!ICONVIEW = \Opt.!ICONVIEW
            when choice == 4 then Opt.!TABLE = \Opt.!TABLE
            when choice == 5 then Opt.!CANVAS = \Opt.!CANVAS
            when choice == 6 then Opt.!XML = \Opt.!XML
            when choice == 7 then Opt.!SQL = \Opt.!SQL
            otherwise leave
        end
    end

    /* InnoTek Font Engine Support */

/* @todo (r=dmik) commented out until there is a GCC compatible FT2 support lib
    do forever
        call SaySay "Enable InnoTek Font Engine support in Qt?"
        Opt.!Extra.!FT2LIB.!enabled =,
            GetYesNo("(answering Y (yes) is recommended)",,
                     Opt.!Extra.!FT2LIB.!enabled)
        if (Opt.!Extra.!FT2LIB.!enabled) then do
            path = MenuSelectPath(,
                'Opt.!Extra.!FT2LIB',,
                'Select where to search for the Innotek Font Engine support library:',,
                'ft2lib.h', 'CheckFT2LIBPath', 'CheckFT2LIBVersion',,
                'ERROR: Could not find the Innotek Font Engine support library'||G.!EOL||,
                'in ''%1''!'||G.!EOL||,
                'Please read the documentation.',,
                'C')
            if (path \== '') then do
                Opt.!Extra.!FT2LIB = path
                leave
            end
            Opt.!Extra.!FT2LIB.!enabled = 0
        end
        else leave
    end

    call WriteConfigCache
*/
    /* Custom Qt defines */

    if (OfficialBuildConfig == '') then do
        call SaySay 'Enter a list of custom Qt defines separated by spaces'
        Opt.!QTCustomDefines =,
            InputLine('(if you don''t know what it is, just press Enter):',,
                      Opt.!QTCustomDefines)
        Opt.!QTCustomDefines = strip(Opt.!QTCustomDefines)
    end

    call WriteConfigCache

    /* Generate setup files for Qt */

    BasicModules = 'kernel tools widgets dialogs styles'
    BasicModulesHeader =,
        '// These modules are present in this configuration of Qt'G.!EOL||,
        '#define QT_MODULE_KERNEL'G.!EOL||,
        '#define QT_MODULE_TOOLS'G.!EOL||,
        '#define QT_MODULE_WIDGETS'G.!EOL||,
        '#define QT_MODULE_DIALOGS'G.!EOL||,
        '#define QT_MODULE_STYLES'G.!EOL

    if (Opt.!RELEASE == 1) then ReleaseOrDebug = 'release'
    else ReleaseOrDebug = 'debug'

    if (Opt.!DLL == 1) then do
        SharedOrStatic = 'shared'
        DllOrStatic = 'shared (dll)'
        SharedOrNot = 'shared'
    end
    else do
        SharedOrStatic = 'static'
        DllOrStatic = 'static'
        SharedOrNot = ''
    end

    OptionalModules = ''
    ModulesHeader = BasicModulesHeader
    if (Opt.!NETWORK == 1) then do
        OptionalModules = OptionalModules 'network'
        ModulesHeader = ModulesHeader || '#define QT_MODULE_NETWORK'G.!EOL
    end
    if (Opt.!WORKSPACE == 1) then do
        OptionalModules = OptionalModules 'workspace'
        ModulesHeader = ModulesHeader || '#define QT_MODULE_WORKSPACE'G.!EOL
    end
    if (Opt.!ICONVIEW == 1) then do
        OptionalModules = OptionalModules 'iconview'
        ModulesHeader = ModulesHeader || '#define QT_MODULE_ICONVIEW'G.!EOL
    end
    if (Opt.!TABLE == 1) then do
        OptionalModules = OptionalModules 'table'
        ModulesHeader = ModulesHeader || '#define QT_MODULE_TABLE'G.!EOL
    end
    if (Opt.!CANVAS == 1) then do
        OptionalModules = OptionalModules 'canvas'
        ModulesHeader = ModulesHeader || '#define QT_MODULE_CANVAS'G.!EOL
    end
    if (Opt.!XML == 1) then do
        OptionalModules = OptionalModules 'xml'
        ModulesHeader = ModulesHeader || '#define QT_MODULE_XML'G.!EOL
    end
    if (Opt.!SQL == 1) then do
        OptionalModules = OptionalModules 'sql'
        ModulesHeader = ModulesHeader || '#define QT_MODULE_SQL'G.!EOL
    end
    OptionalModules = strip(OptionalModules)

    OptionalConfig = ''
    if (Opt.!GIF == 1) then do
        OptionalConfig = OptionalConfig 'gif'
    end
    else do
        OptionalConfig = OptionalConfig 'no-gif'
    end
    if (Opt.!ZLIB == 1) then do
        OptionalConfig = OptionalConfig 'zlib'
    end
    if (Opt.!JPEG == 1) then do
        OptionalConfig = OptionalConfig 'jpeg'
    end
    if (Opt.!PNG == 1) then do
        OptionalConfig = OptionalConfig 'png'
    end
    if (Opt.!MNG == 1) then do
        OptionalConfig = OptionalConfig 'mng'
    end
    OptionalConfig = strip(OptionalConfig)

    ExtraConfig = ''
/* @todo (r=dmik) commented out until there is a GCC compatible FT2 support lib
    if (Opt.!Extra.!FT2LIB.!enabled) then do
        ExtraConfig = ExtraConfig 'pm_ft2lib'
    end
*/
    ExtraConfig = strip(ExtraConfig)

    ConfigHeader =,
        '/* Everything */'G.!EOL||,
        ''G.!EOL||,
        '/* License information */'G.!EOL||,
        '#define QT_PRODUCT_LICENSEE "Free"'G.!EOL||,
        '#define QT_PRODUCT_LICENSE "qt-free"'G.!EOL||,
        ''G.!EOL||,
        '/* Machine byte-order */'G.!EOL||,
        '#define Q_BIG_ENDIAN 4321'G.!EOL||,
        '#define Q_LITTLE_ENDIAN 1234'G.!EOL||,
        '#define Q_BYTE_ORDER Q_LITTLE_ENDIAN'G.!EOL||,
        ''G.!EOL||,
        '/* Compile time features */'G.!EOL||,
        '#ifndef QT_NO_STL'G.!EOL||,
        '#define QT_NO_STL'G.!EOL||,
        '#endif'G.!EOL||,
        '#ifndef QT_NO_STYLE_WINDOWSXP'G.!EOL||,
        '#define QT_NO_STYLE_WINDOWSXP'G.!EOL||,
        '#endif'G.!EOL

    ConfigSource =,
        '#include <qglobal.h>'G.!EOL||,
        ''G.!EOL||,
        '/* Install paths from configure */'G.!EOL||,
        'static const char QT_INSTALL_PREFIX [267] = "qt_nstpath='CPPPath(G.!QTDir)'";'G.!EOL||,
        'static const char QT_INSTALL_BINS   [267] = "qt_binpath='CPPPath(G.!QTDirP)'\\bin";'G.!EOL||,
        'static const char QT_INSTALL_DOCS   [267] = "qt_docpath='CPPPath(G.!QTDirP)'\\doc";'G.!EOL||,
        'static const char QT_INSTALL_HEADERS[267] = "qt_hdrpath='CPPPath(G.!QTDirP)'\\include";'G.!EOL||,
        'static const char QT_INSTALL_LIBS   [267] = "qt_libpath='CPPPath(G.!QTDirP)'\\lib";'G.!EOL||,
        'static const char QT_INSTALL_PLUGINS[267] = "qt_plgpath='CPPPath(G.!QTDirP)'\\plugins";'G.!EOL||,
        'static const char QT_INSTALL_DATA   [267] = "qt_datpath='CPPPath(G.!QTDir)'";'G.!EOL||,
        'static const char QT_INSTALL_TRANSLATIONS [267] = "qt_trnpath='CPPPath(G.!QTDirP)'\\translations";'G.!EOL||,
        ''G.!EOL||,
        '/* strlen( "qt_xxxpath=" ) == 11 */'G.!EOL||,
        'const char *qInstallPath()        { return QT_INSTALL_PREFIX  + 11; }'G.!EOL||,
        'const char *qInstallPathDocs()    { return QT_INSTALL_DOCS    + 11; }'G.!EOL||,
        'const char *qInstallPathHeaders() { return QT_INSTALL_HEADERS + 11; }'G.!EOL||,
        'const char *qInstallPathLibs()    { return QT_INSTALL_LIBS    + 11; }'G.!EOL||,
        'const char *qInstallPathBins()    { return QT_INSTALL_BINS    + 11; }'G.!EOL||,
        'const char *qInstallPathPlugins() { return QT_INSTALL_PLUGINS + 11; }'G.!EOL||,
        'const char *qInstallPathData()    { return QT_INSTALL_DATA    + 11; }'G.!EOL||,
        'const char *qInstallPathTranslations() { return QT_INSTALL_TRANSLATIONS + 11; }'G.!EOL||,
        'const char *qInstallPathSysconf() { return 0; }'G.!EOL

    call SaySay 'You have selected the following compiler configuration:'
    say
    call SaySay 'Library type   : 'DllOrStatic', 'ReleaseOrDebug', multithreaded'
    if (Opt.!DLL == 1) then do
        if (OfficialBuildConfig == '') then
            call SaySay 'Library name   : 'Opt.!CustomDLLName
        else
            call SaySay 'Library name   : qtXYZNN (official build)'
    end
    call SaySay 'Modules        : 'BasicModules OptionalModules
    call SaySay 'Other features : 'OptionalConfig
    if (Opt.!QTCustomDefines \= '') then
        call SaySay 'Custom defines : 'Opt.!QTCustomDefines
    say
/* @todo (r=dmik) commented out until there is a GCC compatible FT2 support lib
    call SaySay 'Innotek Font Engine support  : 'GetYesNoText(Opt.!Extra.!FT2LIB.!enabled)
    say
*/
    call WaitForAnyKey

    call SaySay 'Please wait...'
    say

    qmake_cache = G.!QMakeCache
    qtos2config = G.!QtOS2Config
    qmodules_h = G.!QModulesH
    qconfig_h = G.!QConfigH
    qconfig_cpp = G.!QConfigCPP

    /* do the configure Qt job */

    call DeleteFile qmake_cache
    call DeleteFile qtos2config

    call lineout qmake_cache, 'OBJECTS_DIR=tmp\obj\'ReleaseOrDebug'_mt_'SharedOrStatic
    call lineout qmake_cache, 'MOC_DIR=tmp\moc\'ReleaseOrDebug'_mt_'SharedOrStatic
    call lineout qmake_cache, 'DEFINES+='
    call lineout qmake_cache, 'INCLUDEPATH+='
    call lineout qmake_cache, 'sql-drivers+='
    call lineout qmake_cache, 'sql-plugins+='
    call lineout qmake_cache, 'styles+=windows warp4 motif'
    call lineout qmake_cache, 'style-plugins+='
    call lineout qmake_cache, 'imageformat-plugins+='
    call lineout qmake_cache, 'QT_PRODUCT=qt-enterprise'
    call lineout qmake_cache, 'CONFIG+='OfficialBuildConfig'nocrosscompiler',
                              BasicModules OptionalModules,
                              'minimal-config small-config medium-config large-config full-config ',
                              ReleaseOrDebug' thread' OptionalConfig,
                              'bigcodecs no-tablet incremental create_prl link_prl',
                              ExtraConfig
    call lineout qmake_cache, 'QMAKESPEC=os2-g++'
    call lineout qmake_cache, 'QT_BUILD_TREE='QuotePath(G.!QTDir)
    call lineout qmake_cache, 'QT_SOURCE_TREE='QuotePath(G.!QTDir)
    call lineout qmake_cache, 'QT_INSTALL_PREFIX='QuotePath(G.!QTDir)
    call lineout qmake_cache, 'QT_INSTALL_TRANSLATIONS='QuotePath(G.!QTDirP'\translations')
    call lineout qmake_cache, 'docs.path='QuotePath(G.!QTDirP'\doc')
    call lineout qmake_cache, 'headers.path='QuotePath(G.!QTDirP'\include')
    call lineout qmake_cache, 'plugins.path='QuotePath(G.!QTDirP'\plugins')
    call lineout qmake_cache, 'libs.path='QuotePath(G.!QTDirP'\lib')
    call lineout qmake_cache, 'bins.path='QuotePath(G.!QTDirP'\bin')
    call lineout qmake_cache, 'data.path='QuotePath(G.!QTDirP)
    call lineout qmake_cache, 'translations.path='QuotePath(G.!QTDirP'\translations')
    call lineout qmake_cache, ''
/* @todo (r=dmik) commented out until there is a GCC compatible FT2 support lib
    if (Opt.!Extra.!FT2LIB.!enabled) then do
        call lineout qmake_cache, 'pm_ft2lib.include='Opt.!Extra.!FT2LIB.!include
        call lineout qmake_cache, 'pm_ft2lib.lib='Opt.!Extra.!FT2LIB.!lib
    end
*/
    call lineout qmake_cache

    if (Opt.!DLL == 1 & OfficialBuildConfig == '') then
        call lineout qtos2config, 'QMAKE_QT_DLL_TARGET = 'Opt.!CustomDLLName
    call lineout qtos2config, 'CONFIG += 'SharedOrNot' thread 'ReleaseOrDebug' no_mocdepend'
    call lineout qtos2config, 'QMAKE_DEFINES_QT += 'G.!QTMandatoryDefines
    if (OfficialBuildConfig == '' & Opt.!QTCustomDefines \= '') then
        call lineout qtos2config, 'QMAKE_DEFINES_QT += 'Opt.!QTCustomDefines
    call lineout qtos2config, 'QMAKE_INTERNAL_INCLUDED_FILES += $(QTDIR)\.qtos2config'
    call lineout qtos2config

    /*
     *  Create new headers/sources only if they don't exist yet or if their
     *  current contents differs from the requested configuration to prevent
     *  recompiling every source when no affecting settings are changed
     */

    create = \FileExists(qmodules_h)
    if (\create) then create = CompareFileToVar(qmodules_h, ModulesHeader) \= 0
    if (create) then do
        call DeleteFile qmodules_h
        call charout qmodules_h, ModulesHeader
        call charout qmodules_h
    end

    create = \FileExists(qconfig_h)
    if (\create) then create = CompareFileToVar(qconfig_h, ConfigHeader) \= 0
    if (create) then do
        call DeleteFile qconfig_h
        call charout qconfig_h, ConfigHeader
        call charout qconfig_h
    end

    create = \FileExists(qconfig_cpp)
    if (\create) then create = CompareFileToVar(qconfig_cpp, ConfigSource) \= 0
    if (create) then do
        call DeleteFile qconfig_cpp
        call charout qconfig_cpp, ConfigSource
        call charout qconfig_cpp
    end

    call CopyFile QuotePath(G.!QTDirP)'\qmake\"Makefile.os2-g++"',,
                  G.!QMakeMakefile

    call CopyFile QuotePath(G.!QTDirP)'\"Makefile.os2-g++"',,
                  G.!Makefile

    call SaySay 'The Qt Library has been configured.'
    say

    G.!DidConfigureQt = 1
    return

/**
 */
CompileQt: procedure expose (Globals)

    call SaySay 'Compile Qt Library'
    call SaySay '------------------'
    say

    call SaySay 'The configuration script is about to compile and build the Qt'
    call SaySay 'Library.  All output generated by the compiler and other tools'
    call SaySay 'will be recorded to the ''build.log'' file located in the'
    call SaySay 'installation directory.'
    say
    call SaySay 'Selecting ''Everything'' in the menu below is the right choice if'
    call SaySay 'this is the first time you are instaling and building the Qt Toolkit.'
    call SaySay 'If you want to build a different type of the Qt Library from the same'
    call SaySay 'installation directory (for example, the static library in addition'
    call SaySay 'to the DLL library built before) you may select ''Qt Library only'''
    call SaySay 'from the menu.'
    say

    build_log = G.!QTDirP'\build.log'

    if (FileExists(build_log)) then do
        call DeleteFile build_log'.bak'
        call CopyFile build_log, build_log'.bak'
        call DeleteFile build_log
    end

    build_target = ''

    G.!Choice.0 = 3
    G.!Choice.1 = 'Everything (Qt Library, tools, examples and tutorials)'
    G.!Choice.2 = 'Qt Library and tools only'
    G.!Choice.3 = 'Qt Library only'
    choice = GetChoice('Select what to compile:', 'G.!Choice')
    select
        when choice == 1 then build_target = 'all'
        when choice == 2 then build_target = 'sub-src sub-tools'
        when choice == 3 then build_target = 'sub-src'
        otherwise nop
    end

    no_cleanup_query = (G.!FirstRun & G.!DidConfigureCompiler & G.!DidConfigureQt)
    do_cleanup = 0

    if (\no_cleanup_query) then do
        call SaySay "Clean up *everything* before compiling?"
        do_cleanup =,
            GetYesNo("(answering N (no) is usually the best choice)", 'N')
    end

    if (build_target \== '') then do
        call DeleteFile build_log
        address 'cmd' 'call' G.!QtCmd
        call lineout build_log, 'Build started on 'date()' 'time()||G.!EOL
        if (do_cleanup) then do
            call lineout build_log, 'Cleaning up...'G.!EOL
            call lineout build_log, 'make clean'G.!EOL
            call lineout build_log
            address 'cmd',
                '"'G.!ScriptFile'" --magic-cmd make clean 2>&1 |',
                '"'G.!ScriptFile'" --magic-log 'build_log
        end
        else rc = 0
        if (rc == 0) then do
            call lineout build_log, ''
            call lineout build_log, 'Building...'G.!EOL
            call lineout build_log, 'make 'build_target||G.!EOL
            call lineout build_log
            address 'cmd',
                '"'G.!ScriptFile'" --magic-cmd make' build_target '2>&1 |',
                '"'G.!ScriptFile'" --magic-log 'build_log
        end
        if (rc \== 0) then do
            call SayErr 'FATAL: Error has occured while building the Qt Library!'
            call SayErr 'make returned' rc
            say
            call SayErr 'Please inspect the following file to find the cause of the error:'
            call SayErr ''''build_log''''
            call Done 1
            say
        end
    end

    G.!DidCompileQt = (build_target \== '')
    return

/**
 */
CheckGCCPath: procedure expose (Globals)
    parse arg path, finalCheck
    finalCheck = (finalCheck == 1)
    path = FixDirNoSlash(path)
    if (FileExists(path'\bin\gccenv.cmd') & FileExists(path'\bin\gcc.exe')) then do
        if (finalCheck) then do
            Opt.!GCCPath.!path = ''
            Opt.!GCCPath.!libpath = ''
            return 1
        end
        return path
    end
    /* not valid */
    if (finalCheck) then return 0
    return ''

/**
 */
CheckGCCVersion: procedure expose (Globals)
    parse arg path
    path = FixDirNoSlash(path)
    address 'cmd' 'call' path'\bin\gccenv.cmd' path
    address 'cmd' path'\bin\gcc.exe --version | more'
    say
    if (Opt.!GCCPath.!changed \== 1) then def = 'Y'; else def = ''
    return GetYesNo("Accept the above version?", def)

/**
 */
CheckILINKPath: procedure expose (Globals)
    parse arg path, finalCheck
    finalCheck = (finalCheck == 1)
    path = FixDirNoSlash(path)
    base = path
    if (\finalCheck) then do
        if (translate(right(path, 4)) == '\BIN') then
            base = left(path, length(path) - 4)
    end
    if (FileExists(base'\bin\ilink.exe') & FileExists(base'\dll\cppom30.dll')) then do
        /* the \bin and \dll case (as in the VAC3 installation) */
        if (finalCheck) then do
            Opt.!ILINKPath.!path = base'\bin'
            Opt.!ILINKPath.!libpath = base'\dll'
            return 1
        end
        return base
    end
    else if (FileExists(base'\ilink.exe') & FileExists(base'\cppom30.dll')) then do
        /* ilink.exe and DLLs are in the same dir */
        if (finalCheck) then do
            Opt.!ILINKPath.!path = base
            Opt.!ILINKPath.!libpath = base
            return 1
        end
        return base
    end
    /* not valid */
    if (finalCheck) then return 0
    return ''

/**
 */
CheckILINKVersion: procedure expose (Globals)
    parse arg path
    path = FixDirNoSlash(path)
    call AddPathEnv 'PATH', Opt.!ILINKPath.!path, '''P'''
    call AddPathEnv 'BEGINLIBPATH', Opt.!ILINKPath.!libpath, '''P'''
    address 'cmd' 'ilink.exe | more'
    say
    if (Opt.!ILINKPath.!changed \== 1) then def = 'Y'; else def = ''
    return GetYesNo("Accept the above version?", def)

/**
 */
CheckGNUMakePath: procedure expose (Globals)
    parse arg path, finalCheck
    finalCheck = (finalCheck == 1)
    path = FixDirNoSlash(path)
    if (FileExists(path'\make.exe')) then do
        /* make.exe and DLLs are in the same dir */
        if (FileExists(path'\intl.dll')) then do
            /* have intl.dll */
            if (finalCheck) then do
                Opt.!GNUMakePath.!path = path
                Opt.!GNUMakePath.!libpath = path
                return 1
            end
        end
        else do
            /* no intl.dll */
            if (finalCheck) then do
                Opt.!GNUMakePath.!path = path
                Opt.!GNUMakePath.!libpath = ''
                return 1
            end
        end
        return path
    end
    /* not valid */
    if (finalCheck) then return 0
    return ''

/**
 */
CheckGNUMakeVersion: procedure expose (Globals)
    parse arg path
    path = FixDirNoSlash(path)
    call AddPathEnv 'PATH', Opt.!GNUMakePath.!path, '''P'''
    call AddPathEnv 'BEGINLIBPATH', Opt.!GNUMakePath.!libpath, '''P'''
    address 'cmd' 'make.exe --version | more'
    say
    if (Opt.!GNUMakePath.!changed \== 1) then def = 'Y'; else def = ''
    return GetYesNo("Accept the above version?", def)

/**
 */
CheckFT2LIBPath: procedure expose (Globals)
    parse arg path, finalCheck
    finalCheck = (finalCheck == 1)
    path = FixDirNoSlash(path)
    base = path
    if (\finalCheck) then do
        if (translate(right(path, 8)) == '\INCLUDE') then
            base = left(path, length(path) - 8)
        else if (translate(right(path, 2)) == '\H') then
            base = left(path, length(path) - 2)
    end
    if (FileExists(base'\include\ft2lib.h') & FileExists(base'\lib\ft2lib.lib')) then do
        /* the \include and \lib case */
        if (finalCheck) then do
            Opt.!Extra.!FT2LIB.!include = base'\include'
            Opt.!Extra.!FT2LIB.!lib = base'\lib'
            return 1
        end
        return base
    end
    else if (FileExists(base'\h\ft2lib.h') & FileExists(base'\lib\ft2lib.lib')) then do
        /* the \h and \lib case */
        if (finalCheck) then do
            Opt.!Extra.!FT2LIB.!include = base'\h'
            Opt.!Extra.!FT2LIB.!lib = base'\lib'
            return 1
        end
        return base
    end
    else if (FileExists(base'\ft2lib.h') & FileExists(base'\ft2lib.lib')) then do
        /* both are in the same dir */
        if (finalCheck) then do
            Opt.!Extra.!FT2LIB.!include = base
            Opt.!Extra.!FT2LIB.!lib = base
            return 1
        end
        return base
    end
    /* not valid */
    if (finalCheck) then return 0
    return ''

/**
 */
CheckFT2LIBVersion: procedure expose (Globals)
    parse arg path
    path = FixDirNoSlash(path)
    address 'cmd' 'dir' Opt.!Extra.!FT2LIB.!include'\ft2lib.h',
                        Opt.!Extra.!FT2LIB.!lib'\ft2lib.lib'
    say
    if (Opt.!Extra.!FT2LIB.!changed \== 1) then def = 'Y'; else def = ''
    return GetYesNo("Accept the above version?", def)

/**
 */
GetEnabledText: procedure expose (Globals)
    parse arg n
    if (n == 1) then return '[X]'
    else return '[ ]'

/**
 */
GetYesNoText: procedure expose (Globals)
    parse arg n
    if (n == 1 | n == 'Y' | n == 'y') then return 'Yes'
    else return 'No'

/* utility functions
 ******************************************************************************/

CompareFileToVar: procedure expose (Globals)
    parse arg aFile, aVar
    rc = stream(aFile, 'C', 'OPEN READ')
    if (rc \= 'READY:') then do
        call SayErr 'FATAL: Could not open '''aFile'''!'
        call SayErr 'stream returned 'rc
        call Done 1
    end
    contents = charin(aFile, 1, chars(aFile))
    call stream aFile, 'C', 'CLOSE'
    if (contents < aVar) then return -1
    if (contents > aVar) then return 1
    return 0

CopyFile: procedure expose (Globals)
    parse arg fileFrom, fileTo
    address 'cmd' 'copy' fileFrom fileTo '1>nul 2>nul'
    if (rc \== 0) then do
        call SayErr 'FATAL: Could not copy '''fileFrom''' to '''fileTo'''!'
        call SayErr 'copy returned 'rc
        call Done 1
    end
    return

DeleteFile: procedure expose (Globals)
    parse arg file
    rc = SysFileDelete(file)
    if (rc \= 0 & rc \= 2) then do
        call SayErr 'FATAL: Could not delete file '''file'''!'
        call SayErr 'SysFileDelete returned 'rc
        call Done 1
    end
    return

MakeDir: procedure expose (Globals)
    parse arg path
    rc = SysMkDir(path)
    if (rc \= 0) then do
        call SayErr 'FATAL: Could not create directory '''path'''!'
        call SayErr 'SysMkDir returned 'rc
        call Done 1
    end
    return

SaySay: procedure expose (Globals)
    parse arg str, noeol
    noeol = (noeol == 1)
    if (noeol) then call charout, '    'str
    else say '    'str
    return

SayErr: procedure expose (Globals)
    parse arg str, noeol
    noeol = (noeol == 1)
    if (noeol) then call charout, str
    else say str
    return

SayPrompt: procedure expose (Globals)
    parse arg str, noeol
    noeol = (noeol == 1)
    if (noeol) then call charout, '>>> 'str
    else say '>>> 'str
    return

/**
 *  Waits for any key.
 */
WaitForAnyKey: procedure expose (Globals)
    call SayPrompt 'Press any key to continue...'
    call SysGetKey 'NOECHO'
    say
    return

SayNoMove: procedure expose (Globals)
    parse arg str
    parse value SysCurPos() with row col
    call SysCurState 'OFF'
    call charout, str
    call SysCurPos row, col
    call SysCurState 'ON'
    return

MovePosBy: procedure expose (Globals)
    parse arg delta
    parse value SysCurPos() with row col
    col = col + delta
    row = row + (col % G.!ScreenWidth)
    if (col < 0) then
        row = row - 1
    col = col // G.!ScreenWidth
    if (col < 0) then
        col = col + G.!ScreenWidth
    call SysCurPos row, col
    return

/**
 *  Displays a prompt to input a text line and returns the line entered by the
 *  user. Letters in the mode argument have the following meanings:
 *
 *      N -- empty lines are not allowed (e.g. '' can never be returned)
 *      C -- ESC can be pressed to cancel input ('1B'x is returned)
 *
 *  @param  prompt  prompt to display
 *  @param  default initial line contents
 *  @param  mode    input mode string consisting of letters as described above
 *  @return         entered line
 */
InputLine: procedure expose (Globals)

    parse arg prompt, default, mode
    call SaySay prompt
    say

    mode = translate(mode)
    allow_empty = pos('N', mode) == 0
    allow_cancel = pos('C', mode) > 0

    line = default
    len = length(line)
    n = len

    call SayPrompt line, 1
    extended = 0

    do forever

        key = SysGetKey('NOECHO')
        if (key == 'E0'x) then do
            extended = 1
            iterate
        end

        select
            when (\extended & key == '08'x) then do
                /* Backspace */
                if (n > 0) then do
                    call MovePosBy -1
                    line = delstr(line, n, 1)
                    call SayNoMove substr(line, n)||' '
                    len = len - 1
                    n = n - 1
                end
                iterate
            end
            when (key == '0D'x) then do
                /* Enter */
                if (line == '' & \allow_empty) then iterate
                say
                leave
            end
            when (key == '1B'x) then do
                /* ESC */
                line = key
                leave
            end
            when (extended) then do
                select
                    when (key == '53'x) then do
                        /* Delete */
                        if (n < len) then do
                            line = delstr(line, n + 1, 1)
                            call SayNoMove substr(line, n + 1)||' '
                        end
                        len = len - 1
                    end
                    when (key == '4B'x) then do
                        /* Left arrow */
                        if (n > 0) then do
                            call MovePosBy -1
                            n = n - 1
                        end
                    end
                    when (key == '4D'x) then do
                        /* Right arrow */
                        if (n < len) then do
                            call MovePosBy 1
                            n = n + 1
                        end
                    end
                    when (key == '47'x) then do
                        /* Home */
                        if (n > 0) then do
                            call MovePosBy -n
                            n = 0
                        end
                    end
                    when (key == '4F'x) then do
                        /* End */
                        if (n < len) then do
                            call MovePosBy len - n
                            n = len
                        end
                    end
                    otherwise nop
                end
                extended = 0
                iterate
            end
            otherwise do
                call charout, key
                if (n == len) then do
                    line = line||key
                end
                else do
                    line = insert(key, line, n)
                    call SayNoMove substr(line, n + 2)
                end
                len = len + 1
                n = n + 1
            end
        end
    end

    say
    return line

/**
 *  Shows the prompt to input a directory path and returns the path entered by
 *  the user. The procedure does not return until a valid existing path is
 *  entered or until ESC is pressed in which case it returns ''.
 *
 *  @param  prompt  prompt to show
 *  @param  default initial directory
 *  @return         selected directory
 */
InputDir: procedure expose (Globals)
    parse arg prompt, default
    dir = default
    do forever
        dir = InputLine(prompt, dir, 'NC')
        if (dir == '1B'x) then do
            say
            return '' /* canceled */
        end
        if (DirExists(dir)) then leave
        call SayErr 'The entered directory does not exist.'
        say
    end
    return dir

/**
 *  Shows a Yes/No choice.
 *
 *  @param  prompt  prompt to show (specify '' to suppress the prompt)
 *  @param  default default choice:
 *      ''          - no default choice
 *      'Y' or 1    - default is yes
 *      other       - default is no
 *  @return
 *      1 if Yes is selected, otherwise 0
 */
GetYesNo: procedure expose (Globals)
    parse arg prompt, default
    default = translate(default)
    if (default == 1) then default = 'Y'
    else if (default \== '' & default \== 'Y') then default = 'N'
    if (prompt \= '') then call SaySay prompt
    say
    call SayPrompt '[YN] ', 1
    yn = ReadChoice('YN',, default, 'I')
    say
    say
    return (yn == 'Y')

/**
 *  Shows a menu of choices and returns the menu item number selected by the
 *  user. Letters in the mode argument have the following meanings:
 *
 *      E -- allow to press Enter w/o a choice (will return '')
 *      C -- ESC can be pressed to cancel selection (will return -1)
 *
 *  @param  prompt  prompt to display
 *  @param  stem    stem containing a list of choices
 *  @param  default default choice
 *  @param  mode    input mode string consisting of letters as described above
 *  @return
 *      selected menu item number
 */
GetChoice: procedure expose (Globals)
    parse arg prompt, stem, default, mode
    mode = translate(mode)
    allowEnter = pos('E', mode) > 0
    allowESC = pos('C', mode) > 0
    count = value(stem'.0')
    if (count == 0) then return
    call SaySay prompt
    say
    first = 1
    do forever
        extChoices = ''
        last = first + 9
        if (last > count) then last = count
        choices = substr('1234567890', 1, last - first + 1)
        prompt = choices
        if (allowEnter) then prompt = prompt'/Enter'
        if (allowESC) then do
            prompt = prompt'/Esc'
            choices = choices||'1B'x
        end
        if (first > 1) then do
            extChoices = extChoices||'49'x
            prompt = prompt'/PgUp'
            call SaySay '^^'
        end
        do i = first to last
            ii = i - first + 1
            if (ii == 10) then ii = 0
            call SaySay ii')' value(stem'.'i)
        end
        if (last < count) then do
            extChoices = extChoices||'51'x
            prompt = prompt'/PgDn'
            call SaySay 'vv'
        end
        say
        def = ''
        if (default \== '') then do
            def = default - first + 1
            if (def < 1 | def > 10) then def = ''
            else if (def == 10) then def = 0
        end
        call SayPrompt '['prompt'] ', 1
        n = ReadChoice(choices, extChoices, def, mode)
        say
        say
        if (n == '1B'x) then do
            return -1
        end
        if (n == '0E49'x) then do
            first = first - 10
            iterate
        end
        if (n == '0E51'x) then do
            first = first + 10
            iterate
        end
        if (n \== '') then do
            if (n == 0) then n = 10
            n = n + first - 1
        end
        leave
    end
    return n

/**
 *  Reads a one-key choice from the keyboard.
 *  user. Letters in the mode argument have the following meanings:
 *
 *      E -- allow to press Enter w/o a choice (will return '')
 *      C -- ESC can be pressed to cancel selection (will return '1B'x)
 *      I -- ignore case of pressed letters
 *
 *  @param  choices     string of allowed one-key choices
 *  @param  extChoices  string of allowed one-extended-key choices
 *  @param  default     default choice (can be a key from choices)
 *  @param  mode        input mode string consisting of letters as described above
 *  @return
 *      entered key (prefixed with 'E0'x if from extChoices)
 */
ReadChoice: procedure expose (Globals)
    parse arg choices, extChoices, default, mode
    mode = translate(mode)
    ignoreCase = pos('I', mode) > 0
    allowEnter = pos('E', mode) > 0
    allowCancel = pos('C', mode) > 0
    choice = default
    call charout, choice
    if (ignoreCase) then choice = translate(choice)
    extended = 0
    do forever
        key = SysGetKey('NOECHO')
        if (key == 'E0'x) then do
            extended = 1
            iterate
        end
        if (\extended & ignoreCase) then key = translate(key)
        select
            when (allowCancel & \extended & key == '1B'x) then do
                choice = key
                leave
            end
            when (choice == '' & \extended & verify(key, choices) == 0) then do
                choice = key
            end
            when (extended & verify(key, extChoices) == 0) then do
                choice = '0E'x||key
                leave
            end
            when (\extended & key == '08'x & choice \== '') then do
                /* backspace pressed */
                call charout, key' '
                choice = ''
            end
            when (\extended & key == '0D'x & (choice \== '' | allowEnter)) then do
                leave
            end
            otherwise do
                extended = 0
                iterate
            end
        end
        call charout, key
        extended = 0
    end
    return choice

/**
 *  Shows a menu to select a path from the list of entries.
 *
 *  A special menu entry is automatically added as the last choice that allows
 *  to enter a new location to search for valid paths (this operation will
 *  completely overwrite all the menu entries passed to this function in the
 *  stem).
 *
 *  Letters in the mode argument have the following meanings:
 *
 *      C -- ESC can be pressed to cancel selection (will return '')
 *
 *  @param  stem
 *      stem containing entries to choose from (must be prefixed by a name from
 *      Globals), stem'.!choice' must contain a default choice (or '')
 *  @param  prompt
 *      prompt to display
 *  @param  searchPattern
 *      pattern to search for when selecting suitable paths in the new location
 *      (can include the directory prefix, but wildcards are supported in the
 *      filename part only)
 *  @param  checkPath
 *      name of the funciton to check the path. The first argument is the path
 *      to check. If the second argument is 0, it means a preliminary check (i.e.
 *      the path is the result of the SysFileThree procedure); checkPath must
 *      return a validated (and probably modified) path on success or '' if the
 *       path is invalid. If the second argument is 1, it's a final check
 *      (path is what returned by the preliminary check call); checkPath must
 *      return either 1 on success or 0 on failure. The final check is done
 *      right before calling checkVer, so it can set some global variables in
 *      order to pass necessary data to checkVer and to the global level for
 *      further configuration.
 *  @param  checkVer
 *      name of the funciton to check the version. The argument is a path to
 *      check the version for (as returned by checkPath after the preliminary
 *      check call).
 *  @param  errPath
 *      error message to display when a path check fails (%1 will be replaced
 *      with the failed path name)
 *  @param  mode
 *      input mode string consisting of letters as described above
 *  @return
 *      the selected path or '' if the selection was canceled.
 *      If a non-empty path is returned, stem'.!choice' will contain
 *      the selected item number, otherwise it will be empty.
 */
MenuSelectPath: procedure expose (Globals)

    parse arg stem, prompt, searchPattern, checkPath, checkVer, errPath, mode

    mode = translate(mode)
    if (pos('C', mode) > 0) then mode = 'C'
    else mode = ''

    if (symbol('Static.!MenuSelectPath.!Recent') \= 'VAR') then
        Static.!MenuSelectPath.!Recent = ''

    do forever

        n = value(stem'.0') + 1
        call SysStemInsert stem, n, '[type a location...]'
        if (n == 1) then default = 1
        else default = value(stem'.!choice')

        choice = GetChoice(prompt, stem, default, mode)
        call SysStemDelete stem, n

        if (choice == -1) then return '' /* canceled */

        if (choice == n) then do
            call value stem'.!choice', ''
            path = InputDir('Enter a location where to start searching from ',
                            '(or Esc to cancel):',,
                            Static.!MenuSelectPath.!Recent)
            if (path == '') then iterate /* canceled */
            Static.!MenuSelectPath.!Recent = path
            call SaySay 'Please wait...'
            say
            patternPath = translate(filespec('D', searchPattern) || filespec('P', searchPattern))
            patternName = filespec('N', searchPattern)
            call SysFileTree FixDirNoSlash(path)'\'patternName, 'found', 'FSO'
            found2.0 = 0
            if (found.0 > 0) then do
                do i = 1 to found.0
                    dir = filespec('D', found.i) || filespec('P', found.i)
                    /* check that the found path ends with the pattern path */
                    if (translate(right(dir, length(patternPath))) \== patternPath) then
                        iterate
                    dir = left(dir, length(dir) - length(patternPath))
                    /* check path validity  */
                    interpret 'dir = 'checkPath'("'dir'")'
                    if (dir \== '') then
                        call SysStemInsert 'found2', 1, FixDir(dir)
                end
            end
            if (found2.0 > 0) then do
                call SysStemCopy 'found2', stem
                /* SysStemCopy is bogus and doesn't copy the count field... */
                call value stem'.0', found2.0
                call value stem'.!choice', ''
                call value stem'.!changed', 1
                iterate
            end
        end
        else do
            path = value(stem'.'choice)
            /* check path validity and tell the version check will be done next */
            interpret 'ok = 'checkPath'("'path'", 1)'
            if (ok) then do
                if (value(stem'.!choice') \== choice) then
                    call value stem'.!changed', 1
                interpret 'ok = 'checkVer'("'path'")'
                if (ok) then do
                    call value stem'.!choice', choice
                    return path
                end
                call value stem'.!choice', ''
                iterate
            end
        end

        call SayErr Replace(errPath, '%1', path)
        say
        call value stem'.!choice', ''

    end

/**
 *  Encloses the given path with quotes if it contains
 *  space characters, otherwise returns it w/o changes.
 */
QuotePath: procedure expose (Globals)
    parse arg path
    if (verify(path, ' +', 'M') > 0) then path = '"'path'"'
    return path

/**
 *  Doubles all back slash characters to make the path ready to be put
 *  to a C/C++ source.
 */
CPPPath: procedure expose (Globals)
    parse arg path
    return Replace(path, '\', '\\')

/**
 *  Fixes the directory path by a) converting all slashes to back
 *  slashes and b) ensuring that the trailing slash is present if
 *  the directory is the root directory, and absent otherwise.
 *
 *  @param dir      the directory path
 *  @param noslash
 *      optional argument. If 1, the path returned will not have a
 *      trailing slash anyway. Useful for concatenating it with a
 *      file name.
 */
FixDir: procedure expose (Globals)
    parse arg dir, noslash
    noslash = (noslash = 1)
    dir = translate(dir, '\', '/')
    if (right(dir, 1) == '\' &,
        (noslash | \(length(dir) == 3 & (substr(dir, 2, 1) == ':')))) then
        dir = substr(dir, 1, length(dir) - 1)
    return dir

/**
 *  Shortcut to FixDir(dir, 1)
 */
FixDirNoSlash: procedure expose (Globals)
    parse arg dir
    return FixDir(dir, 1)

/**
 *  Returns 1 if the specified dir exists and 0 otherwise.
 */
DirExists: procedure expose (Globals)
    parse arg dir
    return (GetAbsDirPath(dir) \== '')

/**
 *  Returns the absolute path to the given directory
 *  or an empty string if no directory exists.
 */
GetAbsDirPath: procedure expose (Globals)
    parse arg dir
    if (dir \== '') then do
        curdir = directory()
        dir = directory(FixDir(dir))
        call directory curdir
    end
    return dir

/**
 *  Returns 1 if the specified file exists and 0 otherwise.
 */
FileExists: procedure expose (Globals)
    parse arg file
    return (GetAbsFilePath(file) \= '')

/**
 *  Returns the absolute path to the given file (including the filename)
 *  or an empty string if no file exists.
 */
GetAbsFilePath: procedure expose (Globals)
    parse arg file
    if (file \= '') then do
        file = stream(FixDir(file), 'C', 'QUERY EXISTS')
    end
    return file

/**
 *  Returns the name of the temporary directory.
 *  The returned value doesn't end with a slash.
 */
GetTempDir: procedure expose (Globals)
    dir = value('TEMP',,'OS2ENVIRONMENT')
    if (dir == '') then dir = value('TMP',,'OS2ENVIRONMENT')
    if (dir == '') then dir = SysBootDrive()
    return dir

/**
 *  Magic cmd handler.
 *  Executes the commad passed as --magic-cmd <cmd> and writes the result code
 *  to the standard output in the form of 'RC:<rc>:CR'
 *  (to be used by MagicLogHook).
 */
MagicCmdHook: procedure
    parse arg magic' 'cmd
    if (magic \== '--magic-cmd') then return
    cmd = strip(cmd)
    signal on halt name MagicCmdHalt
    address 'cmd' cmd
    say 'RC:'rc':CR'
    exit 0

MagicCmdHalt:
    say 'RC:255:CR'
    exit 255

/**
 *  Magic log handler.
 */
MagicLogHook: procedure
    parse arg magic' 'file
    if (magic \== '--magic-log') then return
    file = strip(file)
    signal on halt name MagicLogHalt
    rc = 0
    do while (stream('STDIN', 'S') == 'READY')
        line = linein()
        if (left(line, 3) == 'RC:') then do
            if (right(line,3) == ':CR') then do
                rc = substr(line, 4, length(line) - 6)
                iterate
            end
        end
        call lineout, line
        call lineout file, line
    end
    exit rc

MagicLogHalt:
    exit 255

/**
 *  Adds the given path to the contents of the given variable
 *  containing a list of paths separated by semicolons.
 *  This function guarantees that if the given path is already contained in the
 *  variable, the number of occurences will not increase (but the order may be
 *  rearranged depending on the mode argument).
 *
 *  @param  name        variable name
 *  @param  path        path to add
 *  @param  mode        'P' (prepend): remove the old occurence of path (if any)
 *                          and put it to the beginning of the variable's contents
 *                      'A' (append): remove the old occurence of path (if any)
 *                          and put it to the end of the variable's contents
 *                      otherwise: append path to the variable only if it's
 *                          not already contained there
 *  @param  environment either '' to act on REXX variables or 'OS2ENVIRONMENT'
 *                      to act on OS/2 environment variables
 *
 *  @version 1.1
 */
/* <<< export to qt.cmd starts */
AddPathVar: procedure expose (Globals)
    parse arg name, path, mode, environment
    if (path == '') then return
    if (verify(path, ' +', 'M') > 0) then path = '"'path'"'
    mode = translate(mode)
    prepend = (mode == 'P') /* strictly prepend */
    append = (mode == 'A') /* strictly append */
    os2Env = (translate(environment) == 'OS2ENVIRONMENT')
    if (os2Env) then do
        extLibPath = (translate(name) == 'BEGINLIBPATH' | translate(name) == 'ENDLIBPATH')
        if (extLibPath) then cur = SysQueryExtLibPath(left(name, 1))
        else cur = value(name,, environment)
    end
    else cur = value(name)
    /* locate the previous occurence of path */
    l = length(path)
    found = 0; p = 1
    pathUpper = translate(path)
    curUpper = translate(cur)
    do while (\found)
        p = pos(pathUpper, curUpper, p)
        if (p == 0) then leave
        cb = ''; ca = ''
        if (p > 1) then cb = substr(cur, p - 1, 1)
        if (p + l <= length(cur)) then ca = substr(cur, p + l, 1)
        found = (cb == '' | cb == ';') & (ca == '' | ca == ';')
        if (\found) then p = p + 1
    end
    if (found) then do
        /* remove the old occurence when in strict P or A mode */
        if (prepend | append) then cur = delstr(cur, p, l)
    end
    /* add path when necessary */
    if (prepend) then cur = path';'cur
    else if (append | \found) then cur = cur';'path
    /* remove excessive semicolons */
    cur = strip(cur, 'B', ';')
    p = 1
    do forever
        p = pos(';;', cur, p)
        if (p == 0) then leave
        cur = delstr(cur, p, 1)
    end
    if (os2Env) then do
        if (extLibPath) then call SysSetExtLibPath cur, left(name, 1)
        else call value name, cur, environment
    end
    else call value name, cur
    return
/* >>> export to qt.cmd ends */

/**
 *  Shortcut to AddPathVar(name, path, prepend, 'OS2ENVIRONMENT')
 */
/* <<< export to qt.cmd starts */
AddPathEnv: procedure expose (Globals)
    parse arg name, path, prepend
    call AddPathVar name, path, prepend, 'OS2ENVIRONMENT'
    return
/* >>> export to qt.cmd ends */

/**
 *  Replaces all occurences of a given substring in a string with another
 *  substring.
 *
 *  @param  str the string where to replace
 *  @param  s1  the substring which to replace
 *  @param  s2  the substring to replace with
 *  @return     the processed string
 *
 *  @version 1.1
 */
Replace: procedure expose (Globals)
    parse arg str, s1, s2
    l1  = length(s1)
    l2  = length(s2)
    i   = 1
    do while (i > 0)
        i = pos(s1, str, i)
        if (i > 0) then do
            str = delstr(str, i, l1)
            str = insert(s2, str, i-1)
            i = i + l2
        end
    end
    return str

/**
 *  NoValue signal handler.
 */
NoValue:
    errl    = sigl
    say
    say
    say 'EXPRESSION HAS NO VALUE at line #'errl'!'
    say
    say 'This is usually a result of a misnamed variable.'
    say 'Please contact the author.'
    call Done 252

/**
 *  Nonsense handler.
 */
Nonsense:
    errl    = sigl
    say
    say
    say 'NONSENSE at line #'errl'!'
    say
    say 'The author decided that this point of code should'
    say 'have never been reached, but it accidentally had.'
    say 'Please contact the author.'
    call Done 253

/**
 *  Syntax signal handler.
 */
Syntax:
    errn    = rc
    errl    = sigl
    say
    say
    say 'UNEXPECTED PROGRAM TERMINATION!'
    say
    say 'REX'right(errn , 4, '0')': 'ErrorText(rc)' at line #'errl
    say
    say 'Possible reasons:'
    say
    say '  1. Some of REXX libraries are not found but required.'
    say '  2. You have changed this script and made a syntax error.'
    say '  3. Author made a mistake.'
    say '  4. Something else...'
    call Done 254

/**
 *  Halt signal handler.
 */
Halt:
    say
    say 'CTRL-BREAK pressed, exiting.'
    call Done 255

/**
 *  Always called at the end. Should not be called directly.
 *  @param the exit code
 */
Done: procedure expose (Globals)
    parse arg code
    /* protect against recursive calls */
    if (value('G.!Done_done') == 1) then exit code
    call value 'G.!Done_done', 1
    /* cleanup stuff goes there */
    /* ... */
    /* finally, exit */
    exit code

