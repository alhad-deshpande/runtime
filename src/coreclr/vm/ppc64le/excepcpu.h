// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//

// 
// EXCEPCPU.H -
//
// This header file is included from Excep.h if the target platform is PPC64LE
//


#ifndef __excepppc64le_h__
#define __excepppc64le_h__

#include "corerror.h"  // HResults for the COM+ Runtime

#include "../dlls/mscorrc/resource.h"

class FaultingExceptionFrame;


#define THROW_CONTROL_FOR_THREAD_FUNCTION  RedirectForThrowControl
EXTERN_C void RedirectForThrowControl();

#define STATUS_CLR_GCCOVER_CODE         STATUS_PRIVILEGED_INSTRUCTION

//
// No FS:0, nothing to do.
//
#define INSTALL_EXCEPTION_HANDLING_RECORD(record)
#define UNINSTALL_EXCEPTION_HANDLING_RECORD(record)

//
// On Win64, the COMPlusFrameHandler's work is done by our personality routine.
//
#define DECLARE_CPFH_EH_RECORD(pCurThread)

//
// Retrieves the redirected CONTEXT* from the stack frame of one of the
// RedirectedHandledJITCaseForXXX_Stub's.
//
PTR_CONTEXT GetCONTEXTFromRedirectedStubStackFrame(DISPATCHER_CONTEXT * pDispatcherContext);
PTR_CONTEXT GetCONTEXTFromRedirectedStubStackFrame(CONTEXT * pContext);

//
// Retrieves the FaultingExceptionFrame* from the stack frame of
// RedirectForThrowControl.
//
FaultingExceptionFrame *GetFrameFromRedirectedStubStackFrame (DISPATCHER_CONTEXT *pDispatcherContext);

inline
PCODE GetAdjustedCallAddress(PCODE returnAddress)
{
    LIMITED_METHOD_CONTRACT;

    // PPC64LE bl instruction is 4 bytes; LR holds return address (NIA of bl + 4),
    // so the call site is 4 bytes before the return address.
    return returnAddress - 4;
}

BOOL AdjustContextForVirtualStub(EXCEPTION_RECORD *pExceptionRecord, CONTEXT *pContext);

#endif // __excepppc64le_h__

