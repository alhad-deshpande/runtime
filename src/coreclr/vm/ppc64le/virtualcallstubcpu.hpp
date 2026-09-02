// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//
// VirtualCallStubCpu.hpp
//
#ifndef _VIRTUAL_CALL_STUB_PPC64LE_H
#define _VIRTUAL_CALL_STUB_PPC64LE_H

// TODO RESOLVE_STUB
#define DISPATCH_STUB_FIRST_DWORD 0xe80c0028 // ld r0,40(r12)
#define RESOLVE_STUB_FIRST_DWORD  0xe9430000 // ld r10,0(r3)

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

    UINT32 _entryPoint[10];  // 10 instructions (40 bytes)
    size_t  _expectedMT;     // offset 40
    PCODE _implTarget;       // oofset 48 
    PCODE _failTarget;       // oofset 56
};

struct DispatchHolder
{
    static void InitializeStatic() { }

    void Initialize(DispatchHolder* pDispatchHolderRX, PCODE implTarget, PCODE failTarget, size_t expectedMT)
    {
        // r12 points to _entryPoint[0] (stub base), set by the caller.
        // r9 is used as the MethodTable scratch: it is volatile and NOT an argument
        // register in the relevant sense — r3 is the only argument that matters here
        // (the 'this' pointer).  r4 must NOT be used because it carries the second
        // argument (e.g. the key in TryInsert → GetHashCode) and the DispatchStub is
        // a transparent trampoline with no save/restore frame.  r11 must NOT be used
        // because it is the live VSD indirection-cell register (virtualStubParamInfo)
        // consumed by the fail/resolve path.
         _stub._entryPoint[0] = 0xe80c0028; // ld  r0, 40(r12)    ; _expectedMT  → r0
        _stub._entryPoint[1] = 0xe9230000; // ld  r9, 0(r3)      ; actual MT from object → r9
        _stub._entryPoint[2] = 0x7c090000; // cmpd cr0, r9, r0   ; compare actual vs expected MT
        _stub._entryPoint[3] = 0x41820010; // beq target (+16)
        _stub._entryPoint[4] = 0xe98c0038; // ld r12, 56(r12)
        _stub._entryPoint[5] = 0x7d8903a6; // mtspr CTR, r12
        _stub._entryPoint[6] = 0x4e800420; // bctr
        _stub._entryPoint[7] = 0xe98c0030; // target: ld r12, 48(r12)
        _stub._entryPoint[8] = 0x7d8903a6; // mtspr CTR, r12
        _stub._entryPoint[9] = 0x4e800420; // bctr
	
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

    UINT32 _failEntryPoint[failEntryPointLen];
    UINT32 _resolveEntryPoint[resolveEntryPointLen];
    INT32*  _pCounter;               // Base of the Data Region
    size_t  _cacheAddress;           // lookupCache
    size_t  _token;
    PCODE   _resolveWorkerTarget;
};

struct ResolveHolder
{
    static void  InitializeStatic() { }

