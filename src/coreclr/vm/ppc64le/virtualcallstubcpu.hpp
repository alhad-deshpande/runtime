// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//
// VirtualCallStubCpu.hpp
//
#ifndef _VIRTUAL_CALL_STUB_PPC64LE_H
#define _VIRTUAL_CALL_STUB_PPC64LE_H

#define DISPATCH_STUB_FIRST_DWORD 0xf981fff8 // std r12,-8(r1) — first instruction of DispatchStub
#define RESOLVE_STUB_FIRST_DWORD  0xe94c00a8 // ld r10,168(r12) — first instruction of ResolveStub resolveEntryPoint

#define USES_LOOKUP_STUBS   1

// #include <cassert>

struct LookupStub
{
    inline PCODE entryPoint() { LIMITED_METHOD_CONTRACT; return (PCODE)&_entryPoint[0]; }
    inline size_t token() { LIMITED_METHOD_CONTRACT; return _token; }
    inline size_t size() { LIMITED_METHOD_CONTRACT; return sizeof(LookupStub); }
private:
    friend struct LookupHolder;
    UINT32 _entryPoint[6];    // 6 instructions (24 bytes)
    PCODE _resolveWorkerTarget; // offset 24
    size_t _token;            // offset 32
};

struct LookupHolder
{
    private:
        LookupStub _stub;
    public:
 static void InitializeStatic() { }

 void Initialize(LookupHolder* pLookupHolderRX, PCODE resolveWorkerTarget, size_t dispatchToken) {
        // r12 points to _entryPoint[0] (stub base), set by the caller.
        //
        // We must not use r9 (IFormatProvider / arg register) or any other argument
        // register (r3-r10) as scratch. r0 cannot be a D-form load base (hardware
        // reads it as 0). The safe approach is to load _token into r10 FIRST (while
        // r12 is still the stub base), then overwrite r12 with _resolveWorkerTarget.
        //
        // [0] ld   r10, 32(r12)   ; _token → r10        (r12 = stub base)
        // [1] ld   r12, 24(r12)   ; _resolveWorkerTarget → r12 (overwrites stub base)
        // [2] mtctr r12           ; CTR = resolveWorkerTarget
        // [3] bctr                ; jump to resolveWorkerTarget(token=r10, ...)
        // [4] nop
        // [5] nop
        //
        // Encoding:
        //   ld r10,32(r12)  : RT=10 RA=12 DS=8(=32/4)  → 0xe94c0020
        //   ld r12,24(r12)  : RT=12 RA=12 DS=6(=24/4)  → 0xe98c0018
        //   mtctr r12       :                           → 0x7d8903a6
        //   bctr            :                           → 0x4e800420
        _stub._entryPoint[0] = 0xe94c0020; // ld   r10,32(r12)  ; _token → r10
        _stub._entryPoint[1] = 0xe98c0018; // ld   r12,24(r12)  ; _resolveWorkerTarget → r12
        _stub._entryPoint[2] = 0x7d8903a6; // mtctr r12
        _stub._entryPoint[3] = 0x4e800420; // bctr
        _stub._entryPoint[4] = 0x60000000; // nop
        _stub._entryPoint[5] = 0x60000000; // nop
        _stub._resolveWorkerTarget = resolveWorkerTarget;
        _stub._token = dispatchToken;
    }

	LookupStub*    stub()        { LIMITED_METHOD_CONTRACT; return &_stub; }
	static LookupHolder*  FromLookupEntry(PCODE lookupEntry)
	{
            LIMITED_METHOD_CONTRACT;
            return (LookupHolder*) ( lookupEntry - offsetof(LookupHolder, _stub) - offsetof(LookupStub, _entryPoint)  );
	}
};

struct DispatchStub
{
    inline PCODE entryPoint()         { LIMITED_METHOD_CONTRACT; return (PCODE)&_entryPoint[0]; }
    inline size_t expectedMT()  { LIMITED_METHOD_CONTRACT; return _expectedMT; }
    inline PCODE implTarget()   { LIMITED_METHOD_CONTRACT; return _implTarget; }
    inline TADDR implTargetSlot(EntryPointSlots::SlotType *slotTypeRef) const
    {
        LIMITED_METHOD_CONTRACT;
	_ASSERTE(slotTypeRef != nullptr);

	*slotTypeRef = EntryPointSlots::SlotType_Executable;
	return (TADDR)&_implTarget;
    }

