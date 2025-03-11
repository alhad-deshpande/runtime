// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

//*****************************************************************************
// File: primitives.h
//
// Platform-specific debugger primitives
//
//*****************************************************************************

#ifndef PRIMITIVES_H_
#define PRIMITIVES_H_

typedef const BYTE                  CORDB_ADDRESS_TYPE;
typedef DPTR(CORDB_ADDRESS_TYPE)    PTR_CORDB_ADDRESS_TYPE;

#define MAX_INSTRUCTION_LENGTH 4

// Given a return address retrieved during stackwalk,
// this is the offset by which it should be decremented to land at the call instruction.
#define STACKWALK_CONTROLPC_ADJUST_OFFSET 4

#define PRD_TYPE                               LONG
#define CORDbg_BREAK_INSTRUCTION_SIZE 4
#define CORDbg_BREAK_INSTRUCTION (LONG)0x7FFF0008  // PowerPC64 trap instruction (tw 31,31,0)

inline CORDB_ADDRESS GetPatchEndAddr(CORDB_ADDRESS patchAddr)
{
    LIMITED_METHOD_DAC_CONTRACT;
    return patchAddr + CORDbg_BREAK_INSTRUCTION_SIZE;
}

#define InitializePRDToBreakInst(_pPRD)       *(_pPRD) = CORDbg_BREAK_INSTRUCTION
#define PRDIsBreakInst(_pPRD)                 (*(_pPRD) == CORDbg_BREAK_INSTRUCTION)

#define CORDbgGetInstructionEx(_buffer, _requestedAddr, _patchAddr, _dummy1, _dummy2)                          \
    CORDbgGetInstructionExImpl((CORDB_ADDRESS_TYPE *)((_buffer) + (_patchAddr) - (_requestedAddr)));

#define CORDbgSetInstructionEx(_buffer, _requestedAddr, _patchAddr, _opcode, _dummy2)                          \
    CORDbgSetInstructionExImpl((CORDB_ADDRESS_TYPE *)((_buffer) + (_patchAddr) - (_requestedAddr)), (_opcode));

#define CORDbgInsertBreakpointEx(_buffer, _requestedAddr, _patchAddr, _dummy1, _dummy2)                        \
    CORDbgInsertBreakpointExImpl((CORDB_ADDRESS_TYPE *)((_buffer) + (_patchAddr) - (_requestedAddr)));

constexpr CorDebugRegister g_JITToCorDbgReg[] =
{
    REGISTER_POWERPC64_R0,          
    REGISTER_POWERPC64_R1,          // Stack Pointer (R1)
    REGISTER_POWERPC64_R2,          // Table of Contents Pointer (R2)
                                    // Parameters/Return (R3-R10)
    REGISTER_POWERPC64_R3,          // First parameter/return register
    REGISTER_POWERPC64_R4,          // Second parameter/return register
    REGISTER_POWERPC64_R5,          // Third parameter register
    REGISTER_POWERPC64_R6,         
    REGISTER_POWERPC64_R7,         
    REGISTER_POWERPC64_R8,         
    REGISTER_POWERPC64_R9,         
    REGISTER_POWERPC64_R10,
    REGISTER_POWERPC64_R11,        
    REGISTER_POWERPC64_R12,            
    REGISTER_POWERPC64_R13,
                                    // Nonvolatile (R14-R31)
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
    REGISTER_POWERPC64_R31,        
                                    //Special Registers 
    REGISTER_POWERPC64_LR,          // Link Register
    REGISTER_POWERPC64_CTR          // Count Register
};

inline CorDebugRegister ConvertRegNumToCorDebugRegister(ICorDebugInfo::RegNum reg)
{
    LIMITED_METHOD_CONTRACT;
    _ASSERTE(reg >= 0);
    _ASSERTE(static_cast<size_t>(reg) < ARRAY_SIZE(g_JITToCorDbgReg));
    return g_JITToCorDbgReg[reg];
}

inline LPVOID CORDbgGetIP(DT_CONTEXT *context)
{
    LIMITED_METHOD_CONTRACT;
    return (LPVOID)(size_t)(context->Nip);// Link ??
}

inline void CORDbgSetIP(DT_CONTEXT *context, LPVOID ip)
{
    LIMITED_METHOD_CONTRACT;
    context->Link = (DWORD64)ip; // Link?
   // context->Nip = (DWORD64)ip; // Link?
}

inline LPVOID CORDbgGetSP(const DT_CONTEXT * context)
{
    LIMITED_METHOD_CONTRACT;
    return (LPVOID)(size_t)(context->Gpr[1]);
}

inline void CORDbgSetSP(DT_CONTEXT *context, LPVOID sp)
{
    LIMITED_METHOD_CONTRACT;
    context->Gpr[1] = (DWORD64)sp;
}

inline BOOL CompareControlRegisters(const DT_CONTEXT * pCtx1, const DT_CONTEXT * pCtx2)
{
    LIMITED_METHOD_DAC_CONTRACT;

    if ((pCtx1->Nip == pCtx2->Nip) &&
        (pCtx1->Gpr[1] == pCtx2->Gpr[1]))
    {
        return TRUE;
    }
    return FALSE;
}

inline void CORDbgSetInstruction(CORDB_ADDRESS_TYPE* address, PRD_TYPE instruction)
{
    LIMITED_METHOD_DAC_CONTRACT;
    *(volatile PRD_TYPE *)address = instruction;
    __asm__ volatile ("sync; icbi 0,%0; isync" :: "r"(address) : "memory");
}

inline PRD_TYPE CORDbgGetInstruction(UNALIGNED CORDB_ADDRESS_TYPE* address)
{
    LIMITED_METHOD_CONTRACT;
    return *(volatile PRD_TYPE *)address;
}

inline void CORDbgInsertBreakpoint(UNALIGNED CORDB_ADDRESS_TYPE *address)
{
    LIMITED_METHOD_CONTRACT;
    CORDbgSetInstruction(address, CORDbg_BREAK_INSTRUCTION);
}

inline void CORDbgInsertBreakpointExImpl(UNALIGNED CORDB_ADDRESS_TYPE *address)
{
    LIMITED_METHOD_CONTRACT;
    CORDbgSetInstruction(address, CORDbg_BREAK_INSTRUCTION);
}

inline void CORDbgAdjustPCForBreakInstruction(DT_CONTEXT* pContext)
{
    LIMITED_METHOD_CONTRACT;
    // PowerPC64 leaves NIP at the breakpoint instruction. No adjustment needed, hence empty return.
    return;
}

inline bool AddressIsBreakpoint(CORDB_ADDRESS_TYPE* address)
{
    LIMITED_METHOD_CONTRACT;
    return CORDbgGetInstruction(address) == CORDbg_BREAK_INSTRUCTION;
}

class Thread;
// Enable single stepping.
void SetSSFlag(DT_CONTEXT *pCtx, Thread *pThread);

// Disable single stepping
void UnsetSSFlag(DT_CONTEXT *pCtx, Thread *pThread);

// Check if single stepping is enabled.
bool IsSSFlagEnabled(DT_CONTEXT *pCtx, Thread *pThread);

inline bool PRDIsEqual(PRD_TYPE p1, PRD_TYPE p2)
{
    return p1 == p2;
}

inline void InitializePRD(PRD_TYPE *p1)
{
    *p1 = 0;
}

inline bool PRDIsEmpty(PRD_TYPE p1)
{
    LIMITED_METHOD_CONTRACT;
    return p1 == 0;
}

#endif // PRIMITIVES_H_
