// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "pal/dbgmsg.h"
SET_DEFAULT_DEBUG_CHANNEL(EXCEPT); // some headers have code with asserts, so do this first

#include "pal/palinternal.h"
#include "pal/context.h"
#include "pal/signal.hpp"
#include "pal/utils.h"
#include <sys/ucontext.h>

/*++
Function :
    ExecuteHandlerOnCustomStack

    Execute signal handler on a custom stack, the current stack pointer is specified by the customSp
    If the customSp is 0, then the handler is executed on the original stack where the signal was fired.
    It installs a fake stack frame to enable stack unwinding to the signal source location.

Parameters :
    POSIX signal handler parameter list ("man sigaction" for details)
    returnPoint - context to which the function returns if the common_signal_handler returns
    
    (no return value)
--*/
void ExecuteHandlerOnCustomStack(int code, siginfo_t *siginfo, void *context, size_t customSp, SignalHandlerWorkerReturnPoint* returnPoint)
{
    ucontext_t *ucontext = (ucontext_t *)context;
    size_t faultSp = (size_t)MCREG_R1(ucontext->uc_mcontext);
    _ASSERTE(IS_ALIGNED(faultSp, 8));

    if (customSp == 0)
    {
        // ELFv2 (PPC64LE ABI) defines no red zone: the region below SP is not
        // reserved and may be clobbered at any time.  Use faultSp directly,
        // rounded down to the required 16-byte stack alignment.
        customSp = ALIGN_DOWN(faultSp, 16);
    }

    size_t fakeFrameReturnAddress;
    if (IS_ALIGNED(faultSp, 16))
    {
        fakeFrameReturnAddress = (size_t)SignalHandlerWorkerReturnOffset0 + (size_t)CallSignalHandlerWrapper0;
    }
    else
    {
        fakeFrameReturnAddress = (size_t)SignalHandlerWorkerReturnOffset8 + (size_t)CallSignalHandlerWrapper8;
    }

    // Build a fake ELFv2 stack frame so that the stack unwinder can walk from
    // signal_handler_worker back to the faulting instruction.
    //
    // ELFv2 frame layout (32-byte minimum frame):
    //   sp+ 0 : back-chain word  → previous SP (faultSp)
    //   sp+16 : saved LR slot   → faulting PC (for the unwinder)
    //
    // We allocate 32 bytes below customSp to hold this frame.
    size_t* saveArea = (size_t*)(customSp - 32);
    saveArea[0] = faultSp;                                   // back-chain
    saveArea[2] = (size_t)MCREG_Nip(ucontext->uc_mcontext); // saved LR = faulting PC
    size_t sp = customSp - 32;

    // Switch execution to signal_handler_worker on the custom stack.
    //
    // RtlRestoreContext on PPC64LE:
    //   - loads CONTEXT.R0 into r0, then executes  mtlr r0  → sets LR
    //   - loads CONTEXT_NIP into CTR, then branches via  bctr
    //
    // Therefore:
    //   Nip  = signal_handler_worker  (the branch target, dispatched via bctr)
    //   R0   = fakeFrameReturnAddress (put into LR so the unwinder finds it)
    CONTEXT context2;
    RtlCaptureContext(&context2);

    context2.Nip = (size_t)signal_handler_worker;
    context2.R0  = fakeFrameReturnAddress;
    context2.R1  = sp;
    context2.R3  = code;
    context2.R4  = (size_t)siginfo;
    context2.R5  = (size_t)context;
    context2.R6  = (size_t)returnPoint;

    RtlRestoreContext(&context2, NULL);
}
