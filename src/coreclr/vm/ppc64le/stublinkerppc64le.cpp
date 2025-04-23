// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
 
#include "common.h"
 
#include "field.h"
#include "stublink.h"
 
//#include "frames.h"
//#include "excep.h"
//#include "dllimport.h"
//#include "log.h"
#include "comdelegate.h"
//#include "array.h"
//#include "jitinterface.h"
//#include "codeman.h"
//#include "dbginterface.h"
//#include "eeprofinterfaces.h"
//#include "eeconfig.h"
//#include "class.h"
//#include "stublink.inl"
 
 
#ifndef DACCESS_COMPILE

class PPC64LECall : public InstructionFormat
{
public:
	PPC64LECall ()
	{
	}
	virtual UINT GetSizeOfInstruction(UINT refsize, UINT variationCode)
	{
	}
	virtual VOID EmitInstruction(UINT refsize, int64_t fixedUpReference, BYTE *pOutBufferRX, BYTE *pOutBufferRW, UINT variationCode, BYTE *pDataBuffer)
        {
	}
	virtual BOOL CanReach(UINT refsize, UINT variationCode, BOOL fExternal, INT_PTR offset)
	{
	}


};


static BYTE gPPC64LECall[sizeof(PPC64LECall)];

/* static */ void StubLinkerCPU::Init()
{
}

void StubLinkerCPU::EmitBranchOnConditionRegister(CondMask M1, IntReg R2)
{
}

void StubLinkerCPU::EmitBranchRegister(IntReg R2)
{
}

void StubLinkerCPU::EmitLoadRegister(IntReg R1, IntReg R2)
{
}

void StubLinkerCPU::EmitLoadHalfwordImmediate(IntReg R1, int I2)
{
}

void StubLinkerCPU::EmitLoadLogicalImmediateLow(IntReg R1, DWORD I2)
{
}

void StubLinkerCPU::EmitLoadLogicalImmediateHigh(IntReg R1, DWORD I2)
{
}

void StubLinkerCPU::EmitInsertImmediateLow(IntReg R1, DWORD I2)
{
}

void StubLinkerCPU::EmitInsertImmediateHigh(IntReg R1, DWORD I2)
{
}

void StubLinkerCPU::EmitLoadAddress(IntReg R1, int D2, IntReg X2, IntReg B2)
{
}

void StubLinkerCPU::EmitLoadAddress(IntReg R1, int D2, IntReg B2)
{
}

void StubLinkerCPU::EmitLoad(IntReg R1, int D2, IntReg X2, IntReg B2)
{
}

void StubLinkerCPU::EmitLoad(IntReg R1, int D2, IntReg B2)
{
}

void StubLinkerCPU::EmitStore(IntReg R1, int D2, IntReg X2, IntReg B2)
{
}

void StubLinkerCPU::EmitStore(IntReg R1, int D2, IntReg B2)
{
}

void StubLinkerCPU::EmitStoreFloat(VecReg R1, int D2, IntReg X2, IntReg B2)
{
}

void StubLinkerCPU::EmitStoreFloat(VecReg R1, int D2, IntReg B2)
{
}

void StubLinkerCPU::EmitLoadMultiple(IntReg R1, IntReg R3, int D2, IntReg B2)
{
}

void StubLinkerCPU::EmitStoreMultiple(IntReg R1, IntReg R3, int D2, IntReg B2)
{
}

void StubLinkerCPU::EmitLoadImmediate(IntReg target, UINT64 constant)
{
}

void StubLinkerCPU::EmitSaveIncomingArguments(unsigned int cIntRegArgs, unsigned int cFloatRegArgs)
{
}

#if 0
void StubLinkerCPU::EmitProlog(unsigned short cIntRegArgs, unsigned short cVecRegArgs, unsigned short cCalleeSavedRegs, unsigned short cbStackSpace)
{
}

void StubLinkerCPU::EmitEpilog()
{
}

void StubLinkerCPU::EmitLoadIncomingSaveArea(IntReg target)
{
}
#endif

void StubLinkerCPU::EmitCallLabel(CodeLabel *target)
{

}

VOID StubLinkerCPU::EmitComputedInstantiatingMethodStub(MethodDesc* pSharedMD, struct ShuffleEntry *pShuffleEntryArray, void* extraArg)
{
}

VOID StubLinkerCPU::EmitShuffleThunk(ShuffleEntry *pShuffleEntryArray)
{
}

#endif // !DACCESS_COMPILE
