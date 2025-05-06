// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

LEAF_ENTRY GetCurrentSP, _TEXT
LEAF_END GetCurrentSP, _TEXT

LEAF_ENTRY GetCurrentIP, _TEXT
LEAF_END GetCurrentIP, _TEXT

// EXTERN_C void LazyMachStateCaptureState(struct LazyMachState *pState)
LEAF_ENTRY LazyMachStateCaptureState, _TEXT
LEAF_END LazyMachStateCaptureState, _TEXT
