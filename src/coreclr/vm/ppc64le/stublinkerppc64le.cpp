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
	PPC64LECall (): InstructionFormat(InstructionFormat::k32)
	{
	    LIMITED_METHOD_CONTRACT;
	}
	virtual UINT GetSizeOfInstruction(UINT refsize, UINT variationCode)
	{
            LIMITED_METHOD_CONTRACT;

            _ASSERTE(refsize == InstructionFormat::k32);

            return 28;
	}
	virtual VOID EmitInstruction(UINT refsize, int64_t fixedUpReference, BYTE *pOutBufferRX, BYTE *pOutBufferRW, UINT variationCode, BYTE *pDataBuffer)
        {
	    UINT64 target = (UINT64)(((INT64)pOutBufferRX) + fixedUpReference + GetSizeOfInstruction(refsize, variationCode));

            // lis r12, <target>
            pOutBufferRW[0] = 0x3D;
            pOutBufferRW[1] = 0x80;
            *((UINT64*)&pOutBufferRW[2]) = ((UINT64)(target) >> 48 & 0xffff);

            // ori r12, r12, <target>
            pOutBufferRW[4] = 0x61;
            pOutBufferRW[5] = 0x8C;
            *((UINT64*)&pOutBufferRW[6]) = ((UINT64)(target) >> 32) & 0xffff;

            // sldi r12, r12, 32
            pOutBufferRW[8] = 0x79;
            pOutBufferRW[9] = 0x8C;
            pOutBufferRW[10] = 0x07;
            pOutBufferRW[11] = 0xC6;

            // oris r12, r12, <target>
            pOutBufferRW[12] = 0x65;
            pOutBufferRW[13] = 0x8C;
            *((UINT64*)&pOutBufferRW[14]) = ((UINT64)(target) >> 16) & 0xffff;

            // ori r12, r12, <target>
            pOutBufferRW[16] = 0x61;
            pOutBufferRW[17] = 0x8C;
            *((UINT64*)&pOutBufferRW[18]) = (UINT64)(target) & 0xffff;

            // mtctr r12
            pOutBufferRW[20] = 0x7D;
            pOutBufferRW[21] = 0x89;
            pOutBufferRW[22] = 0x03;
            pOutBufferRW[23] = 0xA6;

            // bcctr
            pOutBufferRW[24] = 0x4E;
            pOutBufferRW[25] = 0x80;
            pOutBufferRW[26] = 0x04;
            pOutBufferRW[27] = 0x20;
	}
	/*virtual BOOL CanReach(UINT refsize, UINT variationCode, BOOL fExternal, INT_PTR offset)
	{
            _ASSERTE(refsize == InstructionFormat::k32);

	    if (fExternal)
		return false;

    	    return FitsInI4(offset);
	}*/


};


static BYTE gPPC64LECall[sizeof(PPC64LECall)];

/* static */ void StubLinkerCPU::Init()
{
     CONTRACTL
     {
         THROWS;
         GC_NOTRIGGER;
         INJECT_FAULT(COMPlusThrowOM(););
     }
     CONTRACTL_END;
 #if 0
     new (gX86NearJump) X86NearJump();
     new (gX86CondJump) X86CondJump( InstructionFormat::k8|InstructionFormat::k32);
     new (gX86PushImm32) X86PushImm32(InstructionFormat::k32);
 #endif
     new (gPPC64LECall) PPC64LECall();
}

/*void StubLinkerCPU::EmitBranchOnConditionRegister(CondMask M1, IntReg R2)
{
}*/

void StubLinkerCPU::EmitBranchRegister(IntReg R2)
{
}

void StubLinkerCPU::EmitLoadRegister(IntReg R1, IntReg R2)
{
    _ASSERTE(!"NYI POWERPC64 EmitLoadRegister");
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
    _ASSERTE(!"NYI POWERPC64 EmitLoadImmediate");
}

void StubLinkerCPU::EmitSaveIncomingArguments(unsigned int cIntRegArgs, unsigned int cFloatRegArgs)
{
    _ASSERTE(!"NYI POWERPC64 EmitSaveIncomingArguments");
}
void StubLinkerCPU::EmitCallLabel(CodeLabel *target, BOOL fTailCall, BOOL fIndirect)
{
    _ASSERTE(!"NYI POWERPC64 EmitCallLabel");
}

VOID StubLinkerCPU::EmitComputedInstantiatingMethodStub(MethodDesc* pSharedMD, struct ShuffleEntry *pShuffleEntryArray, void* extraArg)
{
    _ASSERTE(!"NYI POWERPC64 EmitComputedInstantiatingMethodStub");
}

VOID StubLinkerCPU::EmitShuffleThunk(ShuffleEntry *pShuffleEntryArray)
{
    // TODO TARGET_POWERPC64
    _ASSERTE(!"NYI POWERPC64");
}

void StubLinkerCPU::EmitMovReg(IntReg R1, IntReg R2)
{
    _ASSERTE(!"NYI POWERPC64 EmitMovReg");
}

void StubLinkerCPU::EmitMovConstant(IntReg R1, int I2)
{
    _ASSERTE(!"NYI POWERPC64 EmitMovConstant");
}

void StubLinkerCPU::EmitAddImm(IntReg R1, IntReg R2, unsigned int I3)
{
    _ASSERTE(!"NYI POWERPC64 EmitAddImm");
}

unsigned int StubLinkerCPU::GetSavedRegArgsOffset()
{
    _ASSERTE(!"NYI POWERPC64 GetSavedRegArgsOffset");
    return 0;
}

#endif // !DACCESS_COMPILE
