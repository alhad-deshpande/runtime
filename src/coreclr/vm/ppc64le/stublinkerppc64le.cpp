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
	PPC64LECall (): InstructionFormat(InstructionFormat::k64)
	{
	    LIMITED_METHOD_CONTRACT;
	}
	virtual UINT GetSizeOfInstruction(UINT refsize, UINT variationCode)
	{
            LIMITED_METHOD_CONTRACT;

            _ASSERTE(refsize == InstructionFormat::k64);

            return 28;
	}
	virtual VOID EmitInstruction(UINT refsize, int64_t fixedUpReference, BYTE *pOutBufferRX, BYTE *pOutBufferRW, UINT variationCode, BYTE *pDataBuffer)
        {
	    UINT64 target = (UINT64)(((INT64)pOutBufferRX) + fixedUpReference + GetSizeOfInstruction(refsize, variationCode));

            // lis r0, <target>
            *((UINT64*)&pOutBufferRW[0]) = ((UINT64)(target) >> 48 & 0xff);
            *((UINT64*)&pOutBufferRW[1]) = ((UINT64)(target) >> 56 & 0xff);
            pOutBufferRW[2] = 0x00;
            pOutBufferRW[3] = 0x3C;

            // ori r0, r0, <target>
            *((UINT64*)&pOutBufferRW[4]) = ((UINT64)(target) >> 32) & 0xff;
            *((UINT64*)&pOutBufferRW[5]) = ((UINT64)(target) >> 40) & 0xff;
            pOutBufferRW[6] = 0x00;
            pOutBufferRW[7] = 0x60;

            // sldi r0, r0, 32
            pOutBufferRW[8] = 0xC6;
            pOutBufferRW[9] = 0x07;
            pOutBufferRW[10] = 0x00;
            pOutBufferRW[11] = 0x78;

            // oris r0, r0, <target>
            *((UINT64*)&pOutBufferRW[12]) = ((UINT64)(target) >> 16) & 0xff;
            *((UINT64*)&pOutBufferRW[13]) = ((UINT64)(target) >> 24) & 0xff;
            pOutBufferRW[14] = 0x00;
            pOutBufferRW[15] = 0x64;

            // ori r0, r0, <target>
            *((UINT64*)&pOutBufferRW[16]) = ((UINT64)(target) >> 0) & 0xff;
            *((UINT64*)&pOutBufferRW[17]) = ((UINT64)(target) >> 8) & 0xff;
            pOutBufferRW[18] = 0x00;
            pOutBufferRW[19] = 0x60;

            // mtlr r0
            pOutBufferRW[20] = 0xA6;
            pOutBufferRW[21] = 0x03;
            pOutBufferRW[22] = 0x08;
            pOutBufferRW[23] = 0x7C;

            // blrl.....// change instruction hex
            pOutBufferRW[24] = 0x21;
            pOutBufferRW[25] = 0x00;
            pOutBufferRW[26] = 0x80;
            pOutBufferRW[27] = 0x4E;
	}
	/*virtual BOOL CanReach(UINT refsize, UINT variationCode, BOOL fExternal, INT_PTR offset)
	{
            _ASSERTE(refsize == InstructionFormat::k64);

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
    EmitMoveRegister(R2,R1,R2);
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

void StubLinkerCPU::EmitLoadImmediate(IntReg RA, UINT64 Imm)
{
    if (((Imm >> 15) == 0) || ((Imm >> 15) == -1))
    {
	Emit32 ((DWORD)((14 << 26) | (RA << 21) | (0 << 16) | (Imm & 0xffff))); // li %r, Imm
    }
    else if (((Imm >> 31) == 0) || ((Imm >> 31) == -1))
    {
	Emit32((DWORD)((15 << 26) | (RA << 21) | (0 << 16)  | ((Imm >> 16) & 0xffff)));	// lis %r, Imm
	Emit32((DWORD)((24 << 26) | (RA << 21) | (RA << 16)  | (Imm & 0xffff)));	// ori %r, %r, Imm
    } 
    else if (((Imm >> 47) == 0) || ((Imm >> 47) == -1))
    {
	Emit32 ((DWORD)((14 << 26) | (RA << 21) | (0 << 16) | ((Imm >> 32) & 0xffff)));
	Emit32((DWORD)((30 << 26) | (RA << 21) | (RA << 16) | ((32 & 0x1F) << 11) | (((((31) & 0x1F) << 1) | (((31) >> 5) & 0x1)) << 5) | (1 << 2) | ((((32) >> 5) & 0x1) << 1) | 0));
	Emit32((DWORD)((25 << 26) | (RA << 21) | (RA << 16)  | (((Imm >> 16) & 0xffff) & 0xffff)));
	Emit32((DWORD)((24 << 26) | (RA << 21) | (RA << 16)  | ((Imm & 0xffff) & 0xffff)));
	
    } 
    else 
    {
	Emit32((DWORD)((15 << 26) | (RA << 21) | (0 << 16)  | ((Imm >> 48) & 0xffff)));
	Emit32((DWORD)((24 << 26) | (RA << 21) | (RA << 16)  | ((Imm >> 32) & 0xffff)));	// ori %r, %r, Imm
	Emit32((DWORD)((30 << 26) | (RA << 21) | (RA << 16) | ((Imm & 0x1F) << 11) | ((((63 - Imm) & 0x1F) << 1) | (((63 - Imm) >> 5) & 0x1)) | (1 << 2) | (((Imm >> 5) & 0x1) << 1) | 0 ));
	Emit32((DWORD)((25 << 26) | (RA << 21) | (RA << 16)  | (((Imm >> 16) & 0xffff) & 0xffff)));
	Emit32((DWORD)((24 << 26) | (RA << 21) | (RA << 16)  | ((Imm & 0xffff))));
    }
}

void StubLinkerCPU::EmitSaveIncomingArguments(unsigned int cIntRegArgs, unsigned int cFloatRegArgs)
{
    _ASSERTE(cIntRegArgs <= 8);
    _ASSERTE(cFloatRegArgs <= 13);

    // Store integer argument registers
    int disp = 32;
    for (int i=3; i<=cIntRegArgs+3; i++)
    {
    	EmitStoreDoubleWord(i, 1, disp);
	disp = disp + 8;
    }

    // Store call-saved registers
    for (int i=14; i<=31; i++)
    {
    	EmitStoreDoubleWord(i, 1, disp);
	disp = disp + 8;
    }

    // Store floating-point argument registers
    for (int i=1; i<=cFloatRegArgs+1; i++)
    {
    	EmitStoreFloatingPointDouble(i, 1, disp);
	disp = disp + 8;
    }
}
// std %r3, 32(%r1)
void StubLinkerCPU::EmitStoreDoubleWord(IntReg RS, IntReg RA, int DS)
{
    STANDARD_VM_CONTRACT;
    Emit32((DWORD)((62 << 26) | ((RS) << 21) | ((RA) << 16) | DS));
}

// stfd %f1, 256(%r1)
void StubLinkerCPU::EmitStoreFloatingPointDouble(VecReg RS, IntReg RA, int DS)
{
    STANDARD_VM_CONTRACT;
    Emit32((DWORD)((54 << 26) | ((RS) << 21) | ((RA) << 16) | DS));
}

// mr %r3, %r11
void StubLinkerCPU::EmitMoveRegister(IntReg RS, IntReg RA, IntReg RB)
{
    STANDARD_VM_CONTRACT;

    Emit32((DWORD)((31 << 26) | ((RS) << 21) | ((RA) << 16) | ((RB) << 11) | 888));
}

void StubLinkerCPU::EmitCallLabel(CodeLabel *target)
{
    EmitLabelRef(target, reinterpret_cast<PPC64LECall&>(gPPC64LECall), 0);
    //_ASSERTE(!"NYI POWERPC64 EmitCallLabel");
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
