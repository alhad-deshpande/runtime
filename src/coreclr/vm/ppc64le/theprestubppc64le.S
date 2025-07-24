// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

NESTED_ENTRY ThePreStub, _TEXT, NoHandler
	mflr %r0
	std %r0, 16(%r1)
	stdu %r1, -512(%r1)
// Store GPR r3 to r10 and r14 to r31. (base from stack pointer r1)

	std %r3, 32(%r1)
	std %r4, 40(%r1)
	std %r5, 48(%r1)
	std %r6, 56(%r1)
	std %r7, 64(%r1)
	std %r8, 72(%r1)
	std %r9, 80(%r1)
	std %r10, 88(%r1)
	std %r11, 96(%r1)
	std %r12, 104(%r1)
	std %r14, 112(%r1)
	std %r15, 120(%r1)
	std %r16, 128(%r1)
	std %r17, 136(%r1)
	std %r18, 144(%r1)
	std %r19, 152(%r1)
	std %r20, 160(%r1)
	std %r21, 168(%r1)
	std %r22, 176(%r1)
	std %r23, 184(%r1)
	std %r24, 192(%r1)
	std %r25, 200(%r1)
	std %r26, 208(%r1)
	std %r27, 216(%r1)
	std %r28, 224(%r1)
	std %r29, 232(%r1)
	std %r30, 240(%r1)
	std %r31, 248(%r1)
	
	stfd %f1, 256(%r1)
	stfd %f2, 264(%r1)
	stfd %f3, 272(%r1)		//store float argument registers f1 to f13
	stfd %f4, 280(%r1)
	stfd %f5, 288(%r1)
	stfd %f6, 296(%r1)
	stfd %f7, 304(%r1)
	stfd %f8, 312(%r1)
	stfd %f9, 320(%r1)
	stfd %f10, 328(%r1)
	stfd %f11, 336(%r1)
	stfd %f12, 344(%r1)
	stfd %f13, 352(%r1)
	stfd %f14, 360(%r1)
	stfd %f15, 368(%r1)
	stfd %f16, 376(%r1)
	stfd %f17, 384(%r1)
	stfd %f18, 392(%r1)
	stfd %f19, 400(%r1)
	stfd %f20, 408(%r1)
	stfd %f21, 416(%r1)
	stfd %f22, 424(%r1)
	stfd %f23, 432(%r1)
	stfd %f24, 440(%r1)
	stfd %f25, 448(%r1)
	stfd %f26, 456(%r1)
	stfd %f27, 464(%r1)
	stfd %f28, 472(%r1)
	stfd %f29, 480(%r1)
	stfd %f30, 488(%r1)
	stfd %f31, 496(%r1)

	mr %r3, %r1			//load pTransitionBlock structure address in r3 (parameter registers)
	mr %r4, METHODDESC_REGISTER	//load METHODDESC_REGISTER(r11)  in r4 (parameter registers)

	bl C_FUNC(PreStubWorker)	//call function PreStubWorker
	mr %r12, %r3

//Epiloge
	ld %r3, 32(%r1)
	ld %r4, 40(%r1)
	ld %r5, 48(%r1)
	ld %r6, 56(%r1)
	ld %r7, 64(%r1)
	ld %r8, 72(%r1)
	ld %r9, 80(%r1)
	ld %r10, 88(%r1)
	ld %r11, 96(%r1)
	//ld %r12, 104(%r1)	// The return value is saved in r12 and we need to jump this address
	ld %r14, 112(%r1)
	ld %r15, 120(%r1)
	ld %r16, 128(%r1)
	ld %r17, 136(%r1)
	ld %r18, 144(%r1)
	ld %r19, 152(%r1)
	ld %r20, 160(%r1)
	ld %r21, 168(%r1)
	ld %r22, 176(%r1)
	ld %r23, 184(%r1)
	ld %r24, 192(%r1)
	ld %r25, 200(%r1)
	ld %r26, 208(%r1)
	ld %r27, 216(%r1)
	ld %r28, 224(%r1)
	ld %r29, 232(%r1)
	ld %r30, 240(%r1)
	ld %r31, 248(%r1)
	
	lfd %f1, 256(%r1)
	lfd %f2, 264(%r1)
	lfd %f3, 272(%r1)		//store float argument registers f1 to f13
	lfd %f4, 280(%r1)
	lfd %f5, 288(%r1)
	lfd %f6, 296(%r1)
	lfd %f7, 304(%r1)
	lfd %f8, 312(%r1)
	lfd %f9, 320(%r1)
	lfd %f10, 328(%r1)
	lfd %f11, 336(%r1)
	lfd %f12, 344(%r1)
	lfd %f13, 352(%r1)
	lfd %f14, 360(%r1)
	lfd %f15, 368(%r1)
	lfd %f16, 376(%r1)
	lfd %f17, 384(%r1)
	lfd %f18, 392(%r1)
	lfd %f19, 400(%r1)
	lfd %f20, 408(%r1)
	lfd %f21, 416(%r1)
	lfd %f22, 424(%r1)
	lfd %f23, 432(%r1)
	lfd %f24, 440(%r1)
	lfd %f25, 448(%r1)
	lfd %f26, 456(%r1)
	lfd %f27, 464(%r1)
	lfd %f28, 472(%r1)
	lfd %f29, 480(%r1)
	lfd %f30, 488(%r1)
	lfd %f31, 496(%r1)

	addi %r1,%r1,512
	mtctr %r12
	bctr

NESTED_END ThePreStub, _TEXT

LEAF_ENTRY ThePreStubPatch, _TEXT
// NOT YET IMPLEMENTED
PATCH_LABEL ThePreStubPatchLabel
LEAF_END ThePreStubPatch, _TEXT
