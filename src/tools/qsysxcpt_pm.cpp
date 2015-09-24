/****************************************************************************
** $Id: qsysxcpt_pm.cpp 158 2006-12-03 17:22:24Z dmik $
**
** OS/2 System Exception handling routines
**
** Copyright (C) 2006 netlabs.org.
**
** This file is part of the kernel module of the Qt GUI Toolkit.
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

/*
 *  The below code was started as a partial cut & paste from the except.h
 *  and except.c sources from the xwphelpers package (which is a part of the
 *  xworkplace product, see www.xworkplace.org, xworkplace.netlabs.org).
 *  XWorkplace is Copyright (C) 1999-2002 Ulrich Moeller. It has changed a lot
 *  since then, but thanks to XWP authors for a good example.
 */

/// @todo (r=dmik) use plain DosWrite() and friends instead of fprintf() and
//  friends (LIBC may be in a inconsistent state at the point when an exception
//  is thrown (see kLIBC's logstrict.c for some sprintf like stuff)
 
#if !defined(QT_OS2_NO_SYSEXCEPTIONS)
 
#include "qt_os2.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

/** @class QtOS2SysXcptMainHandler qt_os2.h
 *
 *  The QtOS2SysXcptMainHandler class is used to install the OS2/ system
 *  exception handler on the main thread of the Qt application.
 *
 *  The installed exception handler will catch most fatal exceptions (commonly
 *  referred to as traps), such as memory access violation, and create a
 *  detailed report about the state of the failed application (process name,
 *  loaded modules, thread states and call stacks). The created report is saved
 *  in the user home directory and is supposed to be sent to the application
 *  developer (which makes it much easier to find a reason of the trap and
 *  therefore gives a better chance that the problem will be fixed).
 *
 *  In order to install the exception handler, an instance of this class is
 *  created on the stack of the main thread. The best place for it is the
 *  first line of the application's main() function, before doing anything else.
 *  One doesn't need (and <b>should not</b>) create a QApplication instance
 *  before installing the exception handler. If you do so, then QApplication
 *  will install the same exception handler on its own, which is not a preferred
 *  way for two reasons:
 *  <ul>
 *  <li>QApplication uses a hack to attach the exception handler to the
 *      exception handler installed by the compiler's C Library (LIBC). This
 *      hack will not work if there is no exception handler provided by LIBC
 *      or it may work incorrectly if there are other exception handlers
 *      installed after it using the same (or similar hack).
 *  <li>There is no way to specify the exception handler callback (see below).
 *  </ul>
 *
 *  There may be only one instance of this class and it must be created only on
 *  the main (UI) thread. All other threads started by Qt using the QThread
 *  class will have the exception handler installed authomatically before the
 *  QThread::run() method starts execution, so there is no need to install it
 *  explicitly.
 *
 *  Note that QtOS2SysXcptMainHandler is a stack-based class, which means that
 *  you cannot create an instance using the new operator (you will get a
 *  compile time error). You might want to create it as a static variable (for
 *  example, if you want to catch exceptions happening before main() is entered,
 *  such as initialization of other static variables), however this is not
 *  currently supported and may work incorrectly because the OS/2 system
 *  documentation clearly states that the exception handler data must be
 *  allocated on the stack. Here is a typical usage pattern for installing the
 *  exception handler:
 *
 *  \code
 *  int main (int argc, char **argv)
 *  {
 *      QtOS2SysXcptMainHandler sysXcptHandler( psiOS2SysXcptCallback );
 *
 *      QApplication app;
 *      MyWidget wgt;
 *      app.setMainWidget (&wgt);
 *      wgt.show();
 *      return app.exec(); 
 *  }
 *  \endcode
 *
 *  The exception report is an XML file with the well-defined structure and
 *  contains enough information to find symbols corresponding to execution
 *  points on the call stack (provided that there is a .SYM file for a program
 *  or a library module). This file is always created in the SysTraps
 *  subdirectory of the user's home directory (or of the root directory of the
 *  boot drive if no home directory exists). Currently there is no way to
 *  specify a different location. A separate report file is created per every
 *  trap.
 *
 *  In order to store application-specific information in the report file,
 *  an application can install the exception handler callback that is
 *  called at certain points during exception handling. This callback is
 *  specified as an argument to the QtOS2SysXcptMainHandler constructor, but
 *  it will be called on any Qt thread that caught an exception, not
 *  necessarily the main one.
 *
 *  The callback function has the following prototype:
 *  \code
 *  int XcptCallback( QtOS2SysXcptReq req, QtOS2SysXcptWriter writer, int reserved );
 *  \endcode
 *  \a req is one of the QtOS2SysXcptReq values describing what type of
 *  information the exception handler wants to get from the callback, \a writer
 *  is the function used by the callback to write the requested information to
 *  the report file and \a reserved is a reserved value which is currently
 *  always zero.
 *
 *  The callback can be called by the exception handler in two modes: \em ask
 *  mode or \em let mode. In the "ask" mode (indicated by the NULL \a writer
 *  argument), the handler asks whether the callback supports the requested
 *  bit of information, as defined by the \a req argument. In the "let" mode
 *  (where \a writer is not NULL), it lets the callback to write the requested
 *  information to the report file using the supplied \a writer. Note that the
 *  callback should not deal with any structural report details (such as XML
 *  tags or character escaping) -- the string it writes is just an arbitrary
 *  character value for the requested attribute. The meaning of different
 *  QtOS2SysXcptReq values can be easily explained with an example: 
 *
 *  \code
    QString PROG_NAME = "MyApp";
    QString PROG_VERSION = "0.1";
 
    int psiOS2SysXcptCallback( QtOS2SysXcptReq req, QtOS2SysXcptWriter writer, int )
    {
        switch( req )
        {
            // application name
            case QtOS2SysXcptReq_AppName:
                if ( writer ) writer( PROG_NAME.latin1() );
                return TRUE;

            // application version
            case QtOS2SysXcptReq_AppVer:
                if ( writer ) writer( PROG_VERSION.latin1() );
                return TRUE;

            // e-mail to send generated trap reports to
            // (can be used by trap report handling utilities to assist the
            // user to compose and send trap reports to the developer) 
            case QtOS2SysXcptReq_ReportTo:
                if ( writer ) writer( "my-traps@some.where" );
                return TRUE;

            // recommended subject of the generated e-mail message  
            // (same purpose as above) 
            case QtOS2SysXcptReq_ReportSubj:
                // we don't need to provide this kind of information,
                // so simply return FALSE
                return TRUE;

            default:
                break;
        }

        // we don't support any other information requests
        return FALSE;
    }
 *  \endcode
 */

