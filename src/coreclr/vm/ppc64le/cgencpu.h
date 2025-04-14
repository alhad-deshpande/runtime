// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// CGENCPU.H -
//
// Various helper routines for generating PPC64LE assembly code.
//
// DO NOT INCLUDE THIS FILE DIRECTLY - ALWAYS USE CGENSYS.H INSTEAD
//

#ifndef TARGET_POWERPC64_
#error Should only include "PPC64LE\cgencpu.h" for PPC64LE builds
#endif

#ifndef __cgencpu_h__
#define __cgencpu_h__

// Given a return address retrieved during stackwalk,
// this is the offset by which it should be decremented to lend somewhere in a call instruction.
#define STACKWALK_CONTROLPC_ADJUST_OFFSET 1 // ?? Other arch except amd64 defined this offset to 4 -- emitjump

// preferred alignment for data
#define DATA_ALIGNMENT 8

class MethodDesc;

// Need to check how to calculate below sizes JUMP_ALLOCATE_SIZE and BACK_TO_BACK_JUMP_ALLOCATE_SIZE
#define STACK_ALIGN_SIZE                        16    // for ppc64le stack align size is 16 

#define JUMP_ALLOCATE_SIZE                      28   // # bytes to allocate for a 64-bit jump instruction
#define BACK_TO_BACK_JUMP_ALLOCATE_SIZE         28   // # bytes to allocate for a back to back 64-bit jump instruction

// Also in CorCompile.h, FnTableAccess.h
#define USE_INDIRECT_CODEHEADER                 // use CodeHeader, RealCodeHeader construct

#define HAS_NDIRECT_IMPORT_PRECODE              1
#define HAS_FIXUP_PRECODE                       1

// ThisPtrRetBufPrecode one is necessary for closed delegates over static methods with return buffer
#define HAS_THISPTR_RETBUF_PRECODE              1

#define CODE_SIZE_ALIGN                         32   // must alloc code blocks on 4-byte boundaries; for perf reasons we use 32 byte boundaries
#define CACHE_LINE_SIZE                         128  // ppc64le processors have 128-byte cache lines
#define LOG2SLOT                                LOG2_PTRSIZE // LOG2_PTRSIZE is defined in src/coreclr/inc/stdmacros.h

#define ENREGISTERED_PARAMTYPE_MAXSIZE          8   // --refer abi --?? why it is defined 8 size in s390x ?? bytes ( 2*pointer_size = 2*8 = 16 --> maximum size of structure can be passed in registers.)
#define CALLDESCR_ARGREGS                       1   // CallDescrWorker has ArgumentRegister parameter
#define CALLDESCR_FPARGREGS                     1   // CallDescrWorker has FloatArgumentRegisters parameter

#define FLOAT_REGISTER_SIZE 8 // each register in FloatArgumentRegisters is 8 bytes.

#define GetEEFuncEntryPoint(pfn) GFN_TADDR(pfn)

//**********************************************************************
// Parameter size
//**********************************************************************

inline unsigned StackElemSize(unsigned parmSize, bool isValueType = false /* unused */, bool isFloatHfa = false /* unused */)
{
    const unsigned stackSlotSize = 8;
    return ALIGN_UP(parmSize, stackSlotSize);
}

//**********************************************************************
// Frames
//**********************************************************************

// This represents some of the TransitionFrame fields that are
// stored at negative offsets.
//--------------------------------------------------------------------
struct REGDISPLAY;

//--------------------------------------------------------------------
// This represents the arguments that are stored in volatile registers.
// This should not overlap the CalleeSavedRegisters since those are already
// saved separately and it would be wasteful to save the same register twice.
// If we do use a non-volatile register as an argument, then the ArgIterator
// will probably have to communicate this back to the PromoteCallerStack
// routine to avoid a double promotion.
//--------------------------------------------------------------------

#define ENUM_ARGUMENT_REGISTERS() \
    ARGUMENT_REGISTER(R3) \
    ARGUMENT_REGISTER(R4) \
    ARGUMENT_REGISTER(R5) \
    ARGUMENT_REGISTER(R6) \
    ARGUMENT_REGISTER(R7) \
    ARGUMENT_REGISTER(R8) \
    ARGUMENT_REGISTER(R9) \
    ARGUMENT_REGISTER(R10) 