    inline PCODE failTarget()  { LIMITED_METHOD_CONTRACT; return _failTarget; }
    inline size_t size()        { LIMITED_METHOD_CONTRACT; return sizeof(DispatchStub); }

private:
    friend struct DispatchHolder;

    UINT32 _entryPoint[12];  // 12 instructions (48 bytes)
    size_t  _expectedMT;     // offset 48
    PCODE _implTarget;       // offset 56
    PCODE _failTarget;       // offset 64
};

struct DispatchHolder
{
    static void InitializeStatic() { }

    void Initialize(DispatchHolder* pDispatchHolderRX, PCODE implTarget, PCODE failTarget, size_t expectedMT)
    {
        // r12 points to _entryPoint[0] (stub base), set by the caller.
        //
        // r9 and r10 are live parameter-passing registers on PPC64LE ELFv2 ABI and
        // MUST NOT be clobbered by this stub (which has no save/restore frame).
        // r11 is the live VSD indirection-cell register (virtualStubParamInfo) and
        // must also be preserved.  r0 is safe as scratch.  r12 is the stub base
        // (set by bctr on entry) and can be used as scratch once we save it.
        //
        // To compare the object's MethodTable against _expectedMT without touching
        // any argument register, we use the ELFv2 red zone (-8(r1)) as a temporary
        // spill slot for r12 (the stub base).  The red zone is 512 bytes below SP
        // and is guaranteed not to be modified by signal handlers when the process
        // is executing in user space, so it is safe to use here.
        //
        // Sequence (12 instructions, 48 bytes):
        //
        //  [0]  std  r12, -8(r1)    ; spill stub base → red zone
        //  [1]  ld   r0,   0(r3)    ; r0  = actual MethodTable from object (r3='this')
        //  [2]  ld   r12, 48(r12)   ; r12 = _expectedMT   (using saved stub base)
        //  [3]  cmpd cr0, r0, r12   ; compare actual MT vs expected MT
        //  [4]  ld   r12, -8(r1)    ; restore stub base into r12
        //  [5]  bne  +16            ; mismatch → [9] fail path
        //  [6]  ld   r12, 56(r12)   ; impl path: r12 = _implTarget
        //  [7]  mtctr r12
        //  [8]  bctr
        //  [9]  ld   r12, 64(r12)   ; fail path: r12 = _failTarget
        // [10]  mtctr r12
        // [11]  bctr
        //
        // Encodings:
        //   std  r12,-8(r1)   : op=62 RS=12 RA=1 DS=-2(=0x3FFE) XO=0  → 0xF981FFF8
        //   ld   r0,  0(r3)   : op=58 RT=0  RA=3  DS=0          XO=0  → 0xE8030000
        //   ld   r12,48(r12)  : op=58 RT=12 RA=12 DS=12(=48/4)  XO=0  → 0xE98C0030
        //   cmpd cr0,r0,r12   : op=31 BF=0 L=1 RA=0 RB=12 XO=0 Rc=0  → 0x7C206000
        //   ld   r12,-8(r1)   : op=58 RT=12 RA=1  DS=-2(=0x3FFE)XO=0  → 0xE981FFF8
        //   bne  +16          : op=16 BO=4 BI=2 BD=4 AA=0 LK=0        → 0x40820010
        //   ld   r12,56(r12)  : op=58 RT=12 RA=12 DS=14(=56/4)  XO=0  → 0xE98C0038
        //   mtctr r12         :                                        → 0x7D8903A6
        //   bctr              :                                        → 0x4E800420
        //   ld   r12,64(r12)  : op=58 RT=12 RA=12 DS=16(=64/4)  XO=0  → 0xE98C0040
        //   mtctr r12         :                                        → 0x7D8903A6
        //   bctr              :                                        → 0x4E800420
        _stub._entryPoint[0]  = 0xF981FFF8; // std  r12,-8(r1)    ; spill stub base to red zone
        _stub._entryPoint[1]  = 0xE8030000; // ld   r0,  0(r3)    ; r0 = actual MT from object
        _stub._entryPoint[2]  = 0xE98C0030; // ld   r12,48(r12)   ; r12 = _expectedMT
        _stub._entryPoint[3]  = 0x7C206000; // cmpd cr0,r0,r12    ; compare actual vs expected MT
        _stub._entryPoint[4]  = 0xE981FFF8; // ld   r12,-8(r1)    ; restore stub base
        _stub._entryPoint[5]  = 0x40820010; // bne  +16           ; mismatch → fail path [9]
        _stub._entryPoint[6]  = 0xE98C0038; // ld   r12,56(r12)   ; impl path: r12 = _implTarget
        _stub._entryPoint[7]  = 0x7D8903A6; // mtctr r12
        _stub._entryPoint[8]  = 0x4E800420; // bctr
        _stub._entryPoint[9]  = 0xE98C0040; // ld   r12,64(r12)   ; fail path: r12 = _failTarget
        _stub._entryPoint[10] = 0x7D8903A6; // mtctr r12
        _stub._entryPoint[11] = 0x4E800420; // bctr

        _stub._expectedMT = expectedMT;
        _stub._implTarget = implTarget;
        _stub._failTarget = failTarget;
    }

