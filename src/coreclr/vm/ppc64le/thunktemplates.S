// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

// STUB_PAGE_SIZE must match the behavior of GetStubCodePageSize() on this architecture/os
//STUB_PAGE_SIZE = 16384

#define DATA_SLOT(stub, field) C_FUNC(stub##Code) + STUB_PAGE_SIZE + stub##Data__##field

LEAF_ENTRY StubPrecodeCode, _TEXT
// NOT YET IMPLEMENTED
LEAF_END_MARKED StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT
// NOT YET IMPLEMENTED
LEAF_END_MARKED FixupPrecodeCode, _TEXT

LEAF_ENTRY CallCountingStubCode, _TEXT
// NOT YET IMPLEMENTED
LEAF_END_MARKED CallCountingStubCode, _TEXT
