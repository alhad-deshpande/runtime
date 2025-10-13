// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

//extern "C" void CallDescrWorkerInternal(CallDescrData * pCallDescrData);

NESTED_ENTRY CallDescrWorkerInternal, _TEXT, NoHandler
    mflr %r0
    std %r0, 16(%r1)
    std %r2, 24(%r1)
	
	// Save pCallDescrData in %r11
    //mr %r11, %r3 
    lwa %r0, CallDescrData__numStackSlots(%r3)
	li %r4, 2
	divd %r4, %r0, %r4
	mulli %r4, %r4, 2
	sub %r4, %r0, %r4
	cmpdi %r4, 0
	beq LOCAL_LABEL(NO_STACK_PADDING)
	addic %r0, %r0, 1


LOCAL_LABEL(NO_STACK_PADDING):
	li %r4, 0
	sub %r4, %r4, %r0
	mulli %r4, %r4, 8
	
	// minimum stack frame 32 + default paramaters 64
	addic %r4, %r4, -96 //-96
    stdux %r1, %r1, %r4
	std %r3, 32(%r1)

/*LOCAL_LABEL(StoreCalleeSavedRegisters):
	// store callee saved registers
    	std %r14, 96(%r1)
    	std %r15, 104(%r1)
    	std %r16, 112(%r1)
    	std %r17, 120(%r1)
    	std %r18, 128(%r1)
    	std %r19, 136(%r1)
    	std %r20, 144(%r1)
    	std %r21, 152(%r1)
    	std %r22, 160(%r1)
    	std %r23, 168(%r1)
    	std %r24, 176(%r1)
    	std %r25, 184(%r1)
    	std %r26, 192(%r1)
    	std %r27, 200(%r1)
    	std %r28, 208(%r1)
    	std %r29, 216(%r1)
    	std %r30, 224(%r1)
    	std %r31, 232(%r1)

	// store floating point callee saved registers
    	stfd %f14, 240(%r1)
    	stfd %f15, 248(%r1)
    	stfd %f16, 256(%r1)
    	stfd %f17, 264(%r1)
    	stfd %f18, 272(%r1)
    	stfd %f19, 280(%r1)
    	stfd %f20, 288(%r1)
    	stfd %f21, 296(%r1)
    	stfd %f22, 304(%r1)
    	stfd %f23, 312(%r1)
    	stfd %f24, 320(%r1)
    	stfd %f25, 328(%r1)
    	stfd %f26, 336(%r1)
    	stfd %f27, 344(%r1)
    	stfd %f28, 352(%r1)
    	stfd %f29, 360(%r1)
    	stfd %f30, 368(%r1)
    	stfd %f31, 376(%r1) */

	// If there are stack arguments, copy the arguments in.
    ld %r4, CallDescrData__pSrc(%r3)  // Source address
    li %r5, 0                          // Source offset
	li %r6, 96                        // Destination offset on stack
	cmpdi %r0, 0
    beq LOCAL_LABEL(NoStackArguments)
    
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

    // All argument registers are loaded regardless of the actual number
    // of arguments.
    ld %r12, CallDescrData__pArgumentRegisters(%r3)
    ld %r3, 0(%r12)
    std %r3, 40(%r1) // Backup r3 value to be passed as an argument value
    ld %r4, 8(%r12)
    ld %r5, 16(%r12)
    ld %r6, 24(%r12)
    ld %r7, 32(%r12)
    ld %r8, 40(%r12)
    ld %r9, 48(%r12)
    ld %r10, 56(%r12)

    ld %r3, 32(%r1) // Restore original r3 argument
    ld %r12, CallDescrData__pFloatArgumentRegisters(%r3)
    cmpdi %r12, 0
    beq LOCAL_LABEL(NoFloatArguments)
    lfd %f1, 0(%r12)
    lfd %f2, 8(%r12)
    lfd %f3, 16(%r12)
    lfd %f4, 24(%r12)
    lfd %f5, 32(%r12)
    lfd %f6, 40(%r12)
    lfd %f7, 48(%r12)
    lfd %f8, 56(%r12)
    lfd %f9, 64(%r12)
    lfd %f10, 72(%r12)
    lfd %f11, 80(%r12)
    lfd %f12, 88(%r12)
    lfd %f13, 96(%r12)

