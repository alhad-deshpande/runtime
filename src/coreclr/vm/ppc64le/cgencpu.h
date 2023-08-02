// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// CGENCPU.H -
//
// Various helper routines for generating S390X assembly code.
//
// DO NOT INCLUDE THIS FILE DIRECTLY - ALWAYS USE CGENSYS.H INSTEAD
//



#ifndef TARGET_POWERPC64
#error Should only include "ppc64le\cgencpu.h" for POWERPC64LE builds
#endif

#ifndef __cgencpu_h__
#define __cgencpu_h__

#include "stublinkersppc64le.h"

// Given a return address retrieved during stackwalk,
// this is the offset by which it should be decremented to lend somewhere in a call instruction.
#define STACKWALK_CONTROLPC_ADJUST_OFFSET 1

// preferred alignment for data
#define DATA_ALIGNMENT 8

class MethodDesc;
class FramedMethodFrame;
class Module;
struct VASigCookie;
class ComCallMethodDesc;

//
// functions implemented in S390X assembly
//
EXTERN_C void SinglecastDelegateInvokeStub();
//EXTERN_C void FastCallFinalizeWorker(Object *obj, PCODE funcPtr);

//#define COMMETHOD_PREPAD                        16   // # extra bytes to allocate in addition to sizeof(ComCallMethodDesc)
//#define COMMETHOD_CALL_PRESTUB_SIZE             6    // 32-bit indirect relative call
//#define COMMETHOD_CALL_PRESTUB_ADDRESS_OFFSET   -10  // the offset of the call target address inside the prestub

#define STACK_ALIGN_SIZE                        8

#define JUMP_ALLOCATE_SIZE                      8   // # bytes to allocate for a 64-bit jump instruction
#define BACK_TO_BACK_JUMP_ALLOCATE_SIZE         8   // # bytes to allocate for a back to back 64-bit jump instruction
//#define SIZEOF_LOAD_AND_JUMP_THUNK              22   // # bytes to mov r10, X; jmp Z
//#define SIZEOF_LOAD2_AND_JUMP_THUNK             32   // # bytes to mov r10, X; mov r11, Y; jmp Z

// Also in CorCompile.h, FnTableAccess.h
#define USE_INDIRECT_CODEHEADER                 // use CodeHeader, RealCodeHeader construct

#define HAS_NDIRECT_IMPORT_PRECODE              1
#define HAS_FIXUP_PRECODE                       1

// ThisPtrRetBufPrecode one is necessary for closed delegates over static methods with return buffer
#define HAS_THISPTR_RETBUF_PRECODE              1

#define CODE_SIZE_ALIGN                         16   // must alloc code blocks on 8-byte boundaries; for perf reasons we use 16 byte boundaries
#define CACHE_LINE_SIZE                         64   // Current AMD64 processors have 64-byte cache lines as per AMD64 optimization manual
#define LOG2SLOT                                LOG2_PTRSIZE


#define ENREGISTERED_RETURNTYPE_INTEGER_MAXSIZE 8   // bytes
#define ENREGISTERED_PARAMTYPE_MAXSIZE          8   // bytes
#define ENREGISTERED_RETURNTYPE_MAXSIZE         8   // bytes
#define CALLDESCR_ARGREGS                       1    // CallDescrWorker has ArgumentRegister parameter
#define CALLDESCR_FPARGREGS                     1    // CallDescrWorker has FloatArgumentRegisters parameter

//#ifndef TARGET_UNIX
//#define USE_REDIRECT_FOR_GCSTRESS
//#endif // TARGET_UNIX

//
// REX prefix byte
//
//#define REX_PREFIX_BASE         0x40        // 0100xxxx
//#define REX_OPERAND_SIZE_64BIT  0x08        // xxxx1xxx
//#define REX_MODRM_REG_EXT       0x04        // xxxxx1xx     // use for 'middle' 3 bit field of mod/r/m
//#define REX_SIB_INDEX_EXT       0x02        // xxxxxx10
//#define REX_MODRM_RM_EXT        0x01        // XXXXXXX1     // use for low 3 bit field of mod/r/m
//#define REX_SIB_BASE_EXT        0x01        // XXXXXXX1
//#define REX_OPCODE_REG_EXT      0x01        // XXXXXXX1
//
//#define X86_REGISTER_MASK       0x7
//
//#define X86RegFromAMD64Reg(extended_reg) \
//            ((X86Reg)(((int)extended_reg) & X86_REGISTER_MASK))

