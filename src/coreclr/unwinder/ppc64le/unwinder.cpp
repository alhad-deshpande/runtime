// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "stdafx.h"
#include "utilcode.h"
#include "crosscomp.h"
#include "unwinder.h"

// ---------------------------------------------------------------------------
// Status codes (mirror ARM64 / RISCV64 conventions)
// ---------------------------------------------------------------------------
#define STATUS_UNWIND_UNSUPPORTED_VERSION   STATUS_UNSUCCESSFUL
#define STATUS_UNWIND_INVALID_SEQUENCE      STATUS_UNSUCCESSFUL

// ---------------------------------------------------------------------------
// Memory access macros — identical to every other arch in this directory.
// ---------------------------------------------------------------------------
#define MEMORY_READ_BYTE(params, addr)   (*dac_cast<PTR_BYTE>(addr))
#define MEMORY_READ_DWORD(params, addr)  (*dac_cast<PTR_DWORD>(addr))
#define MEMORY_READ_QWORD(params, addr)  (*dac_cast<PTR_UINT64>(addr))

// ---------------------------------------------------------------------------
// Unwind-opcode end-of-sequence test (same encoding as ARM64/RISCV64)
//   0xE4 = end
//   0xE5 = end_c  (chained; treated identically for our purposes)
// ---------------------------------------------------------------------------
#define OPCODE_IS_END(op) (((op) & 0xFE) == 0xE4)

// ---------------------------------------------------------------------------
// Parameter block (ContextPointers only; no stack-bound validation needed
// for the in-process case).
// ---------------------------------------------------------------------------
typedef struct _PPC64LE_UNWIND_PARAMS
{
    PKNONVOLATILE_CONTEXT_POINTERS ContextPointers;
} PPC64LE_UNWIND_PARAMS, *PPPC64LE_UNWIND_PARAMS;