/* static */
bool QtOS2SysXcptMainHandler::installed = FALSE;
QtOS2SysXcptCallback QtOS2SysXcptMainHandler::callback = NULL;
ERR QtOS2SysXcptMainHandler::libcHandler = NULL;

/**
 *  Installs the exception handler on the main thread and registers \a cb
 *  as the exception handler callback. If \a cb is NULL (by default), then no
 *  user-supplied exception callback will be registered.
 */
QtOS2SysXcptMainHandler::QtOS2SysXcptMainHandler (QtOS2SysXcptCallback cb)
{
    rec.prev_structure = NULL;
    rec.ExceptionHandler = NULL;
    
    PTIB ptib = NULL;
    DosGetInfoBlocks (&ptib, NULL);
    Q_ASSERT (ptib && ptib->tib_ptib2);

    if (ptib && ptib->tib_ptib2)
    {
        /* must be instantiated only on the main (first) thread */ 
        Q_ASSERT (ptib->tib_ptib2->tib2_ultid == 1);
        if (ptib->tib_ptib2->tib2_ultid == 1)
        {
            /* must not be already instantiated */
            Q_ASSERT (installed == FALSE);
            Q_ASSERT (libcHandler == NULL);
            if (installed == FALSE && libcHandler == NULL)
            {
                installed = TRUE;
                callback = cb;
                /* install the exception handler for the main thread */ 
                rec.ExceptionHandler = handler;
                DosSetExceptionHandler (&rec);
            }
        }
    }
}

/**
 *  Uninstalls the exception handler installed by the constructor.        
 */
QtOS2SysXcptMainHandler::~QtOS2SysXcptMainHandler()
{
    Q_ASSERT (rec.ExceptionHandler == handler || rec.ExceptionHandler == NULL);
    if (rec.ExceptionHandler == handler)
    {
        /* uninstall the exception handler for the main thread */
        DosUnsetExceptionHandler (&rec);
        rec.ExceptionHandler = NULL;
        callback = NULL; 
        installed = FALSE;
    }
}

static void qt_excEscapeString (FILE *file, const char *pcszStr);

#define XcptPvt QtOS2SysXcptMainHandler::Private

class XcptPvt
{
public:
    static void callbackWriter (const char *msg);

    static inline int askCallback (QtOS2SysXcptReq req)
    {
        if (callback)
            return callback (req, NULL, 0);
        return FALSE; 
    }

    static inline int letCallback (QtOS2SysXcptReq req)
    {
        if (callback)
            return callback (req, callbackWriter, 0);
        return FALSE; 
    }
    
    static FILE *file;
};

/* static */
FILE *XcptPvt::file = NULL;

/* static */
void XcptPvt::callbackWriter (const char *msg)
{
    if (file)
        qt_excEscapeString (file, msg);
}

/** \internal
 *  Writes the given string with [<>&'"] characters escaped for XML.
 */
static void qt_excEscapeString (FILE *file, const char *pcszStr)
{
    const char *pcszChars = "<>&'\"";
    const char *aszEntities[] = { "&lt;", "&gt;", "&amp;", "&apos;", "&quot;" };

    const char *pcsz = pcszStr;
    const char *pcszRepl = NULL;
    size_t cbLen = 0;

    if (!pcsz)
        return;

    while (*pcsz)
    {
        cbLen = strcspn (pcsz, pcszChars);
        fwrite (pcsz, 1, cbLen, file);
        pcsz += cbLen;
        if (!*pcsz)
            break;
        cbLen = strchr (pcszChars, *pcsz) - pcszChars;
        pcszRepl = aszEntities[cbLen];
        fwrite (pcszRepl, 1, strlen(pcszRepl), file);
        ++ pcsz;
    }
}

/** \internal
 *  Writes the given error message.
 */
static void qt_excWriteErrorMsg (FILE *file, ULONG ulIndent, const char *pcszMsg,
                                 ...)
{
    char szBuf[1024];
    va_list args;
    va_start (args, pcszMsg);
    fprintf (file, "%*s<Error message=\"", ulIndent, "");
    vsnprintf (szBuf, sizeof(szBuf), pcszMsg, args);
    szBuf[sizeof(szBuf) - 1] = '\0';
    qt_excEscapeString (file, szBuf);
    fprintf (file, "\"/>\n");
    va_end (args);
}

/** \internal
 *  Writes the register name, value and optionally memory flags
 */
static void qt_excWriteReg (FILE *file, const char *pszName, ULONG ulValue,
                            BOOL bQueryMem = TRUE)
{
    fprintf (file, "      <Register name=\"%s\" value=\"%08lX\"", 
             pszName, ulValue);

    if (bQueryMem)
    {
        APIRET arc;
        ULONG ulCount = 4;
        ULONG ulFlags = 0;
        arc = DosQueryMem ((PVOID) ulValue, &ulCount, &ulFlags);
    
        if (arc == NO_ERROR || arc == ERROR_INVALID_ADDRESS)
        {
            if (arc == NO_ERROR)
            {
                fprintf (file, " flags=\"%08lX\"", ulFlags);
                if ((ulFlags & (PAG_COMMIT | PAG_READ)) ==
                    (PAG_COMMIT | PAG_READ))
                    fprintf (file, " word=\"%08lX\"/>\n", *(ULONG *) ulValue);
                else
                    fprintf (file, "/>\n");
            }
            else
                fprintf (file, " flags=\"invalid\"/>\n");
        }
        else
        {
            fprintf (file, ">\n");
            qt_excWriteErrorMsg (file, 7, "DosQueryMem returned %lu "
                                 "and flags %08lX", arc, ulFlags);
            fprintf (file, "      </Register>\n");
        }
    }
    else
        fprintf (file, "/>\n");
}

/** \internal
 *  Writes information about a signle stack frame.
 */