    void Initialize(ResolveHolder* pResolveHolderRX,
		    PCODE resolveWorkerTarget, PCODE patcherTarget,
		    size_t dispatchToken, UINT32 hashedToken,
		    void * cacheAddr, INT32 * counterAddr)
    {
        // Fill in the stub specific fields
    _stub._cacheAddress        = (size_t) cacheAddr;
	_stub._token               = dispatchToken;
	_stub._resolveWorkerTarget = (size_t) resolveWorkerTarget;
	_stub._pCounter            = counterAddr;

	// -------------------------------
	   // failEntryPoint (48 bytes)
	   // -------------------------------
	   // r11 = indirection cell (set by JIT, virtualStubParamInfo = r11)
	   // r12 = stub base (entry point of this resolve stub, set by caller via r12)
	   // retry:
	   //   ld    r10,152(r12)     ; pCounter*
	   //   lwarx r0,0,r10
	   //   addi  r0,r0,-1
	   //   stwcx r0,0,r10
	   //   bne-  retry            ; -16 from NIA (to lwarx)
	   //   cmpwi r0,0
	   //   bge   +24              ; to resolveEntryPoint start
	   //   ori   r11,r11,1        ; set BACKPATCH_FLAG in r11 (indirection cell register)
	   //   nop; nop; nop; nop
	       _stub._failEntryPoint[0]  = 0xe94c0098; // ld    r10,152(r12)
	       _stub._failEntryPoint[1]  = 0x7c0a0280; // lwarx r0,0,r10
	       _stub._failEntryPoint[2]  = 0x3800ffff; // addi  r0,r0,-1
	       _stub._failEntryPoint[3]  = 0x7c0a02ac; // stwcx r0,0,r10
	       _stub._failEntryPoint[4]  = 0x40c2fff0; // bne-  -16
	       _stub._failEntryPoint[5]  = 0x2c000000; // cmpwi r0,0
	       _stub._failEntryPoint[6]  = 0x40800018; // bge   +24
	       _stub._failEntryPoint[7]  = 0x616b0001; // ori   r11,r11,1  (BACKPATCH_FLAG)
	       _stub._failEntryPoint[8]  = 0x60000000; // nop
	       _stub._failEntryPoint[9]  = 0x60000000; // nop
	       _stub._failEntryPoint[10] = 0x60000000; // nop
	       _stub._failEntryPoint[11] = 0x60000000; // nop
	
	// -------------------------------
	   // resolveEntryPoint (104 bytes)
	   // -------------------------------
	   // r11 = indirection cell (virtualStubParamInfo = REG_R11, set by JIT, preserved here)
	   // r12 = this stub's base address (set by caller)
	   //
	   // SCRATCH: only r0 and r10. r5 (Span<char>.len) and r9 (IFormatProvider) are live
	   // argument registers that must not be clobbered. r0 cannot be a D-form base register
	   // (hardware reads it as 0). r10 is safe; r9 and r5 must be left intact.
	   //
	   // [0]  ld   r10,0(r3)          ; actual MT → r10        RT=10,RA=3,DS=0   → 0xe9430000
	   // [1]  srdi r0,r10,12          ; r0 = MT>>12             rldicl r0,r10,52,12 → 0x7940a302
	   // [2]  add  r0,r0,r10          ; r0 = hash partial       RT=0,RA=0,RB=10   → 0x7c005214
	   // [3]  xori r0,r0,hashedToken  ; (variable)
	   // [4]  andi. r0,r0,CACHE_MASK  ; (variable)
	   // [5]  sldi r0,r0,3            ; slot byte offset         rldicr r0,r0,3,60 → 0x78001f24
	   // [6]  ld   r10,160(r12)       ; cache base → r10        RT=10,RA=12,DS=160 → 0xe94c00a0
	   // [7]  add  r10,r10,r0         ; r10 = slot ptr           RT=10,RA=10,RB=0  → 0x7d4a0214
	   // [8]  std  r10,48(r1)         ; save slot ptr on stack   RS=10,RA=1,DS=48  → 0xf9410030
	   // [9]  ld   r0,0(r10)          ; r0 = cached MT           RT=0,RA=10,DS=0   → 0xe80a0000
	   // [10] ld   r10,0(r3)          ; r10 = actual MT (re-load) RT=10,RA=3,DS=0  → 0xe9430000
	   // [11] cmpd cr0,r10,r0         ; compare MTs              RA=10,RB=0        → 0x7c2a0000
	   // [12] bne  +36 → [22]miss     ; BD=36: NIA=[13]=52, tgt=[22]=88            → 0x40820024
	   // [13] ld   r10,48(r1)         ; restore slot ptr         RT=10,RA=1,DS=48  → 0xe9410030
	   // [14] ld   r0,8(r10)          ; r0 = cached token        RT=0,RA=10,DS=8   → 0xe80a0008
	   // [15] ld   r10,168(r12)       ; r10 = stub token         RT=10,RA=12,DS=168 → 0xe94c00a8
	   // [16] cmpd cr0,r10,r0         ; compare tokens (r5,r9 untouched!) → 0x7c2a0000
	   // [17] bne  +16 → [22]miss     ; BD=16: NIA=[18]=72, tgt=[22]=88            → 0x40820010
	   // [18] ld   r10,48(r1)         ; restore slot ptr         RT=10,RA=1,DS=48  → 0xe9410030
	   // [19] ld   r12,16(r10)        ; target → r12             RT=12,RA=10,DS=16 → 0xe98a0010
	   // [20] mtctr r12                                                             → 0x7d8903a6
	   // [21] bctr                    ; jump with all args intact                  → 0x4e800420
	   // miss (slot ptr at 48(r1) from [8]; r12 still = stub base):
	   // [22] ld   r10,168(r12)       ; token → r10             RT=10,RA=12,DS=168 → 0xe94c00a8
	   // [23] ld   r12,176(r12)       ; worker → r12            RT=12,RA=12,DS=176 → 0xe98c00b0
	   // [24] mtctr r12                                                             → 0x7d8903a6
	   // [25] bctr                                                                  → 0x4e800420
	       _stub._resolveEntryPoint[0]  = 0xe9430000;                          // ld    r10,0(r3)
	       _stub._resolveEntryPoint[1]  = 0x7940a302;                          // srdi  r0,r10,12
	       _stub._resolveEntryPoint[2]  = 0x7c005214;                          // add   r0,r0,r10
	       _stub._resolveEntryPoint[3]  = 0x68000000 | (hashedToken & 0xFFFF); // xori  r0,r0,imm16
	       _stub._resolveEntryPoint[4]  = 0x70000000 | (CALL_STUB_CACHE_MASK & 0xFFFF); // andi. r0,r0,imm16
	       _stub._resolveEntryPoint[5]  = 0x78001f24;                          // sldi  r0,r0,3
	       _stub._resolveEntryPoint[6]  = 0xe94c00a0;                          // ld    r10,160(r12)
	       _stub._resolveEntryPoint[7]  = 0x7d4a0214;                          // add   r10,r10,r0
	       _stub._resolveEntryPoint[8]  = 0xf9410030;                          // std   r10,48(r1)   ; save slot ptr
	       _stub._resolveEntryPoint[9]  = 0xe80a0000;                          // ld    r0,0(r10)    ; cached MT
	       _stub._resolveEntryPoint[10] = 0xe9430000;                          // ld    r10,0(r3)    ; actual MT re-load
	       _stub._resolveEntryPoint[11] = 0x7c2a0000;                          // cmpd  cr0,r10,r0
	       _stub._resolveEntryPoint[12] = 0x40820024;                          // bne   +36 → miss
	       _stub._resolveEntryPoint[13] = 0xe9410030;                          // ld    r10,48(r1)   ; restore slot ptr
	       _stub._resolveEntryPoint[14] = 0xe80a0008;                          // ld    r0,8(r10)    ; cached token
	       _stub._resolveEntryPoint[15] = 0xe94c00a8;                          // ld    r10,168(r12) ; stub token (r5 untouched)
	       _stub._resolveEntryPoint[16] = 0x7c2a0000;                          // cmpd  cr0,r10,r0   ; (r5,r9 never touched)
	       _stub._resolveEntryPoint[17] = 0x40820010;                          // bne   +16 → miss
	       _stub._resolveEntryPoint[18] = 0xe9410030;                          // ld    r10,48(r1)   ; restore slot ptr
	       _stub._resolveEntryPoint[19] = 0xe98a0010;                          // ld    r12,16(r10)  ; target → r12
	       _stub._resolveEntryPoint[20] = 0x7d8903a6;                          // mtctr r12
	       _stub._resolveEntryPoint[21] = 0x4e800420;                          // bctr
	       _stub._resolveEntryPoint[22] = 0xe94c00a8;                          // ld    r10,168(r12) ; miss: token
	       _stub._resolveEntryPoint[23] = 0xe98c00b0;                          // ld    r12,176(r12) ; miss: worker
	       _stub._resolveEntryPoint[24] = 0x7d8903a6;                          // mtctr r12
	       _stub._resolveEntryPoint[25] = 0x4e800420;                          // bctr
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
