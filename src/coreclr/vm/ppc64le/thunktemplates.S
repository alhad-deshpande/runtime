// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

// STUB_PAGE_SIZE must match the behavior of GetStubCodePageSize() on this architecture/os
STUB_PAGE_SIZE = 65536

#define DATA_SLOT(stub, field) (stub##Data__##field)
#define DATA_SLOT_FIXUPTHUNK(stub, field) (stub##Data__##field - 16)


LEAF_ENTRY StubPrecodeCode, _TEXT
	addis %r11, %r12, STUB_PAGE_SIZE@high
	ld %r11, DATA_SLOT(StubPrecode, MethodDesc)(%r11)
			
	addis %r12, %r12, STUB_PAGE_SIZE@high
	ld %r12, DATA_SLOT(StubPrecode, Target)(%r12)
	mtctr %r12
bctr

LEAF_END_MARKED StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT
	addis %r12, %r12, STUB_PAGE_SIZE@high
	ld %r12, DATA_SLOT(FixupPrecode, Target)(%r12)
	mtctr %r12
	bctr

	addis %r11, %r12, STUB_PAGE_SIZE@high
	ld %r11, DATA_SLOT_FIXUPTHUNK(FixupPrecode, MethodDesc)(%r11)

	addis %r12, %r12, STUB_PAGE_SIZE@high
	ld %r12, DATA_SLOT_FIXUPTHUNK(FixupPrecode, PrecodeFixupThunk)(%r12)
	mtctr %r12
	bctr
LEAF_END_MARKED FixupPrecodeCode, _TEXT

LEAF_ENTRY CallCountingStubCode, _TEXT
// NOT YET IMPLEMENTED
LEAF_END_MARKED CallCountingStubCode, _TEXT