// ---------------------------------------------------------------------------
// UPDATE_CONTEXT_POINTERS
//
// Records, in the optional ContextPointers array, the stack address from
// which each nonvolatile integer register (R14–R31) was restored.
// R14 = index 0, R15 = index 1, …, R31 = index 17.
// ---------------------------------------------------------------------------
#define UPDATE_CONTEXT_POINTERS(Params, RegNum, Addr)                         \
    do {                                                                       \
        if (ARGUMENT_PRESENT(Params)) {                                        \
            PKNONVOLATILE_CONTEXT_POINTERS _cp = (Params)->ContextPointers;    \
            if (ARGUMENT_PRESENT(_cp) &&                                       \
                (RegNum) >= 14 && (RegNum) <= 31)                              \
            {                                                                  \
                (&_cp->R14)[(RegNum) - 14] = (PDWORD64)(Addr);                \
            }                                                                  \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// RtlpComputeScopeSize
//
// Counts the number of PPC64LE instructions represented by the unwind codes
// starting at UnwindCodePtr and ending before UnwindCodesEndPtr (or at an
// end opcode). Each unwind byte describes exactly one 4-byte instruction on
// PPC64LE (as documented in unwindppc64le.cpp), except the alloc_m and
// alloc_l multi-byte opcodes — those still represent one instruction (stdu).
//
// Returns the instruction count (== word count == the "OffsetInScope" unit).
// ---------------------------------------------------------------------------
static ULONG
RtlpComputeScopeSize(
    _In_ ULONG_PTR UnwindCodePtr,
    _In_ ULONG_PTR UnwindCodesEndPtr,
    _In_ BOOLEAN   IsEpilog,
    _In_ PPPC64LE_UNWIND_PARAMS UnwindParams)
{
    ULONG ScopeSize = 0;

    while (UnwindCodePtr < UnwindCodesEndPtr)
    {
        BYTE op = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
        if (OPCODE_IS_END(op))
            break;

        ScopeSize++;  // every entry = 1 instruction

        if (op <= 0x1F)
        {
            // alloc_s: 1 byte
            UnwindCodePtr += 1;
        }
        else if (op >= 0xC0 && op <= 0xC7)
        {
            // alloc_m: 2 bytes
            UnwindCodePtr += 2;
        }
        else if (op == 0xE0)
        {
            // alloc_l: 4 bytes
            UnwindCodePtr += 4;
        }
        else
        {
            // nop (0xE3) or any other single-byte code
            UnwindCodePtr += 1;
        }
    }

    // Epilogs have one extra instruction (the blr/return) at the end.
    if (IsEpilog)
        ScopeSize++;

    return ScopeSize;
}

// ---------------------------------------------------------------------------
// RtlpUnwindFunctionFull
//
// Virtually unwinds a PPC64LE managed function by parsing the xdata record
// and executing the unwind opcodes.
//
// The xdata header format is identical to the ARM64 / RISCV64 full-xdata
// layout (as emitted by unwindppc64le.cpp):
//
//   DWORD 0:
//     bits [17: 0]  FunctionLength (in 4-byte instruction units)
//     bits [19:18]  Version (must be 0)
//     bit  [20]     X: 1 if an exception handler RVA follows the unwind codes
//     bit  [21]     E: 1 if a single epilog is packed at offset EpilogCount
//     bits [26:22]  EpilogCount  (or EpilogIndex when E=1)
//     bits [31:27]  CodeWords    (number of 4-byte code words)
//
//   If CodeWords == 0 && EpilogCount == 0:
//     DWORD 1  (extended): [15:0] EpilogCount, [23:16] CodeWords
//
//   Followed by EpilogCount scope-descriptor DWORDs (when E=0).
//   Followed by CodeWords*4 bytes of unwind opcodes.
//   Followed by optional handler RVA DWORD (when X=1).
//
// Opcode encoding emitted by the PPC64LE JIT (unwindAllocStack):
//   alloc_s:  000xxxxx           (1 B)  sp += 16*(x)          x <= 0x1F
//   alloc_m:  11000xxx|xxxxxxxx  (2 B)  sp += 16*(x)          x <= 0x7FF
//   alloc_l:  E0|xx|xx|xx        (4 B)  sp += 16*(x)          x <= 0xFFFFFF
//   nop:      E3                 (1 B)  no-op
//   end:      E4                 (1 B)  end of codes
// ---------------------------------------------------------------------------
static NTSTATUS
RtlpUnwindFunctionFull(
    _In_    ULONG64                ControlPcRva,
    _In_    ULONG64                ImageBase,
    _In_    PT_RUNTIME_FUNCTION    FunctionEntry,
    _Inout_ PCONTEXT               ContextRecord,
    _Out_   PULONG64               EstablisherFrame,
    _Outptr_opt_result_maybenull_ PEXCEPTION_ROUTINE *HandlerRoutine,
    _Out_   PVOID                 *HandlerData,
    _In_    PPPC64LE_UNWIND_PARAMS UnwindParams)
{
    // Assume we unwind to a call site (not a hardware exception frame).
    ContextRecord->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;

    // -----------------------------------------------------------------
    // 1. Parse the xdata header.
    // -----------------------------------------------------------------
    ULONG_PTR UnwindDataPtr = ImageBase + FunctionEntry->UnwindData;

    ULONG HeaderWord = MEMORY_READ_DWORD(UnwindParams, UnwindDataPtr);
    UnwindDataPtr += 4;

    // Version field must be 0.
    if (((HeaderWord >> 18) & 3) != 0)
        return STATUS_UNWIND_UNSUPPORTED_VERSION;

    ULONG FunctionLength  = HeaderWord & 0x3FFFF;          // in instructions
    ULONG UnwindWords     = (HeaderWord >> 27) & 0x1F;
    ULONG EpilogScopeCount = (HeaderWord >> 22) & 0x1F;

    // Extended header when both fields are zero.
    if (EpilogScopeCount == 0 && UnwindWords == 0)
    {
        ULONG ExtWord = MEMORY_READ_DWORD(UnwindParams, UnwindDataPtr);
        UnwindDataPtr += 4;
        UnwindWords      = (ExtWord >> 16) & 0xFF;
        EpilogScopeCount =  ExtWord        & 0xFFFF;
    }

    // E-bit: single epilog packed into the header.
    ULONG UnwindIndex = 0;
    if ((HeaderWord & (1u << 21)) != 0)
    {
        UnwindIndex    = EpilogScopeCount;  // start index inside code array
        EpilogScopeCount = 0;
    }

    // -----------------------------------------------------------------
    // 2. Locate exception handler if X-bit is set.
    // -----------------------------------------------------------------
    PEXCEPTION_ROUTINE ExceptionHandler     = NULL;
    PVOID              ExceptionHandlerData = NULL;

    if ((HeaderWord & (1u << 20)) != 0)
    {
        ULONG_PTR HandlerRvaPtr =
            UnwindDataPtr + 4 * (EpilogScopeCount + UnwindWords);
        ExceptionHandler = (PEXCEPTION_ROUTINE)(ImageBase +
                            MEMORY_READ_DWORD(UnwindParams, HandlerRvaPtr));
        ExceptionHandlerData = (PVOID)(HandlerRvaPtr + 4);
    }

    // -----------------------------------------------------------------
    // 3. Set up pointers into the unwind code array.
    // -----------------------------------------------------------------
    ULONG_PTR UnwindCodePtr    = UnwindDataPtr + 4 * EpilogScopeCount;
    ULONG_PTR UnwindCodesEndPtr = UnwindCodePtr + 4 * UnwindWords;

    // OffsetInFunction is in instruction units (each PPC64 insn = 4 bytes).
    ULONG OffsetInFunction =
        (ULONG)((ControlPcRva - FunctionEntry->BeginAddress) / 4);

    ULONG SkipWords = 0;

    // -----------------------------------------------------------------
    // 4. Determine whether ControlPc is in the prolog, epilog, or body.
    // -----------------------------------------------------------------

    // --- Prolog check ---
    if (OffsetInFunction < 4 * UnwindWords)
    {
        ULONG ScopeSize = RtlpComputeScopeSize(
            UnwindCodePtr, UnwindCodesEndPtr, FALSE, UnwindParams);

        if (OffsetInFunction < ScopeSize)
        {
            // We are in the prolog; execute all codes but skip the ones
            // that correspond to instructions not yet executed.
            SkipWords           = ScopeSize - OffsetInFunction;
            ExceptionHandler     = NULL;
            ExceptionHandlerData = NULL;
            goto ExecuteCodes;
        }
    }

    // --- Epilog check (E-bit: single epilog at end of function) ---
    if ((HeaderWord & (1u << 21)) != 0)
    {
        ULONG remaining = 4 * UnwindWords - UnwindIndex;
        if (OffsetInFunction + remaining >= FunctionLength)
        {
            ULONG ScopeSize = RtlpComputeScopeSize(
                UnwindCodePtr + UnwindIndex, UnwindCodesEndPtr, TRUE, UnwindParams);
            ULONG ScopeStart = FunctionLength - ScopeSize;

            if (OffsetInFunction >= ScopeStart)
            {
                UnwindCodePtr       += UnwindIndex;
                SkipWords            = OffsetInFunction - ScopeStart;
                ExceptionHandler     = NULL;
                ExceptionHandlerData = NULL;
            }
        }
    }
    // --- Epilog check (multiple scope records) ---
    else
    {
        ULONG_PTR ScopePtr = UnwindDataPtr;
        for (ULONG ScopeNum = 0; ScopeNum < EpilogScopeCount; ScopeNum++)
        {
            ULONG ScopeWord  = MEMORY_READ_DWORD(UnwindParams, ScopePtr);
            ScopePtr += 4;

            ULONG ScopeStart = ScopeWord & 0x3FFFF;
            if (OffsetInFunction < ScopeStart)
                break;  // records are ordered; past this, we're in the body

            ULONG ScopeIndex = ScopeWord >> 22;
            if (OffsetInFunction < ScopeStart + (4 * UnwindWords - ScopeIndex))
            {
                ULONG ScopeSize = RtlpComputeScopeSize(
                    UnwindCodePtr + ScopeIndex, UnwindCodesEndPtr, TRUE, UnwindParams);

                if (OffsetInFunction < ScopeStart + ScopeSize)
                {
                    UnwindCodePtr       += ScopeIndex;
                    SkipWords            = OffsetInFunction - ScopeStart;
                    ExceptionHandler     = NULL;
                    ExceptionHandlerData = NULL;
                    break;
                }
            }
        }
    }

ExecuteCodes:
    // -----------------------------------------------------------------
    // 5. Skip SkipWords instructions' worth of unwind codes.
    // -----------------------------------------------------------------
    while (UnwindCodePtr < UnwindCodesEndPtr && SkipWords > 0)
    {
        BYTE op = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
        if (OPCODE_IS_END(op))
            break;

        if (op <= 0x1F)
            UnwindCodePtr += 1;
        else if (op >= 0xC0 && op <= 0xC7)
            UnwindCodePtr += 2;
        else if (op == 0xE0)
            UnwindCodePtr += 4;
        else
            UnwindCodePtr += 1;

        SkipWords--;
    }

    // -----------------------------------------------------------------
    // 6. Execute the remaining unwind opcodes.
    //
    //   Only the alloc_s / alloc_m / alloc_l opcodes are emitted by the
    //   current PPC64LE JIT.  The register-save opcodes (unwindSaveReg)
    //   are still no-ops in the JIT, so we don't need to restore any
    //   callee-saved registers here yet.  They are added when the JIT's
    //   unwindSaveReg implementation is completed.
    // -----------------------------------------------------------------
    NTSTATUS Status = STATUS_SUCCESS;

    while (UnwindCodePtr < UnwindCodesEndPtr)
    {
        BYTE CurCode = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
        UnwindCodePtr += 1;

        // alloc_s (000xxxxx): sp += 16 * x
        if (CurCode <= 0x1F)
        {
            ContextRecord->R1 += 16 * (ULONG64)(CurCode & 0x1F);
        }

        // alloc_m (11000xxx|xxxxxxxx): sp += 16 * x  (x up to 0x7FF)
        else if (CurCode >= 0xC0 && CurCode <= 0xC7)
        {
            ULONG64 x = ((ULONG64)(CurCode & 0x07) << 8) |
                         MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
            UnwindCodePtr++;
            ContextRecord->R1 += 16 * x;
        }

        // alloc_l (11100000|xx|xx|xx): sp += 16 * x  (x up to 0xFFFFFF)
        else if (CurCode == 0xE0)
        {
            ULONG64 x = ((ULONG64)MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr)     << 16) |
                        ((ULONG64)MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr + 1) <<  8) |
                         (ULONG64)MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr + 2);
            UnwindCodePtr += 3;
            ContextRecord->R1 += 16 * x;
        }

        // nop (11100011): no action
        else if (CurCode == 0xE3)
        {
            // nothing
        }

        // end / end_c (11100100 / 11100101): stop processing
        else if (OPCODE_IS_END(CurCode))
        {
            break;
        }

        // Any unrecognised opcode is treated as invalid.
        else
        {
            Status = STATUS_UNWIND_INVALID_SEQUENCE;
            break;
        }
    }

    // -----------------------------------------------------------------
    // 7. Post-processing.
    //
    //   PPC64LE equivalents of ARM64's "Pc = Lr; EstablisherFrame = Sp":
    //     Nip  = Link   (program counter ← link register / return address)
    //     R1   is already the caller-SP after the alloc codes above
    // -----------------------------------------------------------------
    if (NT_SUCCESS(Status))
    {
        ContextRecord->Nip  = ContextRecord->Link;  // return address → PC
        *EstablisherFrame   = ContextRecord->R1;    // caller-SP

        if (ARGUMENT_PRESENT(HandlerRoutine))
            *HandlerRoutine = ExceptionHandler;

        *HandlerData = ExceptionHandlerData;
    }

    return Status;
}