    DispatchStub* stub()      { LIMITED_METHOD_CONTRACT; return &_stub; }

    static DispatchHolder*  FromDispatchEntry(PCODE dispatchEntry)
    {
        LIMITED_METHOD_CONTRACT;
	DispatchHolder* dispatchHolder = (DispatchHolder*) ( dispatchEntry - offsetof(DispatchHolder, _stub) - offsetof(DispatchStub, _entryPoint) );
	return dispatchHolder;
    }

private:
    DispatchStub _stub;
};

struct ResolveStub
{
    inline PCODE failEntryPoint()            { LIMITED_METHOD_CONTRACT; return (PCODE)&_failEntryPoint[0]; }
    inline PCODE resolveEntryPoint()         { LIMITED_METHOD_CONTRACT; return (PCODE)&_resolveEntryPoint[0]; }
    inline size_t  token()                   { LIMITED_METHOD_CONTRACT; return _token; }
    inline INT32*  pCounter()                { LIMITED_METHOD_CONTRACT; return _pCounter; }

    inline size_t  size()                    { LIMITED_METHOD_CONTRACT; return sizeof(ResolveStub); }

private:
    friend struct ResolveHolder;
    const static int failEntryPointLen = 12;
    const static int resolveEntryPointLen = 26;

    UINT32 _failEntryPoint[failEntryPointLen];    // offset 0,   size 48
    UINT32 _resolveEntryPoint[resolveEntryPointLen]; // offset 48,  size 104
    INT32*  _pCounter;               // offset 152
    size_t  _cacheAddress;           // offset 160
    size_t  _token;                  // offset 168
    PCODE   _resolveWorkerTarget;    // offset 176
};

struct ResolveHolder
{
    static void  InitializeStatic() { }