#define NUM_ARGUMENT_REGISTERS 8 // ppc64le : Up to eight arguments can be passed in general-purpose registers r3–r10.

// The order of registers in this macro is hardcoded in assembly code
// at number of places
#define ENUM_CALLEE_SAVED_REGISTERS() \
    CALLEE_SAVED_REGISTER(R14) \
    CALLEE_SAVED_REGISTER(R15) \
    CALLEE_SAVED_REGISTER(R16) \
    CALLEE_SAVED_REGISTER(R17) \
    CALLEE_SAVED_REGISTER(R18) \
    CALLEE_SAVED_REGISTER(R19) \
    CALLEE_SAVED_REGISTER(R20) \
    CALLEE_SAVED_REGISTER(R21) \
    CALLEE_SAVED_REGISTER(R22) \
    CALLEE_SAVED_REGISTER(R23) \
    CALLEE_SAVED_REGISTER(R24) \
    CALLEE_SAVED_REGISTER(R25) \
    CALLEE_SAVED_REGISTER(R26) \
    CALLEE_SAVED_REGISTER(R27) \
    CALLEE_SAVED_REGISTER(R28) \
    CALLEE_SAVED_REGISTER(R29) \
    CALLEE_SAVED_REGISTER(R30) \
    CALLEE_SAVED_REGISTER(R31)

#define NUM_CALLEE_SAVED_REGISTERS 18

#define ENUM_FP_CALLEE_SAVED_REGISTERS() \
    CALLEE_SAVED_REGISTER(F14) \
    CALLEE_SAVED_REGISTER(F15) \
    CALLEE_SAVED_REGISTER(F16) \
    CALLEE_SAVED_REGISTER(F17) \
    CALLEE_SAVED_REGISTER(F18) \
    CALLEE_SAVED_REGISTER(F19) \
    CALLEE_SAVED_REGISTER(F20) \
    CALLEE_SAVED_REGISTER(F21) \
    CALLEE_SAVED_REGISTER(F22) \
    CALLEE_SAVED_REGISTER(F23) \
    CALLEE_SAVED_REGISTER(F24) \
    CALLEE_SAVED_REGISTER(F25) \
    CALLEE_SAVED_REGISTER(F26) \
    CALLEE_SAVED_REGISTER(F27) \
    CALLEE_SAVED_REGISTER(F28) \
    CALLEE_SAVED_REGISTER(F29) \
    CALLEE_SAVED_REGISTER(F30) \
    CALLEE_SAVED_REGISTER(F31)

typedef DPTR(struct ArgumentRegisters) PTR_ArgumentRegisters;
struct ArgumentRegisters {
    #define ARGUMENT_REGISTER(regname) INT_PTR regname;
    ENUM_ARGUMENT_REGISTERS();
    #undef ARGUMENT_REGISTER
};

typedef DPTR(struct CalleeSavedRegisters) PTR_CalleeSavedRegisters;
struct CalleeSavedRegisters {
    #define CALLEE_SAVED_REGISTER(regname) INT_PTR regname;
    ENUM_CALLEE_SAVED_REGISTERS();
    #undef CALLEE_SAVED_REGISTER
};

struct CalleeSavedRegistersPointers {
    #define CALLEE_SAVED_REGISTER(regname) PTR_TADDR p##regname;
    ENUM_CALLEE_SAVED_REGISTERS();
    #undef CALLEE_SAVED_REGISTER
};

#define NUM_FLOAT_ARGUMENT_REGISTERS 13	// ppc64le : Up to thirteen qualified floating-point arguments can be passed in floating-point registers f1–f13

typedef DPTR(struct FloatArgumentRegisters) PTR_FloatArgumentRegisters;
struct FloatArgumentRegisters {
     DWORD64 d[NUM_FLOAT_ARGUMENT_REGISTERS];   // f1-f13
};

#if 0 //only s390x and amd64 has declared this function?
void UpdateRegDisplayFromCalleeSavedRegisters(REGDISPLAY * pRD, CalleeSavedRegisters * pRegs);
#endif

