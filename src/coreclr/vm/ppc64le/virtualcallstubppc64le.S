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
//   r3:  object reference (this pointer)
//   r10: dispatch token  (or ResolveCacheElem* when PROMOTE_CHAIN_FLAG set)
//   r11: indirection cell | flags  (VSD cell register — virtualStubParamInfo = REG_R11;
//        set by the JIT and preserved unmodified through all dispatch stubs)
// Output:
//   Tail-calls resolved target (via CTR)
// Clobbers: r0, r3-r12 (volatile), callee-saved restored in epilog
//
// VSD_ResolveWorker(pTransitionBlock, siteAddr, token, flags):
//   arg r3 = &TransitionBlock  (= new SP after stdu)
//   arg r4 = indirection cell address  (r11 with low-2 flag bits cleared)
//   arg r5 = dispatch token or ResolveCacheElem*  (from r10)
//   arg r6 = flags  (low 2 bits of r11)
//-----------------------------------------------------------------------------
NESTED_ENTRY ResolveWorkerAsmStub, _TEXT, NoHandler
	std %r2, 24(%r1)                             // save caller's r2 (may be JIT TOC)
	addis %r2, %r12, (.TOC.-ResolveWorkerAsmStub)@ha  // establish libcoreclr.so TOC
	addi  %r2, %r2,  (.TOC.-ResolveWorkerAsmStub)@l
	mflr %r0
	std %r0, 16(%r1)
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

	// Build VSD_ResolveWorker arguments from the VSD cell register (r11):
	mr      %r3, %r1                // arg1: pTransitionBlock = new SP
	rldicl  %r6, %r11, 0, 62       // arg4: flags  = r11 & 3  (low 2 bits)
	rldicr  %r4, %r11, 0, 61       // arg2: cell   = r11 & ~3 (flags stripped)
	mr      %r5, %r10               // arg3: token  = r10

	bl      C_FUNC(VSD_ResolveWorker) // r3 = target PC
	mr      %r12, %r3                 // move target to r12 (r2 unchanged: intra-DSO bl)

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
//
// Inputs:
//   r3:      object reference ('this')  — must be preserved
//   r4–r9:   live argument registers    — must ALL be preserved
//   r10:     dispatch token             (set by ResolveStub miss path [22])
//   r11:     indirection cell | flags   (VSD cell register — never written)
//   48(r1):  ResolveCacheElem* head     (stored by ResolveStub miss path [8])
//
// Only r0 and r12 are used as scratch. r0 cannot be a D-form base register.
// r3–r11 are never written; r4–r9 arrive at the target intact.
//
// Clobbers: r0, r12
//
// ResolveCacheElem layout:
//    0: cached MethodTable
//    8: cached token
//   16: target PC
//   24: next pointer
//
// Stack usage: 56(r1) is used as a temporary scratch slot to hold the cached
// target PC across the counter decrement loop. This falls within the caller's
// parameter save area (owned by the caller) and is safe to use as scratch since
// the JIT-generated caller passes all arguments in registers only.
//-----------------------------------------------------------------------------
LEAF_ENTRY ResolveWorkerChainLookupAsmStub, _TEXT
    // BACKPATCH_FLAG check. andi. needs non-r0 dest — use r12.
    andi.   %r12, %r11, BACKPATCH_FLAG
    bne     C_FUNC(ResolveWorkerAsmStub)

    // r12 = ResolveCacheElem* head (from 48(r1)); r10 = dispatch token (intact).
    ld      %r12, 48(%r1)
.Lprobe:
    cmpdi   cr0, %r12, 0
    beq     C_FUNC(ResolveWorkerAsmStub)    // null → full resolve

    // Compare MethodTable: r0 = actual MT, load cached MT for compare.
    ld      %r0, 0(%r3)                     // r0 = actual MT  [r3=base ✓]
    ld      %r12, 0(%r12)                   // r12 = cached MT [r12=base ✓]
    cmpd    cr0, %r0, %r12
    bne     .Lmt_miss

    // MT matched — reload the node pointer, then compare token.
    ld      %r12, 48(%r1)                   // reload node ptr from stack slot
    ld      %r0, 8(%r12)                    // r0 = cached token [r12=base ✓]
    cmpd    cr0, %r10, %r0                  // r10 = dispatch token (r5,r6 never touched)
    bne     .Ltoken_miss

    // Cache hit. Save target at 56(r1) so lwarx can reuse r0.
    ld      %r0, 16(%r12)                   // r0 = cached target PC
    std     %r0, 56(%r1)                    // stash target  [r1=base ✓]

    // Atomically decrement g_dispatch_cache_chain_success_counter.
    // r12 = counter address; r0 = counter value (lwarx safe: RT=r0 ≠ RA=r12).
    addis   %r12, %r2, g_dispatch_cache_chain_success_counter@toc@ha
    ld      %r12, g_dispatch_cache_chain_success_counter@toc@l(%r12)
.Ldec_retry:
    lwarx   %r0, 0, %r12                    // r0 = counter  [r12=base ✓, r0=dest ✓]
    addi    %r0, %r0, -1
    stwcx.  %r0, 0, %r12
    bne-    .Ldec_retry

    cmpwi   cr0, %r0, 0
    blt     .Lpromote

    // Tail-call: reload target from stash, move to r12 for ABI.
    ld      %r12, 56(%r1)                   // r12 = target PC  [r1=base ✓]
    mtctr   %r12
    bctr                                    // all r4–r9 intact ✓

.Lpromote:
    // Reset counter and set PROMOTE_CHAIN_FLAG; pass ResolveCacheElem* in r10.
    li      %r0, INITIAL_SUCCESS_COUNT
    stw     %r0, 0(%r12)                    // reset counter  [r12=counter addr, r0=value ✓]
    ori     %r11, %r11, PROMOTE_CHAIN_FLAG
    ld      %r10, 48(%r1)                   // r10 = ResolveCacheElem* for worker
    b       C_FUNC(ResolveWorkerAsmStub)

.Lmt_miss:
    // r12 holds cached MT (not node ptr). Reload node, advance to next.
    ld      %r12, 48(%r1)                   // reload current node ptr
.Ltoken_miss:
    ld      %r12, 24(%r12)                  // r12 = next node  [r12=base ✓]
    std     %r12, 48(%r1)                   // update stack slot for next probe
    b       .Lprobe

LEAF_END ResolveWorkerChainLookupAsmStub, _TEXT