// ===========================================================================
//  D A C  section  (out-of-process / debugger)
// ===========================================================================
#ifdef DACCESS_COMPILE

UNWIND_INFO * DacGetUnwindInfo(TADDR taUnwindInfo)
{
    // TODO TARGET_POWERPC64: DAC unwind not yet implemented.
    _ASSERTE(!"DacGetUnwindInfo: TARGET_POWERPC64 NYI");
    return NULL;
}

BOOL DacUnwindStackFrame(CONTEXT * pContext, KNONVOLATILE_CONTEXT_POINTERS* pContextPointers)
{
    OOPStackUnwinderPPC64LE unwinder;
    BOOL res = unwinder.Unwind(pContext);

    if (res && pContextPointers)
    {
        // Populate R14–R31 context pointers from the unwound context.
        for (int i = 0; i < 18; i++)
            (&pContextPointers->R14)[i] = &pContext->R14 + i;
    }

    return res;
}

BOOL OOPStackUnwinderPPC64LE::Unwind(CONTEXT * pContext)
{
    DWORD64 ImageBase = 0;
    HRESULT hr = GetModuleBase(pContext->Nip, &ImageBase);
    if (hr != S_OK)
        return FALSE;

    PEXCEPTION_ROUTINE DummyHandlerRoutine;
    PVOID              DummyHandlerData;
    DWORD64            DummyEstablisherFrame;

    DWORD64 startingPc = pContext->Nip;
    DWORD64 startingSp = pContext->R1;

    T_RUNTIME_FUNCTION Rfe;
    if (FAILED(GetFunctionEntry(pContext->Nip, &Rfe, sizeof(Rfe))))
        return FALSE;

    PPC64LE_UNWIND_PARAMS unwindParams;
    unwindParams.ContextPointers = NULL;

    NTSTATUS Status = RtlpUnwindFunctionFull(
        pContext->Nip - ImageBase,
        ImageBase,
        &Rfe,
        pContext,
        &DummyEstablisherFrame,
        &DummyHandlerRoutine,
        &DummyHandlerData,
        &unwindParams);

    if (!NT_SUCCESS(Status))
    {
        pContext->Nip = 0;
        return FALSE;
    }

    // No forward progress → malformed stack.
    if (pContext->Nip == 0 ||
        (startingPc == pContext->Nip && startingSp == pContext->R1))
        return FALSE;

    return TRUE;
}