#define FLOAT_REGISTER_SIZE 8 // each register in FloatArgumentRegisters is 8 bytes.

// Why is the return value ARG_SLOT? On 64-bit systems, that is 64-bits
// and much bigger than necessary for R4, requiring explicit downcasts.
//inline
//ARG_SLOT FPSpillToR4(void* pSpillSlot)
//{
//    LIMITED_METHOD_CONTRACT;
//    return *(DWORD*)pSpillSlot;
//}
//
//inline
//ARG_SLOT FPSpillToR8(void* pSpillSlot)
//{
//    LIMITED_METHOD_CONTRACT;
//    return *(SIZE_T*)pSpillSlot;
//}
//
//inline
//void     R4ToFPSpill(void* pSpillSlot, DWORD  srcFloatAsDWORD)
//{
//    LIMITED_METHOD_CONTRACT;
//    *(SIZE_T*)pSpillSlot = (SIZE_T)srcFloatAsDWORD;
//    *((SIZE_T*)pSpillSlot  1) = 0;
//}
//
//inline
//void     R8ToFPSpill(void* pSpillSlot, SIZE_T  srcDoubleAsSIZE_T)
//{
//    LIMITED_METHOD_CONTRACT;
//    *(SIZE_T*)pSpillSlot = srcDoubleAsSIZE_T;
//    *((SIZE_T*)pSpillSlot  1) = 0;
//}


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
//--------------------------------------------------------------------
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

#define NUM_ARGUMENT_REGISTERS 8

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

//#define SCRATCH_REGISTER_X86REG kRAX

#define THIS_REG R2
#define THIS_kREG 2

#define ARGUMENT_kREG1 2
#define ARGUMENT_kREG2 3

#define NUM_FLOAT_ARGUMENT_REGISTERS 12

typedef DPTR(struct FloatArgumentRegisters) PTR_FloatArgumentRegisters;
struct FloatArgumentRegisters {
     DWORD64 d[NUM_FLOAT_ARGUMENT_REGISTERS];   // f1-f13
};


void UpdateRegDisplayFromCalleeSavedRegisters(REGDISPLAY * pRD, CalleeSavedRegisters * pRegs);


// Sufficient context for Try/Catch restoration.
struct EHContext {
    // Not used
};

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

inline void SetIP(CONTEXT* context, PCODE rip)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        SUPPORTS_DAC;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    context->Nip = (DWORD64) rip;
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
inline void SetSP(CONTEXT *context, TADDR rsp)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        SUPPORTS_DAC;

        PRECONDITION(CheckPointer(context));
    }
    CONTRACTL_END;

    context->R1 = rsp;
}

#define SetFP(context, ebp)
inline TADDR GetFP(const CONTEXT * context)
{
    LIMITED_METHOD_CONTRACT;

    return (TADDR)(context->R31);
}

extern "C" TADDR GetCurrentSP();

extern "C" void InterpreterStubThunk();


// Emits:
//  mov r10, pv1
//  mov rax, pTarget
//  jmp rax
//void EncodeLoadAndJumpThunk (LPBYTE pBuffer, LPVOID pv, LPVOID pTarget);


// Get Rel32 destination, emit jumpStub if necessary
//INT32 rel32UsingJumpStub(INT32 UNALIGNED * pRel32, PCODE target, MethodDesc *pMethod,
//    LoaderAllocator *pLoaderAllocator = NULL, bool throwOnOutOfMemoryWithinRange = true);

// Get Rel32 destination, emit jumpStub if necessary into a preallocated location
//INT32 rel32UsingPreallocatedJumpStub(INT32 UNALIGNED * pRel32, PCODE target, PCODE jumpStubAddr, PCODE jumpStubAddrRW, bool emitJump);