static void qt_excWriteStackFrame (FILE *file, ULONG ulRegEbp, ULONG ulRegEip)
{
    APIRET  arc = NO_ERROR;
    HMODULE hMod = NULLHANDLE;
    char    szMod[CCHMAXPATH] = "";
    ULONG   ulObject = 0,
            ulOffset = 0;

    fprintf (file, "      <Frame pointer=\"%08lX\">\n", ulRegEbp);
    fprintf (file, "       <Location address=\"%08lX\">\n", ulRegEip);
    
    arc = DosQueryModFromEIP (&hMod, &ulObject,
                              sizeof (szMod), szMod, &ulOffset,
                              ulRegEip);

    if (arc != NO_ERROR)
        qt_excWriteErrorMsg (file, 8, "DosQueryModFromEIP returned %lu", arc);
    else
        fprintf (file, "        <Module ID=\"%04lX\" segment=\"%04lX\" "
                                       "offset=\"%08lX\"/>\n",
                 hMod, ulObject + 1, ulOffset); 

    /* write a small memory dump with the location address in the middle */
    {
        enum { enmDelta = 8 };
        UCHAR *pch = (UCHAR *) ulRegEip - enmDelta;
        UCHAR *pchEnd = (UCHAR *) ulRegEip + enmDelta - 1;
        ULONG ulCount = enmDelta * 2;
        ULONG ulFlags = 0;
        APIRET arc = NO_ERROR;
        while (1)
        {
            arc = DosQueryMem ((void *) (ULONG) pch, &ulCount, &ulFlags);
            if (arc == NO_ERROR)
            {
                if (ulCount >= enmDelta * 2)
                    break;
                if (pch + ulCount <= (UCHAR *) ulRegEip)
                {
                    /* ulAddress is outside the pch object */
                    pch += ulCount;
                    ulCount = enmDelta * 2 - ulCount;
                }
                else
                {
                    /* ulAddress is within the pch object */
                    pchEnd = pch += ulCount;
                    break;
                }
            }
            else if (arc == ERROR_INVALID_ADDRESS)
            {
                if ((((ULONG) pch) & 0xFFFFF000) == (ulRegEip & 0xFFFFF000))
                    break; /* the same page, ulAddress inaccessible */
                pch = (UCHAR *) (ulRegEip & 0xFFFFF000);
            }
        }
        fprintf (file, "        <Dump address=\"%08lX\">\n         ", pch);
        if (arc == NO_ERROR &&
            (ulFlags & (PAG_COMMIT | PAG_READ)) == (PAG_COMMIT | PAG_READ))
        {
            for (; pch < pchEnd; ++pch)
                fprintf (file, "%02lX%c", (ULONG) *pch,
                         ulRegEip - (ULONG) pch == 1 ? '-' : ' ');
            fprintf (file, "\n");
        }
        else
            qt_excWriteErrorMsg (file, 0, "%08lX: DosQueryMem returned %lu "
                                 "and flags %08lX", pch, arc, ulFlags);
        fprintf (file, "        </Dump>\n");
    }
    
    fprintf (file, "       </Location>\n");
    fprintf (file, "      </Frame>\n");
}

/** \internal
 *  Walks the stack and writes information about stack frames.
 *  @note This function should be in sync with qt_excWriteModulesOnStack()
 *        because they share the same logic.
 */
static void qt_excWriteStackFrames (FILE *file, ULONG ulStackLimit,
                                    PCONTEXTRECORD pContextRec)
{
    ULONG ulRegEbp = pContextRec->ctx_RegEbp;
    ULONG ulRegEip = pContextRec->ctx_RegEip;

    fprintf (file, "     <Frames>\n");

    /* first the trapping address itself */
    qt_excWriteStackFrame (file, ulRegEbp, ulRegEip);
    
    /* first call to qt_excWriteStackFrame() is done before the EBP validity
     * check below to get a chance to call qt_excWriteStackFrame() for the
     * trapping address itself even if EBP there is invalid. */

    while (ulRegEbp != 0 && ulRegEbp < ulStackLimit)
    {
        /* skip the trapping stack frame -- already written above */
        if (pContextRec->ctx_RegEbp != ulRegEbp)
            qt_excWriteStackFrame (file, ulRegEbp, ulRegEip);

        if ((ulRegEbp & 0x00000FFF) == 0x00000000)
        {
            /* we're on a page boundary: check access */
            ULONG ulCount = 0x1000;
            ULONG ulFlags = 0;
            APIRET arc = DosQueryMem ((void *) ulRegEbp, &ulCount, &ulFlags);
            if (   (arc != NO_ERROR)
                || (   arc == NO_ERROR
                    && (ulFlags & (PAG_COMMIT | PAG_READ))
                        != (PAG_COMMIT | PAG_READ)))
            {
                fprintf (file, "      <Frame pointer=\"%08lX\">\n", ulRegEbp);
                qt_excWriteErrorMsg (file, 7, "DosQueryMem returned %lu "
                                     "and flags %08lX", arc, ulFlags);
                fprintf (file, "      </Frame>\n");
                /* try go to the next page */
                /// @todo (r=dmik) I don't know how much is it accurate,
                //  I've just taken the logic from xwphelpers sources.
                ulRegEbp += 0x1000;
                continue; /* while */
            }
        }

        /* get the return address of the current call */
        ulRegEip = *(((PULONG) ulRegEbp) + 1);
        /* get the address of the outer stack frame */
        ulRegEbp = *((PULONG) ulRegEbp);
    }

    fprintf (file, "     </Frames>\n");
}

/** \internal
 *  Writes the thread information.
 *  If ptib is not NULL, the current thread's information is to be written.
 *  If ptib is NULL, pThrdRec is guaranted not to be NULL.
 */
