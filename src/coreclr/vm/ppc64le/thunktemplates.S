// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

// STUB_PAGE_SIZE must match the behavior of GetStubCodePageSize() on this architecture/os
STUB_PAGE_SIZE = 65536

#define DATA_SLOT(stub, field) C_FUNC(stub##Code) + STUB_PAGE_SIZE + stub##Data__##field


LEAF_ENTRY StubPrecodeCode, _TEXT
        lis %r11, DATA_SLOT(StubPrecode, MethodDesc)@highest
	ori %r11, %r11, DATA_SLOT(StubPrecode, MethodDesc)@higher
	sldi %r11, %r11, 32
	oris %r11, %r11, DATA_SLOT(StubPrecode, MethodDesc)@high
	ori %r11, %r11, DATA_SLOT(StubPrecode, MethodDesc)@l
		
        lis %r12, DATA_SLOT(StubPrecode, Target)@highest
	ori %r12, %r12, DATA_SLOT(StubPrecode, Target)@higher
	sldi %r12, %r12, 32
	oris %r12, %r12, DATA_SLOT(StubPrecode, Target)@high
	ori %r12, %r12, DATA_SLOT(StubPrecode, Target)@l
	mtctr %r12
	bctr

LEAF_END_MARKED StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT
        lis %r12, DATA_SLOT(FixupPrecode, Target)@highest
	ori %r12, %r12, DATA_SLOT(FixupPrecode, Target)@higher
	sldi %r12, %r12, 32
	oris %r12, %r12, DATA_SLOT(FixupPrecode, Target)@high
	ori %r12, %r12, DATA_SLOT(FixupPrecode, Target)@l
	mtctr %r12
	bctr

        lis %r11, DATA_SLOT(FixupPrecode, MethodDesc)@highest
	ori %r11, %r11, DATA_SLOT(FixupPrecode, MethodDesc)@higher
	sldi %r11, %r11, 32
	oris %r11, %r11, DATA_SLOT(FixupPrecode, MethodDesc)@high
	ori %r11, %r11, DATA_SLOT(FixupPrecode, MethodDesc)@l

        lis %r12, DATA_SLOT(FixupPrecode, PrecodeFixupThunk)@highest
	ori %r12, %r12, DATA_SLOT(FixupPrecode, PrecodeFixupThunk)@higher
	sldi %r12, %r12, 32
	oris %r12, %r12, DATA_SLOT(FixupPrecode, PrecodeFixupThunk)@high
	ori %r12, %r12, DATA_SLOT(FixupPrecode, PrecodeFixupThunk)@l
	mtctr %r12
	bctr
	nop
LEAF_END_MARKED FixupPrecodeCode, _TEXT

LEAF_ENTRY CallCountingStubCode, _TEXT
// NOT YET IMPLEMENTED
LEAF_END_MARKED CallCountingStubCode, _TEXT