//void emitCOMStubCall (ComCallMethodDesc *pCOMMethodRX, ComCallMethodDesc *pCOMMethodRW, PCODE target);

void emitJump(LPBYTE pBufferRX, LPBYTE pBufferRW, LPVOID target);

//BOOL isJumpRel32(PCODE pCode);
//PCODE decodeJump32(PCODE pCode);

//BOOL isJumpRel64(PCODE pCode);
//PCODE decodeJump64(PCODE pCode);

//
// On IA64 back to back jumps should be separated by a nop bundle to get
// the best performance from the hardware's branch prediction logic.
// For all other platforms back to back jumps don't require anything special
// That is why we have these two wrapper functions that call emitJump and decodeJump
//
inline void emitBackToBackJump(LPBYTE pBufferRX, LPBYTE pBufferRW, LPVOID target)
{
    WRAPPER_NO_CONTRACT;

    emitJump(pBufferRX, pBufferRW, target);
}

//inline BOOL isBackToBackJump(PCODE pCode)
//{
//    WRAPPER_NO_CONTRACT;
//    SUPPORTS_DAC;
//    return isJumpRel32(pCode) || isJumpRel64(pCode);
//}

inline PCODE decodeBackToBackJump(PCODE pCode)
{
    WRAPPER_NO_CONTRACT;
    SUPPORTS_DAC;
//    if (isJumpRel32(pCode))
//        return decodeJump32(pCode);
//    else
//    if (isJumpRel64(pCode))
//        return decodeJump64(pCode);
//    else
//        return NULL;

    _ASSERTE(!"NYI");
    return NULL;
}

//extern "C" void setFPReturn(int fpSize, INT64 retVal);
//extern "C" void getFPReturn(int fpSize, INT64 *retval);


struct DECLSPEC_ALIGN(8) UMEntryThunkCode
{
    UINT16      m_code[8];
    TADDR       m_pTargetCode;
    TADDR       m_pvSecretParam;

    void Encode(UMEntryThunkCode *pEntryThunkCodeRX, BYTE* pTargetCode, void* pvSecretParam);
    void Poison();

    LPCBYTE GetEntryPoint() const
    {
        LIMITED_METHOD_CONTRACT;

        return (LPCBYTE)this;
    }

    static int GetEntryPointOffset()
    {
        LIMITED_METHOD_CONTRACT;

        return 0;
    }
};

struct HijackArgs
{
    union
    {
        ULONG64 R2;
        ULONG64 ReturnValue[1];
    };
    CalleeSavedRegisters Regs;
//#ifdef TARGET_WINDOWS
//    ULONG64 Rsp;
//#endif
    union
    {
        ULONG64 Rip;
        size_t ReturnAddress;
    };
};

extern PCODE GetPreStubEntryPoint();

// Precode to shuffle this and retbuf for closed delegates over static methods with return buffer
struct ThisPtrRetBufPrecode {

    static const int Type = 0x79;

    UINT16  m_rgCode[24];
    TADDR   m_pTarget;
    TADDR   m_pMethodDesc;