static void qt_excWriteThreadInfo (FILE *file, PTIB ptib, QSTREC *pThrdRec,
                                   PCONTEXTRECORD pContextRec)
{
    ULONG ulStackBase = 0, ulStackLimit = 0;
    ULONG ulCount = 0, ulFlags = 0;
    ULONG ul = 0;
    APIRET arc = NO_ERROR;
    
    if (ptib)
    {
        fprintf (file, "   <Thread ID=\"%04lX\" slot=\"%04lX\" "
                                  "priority=\"%04lX\" ",
                 ptib->tib_ptib2->tib2_ultid, ptib->tib_ordinal,
                 ptib->tib_ptib2->tib2_ulpri);
        if (pThrdRec)
            fprintf (file, "state=\"%02lX\" ", (ULONG) pThrdRec->state);
        fprintf (file, "mc=\"%04lX\" mcf=\"%04lX\">\n",
                 ptib->tib_ptib2->tib2_usMCCount,
                 ptib->tib_ptib2->tib2_fMCForceFlag);
    }
    else
    {
        fprintf (file, "   <Thread ID=\"%04lX\" slot=\"%04lX\" "
                                  "priority=\"%04lX\" state=\"%02lX\">\n",
                 pThrdRec->tid, pThrdRec->slot, pThrdRec->priority,
                 pThrdRec->state);
    }

    /* *** registers */

    fprintf (file, "    <CPU>\n"
                   "     <Registers>\n");

    if (pContextRec->ContextFlags & CONTEXT_SEGMENTS)
    {
        qt_excWriteReg (file, "DS", pContextRec->ctx_SegDs, FALSE);
        qt_excWriteReg (file, "ES", pContextRec->ctx_SegEs, FALSE);
        qt_excWriteReg (file, "FS", pContextRec->ctx_SegFs, FALSE);
        qt_excWriteReg (file, "GS", pContextRec->ctx_SegGs, FALSE);
    }
    
    if (pContextRec->ContextFlags & CONTEXT_INTEGER)
    {
        qt_excWriteReg (file, "EAX", pContextRec->ctx_RegEax);
        qt_excWriteReg (file, "EBX", pContextRec->ctx_RegEbx);
        qt_excWriteReg (file, "ECX", pContextRec->ctx_RegEcx);
        qt_excWriteReg (file, "EDX", pContextRec->ctx_RegEdx);
        qt_excWriteReg (file, "ESI", pContextRec->ctx_RegEsi);
        qt_excWriteReg (file, "EDI", pContextRec->ctx_RegEdi);
    }

    if (pContextRec->ContextFlags & CONTEXT_CONTROL)
    {
        qt_excWriteReg (file, "CS", pContextRec->ctx_SegCs, FALSE);
        qt_excWriteReg (file, "EIP", pContextRec->ctx_RegEip);
        qt_excWriteReg (file, "SS", pContextRec->ctx_SegSs, FALSE);
        qt_excWriteReg (file, "ESP", pContextRec->ctx_RegEsp);
        qt_excWriteReg (file, "EBP", pContextRec->ctx_RegEbp);
        qt_excWriteReg (file, "EFLAGS", pContextRec->ctx_EFlags, FALSE);
    }
    
    fprintf (file, "     </Registers>\n"
                   "    </CPU>\n");

    /* *** stack */

    if (ptib)
    {
        ulStackBase = (ULONG) ptib->tib_pstack;
        ulStackLimit = (ULONG) ptib->tib_pstacklimit;
    }
    else
    if (pContextRec->ContextFlags & CONTEXT_CONTROL)
    {
        /* first, try to find the end of the stack object */
        ulCount = 0xFFFFFFFF;
        arc = DosQueryMem ((void *) pContextRec->ctx_RegEbp,
                           &ulCount, &ulFlags);
        if (arc == NO_ERROR)
        {
            ulStackLimit = pContextRec->ctx_RegEbp + ulCount;
            /* next, try to find the start of the stack object */
            ul = (pContextRec->ctx_RegEbp & 0xFFFFF000);
            while (arc == NO_ERROR && !(ulFlags & PAG_BASE))
            {
                ulCount = 0x1000;
                ul -= 0x1000;
                arc = DosQueryMem ((void *) ul, &ulCount, &ulFlags);
            }
            if (arc == NO_ERROR)
                ulStackBase = ul;
        }
    }

    fprintf (file, "    <Stack");
    if (ulStackBase)
        fprintf (file, " base=\"%08lX\"", ulStackBase);
    if (ulStackLimit)
        fprintf (file, " limit=\"%08lX\"", ulStackLimit);
    fprintf (file, ">\n");
    
    if (pContextRec->ContextFlags & CONTEXT_CONTROL)
        qt_excWriteStackFrames (file, ulStackLimit, pContextRec);

    fprintf (file, "    </Stack>\n");
    fprintf (file, "   </Thread>\n");
}

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

/** @internal
 *  Writes the module description string (if found) as the 'desciption'
 *  attribute. Returns TRUE if the description was found and written.
 */
static BOOL qt_excWriteModuleDesc (FILE *file, char *pszFileName)
{
    /* see OS2TK\h\exe.h, OS2TK\h\exe386.h and LXSPEC.INF for details */
    enum { EXEID = 0x5a4d, LXOffset = 0x3C, sizeof_exe = LXOffset + 4,
           E32MAGIC = 0x584c, e32_nrestab = 34 * 4,
           sizeof_e32 = e32_nrestab + 4,
           max_nresname = 255 + 1 /* trailing 0 */ };
           
    UCHAR achBuf [max (max_nresname, max (sizeof_e32, sizeof_exe))];

    BOOL bFound = FALSE;
    ULONG ul1 = 0, ul2 = 0;
    
    HFILE hf = NULL;
    APIRET arc = NO_ERROR;
    
    arc = DosOpen (pszFileName, &hf, &ul1,
                   0, 0, OPEN_ACTION_OPEN_IF_EXISTS,
                   OPEN_SHARE_DENYWRITE | OPEN_ACCESS_READONLY,
                   NULL);
    if (arc != NO_ERROR)
        return FALSE;

    do
    {
        /* read the old EXE header plus the offset to the LX header */
        arc = DosRead (hf, achBuf, sizeof_exe, &ul1);
        if (arc != NO_ERROR || sizeof_exe != ul1)
            break;
        
        if (*(USHORT *) &achBuf == EXEID)
        {
            /* go to the LX header */
            ul1 = *(ULONG *) &achBuf [LXOffset];
            arc = DosSetFilePtr (hf, (LONG) ul1, FILE_BEGIN, &ul2);
            if (arc != NO_ERROR || ul1 != ul2)
                break;

            /* read it upto the e32_nrestab field */
            arc = DosRead (hf, achBuf, sizeof_e32, &ul1);
            if (arc != NO_ERROR || sizeof_e32 != ul1 ||
                *(USHORT *) &achBuf != E32MAGIC)
                break;
        }
        else
        if (*(USHORT *) &achBuf == E32MAGIC)
        {
            /* there may be no old EXE header, but LX only,
             * read it upto the e32_nrestab field */
            if (sizeof_e32 > sizeof_exe)
            {
                arc = DosRead (hf, achBuf + sizeof_exe,
                               sizeof_e32 - sizeof_exe, &ul1);
                if (arc != NO_ERROR || sizeof_e32 - sizeof_exe != ul1)
                    break;
            }
        }
        else
            break;
        
        /* go to the beginning of the non-resident name table */ 
        ul1 = *(ULONG *) &achBuf [e32_nrestab];
        if (ul1)
        {
            arc = DosSetFilePtr (hf, (LONG) ul1, FILE_BEGIN, &ul2);
            if (arc != NO_ERROR || ul1 != ul2)
                break;
            
            /* read the first entry */
            arc = DosRead (hf, achBuf, max_nresname, &ul1);
            if (arc != NO_ERROR || max_nresname != ul1)
                break;
    
            /* if the ordinal is 0, then it's a description field (we don't do
             * the full table search, since it shoud be ordered by ordinals) */
            if (*(USHORT *) &achBuf [1 + *achBuf] == 0)
            {
                /* place the trailing zero */
                achBuf [1 + *achBuf] = 0;
                /* write the description attribute */
                fprintf (file, " desc=\"%s\"", achBuf + 1);
                bFound = TRUE;
            }
        }
    }
    while (0);
    
    DosClose (hf);
    return bFound;
}

