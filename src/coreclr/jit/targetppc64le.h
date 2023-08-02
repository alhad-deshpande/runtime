// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
#pragma once

#if !defined(TARGET_POWERPC64)
#error The file should not be included for this platform.
#endif

// clang-format off
  #define CPU_LOAD_STORE_ARCH      1
  #define ROUND_FLOAT              0       // Do not round intermed float expression results
  #define CPU_HAS_BYTE_REGS        0

  #define CPBLK_UNROLL_LIMIT       64      // Upper bound to let the code generator to loop unroll CpBlk.
  #define INITBLK_UNROLL_LIMIT     64      // Upper bound to let the code generator to loop unroll InitBlk.

#ifdef FEATURE_SIMD
  #define ALIGN_SIMD_TYPES         1       // whether SIMD type locals are to be aligned
  #define FEATURE_PARTIAL_SIMD_CALLEE_SAVE 1 // Whether SIMD registers are partially saved at calls
#endif // FEATURE_SIMD

  #define FEATURE_FIXED_OUT_ARGS   1       // Preallocate the outgoing arg area in the prolog
  #define FEATURE_STRUCTPROMOTE    1       // JIT Optimization to promote fields of structs into registers
  #define FEATURE_MULTIREG_STRUCT_PROMOTE 1  // True when we want to promote fields of a multireg struct into registers
  #define FEATURE_FASTTAILCALL     1       // Tail calls made as epilog+jmp
  #define FEATURE_TAILCALL_OPT     1       // opportunistic Tail calls (i.e. without ".tail" prefix) made as fast tail calls.
  #define FEATURE_SET_FLAGS        0       // Set to true to force the JIT to mark the trees with GTF_SET_FLAGS when the flags need to be set
  #define FEATURE_IMPLICIT_BYREFS       1  // Support for struct parameters passed via pointers to shadow copies
  #define FEATURE_MULTIREG_ARGS_OR_RET  1  // Support for passing and/or returning single values in more than one register
  #define FEATURE_MULTIREG_ARGS         1  // Support for passing a single argument in more than one register
  #define FEATURE_MULTIREG_RET          1  // Support for returning a single value in more than one register
  #define FEATURE_STRUCT_CLASSIFIER     0  // Uses a classifier function to determine is structs are passed/returned in more than one register
  #define MAX_PASS_SINGLEREG_BYTES      8  // Maximum size of a struct passed in a single register (8-byte).
  #define MAX_PASS_MULTIREG_BYTES      16  // Maximum size of a struct that could be passed in more than one register
  #define MAX_RET_MULTIREG_BYTES       16  // Maximum size of a struct that could be returned in more than one register (Max is an HFA of 2 doubles)
  #define MAX_ARG_REG_COUNT             2  // Maximum registers used to pass a single argument in multiple registers.
  #define MAX_RET_REG_COUNT             2  // Maximum registers used to return a value.
  #define MAX_MULTIREG_COUNT            2  // Maximum number of registers defined by a single instruction (including calls).

  #define NOGC_WRITE_BARRIERS      1       // We have specialized WriteBarrier JIT Helpers that DO-NOT trash the RBM_CALLEE_TRASH registers
  #define USER_ARGS_COME_LAST      1
  #define EMIT_TRACK_STACK_DEPTH   1       // This is something of a workaround.  For both ARM and AMD64, the frame size is fixed, so we don't really
                                           // need to track stack depth, but this is currently necessary to get GC information reported at call sites.
  #define TARGET_POINTER_SIZE      8       // equal to sizeof(void*) and the managed pointer size in bytes for this target
  #define FEATURE_EH               1       // To aid platform bring-up, eliminate exceptional EH clauses (catch, filter, filter-handler, fault) and directly execute 'finally' clauses.
  #define FEATURE_EH_CALLFINALLY_THUNKS 1  // Generate call-to-finally code in "thunks" in the enclosing EH region, protected by "cloned finally" clauses.
  #define ETW_EBP_FRAMED           1       // if 1 we cannot use REG_FP as a scratch register and must setup the frame pointer for most methods
  #define CSE_CONSTS               1       // Enable if we want to CSE constants


  #define REG_FP_FIRST             REG_F0
  #define REG_FP_LAST              REG_F31
  #define FIRST_FP_ARGREG          REG_F0
  #define LAST_FP_ARGREG           REG_F7

  #define REGNUM_BITS              6       // number of bits in a REG_* within registerloongarch64.h
  #define REGSIZE_BYTES            8       // number of bytes in one general purpose register
  #define FP_REGSIZE_BYTES         8       // number of bytes in one FP register
  #define FPSAVE_REGSIZE_BYTES     8       // number of bytes in one FP register that are saved/restored.

  #define MIN_ARG_AREA_FOR_CALL    0       // Minimum required outgoing argument space for a call.

  #define CODE_ALIGN               4       // code alignment requirement
  #define STACK_ALIGN              16      // stack alignment requirement