    void Initialize(ResolveHolder* pResolveHolderRX,
		    PCODE resolveWorkerTarget, PCODE patcherTarget,
		    size_t dispatchToken, UINT32 hashedToken,
		    void * cacheAddr, INT32 * counterAddr)
    {
        // Compile-time layout verification (inside friend, so private members accessible)
        static_assert(offsetof(ResolveStub, _pCounter)            == 152, "ResolveStub::_pCounter offset");
        static_assert(offsetof(ResolveStub, _cacheAddress)        == 160, "ResolveStub::_cacheAddress offset");
        static_assert(offsetof(ResolveStub, _token)               == 168, "ResolveStub::_token offset");
        static_assert(offsetof(ResolveStub, _resolveWorkerTarget) == 176, "ResolveStub::_resolveWorkerTarget offset");

        // Fill in the stub specific fields
        _stub._cacheAddress        = (size_t) cacheAddr;
        _stub._token               = dispatchToken;
        _stub._resolveWorkerTarget = (size_t) resolveWorkerTarget;
        _stub._pCounter            = counterAddr;

	// -------------------------------
	   // failEntryPoint (48 bytes = 12 instructions)
	   // -------------------------------
	   // On PPC64LE the stub page is PROT_READ|PROT_EXEC when double-mapping is
	   // enabled, so the stub cannot write to its own data fields.  External
	   // counter_block memory lives at a RW-alias virtual address that is also
	   // inaccessible from the RX execution context.
	   //
	   // Solution: skip the counter write entirely.  Always set BACKPATCH_FLAG so
	   // that ResolveWorkerAsmStub → BackPatchWorker promotes the call site to
	   // point directly at resolveEntryPoint on the very first miss.  This is
	   // functionally identical to a counter that immediately hits zero; the only
	   // cost is slightly more aggressive promotion (1st miss instead of 100th),
	   // which is acceptable and consistent with a correct VSD implementation.
	   //
	   // r11 = VSD indirection cell — must be preserved and gets BACKPATCH_FLAG set.
	   // r12 = stub base — never touched (resolveEntryPoint uses it for its loads).
	   // No other registers are read or written.
	   //
	   // [0]  ori  r11,r11,1   ; set BACKPATCH_FLAG in indirection cell         → 0x616b0001
	   // [1..11] nop × 11      ; fall through to resolveEntryPoint[0] at +48   → 0x60000000
	   //
	   // Encoding:
	   //   ori r11,r11,1: op=24 RS=11 RA=11 UI=1  → 0x616b0001
	   //   nop:           ori r0,r0,0              → 0x60000000
	   _stub._failEntryPoint[0]  = 0x616b0001; // ori  r11,r11,1  ; BACKPATCH_FLAG always
	   _stub._failEntryPoint[1]  = 0x60000000; // nop
	   _stub._failEntryPoint[2]  = 0x60000000; // nop
	   _stub._failEntryPoint[3]  = 0x60000000; // nop
	   _stub._failEntryPoint[4]  = 0x60000000; // nop
	   _stub._failEntryPoint[5]  = 0x60000000; // nop
	   _stub._failEntryPoint[6]  = 0x60000000; // nop
	   _stub._failEntryPoint[7]  = 0x60000000; // nop
	   _stub._failEntryPoint[8]  = 0x60000000; // nop
	   _stub._failEntryPoint[9]  = 0x60000000; // nop
	   _stub._failEntryPoint[10] = 0x60000000; // nop
	   _stub._failEntryPoint[11] = 0x60000000; // nop
	
	// -------------------------------
	   // resolveEntryPoint (104 bytes = 26 instructions)
	   // -------------------------------
	   // The inline cache lookup accesses ResolveCacheElem objects and the
	   // DispatchCache array — all allocated via 'new' at 0x10010... addresses
	   // on this PPC64LE system.  Those addresses are inaccessible from the RX
	   // stub execution context (kernel memory protection).
	   //
	   // Solution: skip the inline cache entirely.  Load the dispatch token into
	   // r10 and tail-call _resolveWorkerTarget (ResolveWorkerChainLookupAsmStub)
	   // directly.  ResolveWorkerAsmStub performs the full lookup in C++ where all
	   // heap addresses are accessible.  The remaining slots are nops.
	   //
	   // r11 = VSD indirection cell (BACKPATCH_FLAG already set by failEntryPoint)
	   // r12 = stub base
	   // r10 = dispatch token (required by ResolveWorkerAsmStub/ChainLookup ABI)
	   //
	   // [0]  ld   r10,168(r12)   ; r10 = _token (dispatch token)   → 0xe94c00a8
	   // [1]  ld   r12,176(r12)   ; r12 = _resolveWorkerTarget       → 0xe98c00b0
	   // [2]  mtctr r12                                              → 0x7d8903a6
	   // [3]  bctr                ; tail-call ResolveWorkerChainLookup→ 0x4e800420
	   // [4..25] nop × 22                                            → 0x60000000
	   //
	   // Encoding:
	   //   ld r10,168(r12): op=58 RT=10 RA=12 DS=42(=168/4) XO=0   → 0xe94c00a8
	   //   ld r12,176(r12): op=58 RT=12 RA=12 DS=44(=176/4) XO=0   → 0xe98c00b0
	   //   mtctr r12:       XFX SPR=9 RS=12 XO=467                  → 0x7d8903a6
	   //   bctr:                                                     → 0x4e800420
	       _stub._resolveEntryPoint[0]  = 0xe94c00a8; // ld    r10,168(r12) ; _token → r10
	       _stub._resolveEntryPoint[1]  = 0xe98c00b0; // ld    r12,176(r12) ; _resolveWorkerTarget → r12
	       _stub._resolveEntryPoint[2]  = 0x7d8903a6; // mtctr r12
	       _stub._resolveEntryPoint[3]  = 0x4e800420; // bctr
	       for (int i = 4; i < 26; i++)
	           _stub._resolveEntryPoint[i] = 0x60000000; // nop
    }