/** \internal
 *  Writes module information to the log file.
 *  \a ulOrigin is 1 (self), 2 (imported), or 3 (loaded). Other values are
 *  ignored.
 */
static void qt_excWriteModule (FILE *file, USHORT hmte, char *pszName,
                               ULONG ulOrigin = 0)
{
    static const char *apszOrigins[] = { "self", "imported", "loaded" };
    
    FILESTATUS3 Status;
    APIRET arc = NO_ERROR;

    fprintf (file, "   <Module ID=%\"%04lX\" name=\"", hmte);
    qt_excEscapeString (file, pszName);
    fprintf (file, "\"");
    
    arc = DosQueryPathInfo (pszName, FIL_STANDARD, &Status, sizeof (Status));

    if ((ulOrigin >= 1 && ulOrigin <= 3) || arc == NO_ERROR)
    {
        if (ulOrigin >= 1 && ulOrigin <= 3)
            fprintf (file, " origin=\"%s\"",
                           apszOrigins [ulOrigin - 1]);
        if (arc == NO_ERROR)
            fprintf (file, " timestamp=\"%04d-%02d-%02dT%02d:%02d:%02d\"",
                     Status.fdateLastWrite.year + 1980,
                     Status.fdateLastWrite.month, Status.fdateLastWrite.day,
                     Status.ftimeLastWrite.hours, Status.ftimeLastWrite.minutes,
                     Status.ftimeLastWrite.twosecs * 2);
    }

    qt_excWriteModuleDesc (file, pszName);

    fprintf (file, "/>\n");
}

/** \internal
 *  Writes module and all its imports information to the log file.
 *  \a ulOrigin is 1 (self), or 3 (loaded), other values are ignored.
 */
static void qt_excWriteModules (FILE *file, USHORT hmte, QSLREC *pFirstLibRec,
                                ULONG ulOrigin)
{
    QSLREC *pLibRec = pFirstLibRec;
    while (pLibRec)
    {
        if (pLibRec->hmte == hmte &&
            pLibRec->pName /* not yet visited */)
        {
            qt_excWriteModule (file, hmte, (char *) pLibRec->pName, ulOrigin);
            /* mark as visited */
            pLibRec->pName = NULL;
            /* go through imports */
            if (pLibRec->ctImpMod)
            {
                USHORT *pHmte = (USHORT *) (pLibRec + 1);
                for (ULONG i = 0; i < pLibRec->ctImpMod; ++ i, ++ pHmte)
                    qt_excWriteModules (file, *pHmte, pFirstLibRec,
                                        ulOrigin == 1 ? 2 : ulOrigin);
                break;
            }
        }
        pLibRec = (QSLREC *) pLibRec->pNextRec; 
    }
}

/** @internal
 *  Struct for recursive qt_excWriteModulesOnStack() to reduce stack usage
 */
typedef struct _WMOS_STATE
{
    FILE *file;
    ULONG ulRegEbp;
    ULONG ulRegEip;
    ULONG ulStackLimit;
    USHORT hmteExe;
    ULONG cLevel;
    USHORT *pHmteOuter;
} WMOS_STATE;

/** \internal
 *  Walks the stack and writes information about all encountered modules.
 *  Used only when DosQuerySysState() fails (so qt_excWriteModules() is not
 *  applicable). We use recursiveness to avoid module record duplicates.
 *  @note This function should be in sync with qt_excWriteStackFrames()
 *        because they share the same logic.
 */
static void qt_excWriteModulesOnStack (WMOS_STATE *pState)
{
    USHORT hmteThis = 0;
    
    {
        APIRET  arc = NO_ERROR;
        char    szMod [CCHMAXPATH] = "";
        ULONG   ulObject = 0, ulOffset = 0;
    
        arc = DosQueryModFromEIP ((HMODULE *) &hmteThis, &ulObject,
                                  sizeof (szMod), szMod, &ulOffset,
                                  pState->ulRegEip);
        if (arc == NO_ERROR && hmteThis != pState->hmteExe)
        {
            /* look if we've already seen this module using the hmteThis chain
             * on the stack */
            USHORT *pH = &hmteThis;
            /* calculate distance between recursive hmteThis on the stack */
            ULONG ulOuter = (ULONG) pState->pHmteOuter - (ULONG) pH;
            ULONG cL = pState->cLevel;
            while (cL != 0)
            {
                pH = (USHORT *)(((ULONG) pH) + ulOuter);
                if (*pH == hmteThis)
                    break;
                -- cL;
            }
            if (cL == 0)
            {
                /* got an unique module handle */
                DosQueryModuleName (hmteThis, sizeof (szMod), szMod);
                qt_excWriteModule (pState->file, hmteThis, szMod);
            }
        }
    }
    
    /* we do EBP validity check here as well as before the recursive
     * call below to get a chance for the above code (EIP to hmod translation)
     * to be executed even if EBP there is invalid. */

    if (pState->ulRegEbp != 0 && pState->ulRegEbp < pState->ulStackLimit)
    {
        if ((pState->ulRegEbp & 0x00000FFF) == 0x00000000)
        {
            /* we're on a page boundary: check access */
            while (pState->ulRegEbp < pState->ulStackLimit)
            {
                ULONG ulCount = 0x1000;
                ULONG ulFlags = 0;
                APIRET arc = DosQueryMem ((void *) pState->ulRegEbp,
                                          &ulCount, &ulFlags);
                if (   (arc != NO_ERROR)
                    || (   arc == NO_ERROR
                        && (ulFlags & (PAG_COMMIT | PAG_READ))
                            != (PAG_COMMIT | PAG_READ)))
                {
                    /* try go to the next page */
                    /// @todo (r=dmik) I don't know how much is it accurate,
                    //  I've just taken the logic from xwphelpers sources.
                    pState->ulRegEbp += 0x1000;
                    continue; /* while */
                }
                break;
            }
        }

        /* get the return address of the current call */
        pState->ulRegEip = *(((PULONG) pState->ulRegEbp) + 1);
        /* get the address of the outer stack frame */
        pState->ulRegEbp = *((PULONG) pState->ulRegEbp);
    
        if (pState->ulRegEbp != 0 && pState->ulRegEbp < pState->ulStackLimit)
        {
            pState->cLevel ++;
            pState->pHmteOuter = &hmteThis;
            qt_excWriteModulesOnStack (pState);
        }
    }
}

