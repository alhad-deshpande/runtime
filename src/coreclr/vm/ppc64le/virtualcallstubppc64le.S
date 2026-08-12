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

	// Build VSD_ResolveWorker arguments from the VSD cell register (r11):
	mr      %r3, %r1                // arg1: pTransitionBlock = new SP
	rldicl  %r6, %r11, 0, 62       // arg4: flags  = r11 & 3  (low 2 bits)
	rldicr  %r4, %r11, 0, 61       // arg2: cell   = r11 & ~3 (flags stripped)
	mr      %r5, %r10               // arg3: token  = r10

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
//   r3:  object reference
//   r5:  dispatch token
//   r10: ResolveCacheElem* head  (written by ResolveStub miss path at 48(r1), then
//        loaded by the caller before branching here)
//   r11: indirection cell | flags  (VSD cell register, preserved by all dispatch stubs)
// Output:
//   Hit:    tail-calls cached target via CTR
//   Miss/promote: branches to ResolveWorkerAsmStub (r11 carries cell|flags, r10 carries
//                 the ResolveCacheElem* on the promote path)
// Clobbers: r0, r6, r9, r12
// ResolveCacheElem layout:
//   0:  cached MethodTable
//   8:  cached token
//  16:  target PC
//  24:  next pointer
//-----------------------------------------------------------------------------
LEAF_ENTRY ResolveWorkerChainLookupAsmStub, _TEXT
    // If BACKPATCH_FLAG is set in r11, go straight to the full resolve worker.
    // r0 is safe to clobber here; andi. must use a non-r0 source — use r6.
    andi.   %r6, %r11, BACKPATCH_FLAG
    bne     C_FUNC(ResolveWorkerAsmStub)

    mr      %r9, %r10                   // r9 = ResolveCacheElem* head
.Lprobe:
    cmpdi   cr0, %r9, 0
    beq     C_FUNC(ResolveWorkerAsmStub) // empty chain → full resolve

    // Compare MethodTable of the live object against cached entry
    ld      %r6, 0(%r3)                 // actual MethodTable from object header
    ld      %r0, 0(%r9)                 // cached MethodTable in this entry
    cmpd    cr0, %r6, %r0
    bne     .Lnext

    // Compare dispatch token
    ld      %r0, 8(%r9)                 // cached token
    cmpd    cr0, %r5, %r0
    bne     .Lnext

    // Cache hit — load target into r12 for tail-call
    ld      %r12, 16(%r9)

    // Atomically decrement g_dispatch_cache_chain_success_counter.
    // Use r6 (volatile, safe to clobber) as the counter address; r0 cannot be a
    // D-form base register on PPC64LE (hardware reads r0 as literal 0 in that field).
    addis   %r6, %r2, g_dispatch_cache_chain_success_counter@toc@ha
    ld      %r6, g_dispatch_cache_chain_success_counter@toc@l(%r6)
.Ldec_retry:
    lwarx   %r0, 0, %r6
    addi    %r0, %r0, -1
    stwcx.  %r0, 0, %r6
    bne-    .Ldec_retry

    // If counter >= 0 → tail-call the cached target; else promote this entry.
    cmpwi   cr0, %r0, 0
    bge     .Ltail_call

    // Promotion path: reset counter and set PROMOTE_CHAIN_FLAG in r11.
    li      %r0, INITIAL_SUCCESS_COUNT
    stw     %r0, 0(%r6)                 // reset (word store; counter is INT32)

    ori     %r11, %r11, PROMOTE_CHAIN_FLAG // mark promote in VSD cell register

    mr      %r10, %r9                   // pass ResolveCacheElem* in r10 to worker
    b       C_FUNC(ResolveWorkerAsmStub)

.Lnext:
    ld      %r9, 24(%r9)                // advance to next node
    b       .Lprobe

.Ltail_call:
    // PPC64LE ELF v2 ABI: r12 must equal the branch target before bctr.
    // r12 already holds the cached target (loaded at the cache-hit path above).
    mtctr   %r12
    bctr
LEAF_END ResolveWorkerChainLookupAsmStub, _TEXT
