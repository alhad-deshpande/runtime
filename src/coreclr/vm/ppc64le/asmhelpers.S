# Licensed to the .NET Foundation under one or more agreements.
# The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

# EXTERN_C void ProfileEnterNaked(FunctionIDOrClientID functionIDOrClientID, size_t profiledRsp);
# <NOTE>
#
# </NOTE>
NESTED_ENTRY ProfileEnterNaked, _TEXT, NoHandler
# NOT YET IMPLEMENTED
NESTED_END ProfileEnterNaked, _TEXT

# EXTERN_C void ProfileLeaveNaked(FunctionIDOrClientID functionIDOrClientID, size_t profiledRsp);
# <NOTE>
#
# </NOTE>
NESTED_ENTRY ProfileLeaveNaked, _TEXT, NoHandler
# NOT YET IMPLEMENTED
NESTED_END ProfileLeaveNaked, _TEXT

# EXTERN_C void ProfileTailcallNaked(FunctionIDOrClientID functionIDOrClientID, size_t profiledRsp);
# <NOTE>
#
# </NOTE>
NESTED_ENTRY ProfileTailcallNaked, _TEXT, NoHandler
# NOT YET IMPLEMENTED
NESTED_END ProfileTailcallNaked, _TEXT


LEAF_ENTRY JIT_ProfilerEnterLeaveTailcallStub, _TEXT
    # NOT YET IMPLEMENTED
LEAF_END JIT_ProfilerEnterLeaveTailcallStub, _TEXT