/** @internal Simple process info struct */
typedef struct _PROCESSINFO
{
    /* common values */
    char *pszFullName;
    char *pszBaseName;
    /* direct info pointers */
    PPIB ppib;
    PTIB ptib;
    BOOL bHaveSysState; /**< TRUE when both pProcRec and pLibRec are not NULL */ 
    QSPREC *pProcRec;   /**< NULL when bHaveSysState is FALSE */
    QSLREC *pLibRec;    /**< NULL when bHaveSysState is FALSE */
} PROCESSINFO;

/** \internal
 *  Writes exception information to the log file.
 */
static void qt_excWriteException (FILE *file,
                                  PROCESSINFO *pInfo,
                                  PEXCEPTIONREPORTRECORD pReportRec,
                                  PCONTEXTRECORD pContextRec)
{
    ULONG ul = 0;
    ULONG aulBuf [3];

    QSTREC *pThrdRec = NULL;

    /* *** application info */

    {
        fprintf (file, " <Application name=\"");
        if (XcptPvt::askCallback (QtOS2SysXcptReq_AppName) == TRUE)
            XcptPvt::letCallback (QtOS2SysXcptReq_AppName);
        else
            qt_excEscapeString (file, pInfo->pszBaseName);
        fprintf (file, "\" version=\"");
        if (XcptPvt::askCallback (QtOS2SysXcptReq_AppVer) == TRUE)
            XcptPvt::letCallback (QtOS2SysXcptReq_AppVer);
        else
            fprintf (file, "unknown");
        fprintf (file, "\">\n");

        if (XcptPvt::askCallback (QtOS2SysXcptReq_ReportTo) == TRUE)
        {
            fprintf (file, "  <Report to=\"");
            XcptPvt::letCallback (QtOS2SysXcptReq_ReportTo);
            if (XcptPvt::askCallback (QtOS2SysXcptReq_ReportSubj) == TRUE)
            {
                fprintf (file, "\" subject=\"");
                XcptPvt::letCallback (QtOS2SysXcptReq_ReportSubj);
            }
            fprintf (file, "\"/>\n");
        }
        
        fprintf (file, " </Application>\n");
    }
    
    /* *** generic exception info */

    fprintf (file,
             " <Exception type=\"%08lX\" flags=\"%08lX\" address=\"%08lX\">\n",
             pReportRec->ExceptionNum, pReportRec->fHandlerFlags,
             (ULONG) pReportRec->ExceptionAddress);
    
    for (ul = 0; ul < pReportRec->cParameters;  ++ul)
    {
        fprintf (file, "  <Param value=\"%08lX\"/>\n",
                 pReportRec->ExceptionInfo [ul]);
    }

    fprintf (file, " </Exception>\n");
    
    /* *** system info */

    DosQuerySysInfo (QSV_VERSION_MAJOR, QSV_VERSION_REVISION,
                     &aulBuf, sizeof (aulBuf));
    /* Warp 3 is reported as 20.30 */
    /* Warp 4 is reported as 20.40 */
    /* Aurora is reported as 20.45 */

    fprintf (file,
             " <System name=\"OS/2\" version=\"%u.%u.%u\"/>\n",
             aulBuf[0], aulBuf[1], aulBuf[2]);

    /* *** process info */

    fprintf (file, " <Process ID=\"%04lX\" parentID=\"%04lX\">\n",
             pInfo->ppib->pib_ulpid, pInfo->ppib->pib_ulppid);

    fprintf (file, "  <Modules>\n");
    if (pInfo->bHaveSysState)
    {
        /* write process module and it's imports */
        qt_excWriteModules (file, pInfo->ppib->pib_hmte, pInfo->pLibRec, 1);
        /* write loaded modules and their imports */
        if (pInfo->pProcRec->cLib && pInfo->pProcRec->pLibRec)
        {
            for (ULONG i = 0; i < pInfo->pProcRec->cLib; ++ i)
                qt_excWriteModules (file, pInfo->pProcRec->pLibRec [i],
                                    pInfo->pLibRec, 3);
        }
    }
    else
    {
        qt_excWriteModule (file, pInfo->ppib->pib_hmte, pInfo->pszFullName, 1);
        WMOS_STATE State = { file, pContextRec->ctx_RegEbp,
                             pContextRec->ctx_RegEip,
                             (ULONG) pInfo->ptib->tib_pstacklimit,
                             pInfo->ppib->pib_hmte,
                             0 /* cLevel */, NULL /* pHmteOuter */ };
        qt_excWriteModulesOnStack (&State);
    }
    fprintf (file, "  </Modules>\n");
    
    fprintf (file, "  <Threads>\n");
    
    /* first, the current thread */
    pThrdRec = pInfo->bHaveSysState ? pInfo->pProcRec->pThrdRec : NULL;
    if (pThrdRec)
    {
        /* locate the current thread structure */
        for (ul = 0; ul < pInfo->pProcRec->cTCB; ++ ul, ++ pThrdRec)
            if ((ULONG) pThrdRec->tid == pInfo->ptib->tib_ptib2->tib2_ultid)
                break;
        if (ul == pInfo->pProcRec->cTCB)
            pThrdRec = NULL;
    }
    qt_excWriteThreadInfo (file, pInfo->ptib, pThrdRec, pContextRec);

    /* then, all other threads */
    pThrdRec = pInfo->bHaveSysState ? pInfo->pProcRec->pThrdRec : NULL;
    if (pThrdRec)
    {
        /* important to set ContextFlags to CONTEXT_FULL, otherwise we'll
         * get nothing */
        CONTEXTRECORD ContextRec = {CONTEXT_FULL, 0};
        APIRET arc = NO_ERROR;
        /* enter the critical section to effectively suspend all other threads
         * while reading their contexts and walking their stacks */
        DosEnterCritSec();
        for (ul = 0; ul < pInfo->pProcRec->cTCB; ++ ul, ++ pThrdRec)
        {
            if ((ULONG) pThrdRec->tid == pInfo->ptib->tib_ptib2->tib2_ultid)
                continue;
            arc = DosQueryThreadContext (pThrdRec->tid, CONTEXT_FULL,
                                         &ContextRec);
            if (arc == NO_ERROR)
                qt_excWriteThreadInfo (file, NULL, pThrdRec, &ContextRec);
            else
            {
                fprintf (file, "   <Thread ID=\"%04lX\">\n", pThrdRec->tid);
                qt_excWriteErrorMsg (file, 3, "DosQueryThreadContext returned "
                                              "%lu", arc);
                fprintf (file, "   </Thread>\n");
            }
        }
        /* exit the critical section */
        DosExitCritSec();        
    }
    
    fprintf (file, "  </Threads>\n");

    fprintf (file, " </Process>\n");
}

