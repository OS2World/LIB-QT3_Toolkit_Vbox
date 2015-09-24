/* REXX */
/*
 */ G.!Id = '$Id: envcat.cmd 8 2005-11-16 19:36:46Z dmik $' /*
 *
 *  :mode=netrexx:tabSize=4:indentSize=4:noTabs=true:
 *  :folding=explicit:collapseFolds=1:

 *  ----------------------------------------------------------------------------
 *
 *  Splits the contents of the given environment variable to words
 *  and prints every word on a separate line. This is useful to workaround
 *  the CMD.EXE command line length limitation (1024 chars) when calling
 *  programs from GNU make (provided that these programs can read command line
 *  arguments from responce files).
 *
 *  Author: Dmitry A. Kuminov
 *
 *  FREEWARE. NO ANY WARRANTY..., ON YOUR OWN RISC... ETC.
 *
 */

signal on syntax
signal on halt
trace off
numeric digits 12
'@echo off'

/* globals
 ******************************************************************************/

G.!Title = 'envcat r.${revision} [${revision.date} ${revision.time}]'

/* all globals to be exposed in procedures */
Globals = 'G. Opt.'

/* init rexx lib
 ******************************************************************************/

if (RxFuncQuery('SysLoadFuncs')) then do
    call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
    call SysLoadFuncs
end

/* startup code
 ******************************************************************************/

parse value strip(G.!Id,,'$') with,
    'Id: '.' 'G.!Revision' 'G.!RevisionDate' 'G.!RevisionTime' '.
G.!Title = Replace(G.!Title, '${revision}', G.!Revision)
G.!Title = Replace(G.!Title, '${revision.date}', G.!RevisionDate)
G.!Title = Replace(G.!Title, '${revision.time}', G.!RevisionTime)

parse arg args
if (args == '') then call Usage

Opt.!EnvVar = strip(args)

/* Main code
 ******************************************************************************/

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

    drop G.!Words.
    call TokenizeString value(Opt.!EnvVar,,'OS2ENVIRONMENT'), 'G.!Words'
    do n = 1 to G.!Words.0
        say G.!Words.n
    end

    return

/**
 *  Prints the usage information.
 */
Usage: procedure expose (Globals)
    say G.!Title
    say
    say 'Splits the contents of the given environment variable to words'
    say 'and prints every word on a separate line.'
    say
    say 'Usage: envcat.cmd <environment_variable>'

    call Done 0

/**
 *  Returns a list of all words from the string as a stem.
 *  Delimiters are spaces, tabs and new line characters.
 *  Words containg spaces must be enclosed with double
 *  quotes. Double quote symbols that need to be a part
 *  of the word, must be doubled.
 *
 *  @param string   the string to tokenize
 *  @param stem
 *      the name of the stem. The stem must be global
 *      (i.e. its name must start with 'G.!'), for example,
 *      'G.!wordlist'.
 *  @param leave_ws
 *      1 means whitespace chars are considered as a part of words they follow.
 *      Leading whitespace (if any) is always a part of the first word (if any).
 *
 *  @version 1.1
 */
TokenizeString: procedure expose (Globals)

    parse arg string, stem, leave_ws
    leave_ws = (leave_ws == 1)

    delims  = '20090D0A'x
    quote   = '22'x /* " */

    num = 0
    token = ''

    len = length(string)
    last_state = '' /* D - in delim, Q - in quotes, W - in word */
    seen_QW = 0

    do i = 1 to len
        c = substr(string, i, 1)
        /* determine a new state */
        if (c == quote) then do
            if (last_state == 'Q') then do
                /* detect two double quotes in a row */
                if (substr(string, i + 1, 1) == quote) then i = i + 1
                else state = 'W'
            end
            else state = 'Q'
        end
        else if (verify(c, delims) == 0 & last_state \== 'Q') then do
            state = 'D'
        end
        else do
            if (last_state == 'Q') then state = 'Q'
            else state = 'W'
        end
        /* process state transitions */
        if ((last_state == 'Q' | state == 'Q') & state \== last_state) then c = ''
        else if (state == 'D' & \leave_ws) then c = ''
        if (last_state == 'D' & state \== 'D' & seen_QW) then do
            /* flush the token */
            num = num + 1
            call value stem'.'num, token
            token = ''
        end
        token = token||c
        last_state = state
        seen_QW = (seen_QW | state \== 'D')
    end

    /* flush the last token if any */
    if (token \== '' | seen_QW) then do
        num = num + 1
        call value stem'.'num, token
    end

    call value stem'.0', num

    return

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
 *
 *  @param code     exit code
 */
Done: procedure expose (Globals)
    parse arg code
    /* protect against recursive calls */
    if (value('G.!Done_done') == 1) then exit code
    call value 'G.!Done_done', 1
    /* cleanup stuff goes there */
    /* finally, exit */
    exit code

