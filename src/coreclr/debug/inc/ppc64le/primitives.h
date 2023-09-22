#ifndef PRIMITIVES_H_
#define PRIMITIVES_H_

#ifndef CORDB_ADDRESS_TYPE
typedef const BYTE                  CORDB_ADDRESS_TYPE;
typedef DPTR(CORDB_ADDRESS_TYPE)    PTR_CORDB_ADDRESS_TYPE;
#endif
//This is an abstraction to keep x86/ia64 patch data separate
#ifndef PRD_TYPE
#define PRD_TYPE                               DWORD_PTR
#endif

#define MAX_INSTRUCTION_LENGTH 4

// Given a return address retrieved during stackwalk,
// this is the offset by which it should be decremented to lend somewhere in a call instruction.
#define STACKWALK_CONTROLPC_ADJUST_OFFSET 1

//TO DO : this part needs to be reviiwed
// I could  not find break instruction in power.
// in ppc-codegen.h file break is calling tw (trap word)instruction
// with all TO bits set to 1. Hence calculated code as below
// Bit 0-5 = 011111 Bit 6-10(TO)=11111 Bit 11-15 (RA)=00000
// Bit 16-20 (RB)=00000 Bit 21-30=0000000100
#define CORDbg_BREAK_INSTRUCTION_SIZE          4
#define CORDbg_BREAK_INSTRUCTION         (WORD)0x7FE00008

inline CORDB_ADDRESS GetPatchEndAddr(CORDB_ADDRESS patchAddr)
{
    LIMITED_METHOD_DAC_CONTRACT;
    return patchAddr + CORDbg_BREAK_INSTRUCTION_SIZE;
}


#define InitializePRDToBreakInst(_pPRD)       *(_pPRD) = CORDbg_BREAK_INSTRUCTION
#define PRDIsBreakInst(_pPRD)                 (*(_pPRD) == CORDbg_BREAK_INSTRUCTION)

#define CORDbgGetInstructionEx(_buffer, _requestedAddr, _patchAddr, _dummy1, _dummy2)                          \
    CORDbgGetInstruction((CORDB_ADDRESS_TYPE *)((_buffer) + ((_patchAddr) - (_requestedAddr))));

#define CORDbgSetInstructionEx(_buffer, _requestedAddr, _patchAddr, _opcode, _dummy2)                          \
    CORDbgSetInstruction((CORDB_ADDRESS_TYPE *)((_buffer) + ((_patchAddr) - (_requestedAddr))), (_opcode));

#define CORDbgInsertBreakpointEx(_buffer, _requestedAddr, _patchAddr, _dummy1, _dummy2)                        \
    CORDbgInsertBreakpoint((CORDB_ADDRESS_TYPE *)((_buffer) + ((_patchAddr) - (_requestedAddr))));


constexpr CorDebugRegister g_JITToCorDbgReg[] =
{
   REGISTER_POWERPC64_R0,
   REGISTER_POWERPC64_R1,
   REGISTER_POWERPC64_R2,
   REGISTER_POWERPC64_R3,
   REGISTER_POWERPC64_R4,
   REGISTER_POWERPC64_R5,
   REGISTER_POWERPC64_R6,
   REGISTER_POWERPC64_R7,
   REGISTER_POWERPC64_R8,
   REGISTER_POWERPC64_R9,
   REGISTER_POWERPC64_R10,
   REGISTER_POWERPC64_R11,
   REGISTER_POWERPC64_R12,
   REGISTER_POWERPC64_R13,
   REGISTER_POWERPC64_R14,
   REGISTER_POWERPC64_R15,
   REGISTER_POWERPC64_R16,
   REGISTER_POWERPC64_R17,
   REGISTER_POWERPC64_R18,
   REGISTER_POWERPC64_R19,
   REGISTER_POWERPC64_R20,
   REGISTER_POWERPC64_R21,
   REGISTER_POWERPC64_R22,
   REGISTER_POWERPC64_R23,
   REGISTER_POWERPC64_R24,
   REGISTER_POWERPC64_R25,
   REGISTER_POWERPC64_R26,
   REGISTER_POWERPC64_R27,
   REGISTER_POWERPC64_R28,
   REGISTER_POWERPC64_R29,
   REGISTER_POWERPC64_R30,
   REGISTER_POWERPC64_R31
};