/**
 *  Opens an unique log file using \a pcszBasePath as the base path.
 *  On input, \a pszFileName is the desired base file name w/o extension.
 *  If it doesn't fit into the file name length limit (ENAMETOOLONG) for the
 *  FS of the given disk, then a 8x3 file name is attemptet as a fallback.
 *  On output, \a pszFileName will contain the full file name of the opened
 *  log file. Note that \a pszFileName must be at least CCHMAXPATH bytes length.
 */
static FILE *qt_excOpenLogFile (const char *pcszBasePath, char *pszFileName)
{
    FILE *file = NULL;

    char szFileName[CCHMAXPATH];
    char szDir[] = "\\SysTraps";
    char szFile[] = "\\12345678.123";
    enum { cbFileName = sizeof(szFileName) };
    enum { cbDir = sizeof(szDir) };
    enum { cbFile = sizeof(szFile) };

    ULONG ul = 0;
    ULONG cbDirLen = 0;
    ULONG cbLen = 0;
    ULONG ulStamp = 0;
    char *pszStamp = NULL; 
    
    if (pcszBasePath == NULL || pszFileName == NULL)
        return NULL;
    
    if (access (pcszBasePath, F_OK) != 0)
        return NULL;
    
    if (strlen (pcszBasePath) + cbDir + cbFile - 2 >= CCHMAXPATH)
        return NULL;

    /* get the full path if it's not 'X:' */
    if (strlen(pcszBasePath) != 2 ||
         pcszBasePath[1] != ':')
    {
        if (DosQueryPathInfo (pcszBasePath, FIL_QUERYFULLNAME,
                              szFileName, cbFileName) != NO_ERROR)
            return NULL;
    }
    else
        strcpy (szFileName, pcszBasePath);
    
    strcat (szFileName, szDir);
    if (access (szFileName, F_OK) != 0)
        if (mkdir (szFileName, 0777) != 0)
            return NULL;
    
    cbDirLen = strlen (szFileName);
        
    /* first, try to use the desired base file name */

    /* we will append -NN to the file name to add some 'fraction of a
     * second' granularity to avoid name conflicts (0 <= NNN <= 99) */
    cbLen = strlen (pszFileName);
    if (cbDirLen + cbLen + 12 /* \-NN.trp.xml */ < CCHMAXPATH)
    {
        strcat (szFileName, "\\");
        strcat (szFileName, pszFileName);
        cbLen += cbDirLen + 1 /* \ */;
        for (ul = 0; ul < 100; ++ul)
        {
            sprintf (szFileName + cbLen, "-%02ld.trp.xml", ul);
            if (access (szFileName, F_OK) == 0)
                continue;
            file = fopen (szFileName, "wt");
            if (file)
                break;
            if (errno == ENAMETOOLONG)
                break;
        }
    }

    /* next, try a time stamp as a 8x3 file name */

    if (file == NULL)
    {
        pszStamp = szFileName + cbDirLen + 1;
        strcat (szFileName, szFile);
        
        ulStamp = time (NULL);
    
        /* In order to add some 'fraction of a second' granularity to the
         * timestamp, we shift it by 5 bits. This gives us 0x07FFFFFF seconds
         * which is a period of approx. 4,25 years. 5 bits in turn give us 32
         * fractions of a second. */
        ulStamp <<= 5;
        
        /* try some few next stamps if the first one fails */
        for (ul = 0; ul < 32; ++ul, ++ulStamp)
        {
            sprintf (pszStamp, "%08lX.xml", ulStamp);
            if (access (szFileName, F_OK) == 0)
                continue;
            file = fopen (szFileName, "wt");
            if (file)
                break;
        }
    }
    
    if (file)
    {
        strncpy (pszFileName, szFileName, CCHMAXPATH - 1);
        pszFileName[CCHMAXPATH - 1] = '\0';
    }
    
    return file;
}

/** \internal
 *  Qt Exception handler.
 */
