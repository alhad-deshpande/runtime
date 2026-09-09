# Licensed to the .NET Foundation under one or more agreements.
# The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

# EXTERN_C void ProfileEnterNaked(FunctionIDOrClientID functionIDOrClientID, size_t profiledRsp);
# <NOTE>
#
# </NOTE>
NESTED_ENTRY ProfileEnterNaked, _TEXT, NoHandler
# NOT YET IMPLEMENTED
    bl abort
    nop
NESTED_END ProfileEnterNaked, _TEXT

# EXTERN_C void ProfileLeaveNaked(FunctionIDOrClientID functionIDOrClientID, size_t profiledRsp);
# <NOTE>
#
# </NOTE>
NESTED_ENTRY ProfileLeaveNaked, _TEXT, NoHandler
# NOT YET IMPLEMENTED
    bl abort
    nop
NESTED_END ProfileLeaveNaked, _TEXT

# EXTERN_C void ProfileTailcallNaked(FunctionIDOrClientID functionIDOrClientID, size_t profiledRsp);
# <NOTE>
#
# </NOTE>
NESTED_ENTRY ProfileTailcallNaked, _TEXT, NoHandler
# NOT YET IMPLEMENTED
    bl abort
    nop
NESTED_END ProfileTailcallNaked, _TEXT


LEAF_ENTRY JIT_ProfilerEnterLeaveTailcallStub, _TEXT
    # NOT YET IMPLEMENTED
    bl abort
    nop
LEAF_END JIT_ProfilerEnterLeaveTailcallStub, _TEXT

LEAF_ENTRY JIT_GetDynamicNonGCStaticBase_SingleAppDomain, _TEXT
    # NOT YET IMPLEMENTED
    bl abort
    nop
LEAF_END JIT_GetDynamicNonGCStaticBase_SingleAppDomain, _TEXT


LEAF_ENTRY JIT_GetDynamicGCStaticBase_SingleAppDomain, _TEXT
    # NOT YET IMPLEMENTED
    bl abort
    nop
LEAF_END JIT_GetDynamicGCStaticBase_SingleAppDomain, _TEXT

# ------------------------------------------------------------------
# CallEHFunclet
#
# Calls a catch/finally/fault funclet after restoring the managed
# parent frame's nonvolatile GPR state.
#
# PPC64LE ELFv2 arguments:
#   r3 = throwable object (NULL for finally/fault)
#   r4 = funclet entry point
#   r5 = &CONTEXT.R14
#   r6 = address where this helper's SP is stored
#
# Returns:
#   r3 = resume PC returned by the funclet
#
NESTED_ENTRY CallEHFunclet, _TEXT, NoHandler

    # Preserve this helper's LR and TOC in its caller linkage area.
    mflr    %r0
    std     %r0, 16(%r1)
    std     %r2, 24(%r1)

    # 32-byte linkage area + 18 saved GPRs (R14-R31).
    stdu    %r1, -176(%r1)

    # Save this helper's nonvolatile GPRs before replacing them with
    # the managed parent values from CONTEXT.
    std     %r14,  32(%r1)
    std     %r15,  40(%r1)
    std     %r16,  48(%r1)
    std     %r17,  56(%r1)
    std     %r18,  64(%r1)
    std     %r19,  72(%r1)
    std     %r20,  80(%r1)
    std     %r21,  88(%r1)
    std     %r22,  96(%r1)
    std     %r23, 104(%r1)
    std     %r24, 112(%r1)
    std     %r25, 120(%r1)
    std     %r26, 128(%r1)
    std     %r27, 136(%r1)
    std     %r28, 144(%r1)
    std     %r29, 152(%r1)
    std     %r30, 160(%r1)
    std     %r31, 168(%r1)

    # CallHandler needs this value while the funclet is active.
    std     %r1, 0(%r6)

    # r5 = &CONTEXT.R14.
    # R14-R31 are contiguous, 8 bytes each.
    ld      %r14,   0(%r5)
    ld      %r15,   8(%r5)
    ld      %r16,  16(%r5)
    ld      %r17,  24(%r5)
    ld      %r18,  32(%r5)
    ld      %r19,  40(%r5)
    ld      %r20,  48(%r5)
    ld      %r21,  56(%r5)
    ld      %r22,  64(%r5)
    ld      %r23,  72(%r5)
    ld      %r24,  80(%r5)
    ld      %r25,  88(%r5)
    ld      %r26,  96(%r5)
    ld      %r27, 104(%r5)
    ld      %r28, 112(%r5)
    ld      %r29, 120(%r5)
    ld      %r30, 128(%r5)
    ld      %r31, 136(%r5)

    # CONTEXT.R2 is 96 bytes before CONTEXT.R14.
    # Restore the managed parent's TOC value before entering the JIT funclet.
    ld      %r2, -96(%r5)

    # ELFv2 indirect-call convention: put target in r12 and CTR.
    # r3 remains the throwable expected by the JIT catch.
    mr      %r12, %r4
    mtctr   %r12
    bctrl

    # Funclet returns with r1 restored to this helper's SP.
    # Restore this helper's original nonvolatile state.
    ld      %r14,  32(%r1)
    ld      %r15,  40(%r1)
    ld      %r16,  48(%r1)
    ld      %r17,  56(%r1)
    ld      %r18,  64(%r1)
    ld      %r19,  72(%r1)
    ld      %r20,  80(%r1)
    ld      %r21,  88(%r1)
    ld      %r22,  96(%r1)
    ld      %r23, 104(%r1)
    ld      %r24, 112(%r1)
    ld      %r25, 120(%r1)
    ld      %r26, 128(%r1)
    ld      %r27, 136(%r1)
    ld      %r28, 144(%r1)
    ld      %r29, 152(%r1)
    ld      %r30, 160(%r1)
    ld      %r31, 168(%r1)

    addi    %r1, %r1, 176

    ld      %r2, 24(%r1)
    ld      %r0, 16(%r1)
    mtlr    %r0
    blr

NESTED_END CallEHFunclet, _TEXT

# ------------------------------------------------------------------
# CallEHFilterFunclet
#
# Calls an EH filter funclet.
#
# PPC64LE ELFv2 arguments:
#   r3 = throwable object
#   r4 = CallerSP of the dynamically containing frame
#   r5 = filter funclet entry point
#   r6 = address where this helper's SP is stored
#
# Returns:
#   r3 = filter result
#
NESTED_ENTRY CallEHFilterFunclet, _TEXT, NoHandler

    # Preserve this helper's LR and TOC.
    mflr    %r0
    std     %r0, 16(%r1)
    std     %r2, 24(%r1)

    # Minimal PPC64LE linkage frame.
    stdu    %r1, -32(%r1)

    # Record this helper's SP for the VM.
    std     %r1, 0(%r6)

    # r3 remains throwable.
    # r4 remains CallerSP.
    # ELFv2 indirect-call target convention.
    mr      %r12, %r5
    mtctr   %r12
    bctrl

    # Restore helper state.
    addi    %r1, %r1, 32
    ld      %r2, 24(%r1)
    ld      %r0, 16(%r1)
    mtlr    %r0
    blr

NESTED_END CallEHFilterFunclet, _TEXT