LOCAL_LABEL(NoFloatArguments):

    // Call target function.
    ld %r12, CallDescrData__pTarget(%r3)
    ld %r3, 40(%r1) //Restore r3 value to be passed as an argument value from stack
	mtctr %r12
	bctrl

LOCAL_LABEL(CallDescrWorkerInternalReturnAddress):
    // Check expected return value type.
    ld %r4, 32(%r1)
    ld %r0, CallDescrData__fpReturnSize(%r4)
    cmpdi %r0, 0
    beq      LOCAL_LABEL(ReturnsInt)
    cmpdi %r0, 4
    beq      LOCAL_LABEL(ReturnsFloat)
    cmpdi %r0, 8
    beq      LOCAL_LABEL(ReturnsDouble)

    // Store double-precision floating-point value.

LOCAL_LABEL(ReturnsDouble):
    lfd     %f1,CallDescrData__returnValue(%r4)
    b       LOCAL_LABEL(Epilog)

    // Store single-precision floating-point value.

LOCAL_LABEL(ReturnsFloat):
    lfs     %f1,CallDescrData__returnValue+4(%r4)
    b       LOCAL_LABEL(Epilog)

    // Store integer return value.

LOCAL_LABEL(ReturnsInt):
    ld     %r3,CallDescrData__returnValue(%r4)
    // fall through

LOCAL_LABEL(Epilog):

	lwa %r5, CallDescrData__numStackSlots(%r4)
	li %r6, 2
	divd %r6, %r5, %r6
	mulli %r6, %r6, 2
	sub %r6, %r5, %r6
	cmpdi %r6, 0
	beq LOCAL_LABEL(NO_STACK_PADDING_EPILOG)
	addic %r5, %r5, 1

LOCAL_LABEL(NO_STACK_PADDING_EPILOG):
	mulli %r5, %r5, 8
	addic %r6, %r5, 96 	//stack size 96
	//addic %r7, %r6, 16	// restoring r0
	//addic %r9, %r6, 24	// restoring r2 (toc)
	
	//restore callee saved registers
    	/*ld %r14, 96(%r1)
    	ld %r15, 104(%r1)
    	ld %r16, 112(%r1)
    	ld %r17, 120(%r1)
    	ld %r18, 128(%r1)
    	ld %r19, 136(%r1)
    	ld %r20, 144(%r1)
    	ld %r21, 152(%r1)
    	ld %r22, 160(%r1)
    	ld %r23, 168(%r1)
    	ld %r24, 176(%r1)
    	ld %r25, 184(%r1)
    	ld %r26, 192(%r1)
    	ld %r27, 200(%r1)
    	ld %r28, 208(%r1)
    	ld %r29, 216(%r1)
    	ld %r30, 224(%r1)
    	ld %r31, 232(%r1)

	// restore floating point callee saved registers
    	lfd %f14, 240(%r1)
    	lfd %f15, 248(%r1)
    	lfd %f16, 256(%r1)
    	lfd %f17, 264(%r1)
    	lfd %f18, 272(%r1)
    	lfd %f19, 280(%r1)
    	lfd %f20, 288(%r1)
    	lfd %f21, 296(%r1)
    	lfd %f22, 304(%r1)
    	lfd %f23, 312(%r1)
    	lfd %f24, 320(%r1)
    	lfd %f25, 328(%r1)
    	lfd %f26, 336(%r1)
    	lfd %f27, 344(%r1)
    	lfd %f28, 352(%r1)
    	lfd %f29, 360(%r1)
    	lfd %f30, 368(%r1)
    	lfd %f31, 376(%r1) */

	add   %r1, %r1, %r6
	
	//add %r8, %r1, %r7 
	ld      %r2, 24(%r1)
	ld      %r0, 16(%r1)
	mtlr    %r0
        blr

PATCH_LABEL CallDescrWorkerInternalReturnAddressOffset
    .quad LOCAL_LABEL(CallDescrWorkerInternalReturnAddress) - C_FUNC(CallDescrWorkerInternal)

NESTED_END CallDescrWorkerInternal, _TEXT