/* static */
ULONG APIENTRY
QtOS2SysXcptMainHandler::handler (PEXCEPTIONREPORTRECORD pReportRec,
                                  PEXCEPTIONREGISTRATIONRECORD pRegRec,
                                  PCONTEXTRECORD pContextRec,
                                  PVOID pv)
{
    PROCESSINFO Info = {0};

    /* From the VAC++3 docs:
     *      "The first thing an exception handler should do is check the
     *      exception flags. If EH_EXIT_UNWIND is set, meaning
     *      the thread is ending, the handler tells the operating system
     *      to pass the exception to the next exception handler. It does the
     *      same if the EH_UNWINDING flag is set, the flag that indicates
     *      this exception handler is being removed.
     *      The EH_NESTED_CALL flag indicates whether the exception
     *      occurred within an exception handler. If the handler does
     *      not check this flag, recursive exceptions could occur until
     *      there is no stack remaining."
     *
     *      So for all these conditions, we exit immediately.
     */

    if (pReportRec->fHandlerFlags &
        (EH_EXIT_UNWIND | EH_UNWINDING | EH_NESTED_CALL))
        return XCPT_CONTINUE_SEARCH;

    /* setup the process info structure */
    DosGetInfoBlocks (&Info.ptib, &Info.ppib);

    switch (pReportRec->ExceptionNum)
    {
        case XCPT_ACCESS_VIOLATION:
        case XCPT_INTEGER_DIVIDE_BY_ZERO:
        case XCPT_ILLEGAL_INSTRUCTION:
        case XCPT_PRIVILEGED_INSTRUCTION:
        case XCPT_INVALID_LOCK_SEQUENCE:
        case XCPT_INTEGER_OVERFLOW:
        case XCPT_BREAKPOINT:
        {
            /* "real" exceptions: */

            APIRET arc = NO_ERROR;
            PVOID pBuf = NULL;
            
            char szFullExeName [CCHMAXPATH] = "";
            char szBaseExeName [CCHMAXPATH] = "";
            
            TIB2 Tib2Orig;
            DATETIME dt;

            char szFileName [CCHMAXPATH];
            FILE *file = NULL;

            /* raise this thread's priority to do it fast */ 
            Tib2Orig = *Info.ptib->tib_ptib2;
            Info.ptib->tib_ptib2 = &Tib2Orig;
            DosSetPriority (PRTYS_THREAD, PRTYC_REGULAR, PRTYD_MAXIMUM, 0);

            DosBeep (880, 200);

            /* query system state */
            {
                ULONG ulCB = 64 * 1024;
                ULONG cTries = 3;
                while (-- cTries)
                {
                    arc = DosAllocMem (&pBuf, ulCB, PAG_WRITE | PAG_COMMIT);
                    if (arc == NO_ERROR)
                    {
                        memset (pBuf, 0, ulCB);
                        arc = DosQuerySysState (QS_PROCESS | QS_MTE, 0,
                                                Info.ppib->pib_ulpid, 0,
                                                pBuf, ulCB);
                        if (arc == NO_ERROR)
                        {
                            QSPTRREC *pPtrRec = (QSPTRREC *) pBuf;
                            Info.bHaveSysState = TRUE;
                            Info.pProcRec = pPtrRec->pProcRec;
                            Info.pLibRec = pPtrRec->pLibRec;
                        }
                        else if (arc == ERROR_BUFFER_OVERFLOW)
                        {
                            /* retry with some bigger buffer size */
                            DosFreeMem (pBuf);
                            pBuf = NULL;
                            ulCB *= 2;
                            continue;
                        }
                    }
                    break;
                }
            }
        
            /* get the main module name */
            DosQueryModuleName (Info.ppib->pib_hmte,
                                sizeof (szFullExeName), szFullExeName);
            {
                /* find the base name (w/o path and extension) */ 
                char *psz = strrchr (szFullExeName, '\\');
                if (psz)
                    ++ psz;
                if (!psz)
                    psz = szFullExeName;
                strcpy (szBaseExeName, psz);
                psz = strrchr (szBaseExeName, '.');
                if (psz)
                    *psz = '\0';
                Info.pszFullName = szFullExeName;
                Info.pszBaseName = szBaseExeName;
            }
            
            /* compose the desired base file name for the log file
             * (<datetime>-<exe>) */
            DosGetDateTime (&dt);
            sprintf (szFileName, "%04d%02d%02d%02d%02d%02d-%s",
                     dt.year, dt.month, dt.day,
                     dt.hours, dt.minutes, dt.seconds,
                     szBaseExeName);
            
            /* open the log file: */
            
            /* first, try the home directory, then the root drive
             * (see QDir::homeDirPath()) */
            file = qt_excOpenLogFile (getenv ("HOME"), szFileName);
            if (file == NULL)
            {
                char *pszHomeDrive = getenv ("HOMEDRIVE");
                char *pszHomePath = getenv ("HOMEPATH");
                if (pszHomeDrive && pszHomePath &&
                    strlen (pszHomeDrive) + strlen (pszHomePath) < CCHMAXPATH)
                {
                    char szHomePath [CCHMAXPATH];
                    strcpy (szHomePath, pszHomeDrive);
                    strcat (szHomePath, pszHomePath);
                    file = qt_excOpenLogFile (szHomePath, szFileName);
                }
            }
            if (file == NULL)
            {
                char szBootDrive[] = "\0:";
                ULONG ulBootDrive;
                DosQuerySysInfo (QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                                 &ulBootDrive, sizeof(ulBootDrive));
                szBootDrive[0] = (char) ulBootDrive + 'A' - 1;
                file = qt_excOpenLogFile (szBootDrive, szFileName);
            }

            if (file != NULL)
            {
                fprintf (file,
                         "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
                         "<!--\n"
                         "  A fatal error has occured while running this application.\n"
                         "  Please contact the application vendor, describe what happened\n"
                         "  and send the contents of this file saved locally as\n"
                         "  '%s'.\n"
                         "-->\n",
                         szFileName);

                fprintf (file,
                         "<Trap timestamp=\"%04d-%02d-%02dT%02d:%02d:%02d\"" //%c%02d:%02d\""
                              " version=\"1.0\"\n"
                         "      generator=\"Qt Library Version %s\"\n"
                         "      xmlns=\"http://db.hugaida.com/xml/os2/sys/Trap\">\n",
                         dt.year, dt.month, dt.day,
                         dt.hours, dt.minutes, dt.seconds,
                         //(dt.timezone > 0 ? '-' : '+'),
                         //abs (dt.timezone / 60),
                         //abs (dt.timezone % 60),
                         QT_VERSION_STR);

                XcptPvt::file = file;
                
                /* write trap information */
                qt_excWriteException (file, &Info, pReportRec, pContextRec);

                XcptPvt::file = NULL;
                
                fprintf (file, "</Trap>\n\n");
                fclose (file);
                
                /* attempt to display the created file */
                {
                    STARTDATA SData       = {0};
                    PID       pid         = 0;
                    ULONG     ulSessID    = 0;
                    
                    SData.Length  = sizeof (STARTDATA);
                    SData.PgmName = "E.EXE";
                    SData.PgmInputs = szFileName;
                    
                    DosStartSession (&SData, &ulSessID, &pid);
                }
            }
            else
                DosBeep (220, 200);

            if (pBuf != NULL)
                DosFreeMem (pBuf);
            
            /* reset old priority */
            DosSetPriority (PRTYS_THREAD, (Tib2Orig.tib2_ulpri & 0x0F00) >> 8,
                                          (UCHAR) Tib2Orig.tib2_ulpri,
                                          0);
        }
        break;
    }

    /* we never handle the exception ourselves */

    if (Info.ptib->tib_ptib2->tib2_ultid == 1 &&
        QtOS2SysXcptMainHandler::libcHandler != NULL)
    {
        /* we are on the main thread and were installed from qt_init() during
         * QApplication initialization using a hack; pass control back to the
         * LIBC exception handler */
        return QtOS2SysXcptMainHandler::libcHandler (pReportRec, pRegRec,
                                                     pContextRec, pv);
    }
    
    return XCPT_CONTINUE_SEARCH; 
}

#endif // !defined(QT_PM_NO_SYSEXCEPTIONS)