#define ARGUMENTREGISTERS_SIZE sizeof(ArgumentRegisters)


//**********************************************************************
// Exception handling
//**********************************************************************

inline PCODE GetIP(const CONTEXT * context)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        SUPPORTS_DAC;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    return PCODE(context->Nip);
}

inline void SetIP(CONTEXT* context, PCODE ip)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        SUPPORTS_DAC;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    context->Nip = (DWORD64) ip;
}

inline TADDR GetSP(const CONTEXT * context)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        SUPPORTS_DAC;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    return (TADDR)context->R1;
}
inline void SetSP(CONTEXT *context, TADDR sp)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        SUPPORTS_DAC;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    context->R1 = sp;
}

inline void SetFP(CONTEXT *context, TADDR fp)
{
    LIMITED_METHOD_DAC_CONTRACT;

    context->R31 = (DWORD64)fp;
}

inline TADDR GetFP(const CONTEXT * context)
{
    LIMITED_METHOD_CONTRACT;

    return (TADDR)(context->R31);
}


extern "C" TADDR GetCurrentSP();

//**********************************************************************
// Jump thunks
//**********************************************************************

inline void emitJump(LPBYTE pBufferRX, LPBYTE pBufferRW, LPVOID target)
{
    LIMITED_METHOD_CONTRACT;
    UINT32* pCode = (UINT32*)pBufferRW;

    pCode[0] = 0x3d800000 | (((guint64)(target) >> 48) & 0xffff);  // lis r12, 0
    pCode[1] = 0x618c0000 | (((guint64)(target) >> 32) & 0xffff);  // ori r12, r12, <target>
    pCode[2] = 0x798c07c6;                                         // sldi r12, r12, 32
    pCode[3] = 0x658c0000 | (((guint64)(target) >> 16) & 0xffff);  // oris r12, r12, <target>
    pCode[4] = 0x618c0000 | ((guint64)(target)         & 0xffff);  // ori r12, r12, <target>
    pCode[5] = 0x7d8903a6;                                         // mtctr r12
    pCode[6] = 0x4e800420;                                         // bcctr
}

inline PCODE decodeJump(PCODE pCode)
{
	//TODO TARGET_POWERPC64
}

inline void emitBackToBackJump(LPBYTE pBufferRX, LPBYTE pBufferRW, LPVOID target)
{
    WRAPPER_NO_CONTRACT;
    emitJump(pBufferRX, pBufferRW, target);
}

inline PCODE decodeBackToBackJump(PCODE pBuffer)
{
    WRAPPER_NO_CONTRACT;
    return decodeJump(pBuffer);
}

struct HijackArgs
{
    union
    {
        ULONG64 R3;
        ULONG64 ReturnValue[1];
    };
    CalleeSavedRegisters Regs;
    union
    {
        ULONG64 Link;  //As per ABI
        size_t ReturnAddress;
    };
};

extern PCODE GetPreStubEntryPoint();

// Precode to shuffle this and retbuf for closed delegates over static methods with return buffer
struct ThisPtrRetBufPrecode {
	//TODO TARGET_POWERPC64
	//This structure is removed in commit: https://github.com/dotnet/runtime/commit/d7c4f0292dc9840c033c82ec3fa36af57dc3d8f6
};
typedef DPTR(ThisPtrRetBufPrecode) PTR_ThisPtrRetBufPrecode;

// ClrFlushInstructionCache is used when we want to call FlushInstructionCache
// for a specific architecture in the common code, but not for other architectures.
// We call ClrFlushInstructionCache whenever we create or modify code in the heap.
// Currently ClrFlushInstructionCache has no effect on S390X
//

inline BOOL ClrFlushInstructionCache(LPCVOID pCodeAddr, size_t sizeOfCode, bool hasCodeExecutedBefore = false)
{
    //TODO TARGET_POWERPC64  -> need to implement FlushInstructionCache in pal debug.cpp
    //if (hasCodeExecutedBefore)
    //{
    //    FlushInstructionCache(GetCurrentProcess(), pCodeAddr, sizeOfCode);
    //}
    //else
    //{
    //    MemoryBarrier();
    //}
    return TRUE;
}


#endif // __cgencpu_h__
