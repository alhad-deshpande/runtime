// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"

// This is the number of times a successful chain lookup will occur before the
// entry is promoted to the front of the chain. This is declared as extern because
// the default value (CALL_STUB_CACHE_INITIAL_SUCCESS_COUNT) is defined in the header.
// extern size_t g_dispatch_cache_chain_success_counter;
#define CHAIN_SUCCESS_COUNTER g_dispatch_cache_chain_success_counter

#define BACKPATCH_FLAG             1        // Also known as SDF_ResolveBackPatch    in the EE
#define PROMOTE_CHAIN_FLAG         2        // Also known as SDF_ResolvePromoteChain in the EE
#define INITIAL_SUCCESS_COUNT      0x100

.machine "ppc64le"
//-----------------------------------------------------------------------------
// ResolveWorkerAsmStub
// Inputs:
//   r3: object reference
//   r5: dispatch token (or ResolveCacheElem* when PROMOTE_CHAIN_FLAG set)
//   40(r1): indirection cell | flags  (caller wrote at 8(r1))
// Output:
//   Tail-calls target (via CTR)
// Clobbers: r0, r3-r12 (volatile only)
// VSD_ResolveWorker args: r3=obj, r4=cell, r5=token|node, r7=flags
//-----------------------------------------------------------------------------
NESTED_ENTRY ResolveWorkerAsmStub, _TEXT, NoHandler
	mflr %r0
	std %r0, 16(%r1)
	std %r2, 24(%r1)
	std %r31, -8(%r1)
	stdu %r1, -496(%r1)
	.cfi_def_cfa_offset 496
	.cfi_offset 65, 16
	.cfi_offset 31, -8
	mr %r31, %r1
	.cfi_def_cfa_register 31

	std %r3, 32(%r1)
	std %r4, 40(%r1)
	std %r5, 48(%r1)
	std %r6, 56(%r1)
	std %r7, 64(%r1)
	std %r8, 72(%r1)
	std %r9, 80(%r1)
	std %r10, 88(%r1)

	stfd %f1, 96(%r1)
	stfd %f2, 104(%r1)
	stfd %f3, 112(%r1)
	stfd %f4, 120(%r1)
	stfd %f5, 128(%r1)
	stfd %f6, 136(%r1)
	stfd %f7, 144(%r1)
	stfd %f8, 152(%r1)
	stfd %f9, 160(%r1)
	stfd %f10, 168(%r1)
	stfd %f11, 176(%r1)
	stfd %f12, 184(%r1)
	stfd %f13, 192(%r1)

	std %r14, 200(%r1)
	std %r15, 208(%r1)
	std %r16, 216(%r1)
	std %r17, 224(%r1)
	std %r18, 232(%r1)
	std %r19, 240(%r1)
	std %r20, 248(%r1)
	std %r21, 256(%r1)
	std %r22, 264(%r1)
	std %r23, 272(%r1)
	std %r24, 280(%r1)
	std %r25, 288(%r1)
	std %r26, 296(%r1)
	std %r27, 304(%r1)
	std %r28, 312(%r1)
	std %r29, 320(%r1)
	std %r30, 328(%r1)
	std %r31, 336(%r1)

	stfd %f14, 344(%r1)
	stfd %f15, 352(%r1)
	stfd %f16, 360(%r1)
	stfd %f17, 368(%r1)
	stfd %f18, 376(%r1)
	stfd %f19, 384(%r1)
	stfd %f20, 392(%r1)
	stfd %f21, 400(%r1)
	stfd %f22, 408(%r1)
	stfd %f23, 416(%r1)
	stfd %f24, 424(%r1)
	stfd %f25, 432(%r1)
	stfd %f26, 440(%r1)
	stfd %f27, 448(%r1)
	stfd %f28, 456(%r1)
	stfd %f29, 464(%r1)
	stfd %f30, 472(%r1)
	stfd %f31, 480(%r1)

	mr %r3, %r1
	mr %r4, %r11
	mr %r5, %r10
	rldicl %r6, %r4, 0, 62

	bl      C_FUNC(VSD_ResolveWorker) // r3 = target PC
	mr      %r12, %r3                   // move target to r12

