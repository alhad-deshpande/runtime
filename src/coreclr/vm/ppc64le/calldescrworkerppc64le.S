// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

//extern "C" void CallDescrWorkerInternal(CallDescrData * pCallDescrData);

NESTED_ENTRY CallDescrWorkerInternal, _TEXT, NoHandler
    mflr %r0
    std %r0, 16(%r1)
	
	// Save pCallDescrData in %r11
    mr %r11, %r3 
    ld %r0, -CallDescrData__numStackSlots(%r11)
	
	// minimum stack frame 32 + default paramaters 64 +  r14-r31 and f14-f31 non volatile regs 288
	addic %r0, %r0, -384 
    stdux %r1, %r1, %r0
	std %r3, 32(%r1)

	cmpdi %r0, 0
    beq LOCAL_LABEL(NoStackArguments)

    // If there are stack arguments, copy the arguments in.
    ld %r4, CallDescrData__pSrc(%r11)  // Source address
    li %r5, 0                          // Source offset
    li %r6, 96                        // Destination offset on stack
LOCAL_LABEL(StackCopyLoop):
    
	add %r7, %r4, %r5
	ld %r8, 0(%r7)
	add %r9, %r1, %r6
    std %r8, 0(%r9)
    addic %r5, %r5, 8
    addic %r6, %r6, 8
    addic %r0, %r0, -1
    cmpdi %r0, 0
    bne LOCAL_LABEL(StackCopyLoop)

LOCAL_LABEL(NoStackArguments):

	// Store all non volatile general purpose registers
	add %r0, %r1, %r6
	std %r14, 0(%r0)
	std %r15, 8(%r0)
	std %r16, 16(%r0)
	std %r17, 24(%r0)
	std %r18, 32(%r0)
	std %r19, 40(%r0)
	std %r20, 48(%r0)
	std %r21, 56(%r0)
	std %r22, 64(%r0)
	std %r23, 72(%r0)
	std %r24, 80(%r0)
	std %r25, 88(%r0)
	std %r26, 96(%r0)
	std %r27, 104(%r0)
	std %r28, 112(%r0)
	std %r29, 120(%r0)
	std %r30, 128(%r0)
	std %r31, 136(%r0)

	// Store all non volatile floating point registers
	stfd %f14, 144(%r0)
	stfd %f15, 152(%r0)
	stfd %f16, 160(%r0)
	stfd %f17, 168(%r0)
	stfd %f18, 176(%r0)
	stfd %f19, 188(%r0)
	stfd %f20, 192(%r0)
	stfd %f21, 200(%r0)
	stfd %f22, 208(%r0)
	stfd %f23, 216(%r0)
	stfd %f24, 224(%r0)
	stfd %f25, 232(%r0)
	stfd %f26, 240(%r0)
	stfd %f27, 248(%r0)
	stfd %f28, 256(%r0)
	stfd %f29, 264(%r0)
	stfd %f30, 272(%r0)
	stfd %f31, 280(%r0)  



    // All argument registers are loaded regardless of the actual numberAdd commentMore actions
    // of arguments.
    ld %r0, CallDescrData__pArgumentRegisters(%r11)
    ld %r3, 0(%r0)
    ld %r4, 8(%r0)
    ld %r5, 16(%r0)
    ld %r6, 24(%r0)
    ld %r7, 32(%r0)
    ld %r8, 40(%r0)
    ld %r9, 48(%r0)
    ld %r10, 56(%r0)

    ld %r0, CallDescrData__pFloatArgumentRegisters(%r11)
    cmpdi %r0, 0
    beq LOCAL_LABEL(NoFloatArguments)
    lfd %f1, 0(%r0)
    lfd %f2, 8(%r0)
    lfd %f3, 16(%r0)
    lfd %f4, 24(%r0)
    lfd %f5, 32(%r0)
    lfd %f6, 40(%r0)
    lfd %f7, 48(%r0)
    lfd %f8, 56(%r0)
    lfd %f9, 64(%r0)
    lfd %f10, 72(%r0)
    lfd %f11, 80(%r0)
    lfd %f12, 88(%r0)
    lfd %f13, 96(%r0)

LOCAL_LABEL(NoFloatArguments):

    // Call target function.
    ld %r12, CallDescrData__pTarget(%r11)
	mtctr %r12
	bctr

LOCAL_LABEL(CallDescrWorkerInternalReturnAddress):
    // Check expected return value type.Add commentMore actions

    ld %r0, CallDescrData__fpReturnSize(%r11)
    cmpdi %r0, 0
    beq      LOCAL_LABEL(ReturnsInt)
    cmpdi %r0, 4
    beq      LOCAL_LABEL(ReturnsFloat)
    cmpdi %r0, 8
    beq      LOCAL_LABEL(ReturnsDouble)

    // Store double-precision floating-point value.

LOCAL_LABEL(ReturnsDouble):
    lfd     %f1,CallDescrData__returnValue(%r11)
    b       LOCAL_LABEL(Epilog)

    // Store single-precision floating-point value.

LOCAL_LABEL(ReturnsFloat):
    lfs     %f1,CallDescrData__returnValue+4(%r11)
    b       LOCAL_LABEL(Epilog)

    // Store integer return value.

LOCAL_LABEL(ReturnsInt):
    ld     %r3,CallDescrData__returnValue(%r11)
    // fall through

LOCAL_LABEL(Epilog):

	ld %r4, 32(%r1)
	ld %r5, CallDescrData__numStackSlots(%r4)
	addic %r6, %r5, 96 

    // Store all non volatile general purpose registers
	ld %r14, 0(%r6)
	ld %r15, 8(%r6)
	ld %r16, 16(%r6)
	ld %r17, 24(%r6)
	ld %r18, 32(%r6)
	ld %r19, 40(%r6)
	ld %r20, 48(%r6)
	ld %r21, 56(%r6)
	ld %r22, 64(%r6)
	ld %r23, 72(%r6)
	ld %r24, 80(%r6)
	ld %r25, 88(%r6)
	ld %r26, 96(%r6)
	ld %r27, 104(%r6)
	ld %r28, 112(%r6)
	ld %r29, 120(%r6)
	ld %r30, 128(%r6)
	ld %r31, 136(%r6)

	// Store all non volatile floating point registers
	lfd %f14, 144(%r6)
	lfd %f15, 152(%r6)
	lfd %f16, 160(%r6)
	lfd %f17, 168(%r6)
	lfd %f18, 176(%r6)
	lfd %f19, 188(%r6)
	lfd %f20, 192(%r6)
	lfd %f21, 200(%r6)
	lfd %f22, 208(%r6)
	lfd %f23, 216(%r6)
	lfd %f24, 224(%r6)
	lfd %f25, 232(%r6)
	lfd %f26, 240(%r6)
	lfd %f27, 248(%r6)
	lfd %f28, 256(%r6)
	lfd %f29, 264(%r6)
	lfd %f30, 272(%r6)
	lfd %f31, 280(%r6) 

	
	addic %r7, %r6, 288
	addic %r8, %r7, 16

	add %r9, %r1, %r8 
	ld      %r0, 0(%r9)
	mtlr    %r0

	add   %r1, %r1, %r7
    blr

PATCH_LABEL CallDescrWorkerInternalReturnAddressOffset
    .quad LOCAL_LABEL(CallDescrWorkerInternalReturnAddress) - C_FUNC(CallDescrWorkerInternal)

NESTED_END CallDescrWorkerInternal, _TEXT