#else // !DACCESS_COMPILE

// ===========================================================================
//  In-process  RtlVirtualUnwind
// ===========================================================================

PEXCEPTION_ROUTINE RtlVirtualUnwind(
    _In_     ULONG                          HandlerType,
    _In_     ULONG64                        ImageBase,
    _In_     ULONG64                        ControlPc,
    _In_     PT_RUNTIME_FUNCTION            FunctionEntry,
    _Inout_  PCONTEXT                       ContextRecord,
    _Out_    PVOID                         *HandlerData,
    _Out_    PULONG64                       EstablisherFrame,
    __inout_opt PKNONVOLATILE_CONTEXT_POINTERS ContextPointers)
{
    UNREFERENCED_PARAMETER(HandlerType);

    PEXCEPTION_ROUTINE HandlerRoutine = NULL;

    PPC64LE_UNWIND_PARAMS unwindParams;
    unwindParams.ContextPointers = ContextPointers;

    NTSTATUS Status = RtlpUnwindFunctionFull(
        ControlPc - ImageBase,
        ImageBase,
        FunctionEntry,
        ContextRecord,
        EstablisherFrame,
        &HandlerRoutine,
        HandlerData,
        &unwindParams);

    // Callers detect failure by checking for a zeroed PC.
    if (!NT_SUCCESS(Status))
        ContextRecord->Nip = 0;

    return HandlerRoutine;
}

#endif // DACCESS_COMPILE