    void Init(MethodDesc* pMD, LoaderAllocator *pLoaderAllocator)
    {
        WRAPPER_NO_CONTRACT;

        int n = 0;
        // Initially
        // %r3 - This ptr
        // %r4 - ReturnBuffer
        m_rgCode[n++] = 0x1b78; // mr r12 r3
        m_rgCode[n++] = 0x7c6c;
        m_rgCode[n++] = 0x2378; // mr r3 r4
        m_rgCode[n++] = 0x7c83;
        m_rgCode[n++] = 0x6378; // mr r4 r12)
        m_rgCode[n++] = 0x7d84;
        m_rgCode[n++] = (( UINT64(&m_pTarget) >> 48) & 0xffff); // lis r12, (addr >> 48) & 0xffff)
        m_rgCode[n++] = 0x3d80;
        m_rgCode[n++] = ((UINT64(&m_pTarget) >> 32) & 0xffff); // ori r12, r12, (addr >> 32) & 0xffff)
        m_rgCode[n++] = 0x618c;
        m_rgCode[n++] = 0x07c6; // sldi r12, r12, 32
        m_rgCode[n++] = 0x79c0;
        m_rgCode[n++] = ((UINT64(&m_pTarget) >> 16) & 0xffff); // oris r12, r12, (addr >> 16) & 0xffff)
        m_rgCode[n++] = 0x658c;
        m_rgCode[n++] = (UINT64(&m_pTarget) & 0xffff); // ori r12, r12, (addr  & 0xffff)
        m_rgCode[n++] = 0x618c;
        m_rgCode[n++] = 0x0000; // ld %r12, 0, %r12
        m_rgCode[n++] = 0xe98c;
        m_rgCode[n++] = 0x03a6; // mtctr r12
        m_rgCode[n++] = 0x7d89;
        m_rgCode[n++] = 0x0420; // bcctr
        m_rgCode[n++] = 0x4e80;
        m_rgCode[n++] = 0x0000; // nop (ori 0, 0, 0)
        m_rgCode[n++] = 0x6000;
        //_ASSERTE((UINT16*)&m_pTarget == &m_rgCode[n + 6]);
        _ASSERTE(n == ARRAY_SIZE(m_rgCode));

        m_pTarget = GetPreStubEntryPoint();
        m_pMethodDesc = (TADDR)pMD;
    }

    TADDR GetMethodDesc()
    {
        LIMITED_METHOD_DAC_CONTRACT;

        return m_pMethodDesc;
    }

    PCODE GetTarget()
    {
        LIMITED_METHOD_DAC_CONTRACT;
        return m_pTarget;
    }

#ifndef DACCESS_COMPILE
    BOOL SetTargetInterlocked(TADDR target, TADDR expected)
    {
        CONTRACTL
        {
            THROWS;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        ExecutableWriterHolder<ThisPtrRetBufPrecode> precodeWriterHolder(this, sizeof(ThisPtrRetBufPrecode));
        return (TADDR)InterlockedCompareExchange64(
            (LONGLONG*)&precodeWriterHolder.GetRW()->m_pTarget, (TADDR)target, (TADDR)expected) == expected;
    }
#endif // !DACCESS_COMPILE
};
typedef DPTR(ThisPtrRetBufPrecode) PTR_ThisPtrRetBufPrecode;


//#ifndef DACCESS_COMPILE
//
//DWORD GetOffsetAtEndOfFunction(ULONGLONG           uImageBase,
//                               PT_RUNTIME_FUNCTION   pFunctionEntry,
//                               int                 offsetNum = 1);
//
//#endif // DACCESS_COMPILE

// ClrFlushInstructionCache is used when we want to call FlushInstructionCache
// for a specific architecture in the common code, but not for other architectures.
// We call ClrFlushInstructionCache whenever we create or modify code in the heap.
// Currently ClrFlushInstructionCache has no effect on AMD64
//

inline BOOL ClrFlushInstructionCache(LPCVOID pCodeAddr, size_t sizeOfCode, bool hasCodeExecutedBefore = false)
{
//    if (hasCodeExecutedBefore)
//    {
//        FlushInstructionCache(GetCurrentProcess(), pCodeAddr, sizeOfCode);
//    }
//    else
//    {
//        MemoryBarrier();
//    }
    return TRUE;
}

//
// JIT HELPER ALIASING FOR PORTABILITY.
//
// Create alias for optimized implementations of helpers provided on this platform
//
#define JIT_GetSharedGCStaticBase           JIT_GetSharedGCStaticBase_SingleAppDomain
#define JIT_GetSharedNonGCStaticBase        JIT_GetSharedNonGCStaticBase_SingleAppDomain
#define JIT_GetSharedGCStaticBaseNoCtor     JIT_GetSharedGCStaticBaseNoCtor_SingleAppDomain
#define JIT_GetSharedNonGCStaticBaseNoCtor  JIT_GetSharedNonGCStaticBaseNoCtor_SingleAppDomain

#endif // __cgencpu_h__
