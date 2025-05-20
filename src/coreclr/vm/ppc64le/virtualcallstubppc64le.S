// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"

// This is the number of times a successful chain lookup will occur before the
// entry is promoted to the front of the chain. This is declared as extern because
// the default value (CALL_STUB_CACHE_INITIAL_SUCCESS_COUNT) is defined in the header.
// extern size_t g_dispatch_cache_chain_success_counter;
#define CHAIN_SUCCESS_COUNTER g_dispatch_cache_chain_success_counter

#define BACKPATCH_FLAG             1        // Also known as SDF_ResolveBackPatch    in the EE
#define PROMOTE_CHAIN_FLAG         2        // Also known as SDF_ResolvePromoteChain in the EE
#define INITIAL_SUCCESS_COUNT      0x100

// On Input:
//  8(%r15)  contains the address of the indirection cell (with the flags in the low bits)
//  %r0      contains the dispatch token  (slot number or MethodDesc) for the target
//                    or the ResolveCacheElem when the PROMOTE_CHAIN_FLAG is set
NESTED_ENTRY ResolveWorkerAsmStub, _TEXT, NoHandler
// NOT YET COMPLETED
NESTED_END ResolveWorkerAsmStub, _TEXT

// This will perform a quick chained lookup of the entry if the initial cache lookup fails
// On Input:
//  16(%r15) contains the pointer to the ResolveCacheElem
//  8(%r15)  contains the address of the indirection cell (with the flags in the low bits)
//  %r0      contains the dispatch token
//  %r2      contains the object ref (used to look up the MethodTable)
LEAF_ENTRY ResolveWorkerChainLookupAsmStub, _TEXT
// NOT YET COMPLETED
LEAF_END ResolveWorkerChainLookupAsmStub, _TEXT
