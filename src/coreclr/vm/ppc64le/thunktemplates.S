// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

PAGE_SIZE = 65536       //Page size is 64K for ppc64le arch

#define DATA_SLOT(stub, field) C_FUNC(stub##Code) + PAGE_SIZE + stub##Data__##field

LEAF_ENTRY StubPrecodeCode, _TEXT
        ld r12, DATA_SLOT(StubPrecode, MethodDesc)
        ld r3, DATA_SLOT(StubPrecode, Target)
        mtlr r3
        blrl
LEAF_END_MARKED StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT
        ld r3, DATA_SLOT(FixupPrecode, Target)
        mtlr r3
        blrl
        ld r12, DATA_SLOT(FixupPrecode, MethodDesc)
        ld r3, DATA_SLOT(FixupPrecode, PrecodeFixupThunk)
        mtlr r3
        blrl
LEAF_END_MARKED FixupPrecodeCode, _TEXT

LEAF_ENTRY CallCountingStubCode, _TEXT
//TO DO for ppc64le
        lghi %r0, -1
        lgrl %r1, DATA_SLOT(CallCountingStub, RemainingCallCountCell)
        ah %r0, 0(%r1)
        sth %r0, 0(%r1)
        lgrl %r1, DATA_SLOT(CallCountingStub, TargetForMethod)
        bner %r1
        lgrl %r0, DATA_SLOT(CallCountingStub, RemainingCallCountCell)
        lgrl %r1, DATA_SLOT(CallCountingStub, TargetForThresholdReached)
        br %r1
LEAF_END_MARKED CallCountingStubCode, _TEXT

