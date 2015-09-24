/*REXX*/
/*
 *  Command line wrapper to set up the Qt environment
 *  Generated on 2 Nov 2014 20:21:34
 */

QMAKESPEC   = 'os2-g++'
QTDIR       = 'C:\200\trunk'

GCC_PATH        = 'c:\usr'

trace off
address 'cmd'
'@echo off'

if (RxFuncQuery('SysBootDrive')) then do
    call RxFuncAdd 'SysBootDrive', 'RexxUtil', 'SysBootDrive'
    call RxFuncAdd 'SysSetPriority', 'RexxUtil', 'SysSetPriority'
end

/* determine the default DEBUG value using .qtos2config */
DEBUG = 0
do while (lines(QTDIR'\.qtos2config') > 0)
    line = strip(linein(QTDIR'\.qtos2config'))
    if (word(line, 1) == 'CONFIG') then
        if (wordpos('debug', line, 3) > 0) then DEBUG = 1
end
call lineout QTDIR'\.qtos2config'

/* parse command line */
parse arg arg1 args
arg1 = translate(arg1)
if (arg1 == 'D' | arg1 == 'DEBUG') then DEBUG = 1
else if (arg1 == 'R' | arg1 == 'RELEASE') then DEBUG = 0
else parse arg args

/* setup the GCC environment */
'call' GCC_PATH'\bin\gccenv.cmd' GCC_PATH 'WLINK'

call value 'QMAKESPEC', QMAKESPEC, 'OS2ENVIRONMENT'
call value 'QTDIR', QTDIR, 'OS2ENVIRONMENT'

if (\DEBUG) then do
    /* releae versions come first */
    call AddPathEnv 'PATH', QTDIR'\bin', 'A'
    call AddPathEnv 'PATH', QTDIR'\bin\debug', 'A'
    call AddPathEnv 'BEGINLIBPATH', QTDIR'\bin', 'A'
    call AddPathEnv 'BEGINLIBPATH', QTDIR'\bin\debug', 'A'
end
else do
    /* debug versions come first */
    call AddPathEnv 'PATH', QTDIR'\bin\debug', 'A'
    call AddPathEnv 'PATH', QTDIR'\bin', 'A'
    call AddPathEnv 'BEGINLIBPATH', QTDIR'\bin\debug', 'A'
    call AddPathEnv 'BEGINLIBPATH', QTDIR'\bin', 'A'
end

call value 'MAKESHELL', SysBootDrive()'\os2\cmd.exe', 'OS2ENVIRONMENT'

call AddPathEnv 'PATH','c:\usr\bin', 'P'

/*
 * set this to allow for direct linking with DLLs
 * without first making import libraries
 */
call AddPathEnv 'LIBRARY_PATH', SysBootDrive()'\OS2\DLL'
call AddPathEnv 'LIBRARY_PATH', SysBootDrive()'\MPTN\DLL'

/*
 * set our our priority class to IDLE so prevent GCC from eating
 * 100% of CPU load on single-processor machines
 */
call SysSetPriority 1, 0

/* pass arguments to the command shell */
'cmd /c' args
exit rc


/* some useful stuff */

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
AddPathEnv: procedure expose (Globals)
    parse arg name, path, prepend
    call AddPathVar name, path, prepend, 'OS2ENVIRONMENT'
    return

