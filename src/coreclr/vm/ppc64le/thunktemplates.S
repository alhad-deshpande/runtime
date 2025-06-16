// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

// STUB_PAGE_SIZE must match the behavior of GetStubCodePageSize() on this architecture/os
STUB_PAGE_SIZE = 65536

#define DATA_SLOT(stub, field) C_FUNC(stub##Code) + STUB_PAGE_SIZE + stub##Data__##field


LEAF_ENTRY StubPrecodeCode, _TEXT
        lis %r11, DATA_SLOT(StubPrecode, MethodDesc) >> 48 & 0xffff
	ori %r11, %r11, DATA_SLOT(StubPrecode, MethodDesc) >> 32 & 0xffff
	sldi %r11, %r11, 32
	oris %r11, %r11, DATA_SLOT(StubPrecode, MethodDesc) >> 16 & 0xffff
	ori %r11, %r11, DATA_SLOT(StubPrecode, MethodDesc)         & 0xffff
		
        lis %r12, DATA_SLOT(StubPrecode, Target) >> 48 & 0xffff
	ori %r12, %r12, DATA_SLOT(StubPrecode, Target) >> 32 & 0xffff
	sldi %r12, %r12, 32
	oris %r12, %r12, DATA_SLOT(StubPrecode, Target) >> 16 & 0xffff
	ori %r12, %r12, DATA_SLOT(StubPrecode, Target)         & 0xffff
	mtctr %r12
	bctr

LEAF_END_MARKED StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT
// NOT YET IMPLEMENTED
LEAF_END_MARKED FixupPrecodeCode, _TEXT

LEAF_ENTRY CallCountingStubCode, _TEXT
// NOT YET IMPLEMENTED
LEAF_END_MARKED CallCountingStubCode, _TEXT
