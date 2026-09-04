// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

/*  EXCEP.CPP
 *
 */
//

#include "common.h"

#include "frames.h"
#include "threads.h"
#include "excep.h"
#include "object.h"
#include "field.h"
#include "dbginterface.h"
#include "cgensys.h"
#include "comutilnative.h"
#include "sigformat.h"
#include "siginfo.hpp"
#include "gcheaputilities.h"
#include "eedbginterfaceimpl.h" //so we can clearexception in COMPlusThrow
#include "asmconstants.h"

#include "exceptionhandling.h"
#include "virtualcallstub.h"

#if !defined(DACCESS_COMPILE)

VOID ResetCurrentContext()
{
    LIMITED_METHOD_CONTRACT;
}

LONG CLRNoCatchHandler(EXCEPTION_POINTERS* pExceptionInfo, PVOID pv)
{
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif // !DACCESS_COMPILE

PTR_CONTEXT GetCONTEXTFromRedirectedStubStackFrame(DISPATCHER_CONTEXT * pDispatcherContext)
{
    _ASSERTE(!"PPC64LE:NYI GetCONTEXTFromRedirectedStubStackFrame");
    return NULL;
}

PTR_CONTEXT GetCONTEXTFromRedirectedStubStackFrame(CONTEXT * pContext)
{
    _ASSERTE(!"PPC64LE:NYI GetCONTEXTFromRedirectedStubStackFrame");
    return NULL;
}

#if !defined(DACCESS_COMPILE)

FaultingExceptionFrame *GetFrameFromRedirectedStubStackFrame (DISPATCHER_CONTEXT *pDispatcherContext)
{
    _ASSERTE(!"PPC64LE:NYI GetFrameFromRedirectedStubStackFrame");
    return NULL;
}

#endif // !DACCESS_COMPILE

#ifndef DACCESS_COMPILE
// Returns TRUE if caller should resume execution.
BOOL
AdjustContextForVirtualStub(
        EXCEPTION_RECORD *pExceptionRecord,
	CONTEXT *pContext)
{
    LIMITED_METHOD_CONTRACT;

    Thread * pThread = GetThreadNULLOk();

    // We may not have a managed thread object. Example is an AV on the helper thread.
    // (perhaps during StubManager::IsStub)
    if (pThread == NULL)
    {
        return FALSE;
    }

    PCODE f_IP = GetIP(pContext);

    StubCodeBlockKind sk = RangeSectionStubManager::GetStubKind(f_IP);

    if (sk == STUB_CODE_BLOCK_VSD_DISPATCH_STUB)
    {
        if (*PTR_DWORD(f_IP) != DISPATCH_STUB_FIRST_DWORD)
	{
            _ASSERTE(!"AV in DispatchStub at unknown instruction");
	    return FALSE;
	}
    }
    else
    if (sk == STUB_CODE_BLOCK_VSD_RESOLVE_STUB)
    {
        // PPC64LE ResolveStub has three distinct opcode groups that can fault:
        //   opcode 58 (0x3A) — ld/ldu/lwa  : null-this MT load, cache-slot loads
        //   opcode 31 (0x1F) — extended ops : lwarx (counter decrement retry loop)
        //   opcode 62 (0x3E) — std          : store to stack (std r10,48(r1))
        DWORD instr = *PTR_DWORD(f_IP);
        DWORD opcode = instr >> 26;
        if (opcode != 58 &&   // ld/ldu/lwa
            opcode != 31 &&   // lwarx (and other X-form loads)
            opcode != 62)     // std/stdu
        {
            _ASSERTE(!"AV in ResolveStub at unknown instruction");
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }

    // pContext->Link holds the LR as captured by the kernel signal handler —
    // the return address back to the JIT call site (NIA of bl + 4).
    // Adjust by -4 to point at the bl instruction itself.
    PCODE callsite = GetAdjustedCallAddress((PCODE)pContext->Link);

    if (pExceptionRecord != NULL)
    {
        pExceptionRecord->ExceptionAddress = (PVOID)callsite;
    }

    SetIP(pContext, callsite);

    return TRUE;
}

#endif