// Vikas to understand below registers why are they used
  #define RBM_INT_CALLEE_SAVED    (RBM_R14|RBM_R15|RBM_R16|RBM_R17|RBM_R18|RBM_R19|RBM_R20|RBM_R21|RBM_R22|RBM_R23|RBM_R24|RBM_R25|RBM_R26|RBM_R27|RBM_R28|RBM_R29|RBM_R30) //non-volatile
  #define RBM_INT_CALLEE_TRASH    (RBM_R0|RBM_R2|RBM_R3|RBM_R4|RBM_R5|RBM_R6|RBM_R7|RBM_R8|RBM_R9|RBM_R10|RBM_R11|RBM_R12|RBM_R13)
  #define RBM_FLT_CALLEE_SAVED    (RBM_F14|RBM_F15|RBM_F16|RBM_F17|RBM_F18|RBM_F19|RBM_F20|RBM_F21|RBM_F22|RBM_F23|RBM_F24|RBM_F25|RBM_F26|RBM_F27|RBM_F28|RBM_F29|RBM_F30|RBM_F31)
  #define RBM_FLT_CALLEE_TRASH    (RBM_F0|RBM_F1|RBM_F2|RBM_F3|RBM_F4|RBM_F5|RBM_F6|RBM_F7|RBM_F8|RBM_F9|RBM_F10|RBM_F11|RBM_F12|RBM_F13)

  #define RBM_CALLEE_SAVED        (RBM_INT_CALLEE_SAVED | RBM_FLT_CALLEE_SAVED)
  #define RBM_CALLEE_TRASH        (RBM_INT_CALLEE_TRASH | RBM_FLT_CALLEE_TRASH)

  #define REG_DEFAULT_HELPER_CALL_TARGET REG_R12
  #define RBM_DEFAULT_HELPER_CALL_TARGET RBM_R12

  #define RBM_ALLINT              (RBM_INT_CALLEE_SAVED | RBM_INT_CALLEE_TRASH)
  #define RBM_ALLFLOAT            (RBM_FLT_CALLEE_SAVED | RBM_FLT_CALLEE_TRASH)
  #define RBM_ALLDOUBLE            RBM_ALLFLOAT


// non-volatile
  #define REG_CALLEE_SAVED_ORDER   REG_R14,REG_R15,REG_R16,REG_R17,REG_R18,REG_R19,REG_R20,REG_R21,REG_R22,REG_R23,REG_R24,REG_R25,REG_R26,REG_R27,REG_R28,REG_R29,REG_R30,REG_R31
  #define RBM_CALLEE_SAVED_ORDER   RBM_R14,RBM_R15,RBM_R16,RBM_R17,RBM_R18,RBM_R19,RBM_R20,RBM_R21,RBM_R22,RBM_R23,RBM_R24,RBM_R25,RBM_R26,RBM_R27,RBM_R28,RBM_R29,RBM_R30,RBM_R31

  #define CNT_CALLEE_SAVED        (18)
  #define CNT_CALLEE_TRASH        (14)
  #define CNT_CALLEE_ENREG        (CNT_CALLEE_SAVED-1)

  #define CNT_CALLEE_SAVED_FLOAT  (18)
  #define CNT_CALLEE_TRASH_FLOAT  (14)

  #define REG_FPBASE               REG_R31
  #define RBM_FPBASE               RBM_R31
  #define STR_FPBASE               "r31"
  #define REG_SPBASE               REG_R1
  #define RBM_SPBASE               RBM_R1
  #define STR_SPBASE               "r1"

  #define RBM_ARG_0                RBM_R0
  #define RBM_ARG_1                RBM_R1
  #define RBM_ARG_2                RBM_R2
  #define RBM_ARG_3                RBM_R3
  #define RBM_ARG_4                RBM_R4
  #define RBM_ARG_5                RBM_R5
  #define RBM_ARG_6                RBM_R6
  #define RBM_ARG_7                RBM_R7

  #define REG_FLTARG_0             REG_F0
  #define REG_FLTARG_1             REG_F1
  #define REG_FLTARG_2             REG_F2
  #define REG_FLTARG_3             REG_F3
  #define REG_FLTARG_4             REG_F4
  #define REG_FLTARG_5             REG_F5
  #define REG_FLTARG_6             REG_F6
  #define REG_FLTARG_7             REG_F7

  #define RBM_FLTARG_0             RBM_F0
  #define RBM_FLTARG_1             RBM_F1
  #define RBM_FLTARG_2             RBM_F2
  #define RBM_FLTARG_3             RBM_F3
  #define RBM_FLTARG_4             RBM_F4
  #define RBM_FLTARG_5             RBM_F5
  #define RBM_FLTARG_6             RBM_F6
  #define RBM_FLTARG_7             RBM_F7

  #define RBM_ARG_REGS            (RBM_ARG_0|RBM_ARG_1|RBM_ARG_2|RBM_ARG_3|RBM_ARG_4|RBM_ARG_5|RBM_ARG_6|RBM_ARG_7)
  #define RBM_FLTARG_REGS         (RBM_FLTARG_0|RBM_FLTARG_1|RBM_FLTARG_2|RBM_FLTARG_3|RBM_FLTARG_4|RBM_FLTARG_5|RBM_FLTARG_6|RBM_FLTARG_7)


  // The following defines are useful for iterating a regNumber
  #define REG_FIRST                REG_R0
  #define REG_INT_FIRST            REG_R0
  #define REG_INT_LAST             REG_R31
  #define REG_INT_COUNT            (REG_INT_LAST - REG_INT_FIRST + 1)
  #define REG_NEXT(reg)           ((regNumber)((unsigned)(reg) + 1))
  #define REG_PREV(reg)           ((regNumber)((unsigned)(reg) - 1))

  #define MAX_REG_ARG              8
  #define MAX_FLOAT_REG_ARG        8

  #define REG_ARG_0                REG_R0
  #define REG_ARG_1                REG_R1
  #define REG_ARG_2                REG_R2
  #define REG_ARG_3                REG_R3
  #define REG_ARG_4                REG_R4
  #define REG_ARG_5                REG_R5
  #define REG_ARG_6                REG_R6
  #define REG_ARG_7                REG_R7

  extern const regNumber fltArgRegs [MAX_FLOAT_REG_ARG];
  extern const regMaskTP fltArgMasks[MAX_FLOAT_REG_ARG];
