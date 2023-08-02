// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

/*****************************************************************************/

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#if defined(TARGET_POWERPC64)

#include "target.h"

const char*            Target::g_tgtCPUName           = "ppc64le";
const Target::ArgOrder Target::g_tgtArgOrder          = ARG_ORDER_R2L;  // TODO for ppc64le
const Target::ArgOrder Target::g_tgtUnmanagedArgOrder = ARG_ORDER_R2L;  // TODO for ppc64le

// clang-format off
const regNumber intArgRegs [] = { REG_R2, REG_R3, REG_R4, REG_R5, REG_R6 };
const regMaskTP intArgMasks[] = { RBM_R2, RBM_R3, RBM_R4, RBM_R5, RBM_R6 };
const regNumber fltArgRegs [] = { REG_V0, REG_V2, REG_V4, REG_V6 };
const regMaskTP fltArgMasks[] = { RBM_V0, RBM_V2, RBM_V4, RBM_V6 };
// clang-format on

#endif // TARGET_POWERPC64