    ResolveStub* stub()      { LIMITED_METHOD_CONTRACT; return &_stub; }

    static ResolveHolder*  FromFailEntry(PCODE failEntry);
    static ResolveHolder*  FromResolveEntry(PCODE resolveEntry);

private:
    ResolveStub _stub;
};

/*VTableCallStub**************************************************************************************
These are jump stubs that perform a vtable-base virtual call. These stubs assume that an object is placed
in the first argument register (this pointer). From there, the stub extracts the MethodTable pointer, followed by the
vtable pointer, and finally jumps to the target method at a given slot in the vtable.
*/
struct VTableCallStub
{
    friend struct VTableCallHolder;

    inline size_t size()
    {
        _ASSERTE(!"PPC64LE:NYI");
	return 0;
    }

    inline PCODE        entryPoint()        const { LIMITED_METHOD_CONTRACT;  return (PCODE)&_entryPoint[0]; }

    inline size_t token()
    {
        _ASSERTE(!"PPC64LE:NYI");
	return 0;
    }

private:
    BYTE    _entryPoint[0];         // Dynamically sized stub. See Initialize() for more details.
};

/* VTableCallHolders are the containers for VTableCallStubs, they provide for any alignment of
stubs as necessary.  */
struct VTableCallHolder
{
    void  Initialize(unsigned slot);

    VTableCallStub* stub() { LIMITED_METHOD_CONTRACT;  return reinterpret_cast<VTableCallStub *>(this); }

    static size_t GetHolderSize(unsigned slot)
    {
        _ASSERTE(!"PPC64LE:NYI");
	return 0;
    }

    static VTableCallHolder* FromVTableCallEntry(PCODE entry) { LIMITED_METHOD_CONTRACT; return (VTableCallHolder*)entry; }

private:
    // VTableCallStub follows here. It is dynamically sized on allocation because it could
    // use short/long instruction sizes for LDR, depending on the slot value.
};

#ifdef DECLARE_DATA

#ifndef DACCESS_COMPILE
ResolveHolder* ResolveHolder::FromFailEntry(PCODE failEntry)
{
    LIMITED_METHOD_CONTRACT;
    ResolveHolder* resolveHolder = (ResolveHolder*) ( failEntry - offsetof(ResolveHolder, _stub) - offsetof(ResolveStub, _failEntryPoint) );
    return resolveHolder;
}

ResolveHolder* ResolveHolder::FromResolveEntry(PCODE resolveEntry)
{
    LIMITED_METHOD_CONTRACT;
    ResolveHolder* resolveHolder = (ResolveHolder*) ( resolveEntry - offsetof(ResolveHolder, _stub) - offsetof(ResolveStub, _resolveEntryPoint) );
    return resolveHolder;
}

void VTableCallHolder::Initialize(unsigned slot)
{
    _ASSERTE(!"TARGET_POWERPC64:NYI");
}

#endif // DACCESS_COMPILE

#endif //DECLARE_DATA

#endif // #endif // _VIRTUAL_CALL_STUB_PPC64LE_H