//
// Mapping from ICorDebugInfo register numbers to CorDebugRegister
// numbers. Note: this must match the order in corinfo.h.
//
inline CorDebugRegister ConvertRegNumToCorDebugRegister(ICorDebugInfo::RegNum reg)
{
    _ASSERTE(reg >= 0);
    _ASSERTE(static_cast<size_t>(reg) < ARRAY_SIZE(g_JITToCorDbgReg));
    return g_JITToCorDbgReg[reg];
}


//
// inline function to access/modify the CONTEXT
//
inline LPVOID CORDbgGetIP(DT_CONTEXT* context)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    return (LPVOID) context->Nip;
}

inline void CORDbgSetIP(DT_CONTEXT* context, LPVOID rip)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    context->Nip = (DWORD64) rip;
}

inline LPVOID CORDbgGetSP(const DT_CONTEXT * context)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    return (LPVOID)context->R1;
}
inline void CORDbgSetSP(DT_CONTEXT *context, LPVOID rsp)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    context->R1 = (UINT_PTR)rsp;
}

inline LPVOID CORDbgGetFP(const DT_CONTEXT * context)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    return (LPVOID)context->R31;
}

inline void CORDbgSetFP(DT_CONTEXT *context, LPVOID rbp)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    context->R31 = (UINT_PTR)rbp;
}

// compare the RIP, RSP, and RBP
inline BOOL CompareControlRegisters(const DT_CONTEXT * pCtx1, const DT_CONTEXT * pCtx2)
{
    LIMITED_METHOD_DAC_CONTRACT;

    if ((pCtx1->Nip == pCtx2->Nip) &&
        (pCtx1->R1 == pCtx2->R1) &&
        (pCtx1->R31 == pCtx2->R31))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/* ========================================================================= */
//
// Routines used by debugger support functions such as codepatch.cpp or
// exception handling code.
//
// GetInstruction, InsertBreakpoint, and SetInstruction all operate on
// a _single_ byte of memory. This is really important. If you only
// save one byte from the instruction stream before placing a breakpoint,
// you need to make sure to only replace one byte later on.
//


inline PRD_TYPE CORDbgGetInstruction(UNALIGNED CORDB_ADDRESS_TYPE* address)
{
    LIMITED_METHOD_CONTRACT;

    return *address;                    // retrieving only one byte is important
}

//TO DO - change beow function for ppc64le is pending
inline void CORDbgSetInstruction(UNALIGNED CORDB_ADDRESS_TYPE* address,
                                 PRD_TYPE instruction)
{
    // In a DAC build, this function assumes the input is an host address.
    LIMITED_METHOD_DAC_CONTRACT;

    *((unsigned char*)address) =
        (unsigned char) instruction;    // setting one byte is important
    FlushInstructionCache(GetCurrentProcess(), address, 1);

}

inline void CORDbgInsertBreakpoint(UNALIGNED CORDB_ADDRESS_TYPE *address)
{
    LIMITED_METHOD_CONTRACT;

    CORDbgSetInstruction(address, CORDbg_BREAK_INSTRUCTION);

}


inline void CORDbgAdjustPCForBreakInstruction(DT_CONTEXT* pContext)
{
    LIMITED_METHOD_CONTRACT;

    pContext->Nip -= 1; //To DO for ppc64le. Only register name changed.
}

inline bool AddressIsBreakpoint(CORDB_ADDRESS_TYPE *address)
{
    LIMITED_METHOD_CONTRACT;

    return *address == CORDbg_BREAK_INSTRUCTION;
}

inline void SetSSFlag(DT_CONTEXT *pContext)
{
        _ASSERTE(!"unimplemented on POWERPC64 yet");
    //_ASSERTE(pContext != NULL);
    //pContext->EFlags |= 0x100;
}

inline void UnsetSSFlag(DT_CONTEXT *pContext)
{
        _ASSERTE(!"unimplemented on POWERPC64 yet");
    //_ASSERTE(pContext != NULL);
    //pContext->EFlags &= ~0x100;
}

inline bool IsSSFlagEnabled(DT_CONTEXT * context)
{
    _ASSERTE(!"unimplemented on POWERPC64 yet");
    //_ASSERTE(context != NULL);
    return 0;
}


inline bool PRDIsEqual(PRD_TYPE p1, PRD_TYPE p2){
    return p1 == p2;
}
inline void InitializePRD(PRD_TYPE *p1) {
    *p1 = 0;
}

inline bool PRDIsEmpty(PRD_TYPE p1) {
    return p1 == 0;
}

#endif // PRIMITIVES_H_