//Epiloge
	ld %r3, 32(%r1)
	ld %r4, 40(%r1)
	ld %r5, 48(%r1)
	ld %r6, 56(%r1)
	ld %r7, 64(%r1)
	ld %r8, 72(%r1)
	ld %r9, 80(%r1)
	ld %r10, 88(%r1)

	lfd %f1, 96(%r1)
	lfd %f2, 104(%r1)
	lfd %f3, 112(%r1)
	lfd %f4, 120(%r1)
	lfd %f5, 128(%r1)
	lfd %f6, 136(%r1)
	lfd %f7, 144(%r1)
	lfd %f8, 152(%r1)
	lfd %f9, 160(%r1)
	lfd %f10, 168(%r1)
	lfd %f11, 176(%r1)
	lfd %f12, 184(%r1)
	lfd %f13, 192(%r1)

	ld %r14, 200(%r1)
	ld %r15, 208(%r1)
	ld %r16, 216(%r1)
	ld %r17, 224(%r1)
	ld %r18, 232(%r1)
	ld %r19, 240(%r1)
	ld %r20, 248(%r1)
	ld %r21, 256(%r1)
	ld %r22, 264(%r1)
	ld %r23, 272(%r1)
	ld %r24, 280(%r1)
	ld %r25, 288(%r1)
	ld %r26, 296(%r1)
	ld %r27, 304(%r1)
	ld %r28, 312(%r1)
	ld %r29, 320(%r1)
	ld %r30, 328(%r1)
	ld %r31, 336(%r1)

	lfd %f14, 344(%r1)
	lfd %f15, 352(%r1)
	lfd %f16, 360(%r1)
	lfd %f17, 368(%r1)
	lfd %f18, 376(%r1)
	lfd %f19, 384(%r1)
	lfd %f20, 392(%r1)
	lfd %f21, 400(%r1)
	lfd %f22, 408(%r1)
	lfd %f23, 416(%r1)
	lfd %f24, 424(%r1)
	lfd %f25, 432(%r1)
	lfd %f26, 440(%r1)
	lfd %f27, 448(%r1)
	lfd %f28, 456(%r1)
	lfd %f29, 464(%r1)
	lfd %f30, 472(%r1)
	lfd %f31, 480(%r1)

	addi %r1, %r1, 496
	.cfi_def_cfa %r1, 0
	ld %r0, 16(%r1)
	ld %r2, 24(%r1)
	mtlr %r0
	ld %r31, -8(%r1)
	mtctr %r12
	bctr

NESTED_END ResolveWorkerAsmStub, _TEXT


//-----------------------------------------------------------------------------
// ResolveWorkerChainLookupAsmStub
// Inputs:
//   r3: object reference
//   r5: dispatch token
//   8(r1): indirection cell | flags
//   16(r1): ResolveCacheElem* head
// Output:
//   Hit:    tail-calls cached target
//   Miss/promote: jumps to ResolveWorkerAsmStub
// Clobbers: r0, r3-r12 (volatile only)
// ResolveCacheElem layout assumed:
//   0: cached MethodTable
//   8: cached token
//  16: target PC
//  24: next (ResolveCacheElem*)
//-----------------------------------------------------------------------------
LEAF_ENTRY ResolveWorkerChainLookupAsmStub, _TEXT
    // If BACKPATCH_FLAG set, go straight to worker
    ld      %r0, 8(%r1)                 // cell|flags
    andi.   %r0, %r0, BACKPATCH_FLAG
    bne     ResolveWorkerAsmStub

    ld      %r9, 16(%r1)                // r9 = head
.Lprobe:
    cmpdi   cr0, %r9, 0
    beq     ResolveWorkerAsmStub      // empty chain → resolve

    // Compare MethodTable
    ld      %r6, 0(%r3)                 // actual MethodTable
    ld      %r0, 0(%r9)                 // cached MethodTable
    cmpd    cr0, %r6, %r0
    bne     .Lnext

    // Compare token
    ld      %r0, 8(%r9)                 // cached token
    cmpd    cr0, %r5, %r0
    bne     .Lnext

    // Cache hit: r12 = target
    ld      %r12, 16(%r9)

    // Decrement global success counter atomically
    addis  %r11, %r2, g_dispatch_cache_chain_success_counter@toc@ha
    ld     %r11, g_dispatch_cache_chain_success_counter@toc@l(%r11)
.Ldec_retry:
    lwarx   %r0, 0, %r11
    addi    %r0, %r0, -1
    stwcx.  %r0, 0, %r11
    bne-    .Ldec_retry

    // If counter >= 0 → tail-call; else promote chain
    cmpwi   cr0, %r0, 0
    bge     .Ltail_call

    // Promotion path: reset counter, set PROMOTE_CHAIN_FLAG in caller slot
    li      %r0, INITIAL_SUCCESS_COUNT
    std     %r0, 0(%r11)

    ld      %r0, 8(%r1)                 // cell|flags (caller frame)
    ori     %r0, %r0, PROMOTE_CHAIN_FLAG
    std     %r0, 8(%r1)

    mr      %r5, %r9                    // r5 = ResolveCacheElem*
    b       ResolveWorkerAsmStub

.Lnext:
    ld      %r9, 24(%r9)                // advance to next
    b       .Lprobe

.Ltail_call:
    mtctr   %r12
    bctr
LEAF_END ResolveWorkerChainLookupAsmStub, _TEXT
