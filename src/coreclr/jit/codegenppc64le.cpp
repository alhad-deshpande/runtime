// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                        PPC64LE Code Generator Common Code                 XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#ifdef TARGET_POWERPC64 // This file is ONLY used for POWERPC64 architecture

#include "codegen.h"
#include "lower.h"
#include "gcinfo.h"
#include "emit.h"
#include "patchpointinfo.h"

// TODO POWERPC64


//------------------------------------------------------------------------
// genStackPointerConstantAdjustment: add a specified constant value to the stack pointer.
// No probe is done.
//
// Arguments:
//    spDelta                 - the value to add to SP. Must be negative or zero.
//    regTmp                  - an available temporary register that is used if 'spDelta' cannot be encoded by
//                              'sub sp, sp, #spDelta' instruction.
//                              Can be REG_NA if the caller knows for certain that 'spDelta' fits into the immediate
//                              value range.
//
// Return Value:
//    None.
//
void CodeGen::genStackPointerConstantAdjustment(ssize_t spDelta, regNumber regTmp)
{
    //_ASSERTE("!NYI");
    abort();
}

//------------------------------------------------------------------------
// genStackPointerConstantAdjustmentWithProbe: add a specified constant value to the stack pointer,
// and probe the stack as appropriate. Should only be called as a helper for
// genStackPointerConstantAdjustmentLoopWithProbe.
//
// Arguments:
//    spDelta                 - the value to add to SP. Must be negative or zero. If zero, the probe happens,
//                              but the stack pointer doesn't move.
//    regTmp                  - temporary register to use as target for probe load instruction
//
// Return Value:
//    None.
//
void CodeGen::genStackPointerConstantAdjustmentWithProbe(ssize_t spDelta, regNumber regTmp)
{
    //_ASSERTE("!NYI");
    abort();
}

//------------------------------------------------------------------------
// genStackPointerConstantAdjustmentLoopWithProbe: Add a specified constant value to the stack pointer,
// and probe the stack as appropriate. Generates one probe per page, up to the total amount required.
// This will generate a sequence of probes in-line.
//
// Arguments:
//    spDelta                 - the value to add to SP. Must be negative.
//    regTmp                  - temporary register to use as target for probe load instruction
//
// Return Value:
//    Offset in bytes from SP to last probed address.
//
target_ssize_t CodeGen::genStackPointerConstantAdjustmentLoopWithProbe(ssize_t spDelta, regNumber regTmp)
{
    //_ASSERTE("!NYI");
    abort();
}

//------------------------------------------------------------------------
// genLclHeap: Generate code for localloc (GT_LCLHEAP).
//
// Arguments:
//    tree - the GT_LCLHEAP node
//
// PPC64LE ELFv2 localloc frame layout:
//
//   BEFORE:
//     caller_SP - 0    ← grandparent backchain
//     caller_SP - 8    ← saved old r31
//     caller_SP - 16   ← top of our frame / localloc anchor
//     r31 = r1 = caller_SP - totalFrameSize  (frame base)
//     r1 + 0 .. r1 + totalFrameSize  ← entire frame
//
//   AFTER localloc(allocSize):
//     new_r1 = old_r1 - allocSize  (SP shifted down)
//     new_r1 + 0 .. +totalFrameSize  ← frame COPIED down by allocSize
//     new_r1 + totalFrameSize ..      ← localloc block (zeroed)  ← targetReg
//                  .. + allocSize
//     caller_SP - 16                 ← top of localloc block
//
// r31 is loaded with caller_SP for backchain writes, then restored to new r1.
// The epilog restores r31 from the callee-save area slot, not from the register.
//
void CodeGen::genLclHeap(GenTree* tree)
{
    assert(tree->OperGet() == GT_LCLHEAP);
    assert(compiler->compLocallocUsed);

    emitter*  emit      = GetEmitter();
    GenTree*  size      = tree->AsOp()->gtOp1;
    regNumber targetReg = tree->GetRegNum();
    var_types type      = genActualType(size->gtType);
    emitAttr  easz      = emitTypeSize(type);

    noway_assert((genActualType(size->gtType) == TYP_INT) || (genActualType(size->gtType) == TYP_I_IMPL));
    noway_assert(isFramePointerUsed());
    noway_assert(genStackLevel == 0);

    const int   totalFrameSize = genTotalFrameSize();
    BasicBlock* endLabel       = nullptr;

    // -----------------------------------------------------------------------
    // Step 1: r31 ← caller_SP (= 0(r1)).
    // r31 is clobbered here; the epilog restores it from the callee-save slot.
    // We use r31 to hold caller_SP so we can write the correct ELFv2 backchain
    // (0(new_r1) = caller_SP) after every SP decrement.
    // -----------------------------------------------------------------------
    emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_FP, REG_SPBASE, 0);

    // -----------------------------------------------------------------------
    // Step 2: determine allocSize (aligned to STACK_ALIGN)
    // -----------------------------------------------------------------------
    size_t    amount = 0;
    regNumber regCnt = REG_NA; // holds allocSize for non-constant path

    if (size->IsCnsIntOrI())
    {
        assert(size->isContained());
        amount = size->AsIntCon()->gtIconVal;
        if (amount == 0)
        {
            // Zero size: return pointer = caller_SP - 16 (top of localloc area).
            genInstrWithConstant(INS_addi, EA_PTRSIZE, targetReg, REG_FP, -16, REG_R0);
            goto BAILOUT;
        }
        amount = AlignUp(amount, STACK_ALIGN);
    }
    else
    {
        genConsumeRegAndCopy(size, targetReg);
        endLabel = genCreateTempLabel();
        emit->emitIns_R_I(INS_cmpdi, easz, targetReg, 0);
        inst_JMP(EJ_eq, endLabel);

        regCnt = internalRegisters.Extract(tree);
        inst_Mov(type, regCnt, targetReg, /* canSkip */ true);
        // Round up to STACK_ALIGN (16): add 15, then clear the bottom 4 bits.
        // andi. only has a 16-bit unsigned immediate and cannot encode ~0xF correctly
        // as a 64-bit mask.  Use srdi+sldi instead: shift right 4 then shift left 4
        // zeroes bits 63:60 (IBM numbering) = clears the bottom 4 bits (LE LSBs).
        emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regCnt, regCnt, STACK_ALIGN - 1);
        emit->emitIns_R_R_I(INS_srdi, EA_PTRSIZE, regCnt, regCnt, 4); // regCnt >>= 4
        emit->emitIns_R_R_I(INS_sldi, EA_PTRSIZE, regCnt, regCnt, 4); // regCnt <<= 4 (bottom 4 bits cleared)
    }

    // -----------------------------------------------------------------------
    // Step 3: move SP down by allocSize and immediately write backchain.
    //   new_r1 = old_r1 - allocSize
    //   0(new_r1) = caller_SP  (held in r31)
    // -----------------------------------------------------------------------
    if (size->IsCnsIntOrI())
    {
        genInstrWithConstant(INS_addi, EA_PTRSIZE, REG_SPBASE, REG_SPBASE,
                             -(ssize_t)amount, REG_R0);
    }
    else
    {
        // subf r1, regCnt, r1  →  r1 = r1 - regCnt
        emit->emitIns_R_R_R(INS_subf, EA_PTRSIZE, REG_SPBASE, regCnt, REG_SPBASE);
    }
    emit->emitIns_R_R_I(INS_std, EA_PTRSIZE, REG_FP, REG_SPBASE, 0);

    // -----------------------------------------------------------------------
    // Step 4: copy old frame contents to new position.
    //
    //   src  = r31 - frameSize + 8   (old r1 + 8, skip the backchain word)
    //   dst  = r1  + 8               (new r1 + 8)
    //   count = frameSize - 8 bytes, 8 bytes at a time
    //
    // After copy, dst points to r1 + 8 + (frameSize - 8) = r1 + frameSize
    //           = (caller_SP - frameSize - allocSize) + frameSize
    //           = caller_SP - allocSize
    //
    // Step 5: zero localloc block [caller_SP - 16 - allocSize .. caller_SP - 16).
    //   targetReg = r31 - 16 - allocSize  = caller_SP - 16 - allocSize  ✓
    //
    // r0 = 8-byte copy/zero scratch (not LSRA-allocated)
    // -----------------------------------------------------------------------
    {
        const int copyBytes = totalFrameSize - 24;

        // Constant path:     Extract regCtr, regSrc, regDst  (3 regs)
        // Non-constant path: regCnt already extracted in Step 2; then regCtr, regSrc, regDst
        regNumber regCtr = internalRegisters.Extract(tree); // loop counter (NOT r13/TLS)
        regNumber regSrc = internalRegisters.Extract(tree);
        regNumber regDst = internalRegisters.Extract(tree);

        // regSrc = old_r1 + 8  (r31=caller_SP, r31-(totalFrameSize-8)=old_r1+8)
        genInstrWithConstant(INS_addi, EA_PTRSIZE, regSrc, REG_FP,
                             -(ssize_t)(totalFrameSize - 8), REG_R0);

        // regDst = new_r1 + 8
        genInstrWithConstant(INS_addi, EA_PTRSIZE, regDst, REG_SPBASE,
                             8, REG_R0);

        // Copy loop: copyBytes = totalFrameSize - 24, 8 bytes per iteration.
        if (copyBytes > 0)
        {
            instGen_Set_Reg_To_Imm(EA_PTRSIZE, regCtr, copyBytes);
            BasicBlock* copyLoop = genCreateTempLabel();
            genDefineTempLabel(copyLoop);
            emit->emitIns_R_R_I(INS_ld,   EA_PTRSIZE, REG_R0, regSrc, 0);
            emit->emitIns_R_R_I(INS_std,  EA_PTRSIZE, REG_R0, regDst, 0);
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regSrc, regSrc,   8);
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regDst, regDst,   8);
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regCtr, regCtr,  -8);
            emit->emitIns_R_I(INS_cmpdi,  EA_PTRSIZE, regCtr, 0);
            inst_JMP(EJ_gt, copyLoop);
        }

        // -----------------------------------------------------------------------
        // Step 5: zero [caller_SP - 16 - allocSize .. caller_SP - 16).
        //   targetReg = r31 - 16 - allocSize
        // -----------------------------------------------------------------------
        instGen_Set_Reg_To_Zero(EA_PTRSIZE, REG_R0);

        if (size->IsCnsIntOrI())
        {
            // targetReg = r31 - 16 - amount  (r31 = caller_SP)
            genInstrWithConstant(INS_addi, EA_PTRSIZE, targetReg, REG_FP,
                                 -(ssize_t)(16 + amount), regCtr);

            if (amount <= compiler->getUnrollThreshold(Compiler::UnrollKind::Memset))
            {
                for (ssize_t off = 0; off < (ssize_t)amount; off += REGSIZE_BYTES)
                    emit->emitIns_R_R_I(INS_std, EA_PTRSIZE, REG_R0, targetReg, off);
            }
            else
            {
                emit->emitIns_Mov(INS_mov, EA_PTRSIZE, regDst, targetReg, /* canSkip */ false);
                instGen_Set_Reg_To_Imm(EA_PTRSIZE, regCtr, (ssize_t)amount);
                BasicBlock* zeroLoop = genCreateTempLabel();
                genDefineTempLabel(zeroLoop);
                emit->emitIns_R_R_I(INS_std,  EA_PTRSIZE, REG_R0, regDst, 0);
                emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regDst, regDst,   8);
                emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regCtr, regCtr,  -8);
                emit->emitIns_R_I(INS_cmpdi,  EA_PTRSIZE, regCtr, 0);
                inst_JMP(EJ_gt, zeroLoop);
            }
        }
        else
        {
            // Localloc block layout (non-constant allocSize):
            //   start = new_r1 + (totalFrameSize - 16) = r1 + (totalFrameSize - 16)
            //   end   = r31 - 16 = caller_SP - 16
            //   size  = (r31 - 16) - start = allocSize
            //
            // targetReg = r1 + (totalFrameSize - 16)  (start of block, returned to caller)
            genInstrWithConstant(INS_addi, EA_PTRSIZE, targetReg, REG_SPBASE,
                                 (ssize_t)(totalFrameSize - 16), REG_R0);

            // regCtr = (r31 - 16) - targetReg = allocSize
            // Step 1: regCtr = r31 - 16
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regCtr, REG_FP, -16);
            // Step 2: regCtr = regCtr - targetReg  (subf D,A,B → D=B-A)
            emit->emitIns_R_R_R(INS_subf, EA_PTRSIZE, regCtr, targetReg, regCtr);

            emit->emitIns_Mov(INS_mov, EA_PTRSIZE, regDst, targetReg, /* canSkip */ false);
            BasicBlock* zeroLoop = genCreateTempLabel();
            genDefineTempLabel(zeroLoop);
            emit->emitIns_R_R_I(INS_std,  EA_PTRSIZE, REG_R0, regDst, 0);
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regDst, regDst,   8);
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regCtr, regCtr,  -8);
            emit->emitIns_R_I(INS_cmpdi,  EA_PTRSIZE, regCtr, 0);
            inst_JMP(EJ_gt, zeroLoop);
        }
    }

BAILOUT:
    // r31 currently holds caller_SP (loaded at Step 1).
    // Restore r31 = new frame base (= new r1) so that FP-relative locals are correct.
    // Do NOT write r31 into -8(r31): that slot holds the saved original r31 written
    // by the prolog and must not be clobbered — the epilog reloads r31 from it.
    GetEmitter()->emitIns_Mov(INS_mov, EA_PTRSIZE, REG_FP, REG_SPBASE, /* canSkip */ false);
    GetEmitter()->emitIns_R_R_I(INS_std,  EA_PTRSIZE, REG_FP, REG_SPBASE, -8);
    if (endLabel != nullptr)
        genDefineTempLabel(endLabel);

    genProduceReg(tree);
}

//------------------------------------------------------------------------
// genSetRegToConst: Generate code to set a register 'targetReg' of type 'targetType'
//    to the constant specified by the constant (GT_CNS_INT or GT_CNS_DBL) in 'tree'.
//
// Notes:
//    This does not call genProduceReg() on the target register.
//
void CodeGen::genSetRegToConst(regNumber targetReg, var_types targetType, GenTree* tree)
{
    switch (tree->gtOper)
    {
        case GT_CNS_INT:
	{
            // relocatable values tend to come down as a CNS_INT of native int type
            // so the line between these two opcodes is kind of blurry
            GenTreeIntConCommon* con    = tree->AsIntConCommon();
            ssize_t              cnsVal = con->IconValue();

            emitAttr attr = emitActualTypeSize(targetType);

            // TODO-CQ: Currently we cannot do this for all handles because of
            // https://github.com/dotnet/runtime/issues/60712
            if (con->ImmedValNeedsReloc(compiler))
            {
                attr = EA_SET_FLG(attr, EA_CNS_RELOC_FLG);
            }

            if (targetType == TYP_BYREF)
            {
                attr = EA_SET_FLG(attr, EA_BYREF_FLG);
            }

            instGen_Set_Reg_To_Imm(attr, targetReg, cnsVal);
            regSet.verifyRegUsed(targetReg);
        }
        break;

        case GT_CNS_DBL:
        {
            emitter* emit       = GetEmitter();
            emitAttr size       = emitActualTypeSize(tree);
            double   constValue = tree->AsDblCon()->DconValue();

            // For PPC64LE, we load floating-point constants via GPR and stack
            // Get the bit representation of the double/float value
            int64_t constValueBits;
            emitAttr gprSize;
            instruction storeIns;
            instruction loadIns;
            
            if (size == EA_4BYTE)
            {
                // Float constant
                float fltVal = (float)constValue;
                constValueBits = *(int32_t*)&fltVal;
                gprSize = EA_4BYTE;
                storeIns = INS_stw;
                loadIns = INS_lfs;
            }
            else
            {
                // Double constant
                constValueBits = *(int64_t*)&constValue;
                gprSize = EA_8BYTE;
                storeIns = INS_std;
                loadIns = INS_lfd;
            }
            
            // Get a temp integer register to hold the constant bits
            regNumber tempReg = internalRegisters.GetSingle(tree);
            
            // Load the constant bit pattern into the temp GPR
            instGen_Set_Reg_To_Imm(gprSize, tempReg, constValueBits);
            
            // For PPC64LE, we need to transfer the value via stack
            // Use the top of the local frame (genTotalFrameSize() - 16) for temporary storage
            // This ensures we don't overwrite the linkage area or parameter save area
            int offset = genTotalFrameSize() - 16;
            
            // Store the GPR value to the temporary stack location
            emit->emitIns_R_R_I(storeIns, gprSize, tempReg, REG_SPBASE, offset);
            
            // Load the floating-point value from the temporary stack location
            emit->emitIns_R_R_I(loadIns, size, targetReg, REG_SPBASE, offset);
            
            regSet.verifyRegUsed(targetReg);
        }
        break;

	default:
	    abort();
    }
}

//------------------------------------------------------------------------
// genCodeForCompare: Produce code for a GT_EQ/GT_NE/GT_LT/GT_LE/GT_GE/GT_GT/GT_CMP node.
//
// Arguments:
//    tree - the node
//
void CodeGen::genCodeForCompare(GenTreeOp* tree)
{
    regNumber targetReg = tree->GetRegNum();
    emitter*  emit      = GetEmitter();

    GenTree*  op1     = tree->gtOp1;
    GenTree*  op2     = tree->gtOp2;
    var_types op1Type = genActualType(op1->TypeGet());
    var_types op2Type = genActualType(op2->TypeGet());
    instruction ins;

    assert(!op1->isUsedFromMemory());

    emitAttr cmpSize = EA_ATTR(genTypeSize(op1Type));

    assert(genTypeSize(op1Type) == genTypeSize(op2Type));

    if (varTypeIsFloating(op1Type))
    {
	// Floating-point comparison
    	assert(varTypeIsFloating(op2Type));
    	assert(!op1->isContainedIntOrIImmed());
    	assert(!op2->isContainedIntOrIImmed());

    	// PowerPC floating-point comparison instructions:
    	// fcmpu - Floating Compare Unordered (doesn't trap on NaN)
    	// fcmpo - Floating Compare Ordered (traps on NaN)
    	// Use fcmpu for standard comparisons (matches C# semantics)

    	ins = INS_fcmpu;

    	// fcmpu cr0, fA, fB
   	 // Sets CR0 condition register bits:
   	 //   CR0[LT] (bit 0) = fA < fB
    	//   CR0[GT] (bit 1) = fA > fB
    	//   CR0[EQ] (bit 2) = fA == fB
    	//   CR0[UN] (bit 3) = unordered (one or both operands are NaN)

    	emit->emitIns_R_R(ins, cmpSize, op1->GetRegNum(), op2->GetRegNum());
    }
    else
    {
 assert(!varTypeIsFloating(op2Type));
 assert(!op1->isContainedIntOrIImmed());

 // Determine whether this is an unsigned comparison.  The JIT sets GTF_UNSIGNED on
 // the comparison node when the C# source operands are unsigned (e.g. uint / nuint).
 // Unsigned comparisons must use cmpl[w|d][i] rather than cmp[w|d][i] so that the
 // hardware treats the operands as unsigned integers.  Using the signed forms (cmpw /
 // cmpwi) when the value in a register has bit 31 set would sign-extend it into a
 // negative 32-bit quantity and produce wrong branch outcomes — this is exactly the
 // bug that caused Dictionary.FindValue to throw ConcurrentOperationsNotSupportedException
 // when the collision counter exceeded INT_MAX.
 const bool isUnsigned = tree->IsUnsigned();

 // Use the immediate compare form only when the constant is contained (i.e. lowering
 // decided it is small enough to fold) and the value fits in the 16-bit field.
 // For unsigned immediate compares the field is zero-extended (UIMM16): range [0, 65535].
 // For signed immediate compares it is sign-extended (SIMM16): range [-32768, 32767].
 // Any value outside the applicable range falls through to the register-compare path.
 if (op2->isContainedIntOrIImmed())
 {
     ssize_t immVal = op2->AsIntConCommon()->IconValue();
     bool    fitsInImm;
     if (isUnsigned)
         fitsInImm = (immVal >= 0 && immVal <= 65535); // UIMM16
     else
         fitsInImm = (immVal >= -32768 && immVal <= 32767); // SIMM16

     if (fitsInImm)
     {
         if (isUnsigned)
             ins = (cmpSize == EA_8BYTE) ? INS_cmpldi : INS_cmplwi;
         else
             ins = (cmpSize == EA_8BYTE) ? INS_cmpdi : INS_cmpwi;
         emit->emitIns_R_I(ins, cmpSize, op1->GetRegNum(), immVal);
     }
     else
     {
         // Constant does not fit in the immediate field; materialise it and use
         // the register-compare form.
         regNumber tmpReg = internalRegisters.GetSingle(tree);
         instGen_Set_Reg_To_Imm(cmpSize, tmpReg, immVal);
         if (isUnsigned)
             ins = (cmpSize == EA_8BYTE) ? INS_cmpld : INS_cmplw;
         else
             ins = (cmpSize == EA_8BYTE) ? INS_cmpd : INS_cmpw;
         emit->emitIns_R_R(ins, cmpSize, op1->GetRegNum(), tmpReg);
     }
 }
 else
 {
     // op2 is a non-constant or a non-contained constant already in a register.
     if (isUnsigned)
         ins = (cmpSize == EA_8BYTE) ? INS_cmpld : INS_cmplw;
     else
         ins = (cmpSize == EA_8BYTE) ? INS_cmpd : INS_cmpw;
     emit->emitIns_R_R(ins, cmpSize, op1->GetRegNum(), op2->GetRegNum());
 }
    }

    if (targetReg != REG_NA)
    {
 	 // Need to materialize the comparison result into a register
   	 // Use inst_SETCC to convert CR0 flags to 0 or 1
    
   	 GenCondition condition;
    	if (varTypeIsFloating(op1Type))
    	{
        	// Floating-point comparison
        	condition = GenCondition::FromFloatRelop(tree);
    	}
    	else
    	{
        	// Integer comparison
        	condition = GenCondition::FromIntegralRelop(tree);
    	}
        inst_SETCC(condition, tree->TypeGet(), targetReg);
        genProduceReg(tree);
    }

}

//------------------------------------------------------------------------
// genCodeForTreeNode Generate code for a single node in the tree.
//
// Preconditions:
//    All operands have been evaluated.
//
void CodeGen::genCodeForTreeNode(GenTree* treeNode)
{
    regNumber targetReg  = treeNode->GetRegNum();
    var_types targetType = treeNode->TypeGet();
    emitter*  emit       = GetEmitter();

#ifdef DEBUG
    // Validate that all the operands for the current node are consumed in order.
    // This is important because LSRA ensures that any necessary copies will be
    // handled correctly.
    lastConsumedNode = nullptr;
    if (compiler->verbose)
    {
        unsigned seqNum = treeNode->gtSeqNum; // Useful for setting a conditional break in Visual Studio
        compiler->gtDispLIRNode(treeNode, "Generating: ");
    }
#endif // DEBUG

    // Is this a node whose value is already in a register?  LSRA denotes this by
    // setting the GTF_REUSE_REG_VAL flag.
    if (treeNode->IsReuseRegVal())
    {
        genCodeForReuseVal(treeNode);
        return;
    }

    // contained nodes are part of their parents for codegen purposes
    // ex : immediates, most LEAs
    if (treeNode->isContained())
    {
        return;
    }

    switch (treeNode->gtOper)
    {
	case GT_NOP:
	    break;

	case GT_CNS_INT:
	case GT_CNS_DBL:
	    genSetRegToConst(targetReg, targetType, treeNode);
	           genProduceReg(treeNode);
	    break;

	case GT_IND:
	    genCodeForIndir(treeNode->AsIndir());
	    break;

	case GT_STOREIND:
	    genCodeForStoreInd(treeNode->AsStoreInd());
	    break;

	case GT_LEA:
	    genLeaInstruction(treeNode->AsAddrMode());
	    break;

	case GT_CMP:
	case GT_EQ:
	case GT_NE:
	case GT_LT:
	case GT_LE:
	case GT_GE:
	case GT_GT:
	    genConsumeOperands(treeNode->AsOp());
	    genCodeForCompare(treeNode->AsOp());
	    break;

	case GT_JCC:
	    genCodeForJcc(treeNode->AsCC());
            break;

	case GT_CALL:
	    genCall(treeNode->AsCall());
            break;

	case GT_IL_OFFSET:
            // Do nothing; these nodes are simply markers for debug info.
            break;

	case GT_NO_OP:
            instGen(INS_nop);
            break;

	case GT_STORE_LCL_VAR:
            genCodeForStoreLclVar(treeNode->AsLclVar());
            break;

	case GT_STORE_LCL_FLD:
            genCodeForStoreLclFld(treeNode->AsLclFld());
            break;

	case GT_LCL_VAR:
	    genCodeForLclVar(treeNode->AsLclVar());
	    break;

	case GT_LCL_FLD:
	    genCodeForLclFld(treeNode->AsLclFld());
	    break;

	case GT_RETFILT:
	case GT_RETURN:
	    genReturn(treeNode);
	    break;

	case GT_PUTARG_REG:
	    genPutArgReg(treeNode->AsOp());
	    break;

	case GT_PUTARG_STK:
	    genPutArgStk(treeNode->AsPutArgStk());
	    break;

	case GT_PUTARG_SPLIT:
	    genPutArgSplit(treeNode->AsPutArgSplit());
	    break;

	case GT_CAST:
	    genCodeForCast(treeNode->AsOp());
	    break;

        case GT_ADD:
        case GT_SUB:
        case GT_MUL:
            genConsumeOperands(treeNode->AsOp());
            genCodeForBinary(treeNode->AsOp());
            break;

        case GT_MULHI:
            genCodeForMulHi(treeNode->AsOp());
            break;

        case GT_DIV:
        case GT_UDIV:
        case GT_MOD:
        case GT_UMOD:
            genConsumeOperands(treeNode->AsOp());
            genCodeForBinary(treeNode->AsOp());
            break;

        case GT_AND:
        case GT_OR:
        case GT_XOR:
            genConsumeOperands(treeNode->AsOp());
            genCodeForBinary(treeNode->AsOp());
            break;
        
	case GT_LSH:
	case GT_RSH:
	case GT_RSZ:
	case GT_ROL:
	case GT_ROR:
	   	    genConsumeOperands(treeNode->AsOp());
	           genCodeForShift(treeNode);
	   	    break;

	       case GT_NEG:
	       case GT_NOT:
	           genConsumeRegs(treeNode->gtGetOp1());
	           genCodeForNegNot(treeNode);
	           break;

	case GT_STORE_BLK:
	    genCodeForStoreBlk(treeNode->AsBlk());
	    break;
        case GT_INDEX_ADDR:
	    genCodeForIndexAddr(treeNode->AsIndexAddr());
	    break;

        case GT_LCL_ADDR:
	    genCodeForLclAddr(treeNode->AsLclFld());
	    break;
	case GT_COPY:
	    // This is handled at the time we call genConsumeReg() on the GT_COPY
	    break;

	case GT_RELOAD:
	    // do nothing - reload is just a marker.
	    // The parent node will call genConsumeReg on this which will trigger the unspill of this node's child
	    // into the register specified in this node.
	    break;

	case GT_NULLCHECK:
	    genCodeForNullCheck(treeNode->AsIndir());
	    break;

	case GT_CATCH_ARG:

	           noway_assert(handlerGetsXcptnObj(compiler->compCurBB->bbCatchTyp));

	           /* Catch arguments get passed in a register. genCodeForBBlist()
	              would have marked it as holding a GC object, but not used. */

	           noway_assert(gcInfo.gcRegGCrefSetCur & RBM_EXCEPTION_OBJECT);
	           genConsumeReg(treeNode);
	           break;

	case GT_JMPTABLE:
	    genJumpTable(treeNode);
	    break;

	case GT_SWITCH_TABLE:
	    genTableBasedSwitch(treeNode);
	    break;

        case GT_START_PREEMPTGC:
            // We are about to enter preemptive GC mode (native call). Kill all
            // callee-saved GC-ref/byref register liveness and define a temp label
            // so that GC liveness info is propagated to subsequent emitter calls.
            gcInfo.gcMarkRegSetNpt(RBM_INT_CALLEE_SAVED);
            genDefineTempLabel(genCreateTempLabel());
            break;

        case GT_RETURNTRAP:
            genCodeForReturnTrap(treeNode->AsOp());
            break;

        case GT_PINVOKE_PROLOG:
            // Verify that no live GC refs exist in non-argument registers at the
            // PInvoke call boundary. Argument registers holding managed refs are
            // the only permitted survivors at this point.
            noway_assert(((gcInfo.gcRegGCrefSetCur | gcInfo.gcRegByrefSetCur) &
                          ~fullIntArgRegMask(compiler->info.compCallConv)) == 0);
#ifdef PSEUDORANDOM_NOP_INSERTION
            // The runtime requires predictable codegen at PInvoke boundaries for
            // safe-point patching.
            GetEmitter()->emitDisableRandomNops();
#endif // PSEUDORANDOM_NOP_INSERTION
            break;

        case GT_LABEL:
            // Capture the return address of the following native call into targetReg.
            // This is stored into InlinedCallFrame.m_pCallerReturnAddress by the
            // surrounding GT_STORE_LCL_FLD node.
            // emitIns_R_L emits: bcl 20,31,$+4  (get PC into LR)
            //                    mflr targetReg  (move LR to targetReg)
            //                    addi targetReg, targetReg, <label_offset>
            genPendingCallLabel = genCreateTempLabel();
            GetEmitter()->emitIns_R_L(INS_addi, EA_PTRSIZE, genPendingCallLabel, targetReg);
            break;

        case GT_BOUNDS_CHECK:
            genRangeCheck(treeNode);
            break;

        case GT_PHYSREG:
            genCodeForPhysReg(treeNode->AsPhysReg());
            break;

        case GT_LCLHEAP:
            genLclHeap(treeNode);
            break;

        default:
            printf("ERROR: Unhandled tree node operation: %s (oper=%d)\n",
                   GenTree::OpName(treeNode->gtOper), treeNode->gtOper);
            printf("Tree node details: type=%s, flags=0x%x\n",
                   varTypeName(treeNode->TypeGet()), treeNode->gtFlags);
            abort();
    }
}

//------------------------------------------------------------------------
// genCodeForMulHi: Generate code for GT_MULHI — the upper half of a full-width
//                  integer multiply (used by the Lemire FastMod algorithm).
//
// Arguments:
//    treeNode - the GT_MULHI node
//
// Notes:
//    PowerPC64 has dedicated multiply-high instructions:
//      mulhdu rD, rA, rB  – unsigned doubleword: bits [127:64] of rA × rB (64-bit)
//      mulhd  rD, rA, rB  – signed   doubleword: bits [127:64] of rA × rB (64-bit)
//      mulhwu rD, rA, rB  – unsigned word:       bits [63:32]  of rA × rB (32-bit)
//      mulhw  rD, rA, rB  – signed   word:       bits [63:32]  of rA × rB (32-bit)
//
//    IMPORTANT: We must NOT use ppc64UseWideArith() here, because FastMod passes
//    a ulong multiplier (TYP_LONG) and a uint value (TYP_INT).  ppc64UseWideArith
//    would return true (because one operand is TYP_LONG), which would select the
//    doubleword form — correct for 64-bit operands — but only if the 32-bit operand
//    was zero-extended into the register.  If it was sign-extended by an `lwa` load
//    and its MSB is set, mulhdu would read a large negative 64-bit number and
//    produce a wrong result.  We therefore use the node's own type plus the
//    GTF_UNSIGNED flag to pick the instruction, which is exactly what the JIT
//    intends for the operation.
//
void CodeGen::genCodeForMulHi(GenTreeOp* treeNode)
{
    assert(treeNode->OperIs(GT_MULHI));
    assert(!treeNode->gtOverflowEx());

    genConsumeOperands(treeNode);

    regNumber targetReg  = treeNode->GetRegNum();
    var_types targetType = treeNode->TypeGet();
    emitter*  emit       = GetEmitter();
    emitAttr  attr       = emitActualTypeSize(treeNode);
    bool      isUnsigned = (treeNode->gtFlags & GTF_UNSIGNED) != 0;

    GenTree* op1 = treeNode->gtGetOp1();
    GenTree* op2 = treeNode->gtGetOp2();

    assert(!varTypeIsFloating(targetType));
    assert(targetReg != REG_NA);
    assert(!op1->isContained());
    assert(!op2->isContained());

    instruction ins;
    if (EA_SIZE(attr) == EA_8BYTE)
    {
        // mulhdu / mulhd: high 64 bits of a 128-bit product
        ins = isUnsigned ? INS_mulhdu : INS_mulhd;
    }
    else
    {
        // mulhwu / mulhw: high 32 bits of a 64-bit product
        assert(EA_SIZE(attr) == EA_4BYTE);
        ins = isUnsigned ? INS_mulhwu : INS_mulhw;
    }

    emit->emitIns_R_R_R(ins, attr, targetReg, op1->GetRegNum(), op2->GetRegNum());

    genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// ppc64UseWideArith: Decide whether to emit 64-bit (doubleword) arithmetic
//                    instructions instead of 32-bit (word) ones.
//
// Arguments:
//    op1 - first operand tree node
//    op2 - second operand tree node
//
// Return Value:
//    true  – at least one operand carries a 64-bit type (TYP_LONG or TYP_I_IMPL);
//             use mulld / divd / divdu etc.
//    false – both operands are 32-bit (TYP_INT or smaller);
//             use mullw / divw / divwu etc.
//
// Notes:
//    Instruction selection must be based on the *operand* widths, not the
//    result-node type.  The JIT routinely produces GT_MUL/GT_DIV nodes typed
//    TYP_LONG when both operands are TYP_INT (e.g. pointer-stride arithmetic
//    widens int×int to long so the result fits a 64-bit add).
//
//    On PPC64, the doubleword forms (mulld, divd, …) read the full 64-bit
//    register.  After an `lwa` sign-extend of a 32-bit value whose MSB is set
//    (e.g. 0x80000001 → 0xFFFFFFFF_80000001), mulld treats the value as a
//    large negative 64-bit number and produces a product that wraps into an
//    unmapped address — causing the segfault observed in
//    String.GetNonRandomizedHashCode on PPC64LE.
//
//    The word forms (mullw, divw, …) read only bits [31:0] of each operand,
//    which is always correct for TYP_INT values regardless of what the upper
//    32 bits contain.  Their 64-bit sign-extended result is safe for
//    subsequent 64-bit pointer-arithmetic.
//
static bool ppc64UseWideArith(GenTree* op1, GenTree* op2)
{
    var_types t1 = genActualType(op1->TypeGet());
    var_types t2 = genActualType(op2->TypeGet());
    return (t1 == TYP_LONG) || (t1 == TYP_I_IMPL) || (t2 == TYP_LONG) || (t2 == TYP_I_IMPL);
}

//------------------------------------------------------------------------
// genCodeForBinary: Generate code for many binary arithmetic operators
//
// Arguments:
//    treeNode - tree node
//
// Notes:
//    This method is expected to have called genConsumeOperands() before calling it.
//    Handles GT_ADD, GT_SUB, GT_MUL, GT_DIV, GT_UDIV, GT_MOD, GT_UMOD
//    for both integer and floating-point types.
//
void CodeGen::genCodeForBinary(GenTreeOp* treeNode)
{
          const genTreeOps oper       = treeNode->OperGet();
          regNumber        targetReg  = treeNode->GetRegNum();
          var_types        targetType = treeNode->TypeGet();
          emitter*         emit       = GetEmitter();

          assert(oper == GT_ADD || oper == GT_SUB || oper == GT_MUL ||
                 oper == GT_DIV || oper == GT_UDIV || oper == GT_MOD || oper == GT_UMOD ||
                 oper == GT_AND || oper == GT_OR || oper == GT_XOR);

          GenTree* op1 = treeNode->gtGetOp1();
	  GenTree* op2 = treeNode->gtGetOp2();

	  regNumber op1reg = op1->GetRegNum();
	  regNumber op2reg = op2->GetRegNum();

          instruction ins;
          emitAttr    attr = emitActualTypeSize(treeNode);


          // PowerPC64LE instruction selection based on operation and type
          if (varTypeIsFloating(targetType))
          {
              // Floating-point operations
              switch (oper)
              {
                  case GT_ADD:
                      ins = (attr == EA_4BYTE) ? INS_fadds : INS_fadd;
                      break;
                  case GT_SUB:
                      ins = (attr == EA_4BYTE) ? INS_fsubs : INS_fsub;
                      break;
                  case GT_MUL:
                      ins = (attr == EA_4BYTE) ? INS_fmuls : INS_fmul;
                      break;
                  case GT_DIV:
                      ins = (attr == EA_4BYTE) ? INS_fdivs : INS_fdiv;
                      break;
                  default:
                      unreached();
              }

              // Emit floating-point instruction: ins targetReg, op1reg, op2reg
              emit->emitIns_R_R_R(ins, attr, targetReg, op1reg, op2reg);
          }
          else
          {
              // Integer operations
              bool isImmediate = op2->isContainedIntOrIImmed();

              switch (oper)
              {
                  case GT_ADD:
                      if (isImmediate)
                      {
                          ssize_t imm = op2->AsIntConCommon()->IconValue();
                          // addi: add immediate (16-bit signed immediate)
                          ins = INS_addi;
                          emit->emitIns_R_R_I(ins, attr, targetReg, op1reg, imm);
                      }
                      else
                      {
                          // add: register add
                          ins = INS_add;
                          emit->emitIns_R_R_R(ins, attr, targetReg, op1reg, op2reg);
                      }
                      break;

                  case GT_SUB:
                      // PowerPC64LE doesn't have subi, use addi with negated immediate
                      // or use subf (subtract from) instruction
                      ins = INS_subf;
                      emit->emitIns_R_R_R(ins, attr, targetReg, op2reg, op1reg); // Note: operands reversed for subf
                      break;

                  case GT_MUL:
                      // Use mulld (64-bit) when either operand is 64-bit, mullw (32-bit) otherwise.
                      // See ppc64UseWideArith for the full rationale.
                      ins = ppc64UseWideArith(op1, op2) ? INS_mulld : INS_mullw;
                      emit->emitIns_R_R_R(ins, attr, targetReg, op1reg, op2reg);
                      break;

                  case GT_DIV:
                      // Use divd (64-bit) when either operand is 64-bit, divw (32-bit) otherwise.
                      ins = ppc64UseWideArith(op1, op2) ? INS_divd : INS_divw;
                      emit->emitIns_R_R_R(ins, attr, targetReg, op1reg, op2reg);
                      break;

                  case GT_UDIV:
                      // Use divdu (64-bit) when either operand is 64-bit, divwu (32-bit) otherwise.
                      ins = ppc64UseWideArith(op1, op2) ? INS_divdu : INS_divwu;
                      emit->emitIns_R_R_R(ins, attr, targetReg, op1reg, op2reg);
                      break;

                  case GT_MOD:
                  case GT_UMOD:
    {
   // Compute: remainder = dividend - (quotient * divisor)
   // Algorithm:
   //   1. quotient = dividend / divisor  (divd/divw or divdu/divwu)
   //   2. temp = quotient * divisor      (mulld/mullw)
   //   3. remainder = dividend - temp    (subf)

   // Need a temporary register for the quotient
   regNumber tempReg = internalRegisters.GetSingle(treeNode);

   // Use wide (64-bit) instructions when either operand is 64-bit.
   bool wide = ppc64UseWideArith(op1, op2);

   // Step 1: Compute quotient in tempReg
   instruction divIns;
   if (oper == GT_MOD)
   {
       divIns = wide ? INS_divd : INS_divw;   // signed division
   }
   else // GT_UMOD
   {
       divIns = wide ? INS_divdu : INS_divwu;  // unsigned division
   }
   emit->emitIns_R_R_R(divIns, attr, tempReg, op1reg, op2reg);

   // Step 2: Multiply quotient by divisor, result in tempReg
   emit->emitIns_R_R_R(wide ? INS_mulld : INS_mullw, attr, tempReg, tempReg, op2reg);

   // Step 3: Subtract to get remainder: targetReg = op1reg - tempReg
   // subf rD, rA, rB computes rD = rB - rA, so we use subf targetReg, tempReg, op1reg
   emit->emitIns_R_R_R(INS_subf, attr, targetReg, tempReg, op1reg);
      }
      break;

                 case GT_AND:
                    // and: bitwise AND
                    if (isImmediate)
                    {
                        ssize_t imm = op2->AsIntConCommon()->IconValue();
                        ins = INS_andi;
                        emit->emitIns_R_R_I(ins, attr, targetReg, op1reg, imm);
                    }
                    else
                    {
                        ins = INS_and_ins;
                        emit->emitIns_R_R_R(ins, attr, targetReg, op1reg, op2reg);
                    }
                    break;


                case GT_OR:
                    // or: bitwise OR
                    if (isImmediate)
                    {
                        ssize_t imm = op2->AsIntConCommon()->IconValue();
                        ins = INS_ori;
                        emit->emitIns_R_R_I(ins, attr, targetReg, op1reg, imm);
                    }
                    else
                    {
                        ins = INS_or_ins;
                        emit->emitIns_R_R_R(ins, attr, targetReg, op1reg, op2reg);
                    }
                    break;


		case GT_XOR:
                    // xor: bitwise XOR
                    if (isImmediate)
                    {
                        ssize_t imm = op2->AsIntConCommon()->IconValue();
                        ins = INS_xori;
                        emit->emitIns_R_R_I(ins, attr, targetReg, op1reg, imm);
                    }
                    else
                    {
                        ins = INS_xor_ins;
                        emit->emitIns_R_R_R(ins, attr, targetReg, op1reg, op2reg);
                    }
                    break;

		default:
                      unreached();
              }
          }

          genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// genCodeForNegNot: Generate code for GT_NEG and GT_NOT.
//
// Arguments:
//    tree - the GT_NEG or GT_NOT node
//
// GT_NEG on integer types:  neg  rD, rA
// GT_NEG on float types:    fneg fD, fB
// GT_NOT on integer types:  nor  rA, rS, rS  (bitwise NOT: ~A = A NOR A)
//
void CodeGen::genCodeForNegNot(GenTree* tree)
{
    assert(tree->OperIs(GT_NEG, GT_NOT));

    regNumber targetReg  = tree->GetRegNum();
    GenTree*  op1        = tree->gtGetOp1();
    regNumber op1reg     = op1->GetRegNum(); // already consumed by caller
    emitAttr  attr       = emitActualTypeSize(tree);

    assert(!tree->isContained());
    assert(targetReg != REG_NA);

    if (tree->OperIs(GT_NEG))
    {
        if (varTypeIsFloating(tree->TypeGet()))
        {
            // fneg fD, fB — flip the sign bit of a floating-point value
            assert(emitter::isFloatReg(targetReg));
            assert(emitter::isFloatReg(op1reg));
            GetEmitter()->emitIns_R_R(INS_fneg, attr, targetReg, op1reg);
        }
        else
        {
            // neg rD, rA — two's-complement negation
            GetEmitter()->emitIns_R_R(INS_neg, attr, targetReg, op1reg);
        }
    }
    else
    {
        // GT_NOT: bitwise complement — PowerPC has no single NOT instruction;
        // implement as NOR rA, rS, rS  (A NOR A == ~A)
        GetEmitter()->emitIns_R_R_R(INS_nor, attr, targetReg, op1reg, op1reg);
    }

    genProduceReg(tree);
}



//---------------------------------------------------------------------
// genSetGSSecurityCookie: Set the "GS" security cookie in the prolog.
//
// Arguments:
//     initReg        - register to use as a scratch register
//     pInitRegZeroed - OUT parameter. *pInitRegZeroed is set to 'false' if and only if
//                      this call sets 'initReg' to a non-zero value.
//
// Return Value:
//     None
//
void CodeGen::genSetGSSecurityCookie(regNumber initReg, bool* pInitRegZeroed)
{
    assert(compiler->compGeneratingProlog);

    if (!compiler->getNeedsGSSecurityCookie())
    {
        return;
    }

    if (compiler->opts.IsOSR() && compiler->info.compPatchpointInfo->HasSecurityCookie())
    {
        // Cookie was already initialised on the original frame.
        return;
    }

    emitter* emit = GetEmitter();

    if (compiler->gsGlobalSecurityCookieAddr == nullptr)
    {
        // Compile-time constant: load the value and store it to the stack slot.
        noway_assert(compiler->gsGlobalSecurityCookieVal != 0);
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, initReg, compiler->gsGlobalSecurityCookieVal);
        emit->emitIns_S_R(INS_std, EA_PTRSIZE, initReg, compiler->lvaGSSecurityCookie, 0);
    }
    else
    {
        // NGen / R2R: the cookie lives at a runtime address; load the pointer,
        // dereference it, then store the value to the stack slot.
        instGen_Set_Reg_To_Imm(EA_HANDLE_CNS_RELOC, initReg,
                               (ssize_t)compiler->gsGlobalSecurityCookieAddr);
        emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, initReg, initReg, 0);
        emit->emitIns_S_R(INS_std, EA_PTRSIZE, initReg, compiler->lvaGSSecurityCookie, 0);
    }

    *pInitRegZeroed = false;
}

//------------------------------------------------------------------------
// genEmitGSCookieCheck: Generate code to check that the GS cookie
// wasn't thrashed by a buffer overrun.
//
void CodeGen::genEmitGSCookieCheck(bool pushReg)
{
    noway_assert(compiler->gsGlobalSecurityCookieAddr || compiler->gsGlobalSecurityCookieVal);

    assert(GetEmitter()->emitGCDisabled());

    // We need two temporary registers to load the GS cookie values and compare them.
    // We can't use any argument registers if 'pushReg' is true (meaning we have a JMP
    // call). They should be callee-trash registers with nothing interesting at this point.
    // LSRA has no IR node for this check so it cannot allocate registers for us.
    regNumber regGSConst = REG_GSCOOKIE_TMP_0; // R12 — call-target scratch, not an arg reg, not VSD cell
    regNumber regGSValue = REG_GSCOOKIE_TMP_1; // R11 — frame-loaded value only, dead after cmpd

    if (compiler->gsGlobalSecurityCookieAddr == nullptr)
    {
        // Compile-time constant cookie: load the value directly.
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, regGSConst, compiler->gsGlobalSecurityCookieVal);
    }
    else
    {
        // NGen / R2R case: cookie lives at a runtime address; load the pointer then
        // dereference it.
        instGen_Set_Reg_To_Imm(EA_HANDLE_CNS_RELOC, regGSConst,
                               (ssize_t)compiler->gsGlobalSecurityCookieAddr);
        GetEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, regGSConst, regGSConst, 0);
        regSet.verifyRegUsed(regGSConst);
    }

    // Load this method's saved GS cookie from the stack frame.
    GetEmitter()->emitIns_R_S(INS_ld, EA_PTRSIZE, regGSValue, compiler->lvaGSSecurityCookie, 0);

    // Compare: if equal the cookie is intact, jump past the failure call.
    BasicBlock* gsCheckBlk = genCreateTempLabel();
    GetEmitter()->emitIns_R_R(INS_cmpd, EA_PTRSIZE, regGSConst, regGSValue);
    inst_JMP(EJ_eq, gsCheckBlk);

    // Cookie mismatch — call the fast-fail helper.  regGSConst is free to use
    // as the call target scratch register.
    genEmitHelperCall(CORINFO_HELP_FAIL_FAST, 0, EA_UNKNOWN, regGSConst);

    genDefineTempLabel(gsCheckBlk);
}

//---------------------------------------------------------------------
// genIntrinsic - generate code for a given intrinsic
//
// Arguments
//    treeNode - the GT_INTRINSIC node
//
// Return value:
//    None
//
void CodeGen::genIntrinsic(GenTreeIntrinsic* treeNode)
{
    //_ASSERTE("!NYI");
    abort();
}

//---------------------------------------------------------------------
// genPutArgStk - generate code for a GT_PUTARG_STK node
//
// Arguments
//    treeNode - the GT_PUTARG_STK node
//
// Return value:
//    None
//
void CodeGen::genPutArgStk(GenTreePutArgStk* treeNode)
{
    assert(treeNode->OperIs(GT_PUTARG_STK));
    emitter* emit = GetEmitter();

    // This is the varNum for our store operations,
    // typically this is the varNum for the Outgoing arg space
    // When we are generating a tail call it will be the varNum for arg0
    unsigned varNumOut    = (unsigned)-1;
    unsigned argOffsetMax = (unsigned)-1; // Records the maximum size of this area for assert checks

    // Get argument offset to use with 'varNumOut'
    // Here we cross check that argument offset hasn't changed from lowering to codegen since
    // we are storing arg slot number in GT_PUTARG_STK node in lowering phase.
    unsigned argOffsetOut = treeNode->getArgOffset();

    // Whether to setup stk arg in incoming or out-going arg area?
    // Fast tail calls implemented as epilog+jmp = stk arg is setup in incoming arg area.
    // All other calls - stk arg is setup in out-going arg area.
    if (treeNode->putInIncomingArgArea())
    {
        varNumOut    = getFirstArgWithStackSlot();
        argOffsetMax = compiler->compArgSize;
#if FEATURE_FASTTAILCALL
        // This must be a fast tail call.
        assert(treeNode->gtCall->IsFastTailCall());

        // Since it is a fast tail call, the existence of first incoming arg is guaranteed
        // because fast tail call requires that in-coming arg area of caller is >= out-going
        // arg area required for tail call.
        LclVarDsc* varDsc = compiler->lvaGetDesc(varNumOut);
        assert(varDsc != nullptr);
#endif // FEATURE_FASTTAILCALL
    }
    else
    {
        varNumOut    = compiler->lvaOutgoingArgSpaceVar;
        argOffsetMax = compiler->lvaOutgoingArgSpaceSize;
    }

    GenTree* source = treeNode->gtGetOp1();

    JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk - source: oper=%s, type=%s, TypeIs(TYP_STRUCT)=%d, isContained=%d\n",
           GenTree::OpName(source->OperGet()), varTypeName(source->TypeGet()),
           source->TypeIs(TYP_STRUCT), source->isContained());

    if (!source->TypeIs(TYP_STRUCT)) // a normal non-Struct argument
    {
        JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk - Taking non-struct path\n");
        if (varTypeIsSIMD(source->TypeGet()))
        {
            // SIMD types not yet supported for PPC64LE
            NYI_POWERPC64("genPutArgStk - SIMD types");
        }

        var_types slotType = genActualType(source);

        instruction storeIns  = ins_Store(slotType);
        emitAttr    storeAttr = emitTypeSize(slotType);

        // If it is contained then source must be the integer constant zero
        if (source->isContained())
        {
            assert(source->OperGet() == GT_CNS_INT);
            assert(source->AsIntConCommon()->IconValue() == 0);

            // Use r0 (which is always zero in PPC64LE when used as source)
            // Actually, we need to load 0 into a register first
            regNumber zeroReg = REG_R0;
            emit->emitIns_R_I(INS_li, EA_PTRSIZE, zeroReg, 0);
            emit->emitIns_S_R(storeIns, storeAttr, zeroReg, varNumOut, argOffsetOut);
        }
        else
        {
            genConsumeReg(source);
            regNumber srcReg = source->GetRegNum();
            
            // For HFA struct fields, the register may be a float register even though slotType is TYP_LONG
            // Override the instruction if we have a float register
            if (genIsValidFloatReg(srcReg))
            {
                if (storeAttr == EA_8BYTE)
                {
                    storeIns = INS_stfd;  // Store double (8 bytes)
                }
                else if (storeAttr == EA_4BYTE)
                {
                    storeIns = INS_stfs;  // Store single (4 bytes)
                }
                JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk (non-struct) - Float register detected, overriding instruction to %s for %s (slotType=%s, attr=%d)\n",
                       genInsName(storeIns), getRegName(srcReg), varTypeName(slotType), (int)storeAttr);
            }
            
            emit->emitIns_S_R(storeIns, storeAttr, srcReg, varNumOut, argOffsetOut);
        }
        argOffsetOut += EA_SIZE_IN_BYTES(storeAttr);
        assert(argOffsetOut <= argOffsetMax); // We can't write beyond the outgoing arg area
    }
    else // We have some kind of a struct argument
    {
        JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk - Taking struct path\n");
        assert(source->isContained()); // We expect that this node was marked as contained in Lower

        if (source->OperGet() == GT_FIELD_LIST)
        {
            JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk - Calling genPutArgStkFieldList\n");
            genPutArgStkFieldList(treeNode, varNumOut);
        }
        else
        {
            JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk - Handling local read or BLK\n");
            noway_assert(source->OperIsLocalRead() || source->OperIs(GT_BLK));

            var_types targetType = source->TypeGet();
            noway_assert(varTypeIsStruct(targetType));

            // We will copy this struct to the stack, possibly using a ld/std instruction
            // Setup loReg from the internal registers that we reserved in lower.
            //
            regNumber loReg = internalRegisters.Extract(treeNode);

            GenTreeLclVarCommon* srcLclNode = nullptr;
            regNumber            addrReg    = REG_NA;
            ClassLayout*         layout     = nullptr;

            // Setup "layout", "srcLclNode" and "addrReg".
            if (source->OperIsLocalRead())
            {
                srcLclNode        = source->AsLclVarCommon();
                layout            = srcLclNode->GetLayout(compiler);
                LclVarDsc* varDsc = compiler->lvaGetDesc(srcLclNode);

                // This struct must live on the stack frame.
                assert(varDsc->lvOnFrame && !varDsc->lvRegister);
            }
            else // we must have a GT_BLK
            {
                layout  = source->AsBlk()->GetLayout();
                addrReg = genConsumeReg(source->AsBlk()->Addr());
            }

            unsigned srcSize = layout->GetSize();

            // HFA structs cannot contain GC pointers.
            // Non-HFA structs of any size may be passed by value on the stack per PPC64LE ELFv2 ABI.
            if (compiler->IsHfa(layout->GetClassHandle()))
            {
                noway_assert(!layout->HasGCPtr());
            }

            unsigned dstSize = treeNode->GetStackByteSize();

            JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk - dstSize=%u, srcSize=%u\n", dstSize, srcSize);

            // PPC64LE: If dstSize is 0, this struct is passed entirely in registers
            // and should not be processed by genPutArgStk. This can happen for HFAs.
            if (dstSize == 0)
            {
                JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk - dstSize is 0, returning early\n");
                // This struct is passed entirely in registers via GT_FIELD_LIST
                // Nothing to do here - the individual fields will be handled by genPutArgReg
                return;
            }
            
            JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk - dstSize is not 0, continuing\n");

            // We can generate smaller code if store size is a multiple of TARGET_POINTER_SIZE.
            // The dst size can be rounded up to PUTARG_STK size. The src size can be rounded up
            // if it reads a local variable because reading "too much" from a local cannot fault.
            //
            if ((dstSize != srcSize) && (srcLclNode != nullptr))
            {
                unsigned widenedSrcSize = roundUp(srcSize, TARGET_POINTER_SIZE);
                if (widenedSrcSize <= dstSize)
                {
                    srcSize = widenedSrcSize;
                }
            }

            assert(srcSize <= dstSize);

            int      remainingSize = srcSize;
            unsigned structOffset  = 0;
            unsigned lclOffset     = (srcLclNode != nullptr) ? srcLclNode->GetLclOffs() : 0;
            unsigned nextIndex     = 0;

            // For PPC64LE, we will generate a ld and std instruction each loop
            //             ld      r2, 0(r3)
            //             std     r2, offset(r1)
            // For HFA structs, we use lfd/stfd or lfs/stfs instead
            
            JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk - About to check HFA, srcSize=%u, dstSize=%u, remainingSize=%d\n",
                   srcSize, dstSize, remainingSize);
            
            // Check if this is an HFA struct
            // Note: We check the struct type, not the register type, because for stack-only
            // HFA structs, loReg might be an integer register used as a temporary
            bool isHfa = false;
            var_types hfaType = TYP_UNDEF;
            unsigned hfaSlots = 0;
            
            // Try to get class handle - first from lvClassHnd (if source is local), then from layout
            CORINFO_CLASS_HANDLE structHnd = NO_CLASS_HANDLE;
            if (srcLclNode != nullptr)
            {
                LclVarDsc* varDsc = compiler->lvaGetDesc(srcLclNode->GetLclNum());
                structHnd = varDsc->lvClassHnd;
            }
            if (structHnd == NO_CLASS_HANDLE)
            {
                structHnd = layout->GetClassHandle();
            }
            
            if (structHnd != NO_CLASS_HANDLE &&
                IsPpc64leHfaLikeStruct(compiler, structHnd, &hfaType, &hfaSlots))
            {
                isHfa = true;
                JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk: Detected HFA struct, loReg=%s, hfaType=%s, hfaSlots=%u, structHnd=%p\n",
                       getRegName(loReg), varTypeName(hfaType), hfaSlots, structHnd);
            }
            else
            {
                JITDUMP("[PPC64LE HFA DEBUG] genPutArgStk: NOT HFA - structHnd=%p, srcLclNode=%p\n",
                       structHnd, srcLclNode);
            }
            
            // For HFA structs on stack, process field by field (floats are 4 bytes, doubles are 8 bytes)
            // This is different from register passing where each field consumes 8 bytes (GPR slot)
            if (isHfa)
            {
                unsigned fieldSize = (hfaType == TYP_FLOAT) ? 4 : 8;
                instruction loadIns = (hfaType == TYP_FLOAT) ? INS_lfs : INS_lfd;
                instruction storeIns = (hfaType == TYP_FLOAT) ? INS_stfs : INS_stfd;
                emitAttr attr = (hfaType == TYP_FLOAT) ? EA_4BYTE : EA_8BYTE;
                
                while (remainingSize >= fieldSize)
                {
                    if (srcLclNode != nullptr)
                    {
                        // Load from our local source
                        emit->emitIns_R_S(loadIns, attr, loReg, srcLclNode->GetLclNum(),
                                          lclOffset + structOffset);
                    }
                    else
                    {
                        // check for case of destroying the addrRegister while we still need it
                        assert(loReg != addrReg || remainingSize == fieldSize);

                        // Load from our address expression source
                        emit->emitIns_R_R_I(loadIns, attr, loReg, addrReg, structOffset);
                    }

                    // Emit store instruction to store the field into the outgoing argument area
                    // On stack, HFA fields are packed at their natural size (4 bytes for float, 8 for double)
                    emit->emitIns_S_R(storeIns, attr, loReg, varNumOut, argOffsetOut);
                    argOffsetOut += fieldSize;  // Advance by actual field size on stack
                    assert(argOffsetOut <= argOffsetMax);

                    remainingSize -= fieldSize;
                    structOffset += fieldSize;
                    nextIndex++;
                }
            }
            else
            {
                // Non-HFA structs: process in 8-byte chunks
                while (remainingSize >= TARGET_POINTER_SIZE)
                {
                    var_types type = layout->GetGCPtrType(nextIndex);
                    instruction loadIns = INS_ld;
                    instruction storeIns = INS_std;
                    emitAttr attr = emitTypeSize(type);

                    if (srcLclNode != nullptr)
                    {
                        // Load from our local source
                        emit->emitIns_R_S(loadIns, attr, loReg, srcLclNode->GetLclNum(),
                                          lclOffset + structOffset);
                    }
                    else
                    {
                        // check for case of destroying the addrRegister while we still need it
                        assert(loReg != addrReg || remainingSize == TARGET_POINTER_SIZE);

                        // Load from our address expression source
                        emit->emitIns_R_R_I(loadIns, attr, loReg, addrReg, structOffset);
                    }

                    // Emit store instruction to store the register into the outgoing argument area
                    emit->emitIns_S_R(storeIns, attr, loReg, varNumOut, argOffsetOut);
                    argOffsetOut += TARGET_POINTER_SIZE;  // We stored 8-bytes of the struct
                    assert(argOffsetOut <= argOffsetMax); // We can't write beyond the outgoing arg area

                    remainingSize -= TARGET_POINTER_SIZE; // We loaded 8-bytes of the struct
                    structOffset += TARGET_POINTER_SIZE;
                    nextIndex++;
                }
            }

            // Handle any remaining bytes (less than 8 bytes)
            while (remainingSize > 0)
            {
                var_types type;
                instruction loadIns;
                instruction storeIns;
                unsigned moveSize;

                if (remainingSize >= 4)
                {
                    moveSize = 4;
                    type = layout->GetGCPtrType(nextIndex);
                    loadIns = INS_lwz;
                    storeIns = INS_stw;
                }
                else if (remainingSize >= 2)
                {
                    moveSize = 2;
                    type = TYP_USHORT;
                    loadIns = INS_lhz;
                    storeIns = INS_sth;
                }
                else
                {
                    moveSize = 1;
                    type = TYP_UBYTE;
                    loadIns = INS_lbz;
                    storeIns = INS_stb;
                }

                emitAttr attr = emitTypeSize(type);

                if (srcLclNode != nullptr)
                {
                    // Load from our local source
                    emit->emitIns_R_S(loadIns, attr, loReg, srcLclNode->GetLclNum(), lclOffset + structOffset);
                }
                else
                {
                    assert(loReg != addrReg);
                    // Load from our address expression source
                    emit->emitIns_R_R_I(loadIns, attr, loReg, addrReg, structOffset);
                }

                // Emit a store instruction to store the register into the outgoing argument area
                emit->emitIns_S_R(storeIns, attr, loReg, varNumOut, argOffsetOut);
                argOffsetOut += moveSize;
                assert(argOffsetOut <= argOffsetMax); // We can't write beyond the outgoing arg area

                structOffset += moveSize;
                remainingSize -= moveSize;
            }
        }
    }
}

//---------------------------------------------------------------------
// genPutArgReg - generate code for a GT_PUTARG_REG node
//
// Arguments
//    tree - the GT_PUTARG_REG node
//
// Return value:
//    None
//
void CodeGen::genPutArgReg(GenTreeOp* tree)
{
    assert(tree->OperIs(GT_PUTARG_REG));

    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->GetRegNum();

    assert(targetType != TYP_STRUCT);

    GenTree* op1 = tree->gtOp1;
    genConsumeReg(op1);

    // For HFA struct fields, the tree type may be TYP_LONG but registers are float registers
    // Override the type based on the actual register type to use correct move instruction
    var_types moveType = targetType;
    if (genIsValidFloatReg(targetReg) && genIsValidFloatReg(op1->GetRegNum()))
    {
        // Both registers are float registers - determine type from size
        if (targetType == TYP_LONG || emitActualTypeSize(targetType) == EA_8BYTE)
        {
            moveType = TYP_DOUBLE;
        }
        else if (emitActualTypeSize(targetType) == EA_4BYTE)
        {
            moveType = TYP_FLOAT;
        }
        JITDUMP("[PPC64LE HFA DEBUG] genPutArgReg - Float registers detected, overriding type from %s to %s for %s -> %s\n",
               varTypeName(targetType), varTypeName(moveType), getRegName(op1->GetRegNum()), getRegName(targetReg));
    }

    // If child node is not already in the register we need, move it
    inst_Mov(moveType, targetReg, op1->GetRegNum(), /* canSkip */ true);

    genProduceReg(tree);
}

//---------------------------------------------------------------------
// genPutArgSplit - generate code for a GT_PUTARG_SPLIT node
//
// Arguments
//    tree - the GT_PUTARG_SPLIT node
//
// Return value:
//    None
//
void CodeGen::genPutArgSplit(GenTreePutArgSplit* treeNode)
{
    assert(treeNode->OperIs(GT_PUTARG_SPLIT));

    GenTree* source       = treeNode->gtOp1;
    emitter* emit         = GetEmitter();
    unsigned varNumOut    = compiler->lvaOutgoingArgSpaceVar;
    unsigned argOffsetMax = compiler->lvaOutgoingArgSpaceSize;

    if (source->OperGet() == GT_FIELD_LIST)
    {
        // Evaluate each of the GT_FIELD_LIST items into their register
        // and store their register into the outgoing argument area
        unsigned regIndex         = 0;
        unsigned firstOnStackOffs = UINT_MAX;

        for (GenTreeFieldList::Use& use : source->AsFieldList()->Uses())
        {
            GenTree*  nextArgNode = use.GetNode();
            regNumber fieldReg    = nextArgNode->GetRegNum();
            genConsumeReg(nextArgNode);

            if (regIndex >= treeNode->gtNumRegs)
            {
                if (firstOnStackOffs == UINT_MAX)
                {
                    firstOnStackOffs = use.GetOffset();
                }

                var_types type   = use.GetType();
                unsigned  offset = treeNode->getArgOffset() + use.GetOffset() - firstOnStackOffs;
                // We can't write beyond the outgoing arg area
                assert((offset + genTypeSize(type)) <= argOffsetMax);

                // Emit store instructions to store the registers produced by the GT_FIELD_LIST into the outgoing
                // argument area
                emit->emitIns_S_R(ins_Store(type), emitTypeSize(type), fieldReg, varNumOut, offset);
            }
            else
            {
                var_types type   = treeNode->GetRegType(regIndex);
                regNumber argReg = treeNode->GetRegNumByIdx(regIndex);

                // If child node is not already in the register we need, move it
                inst_Mov(type, argReg, fieldReg, /* canSkip */ true);

                regIndex++;
            }
        }
    }
    else
    {
        var_types targetType = source->TypeGet();
        
        // For HFA structs, the source might not be contained and the type might not be TYP_STRUCT
        // (it could be TYP_LONG for an 8-byte field). Check if this is an HFA struct.
        bool isHfaStruct = false;
        if (source->OperIsLocalRead())
        {
            LclVarDsc* varDsc = compiler->lvaGetDesc(source->AsLclVarCommon()->GetLclNum());
            CORINFO_CLASS_HANDLE classHnd = varDsc->lvClassHnd;
            if (classHnd == NO_CLASS_HANDLE && varDsc->GetLayout() != nullptr)
            {
                classHnd = varDsc->GetLayout()->GetClassHandle();
            }
            
            if (classHnd != NO_CLASS_HANDLE)
            {
                var_types hfaType;
                unsigned hfaSlots;
                isHfaStruct = IsPpc64leHfaLikeStruct(compiler, classHnd, &hfaType, &hfaSlots);
                if (isHfaStruct)
                {
                    JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - HFA struct detected (type=%s, contained=%d)\n",
                           varTypeName(targetType), source->isContained());
                }
            }
        }
        
        // For regular structs, source must be contained and type must be struct
        // For HFA structs, we relax these requirements
        assert((source->isContained() && varTypeIsStruct(targetType)) || isHfaStruct);

        // We need a register to store intermediate values that we are loading
        // from the source into. We can usually use one of the target registers
        // that will be overridden anyway. The exception is when the source is
        // in a register and that register is the unique target register we are
        // placing. LSRA will always allocate an internal register when there
        // is just one target register to handle this situation.
        //
        int          firstRegToPlace;
        regNumber    valueReg     = REG_NA;
        unsigned     srcLclNum    = BAD_VAR_NUM;
        unsigned     srcLclOffset = 0;
        regNumber    addrReg      = REG_NA;
        var_types    addrType     = TYP_UNDEF;
        ClassLayout* layout       = nullptr;

        if (source->OperIsLocalRead())
        {
            srcLclNum         = source->AsLclVarCommon()->GetLclNum();
            srcLclOffset      = source->AsLclVarCommon()->GetLclOffs();
            LclVarDsc* varDsc = compiler->lvaGetDesc(srcLclNum);

            // For HFA structs, we need custom handling because:
            // 1. The allocated registers are float registers
            // 2. Normal struct handling uses integer load/store instructions
            // 3. Can't use integer instructions with float registers
            if (isHfaStruct)
            {
                JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - HFA struct, using custom float load/store\n");
                
                // Get HFA element type and size
                layout = varDsc->GetLayout();
                CORINFO_CLASS_HANDLE classHnd = layout->GetClassHandle();
                
                var_types hfaType;
                unsigned hfaSlots;
                IsPpc64leHfaLikeStruct(compiler, classHnd, &hfaType, &hfaSlots);
                
                unsigned fieldSize = (hfaType == TYP_FLOAT) ? 4 : 8;
                instruction loadIns = (hfaType == TYP_FLOAT) ? INS_lfs : INS_lfd;
                instruction storeIns = (hfaType == TYP_FLOAT) ? INS_stfs : INS_stfd;
                
                // Load fields into registers
                for (unsigned i = 0; i < treeNode->gtNumRegs; i++)
                {
                    regNumber targetReg = treeNode->GetRegNumByIdx(i);
                    unsigned fieldOffset = srcLclOffset + (i * fieldSize);
                    
                    GetEmitter()->emitIns_R_S(loadIns, emitActualTypeSize(hfaType), targetReg, srcLclNum, fieldOffset);
                    
                    JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - Loaded HFA field %d from V%02u+%u to %s\n",
                           i, srcLclNum, fieldOffset, getRegName(targetReg));
                }
                
                // Store remaining fields to stack
                unsigned stackFields = hfaSlots - treeNode->gtNumRegs;
                if (stackFields > 0)
                {
                    unsigned argOffsetOut = treeNode->getArgOffset();
                    
                    // Get internal temp register allocated by LSRA for stack fields
                    // Don't use target registers as they're needed for the call
                    regNumber tempReg = REG_NA;
                    if (internalRegisters.Count(treeNode, RBM_ALLFLOAT) > 0)
                    {
                        tempReg = internalRegisters.GetSingle(treeNode, RBM_ALLFLOAT);
                        JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - Using internal register %s for stack fields\n",
                               getRegName(tempReg));
                    }
                    else
                    {
                        // Fallback: use a volatile float register that's not in use
                        // f0 is volatile and not used for arguments in this context
                        tempReg = REG_F0;
                        JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - No internal register, using fallback %s\n",
                               getRegName(tempReg));
                    }
                    
                    for (unsigned i = 0; i < stackFields; i++)
                    {
                        unsigned fieldOffset = srcLclOffset + ((treeNode->gtNumRegs + i) * fieldSize);
                        unsigned stackOffset = argOffsetOut + (i * fieldSize);
                        
                        GetEmitter()->emitIns_R_S(loadIns, emitActualTypeSize(hfaType), tempReg, srcLclNum, fieldOffset);
                        GetEmitter()->emitIns_S_R(storeIns, emitActualTypeSize(hfaType), tempReg, varNumOut, stackOffset);
                        
                        JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - Stored HFA field %d from V%02u+%u to stack+%u using %s\n",
                               treeNode->gtNumRegs + i, srcLclNum, fieldOffset, stackOffset, getRegName(tempReg));
                    }
                }
                
                // Mark the node as having been handled
                // The code in codegencommon.cpp will still process this node,
                // but it will just move registers (which may be no-ops if source == dest)
                genProduceReg(treeNode);
                return;
            }
            
            // Get layout for non-HFA structs
            layout = source->AsLclVarCommon()->GetLayout(compiler);

            // This struct must live on the stack frame.
            assert(varDsc->lvOnFrame && !varDsc->lvRegister);

            // No possible conflicts, just use the first register as the value register.
            firstRegToPlace = 0;
            valueReg        = treeNode->GetRegNumByIdx(0);
        }
        else if (source->OperIs(GT_BLK))
        {
            layout   = source->AsBlk()->GetLayout();
            addrReg  = genConsumeReg(source->AsBlk()->Addr());
            addrType = source->AsBlk()->Addr()->TypeGet();

            // Check if this is an HFA struct from GT_BLK source
            if (isHfaStruct)
            {
                CORINFO_CLASS_HANDLE classHnd = layout->GetClassHandle();
                
                var_types hfaType;
                unsigned hfaSlots;
                IsPpc64leHfaLikeStruct(compiler, classHnd, &hfaType, &hfaSlots);
                
                unsigned fieldSize = (hfaType == TYP_FLOAT) ? 4 : 8;
                instruction loadIns = (hfaType == TYP_FLOAT) ? INS_lfs : INS_lfd;
                instruction storeIns = (hfaType == TYP_FLOAT) ? INS_stfs : INS_stfd;
                
                JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - HFA struct from GT_BLK (type=%s, slots=%u, gtNumRegs=%u)\n",
                       varTypeName(hfaType), hfaSlots, treeNode->gtNumRegs);
                
                // Load fields into registers
                for (unsigned i = 0; i < treeNode->gtNumRegs; i++)
                {
                    regNumber targetReg = treeNode->GetRegNumByIdx(i);
                    unsigned fieldOffset = i * fieldSize;
                    
                    GetEmitter()->emitIns_R_R_I(loadIns, emitActualTypeSize(hfaType), targetReg, addrReg, fieldOffset);
                    
                    JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - Loaded HFA field %d from [%s+%u] to %s\n",
                           i, getRegName(addrReg), fieldOffset, getRegName(targetReg));
                }
                
                // Store remaining fields to stack
                unsigned stackFields = hfaSlots - treeNode->gtNumRegs;
                if (stackFields > 0)
                {
                    unsigned argOffsetOut = treeNode->getArgOffset();
                    
                    // Use first target register as temporary (it's already been placed)
                    regNumber tempReg = treeNode->GetRegNumByIdx(0);
                    
                    for (unsigned i = 0; i < stackFields; i++)
                    {
                        unsigned fieldOffset = (treeNode->gtNumRegs + i) * fieldSize;
                        unsigned stackOffset = argOffsetOut + (i * fieldSize);
                        
                        GetEmitter()->emitIns_R_R_I(loadIns, emitActualTypeSize(hfaType), tempReg, addrReg, fieldOffset);
                        GetEmitter()->emitIns_S_R(storeIns, emitActualTypeSize(hfaType), tempReg, varNumOut, stackOffset);
                        
                        JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - Stored HFA field %d from [%s+%u] to stack+%u\n",
                               treeNode->gtNumRegs + i, getRegName(addrReg), fieldOffset, stackOffset);
                    }
                }
                
                genProduceReg(treeNode);
                return;
            }

            regNumber allocatedValueReg = REG_NA;
            if (treeNode->gtNumRegs == 1)
            {
                allocatedValueReg = internalRegisters.Extract(treeNode);
            }

            // Pick a register to store intermediate values in for the to-stack
            // copy. It must not conflict with addrReg.
            valueReg = treeNode->GetRegNumByIdx(0);
            if (valueReg == addrReg)
            {
                if (treeNode->gtNumRegs == 1)
                {
                    valueReg = allocatedValueReg;
                }
                else
                {
                    valueReg = treeNode->GetRegNumByIdx(1);
                }
            }

            // Find first register to place. If we are placing addrReg, then
            // make sure we place it last to avoid clobbering its value.
            //
            // The loop below will start at firstRegToPlace and place
            // treeNode->gtNumRegs registers in order, with wraparound. For
            // example, if the registers to place are r3, r4, r5=addrReg, r6
            // then we will set firstRegToPlace = 3 (r6) and the loop below
            // will place r6, r3, r4, r5. The last placement will clobber
            // addrReg.
            firstRegToPlace = 0;
            for (unsigned i = 0; i < treeNode->gtNumRegs; i++)
            {
                if (treeNode->GetRegNumByIdx(i) == addrReg)
                {
                    firstRegToPlace = i + 1;
                    break;
                }
            }
        }
        else if (source->OperIs(GT_FIELD_LIST))
        {
            // For FIELD_LIST sources (created by fgMorphMultiregStructArg), detect if this is an HFA struct
            // by checking the first field's source
            GenTreeFieldList* fieldList = source->AsFieldList();
            GenTreeFieldList::Use* firstUse = fieldList->Uses().GetHead();
            if (firstUse != nullptr)
            {
                GenTree* firstFieldNode = firstUse->GetNode();
                if (firstFieldNode->OperIs(GT_LCL_FLD, GT_LCL_VAR))
                {
                    unsigned srcLclNum = firstFieldNode->OperIs(GT_LCL_FLD)
                                        ? firstFieldNode->AsLclFld()->GetLclNum()
                                        : firstFieldNode->AsLclVar()->GetLclNum();
                    LclVarDsc* varDsc = compiler->lvaGetDesc(srcLclNum);
                    CORINFO_CLASS_HANDLE classHnd = varDsc->lvClassHnd;
                    if (classHnd == NO_CLASS_HANDLE && varDsc->GetLayout() != nullptr)
                    {
                        classHnd = varDsc->GetLayout()->GetClassHandle();
                    }
                    
                    if (classHnd != NO_CLASS_HANDLE)
                    {
                        var_types hfaType;
                        unsigned hfaSlots;
                        isHfaStruct = IsPpc64leHfaLikeStruct(compiler, classHnd, &hfaType, &hfaSlots);
                    }
                }
            }
            
            // For FIELD_LIST sources (created by fgMorphMultiregStructArg), the fields are already
            // loaded into registers. We just need to handle HFA structs specially.
            if (isHfaStruct)
            {
                JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - HFA struct from GT_FIELD_LIST\n");
                
                // Get the FIELD_LIST
                GenTreeFieldList* fieldList = source->AsFieldList();
                
                // Determine HFA element type from the first field
                GenTreeFieldList::Use* firstUse = fieldList->Uses().GetHead();
                var_types hfaType = firstUse->GetNode()->TypeGet();
                assert(varTypeIsFloating(hfaType));
                
                unsigned fieldSize = (hfaType == TYP_FLOAT) ? 4 : 8;
                instruction loadIns = (hfaType == TYP_FLOAT) ? INS_lfs : INS_lfd;
                instruction storeIns = (hfaType == TYP_FLOAT) ? INS_stfs : INS_stfd;
                
                // Fields within gtNumRegs are already in registers.
                // Fields beyond gtNumRegs need to be loaded from source and stored to stack.
                unsigned fieldIndex = 0;
                unsigned stackFieldIndex = 0;
                unsigned argOffsetOut = treeNode->getArgOffset();
                
                // Count total fields first
                unsigned totalFieldCount = 0;
                for (GenTreeFieldList::Use& use : fieldList->Uses())
                {
                    totalFieldCount++;
                }
                
                // Find the source local variable to load stack fields from
                GenTree* firstFieldNode = firstUse->GetNode();
                unsigned srcLclNum = BAD_VAR_NUM;
                if (firstFieldNode->OperIs(GT_LCL_FLD))
                {
                    srcLclNum = firstFieldNode->AsLclFld()->GetLclNum();
                }
                else if (firstFieldNode->OperIs(GT_LCL_VAR))
                {
                    srcLclNum = firstFieldNode->AsLclVar()->GetLclNum();
                }
                
                // Get temp register if we have stack fields
                regNumber tempReg = REG_NA;
                if (totalFieldCount > treeNode->gtNumRegs && srcLclNum != BAD_VAR_NUM)
                {
                    tempReg = internalRegisters.GetSingle(treeNode, RBM_ALLFLOAT);
                }
                
                for (GenTreeFieldList::Use& use : fieldList->Uses())
                {
                    GenTree* fieldNode = use.GetNode();
                    
                    if (fieldIndex >= treeNode->gtNumRegs)
                    {
                        // This field goes to stack
                        unsigned stackOffset = argOffsetOut + (stackFieldIndex * fieldSize);
                        
                        // Load from source local variable and store to stack
                        if (srcLclNum != BAD_VAR_NUM && tempReg != REG_NA)
                        {
                            unsigned srcOffset = fieldIndex * fieldSize;
                            GetEmitter()->emitIns_R_S(loadIns, emitActualTypeSize(hfaType), tempReg, srcLclNum, srcOffset);
                            GetEmitter()->emitIns_S_R(storeIns, emitActualTypeSize(hfaType), tempReg, varNumOut, stackOffset);
                            
                            JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - Loaded FIELD_LIST field %d from V%02u+%u to %s, stored to stack+%u\n",
                                   fieldIndex, srcLclNum, srcOffset, getRegName(tempReg), stackOffset);
                        }
                        else
                        {
                            // Fallback: use the register allocated to this field (if any)
                            regNumber fieldReg = fieldNode->GetRegNum();
                            if (fieldReg != REG_NA)
                            {
                                GetEmitter()->emitIns_S_R(storeIns, emitActualTypeSize(hfaType), fieldReg, varNumOut, stackOffset);
                                JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - Stored FIELD_LIST field %d from %s to stack+%u\n",
                                       fieldIndex, getRegName(fieldReg), stackOffset);
                            }
                            else
                            {
                                assert(!"Stack field has no register and no source local");
                            }
                        }
                        stackFieldIndex++;
                    }
                    else
                    {
                        // This field goes to a register - it's already there from the FIELD_LIST
                        regNumber fieldReg = fieldNode->GetRegNum();
                        JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - FIELD_LIST field %d already in register %s\n",
                               fieldIndex, getRegName(fieldReg));
                    }
                    
                    fieldIndex++;
                }
                
                genProduceReg(treeNode);
                return;
            }
            
            // For non-HFA FIELD_LIST, fall through to common handling below
            assert(!"GT_FIELD_LIST for non-HFA struct in genPutArgSplit not yet implemented");
        }
        else
        {
            assert(!"Unexpected source type in genPutArgSplit");
        }

        // Put on stack first
        // For HFA structs, calculate offset based on actual field size, not TARGET_POINTER_SIZE
        unsigned structOffset;
        if (isHfaStruct)
        {
            // Get HFA element type to determine field size
            CORINFO_CLASS_HANDLE classHnd = layout->GetClassHandle();
            
            var_types hfaType;
            unsigned hfaSlots;
            IsPpc64leHfaLikeStruct(compiler, classHnd, &hfaType, &hfaSlots);
            
            unsigned fieldSize = (hfaType == TYP_FLOAT) ? 4 : 8;
            structOffset = treeNode->gtNumRegs * fieldSize;
            
            JITDUMP("[PPC64LE HFA DEBUG] genPutArgSplit - HFA struct offset calculation: gtNumRegs=%u, fieldSize=%u, structOffset=%u, layoutSize=%u\n",
                   treeNode->gtNumRegs, fieldSize, structOffset, layout->GetSize());
        }
        else
        {
            structOffset = treeNode->gtNumRegs * TARGET_POINTER_SIZE;
        }
        
        unsigned remainingSize = layout->GetSize() - structOffset;
        unsigned argOffsetOut  = treeNode->getArgOffset();

        assert((remainingSize > 0) && (roundUp(remainingSize, TARGET_POINTER_SIZE) == treeNode->GetStackByteSize()));
        while (remainingSize > 0)
        {
            var_types type;
            instruction loadIns;
            instruction storeIns;
            unsigned moveSize;

            if (remainingSize >= TARGET_POINTER_SIZE)
            {
                type = layout->GetGCPtrType(structOffset / TARGET_POINTER_SIZE);
                loadIns = INS_ld;
                storeIns = INS_std;
                moveSize = TARGET_POINTER_SIZE;
            }
            else if (remainingSize >= 4)
            {
                type = TYP_INT;
                loadIns = INS_lwz;
                storeIns = INS_stw;
                moveSize = 4;
            }
            else if (remainingSize >= 2)
            {
                type = TYP_USHORT;
                loadIns = INS_lhz;
                storeIns = INS_sth;
                moveSize = 2;
            }
            else
            {
                assert(remainingSize == 1);
                type = TYP_UBYTE;
                loadIns = INS_lbz;
                storeIns = INS_stb;
                moveSize = 1;
            }

            emitAttr attr = emitActualTypeSize(type);

            if (srcLclNum != BAD_VAR_NUM)
            {
                // Load from our local source
                emit->emitIns_R_S(loadIns, attr, valueReg, srcLclNum, srcLclOffset + structOffset);
            }
            else
            {
                assert(valueReg != addrReg);

                // Load from our address expression source
                emit->emitIns_R_R_I(loadIns, attr, valueReg, addrReg, structOffset);
            }

            // Emit the instruction to store the register into the outgoing argument area
            emit->emitIns_S_R(storeIns, attr, valueReg, varNumOut, argOffsetOut);
            argOffsetOut += moveSize;
            assert(argOffsetOut <= argOffsetMax);

            remainingSize -= moveSize;
            structOffset += moveSize;
        }

        // Place registers starting from firstRegToPlace. It should ensure we
        // place addrReg last (if we place it at all).
        structOffset         = static_cast<unsigned>(firstRegToPlace) * TARGET_POINTER_SIZE;
        unsigned curRegIndex = firstRegToPlace;

        for (unsigned regsPlaced = 0; regsPlaced < treeNode->gtNumRegs; regsPlaced++)
        {
            if (curRegIndex == treeNode->gtNumRegs)
            {
                curRegIndex  = 0;
                structOffset = 0;
            }

            regNumber targetReg = treeNode->GetRegNumByIdx(curRegIndex);
            var_types type      = treeNode->GetRegType(curRegIndex);

            if (srcLclNum != BAD_VAR_NUM)
            {
                // Load from our local source
                emit->emitIns_R_S(INS_ld, emitTypeSize(type), targetReg, srcLclNum, srcLclOffset + structOffset);
            }
            else
            {
                assert((addrReg != targetReg) || (regsPlaced == treeNode->gtNumRegs - 1));

                // Load from our address expression source
                emit->emitIns_R_R_I(INS_ld, emitTypeSize(type), targetReg, addrReg, structOffset);
            }

            curRegIndex++;
            structOffset += TARGET_POINTER_SIZE;
        }
    }
    genProduceReg(treeNode);
}

#ifdef FEATURE_SIMD
//----------------------------------------------------------------------------------
// genMultiRegStoreToSIMDLocal: store multi-reg value to a single-reg SIMD local
//
// Arguments:
//    lclNode  -  GentreeLclVar of GT_STORE_LCL_VAR
//
// Return Value:
//    None
//
void CodeGen::genMultiRegStoreToSIMDLocal(GenTreeLclVar* lclNode)
{

    //_ASSERTE("!NYI");
    abort();
}

#endif // FEATURE_SIMD

//------------------------------------------------------------------------
// genCodeForStoreLclVar: Produce code for a GT_STORE_LCL_VAR node.
//
// Arguments:
//    lclNode - the GT_STORE_LCL_VAR node
//
void CodeGen::genCodeForStoreLclVar(GenTreeLclVar* lclNode)
{
    GenTree* data = lclNode->gtOp1;

    // Stores from a multi-reg source are handled separately.
    if (data->gtSkipReloadOrCopy()->IsMultiRegNode())
    {
        genMultiRegStoreToLocal(lclNode);
        return;
    }

    LclVarDsc* varDsc = compiler->lvaGetDesc(lclNode);
    if (lclNode->IsMultiReg())
    {
        // This is the case of storing to a multi-reg HFA local from a fixed-size SIMD type.
        // Note: PPC64LE may not support HFA in the same way as ARM64, but keeping structure similar
        assert(varTypeIsSIMD(data) && varDsc->lvIsHfa());
        regNumber    operandReg = genConsumeReg(data);
        unsigned int regCount   = varDsc->lvFieldCnt;
        for (unsigned i = 0; i < regCount; ++i)
        {
            regNumber varReg = lclNode->GetRegByIndex(i);
            assert(varReg != REG_NA);
            unsigned   fieldLclNum = varDsc->lvFieldLclStart + i;
            LclVarDsc* fieldVarDsc = compiler->lvaGetDesc(fieldLclNum);
            // TODO-PPC64LE: Implement appropriate vector element extraction for PPC64LE
            // This may require different instructions than ARM64's INS_dup
            //NYI_PPC64("genCodeForStoreLclVar - multi-reg HFA store");
            abort();
        }
        genProduceReg(lclNode);
    }
    else
    {
        regNumber targetReg = lclNode->GetRegNum();
        emitter*  emit      = GetEmitter();

        unsigned  varNum     = lclNode->GetLclNum();
        var_types targetType = varDsc->GetRegisterType(lclNode);

#ifdef FEATURE_SIMD
        // storing of TYP_SIMD12 (i.e. Vector3) field
        if (targetType == TYP_SIMD12)
        {
            genStoreLclTypeSimd12(lclNode);
            return;
        }
#endif // FEATURE_SIMD

        genConsumeRegs(data);

        regNumber dataReg = REG_NA;
        if (data->isContained())
        {
            // This is only possible for a zero-init or bitcast.
            const bool zeroInit = (data->IsIntegralConst(0) || data->IsVectorZero());
            assert(zeroInit || data->OperIs(GT_BITCAST));

            if (zeroInit && varTypeIsSIMD(targetType))
            {
                if (targetReg != REG_NA)
                {
                    // TODO-PPC64LE: Implement SIMD zero initialization for PPC64LE
                    // This may use vector instructions like xxlxor or similar
                    //NYI_PPC64("genCodeForStoreLclVar - SIMD zero init to register");
                    abort();
                }
                else
                {
                    // Store zero to stack-based SIMD local
                    // TODO-PPC64LE: Implement stack store of zero SIMD value
                    //NYI_PPC64("genCodeForStoreLclVar - SIMD zero init to stack");
                    abort();
                }
                genUpdateLifeStore(lclNode, targetReg, varDsc);
                return;
            }
            if (zeroInit)
            {
                // For PPC64LE, we can use R0 as zero register in some contexts
                dataReg = REG_R0;
            }
            else
            {
                const GenTree* bitcastSrc = data->AsUnOp()->gtGetOp1();
                assert(!bitcastSrc->isContained());
                dataReg = bitcastSrc->GetRegNum();
            }
        }
        else
        {
            assert(!data->isContained());
            dataReg = data->GetRegNum();
        }
        assert(dataReg != REG_NA);

        if (targetReg == REG_NA) // store into stack based LclVar
        {
            inst_set_SV_var(lclNode);

            instruction ins  = ins_Store(targetType);
            emitAttr    attr = emitTypeSize(targetType);

            // For HFA structs, if dataReg is a float register, we need to use float store instructions
            // even though targetType might be TYP_LONG
            // Check if this is an HFA struct with float register
            var_types hfaType = TYP_UNDEF;
            unsigned hfaSlots = 0;
            
            if (genIsValidFloatReg(dataReg) && varTypeIsStruct(varDsc))
            {
                // Try to get class handle - first from lvClassHnd, then from GetLayout()
                CORINFO_CLASS_HANDLE classHnd = varDsc->lvClassHnd;
                if (classHnd == NO_CLASS_HANDLE)
                {
                    ClassLayout* layout = varDsc->GetLayout();
                    if (layout != nullptr)
                    {
                        classHnd = layout->GetClassHandle();
                    }
                }
                
                if (classHnd != NO_CLASS_HANDLE && IsPpc64leHfaLikeStruct(compiler, classHnd, &hfaType, &hfaSlots))
                {
                    // This is an HFA struct, compute attr from HFA element type and determine instruction
                    attr = emitActualTypeSize(hfaType);
                    if (attr == EA_8BYTE)
                    {
                        ins = INS_stfd;  // Store double (8 bytes)
                    }
                    else // EA_4BYTE
                    {
                        ins = INS_stfs;  // Store single (4 bytes)
                    }
                    JITDUMP("[PPC64LE HFA DEBUG] genCodeForStoreLclVar - HFA detected (element type %s), overriding instruction to %s for %s -> V%02u (original type=%s, attr=%d, hfaSlots=%u, lvIsParam=%d)\n",
                           varTypeName(hfaType), genInsName(ins), getRegName(dataReg), varNum, varTypeName(targetType), (int)attr, hfaSlots, varDsc->lvIsParam);
                }
            }

            emit->emitIns_S_R(ins, attr, dataReg, varNum, /* offset */ 0); //This will be handled via code implemented at lclvars.cpp:7063
        }
        else // store into register (i.e move into register)
        {
            // Assign into targetReg when dataReg (from op1) is not the same register
            if (varTypeIsIntegral(targetType) && emit->isGeneralRegister(targetReg) && emit->isGeneralRegister(dataReg))
            {
                // For PPC64LE, we may need sign/zero extension
                // Use appropriate move instruction with extension if needed
                inst_Mov(targetType, targetReg, dataReg, /* canSkip */ true);
            }
            else
            {
                // For floating point or when no extension needed
                inst_Mov(targetType, targetReg, dataReg, /* canSkip */ true);
            }
        }
        genUpdateLifeStore(lclNode, targetReg, varDsc);
    }
}

//------------------------------------------------------------------------
// genCodeForStoreLclFld: Produce code for a GT_STORE_LCL_FLD node.
//
// Arguments:
//    tree - the GT_STORE_LCL_FLD node
//
void CodeGen::genCodeForStoreLclFld(GenTreeLclFld* tree)
{
    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->GetRegNum();
    emitter*  emit       = GetEmitter();
    
    noway_assert(targetType != TYP_STRUCT);

    // record the offset
    unsigned offset = tree->GetLclOffs();

    // We must have a stack store with GT_STORE_LCL_FLD
    noway_assert(targetReg == REG_NA);

    unsigned   varNum = tree->GetLclNum();
    LclVarDsc* varDsc = compiler->lvaGetDesc(varNum);

    GenTree* data = tree->gtOp1;
    genConsumeRegs(data);

    regNumber dataReg = REG_NA;
    if (data->isContainedIntOrIImmed())
    {
        assert(data->IsIntegralConst(0));
        dataReg = REG_R0;  // Use R0 as zero register
    }
    else if (data->isContained())
    {
        assert(data->OperIs(GT_BITCAST));
        const GenTree* bitcastSrc = data->AsUnOp()->gtGetOp1();
        assert(!bitcastSrc->isContained());
        dataReg = bitcastSrc->GetRegNum();
    }
    else
    {
        assert(!data->isContained());
        dataReg = data->GetRegNum();
    }
    assert(dataReg != REG_NA);

    // Determine the actual type to store based on the source register type
    // For HFA structs on PPC64LE, the tree type may be TYP_LONG but the register is a float register
    var_types storeType = targetType;
    
    // Check if this is an HFA struct with float register
    var_types hfaType = TYP_UNDEF;
    unsigned hfaSlots = 0;
    
    if (genIsValidFloatReg(dataReg) && varTypeIsStruct(varDsc))
    {
        // Try to get class handle - first from lvClassHnd, then from GetLayout()
        CORINFO_CLASS_HANDLE classHnd = varDsc->lvClassHnd;
        if (classHnd == NO_CLASS_HANDLE)
        {
            ClassLayout* layout = varDsc->GetLayout();
            if (layout != nullptr)
            {
                classHnd = layout->GetClassHandle();
            }
        }
        
        if (classHnd != NO_CLASS_HANDLE && IsPpc64leHfaLikeStruct(compiler, classHnd, &hfaType, &hfaSlots))
        {
            // This is an HFA struct, use the HFA element type
            storeType = hfaType;
            JITDUMP("[PPC64LE HFA DEBUG] genCodeForStoreLclFld - HFA detected, overriding store type from %s to %s for %s -> V%02u+%u (hfaSlots=%u, lvIsParam=%d)\n",
                   varTypeName(targetType), varTypeName(storeType), getRegName(dataReg), varNum, offset, hfaSlots, varDsc->lvIsParam);
        }
    }

    instruction ins  = ins_Store(storeType);
    emitAttr    attr = emitTypeSize(storeType);

    emit->emitIns_S_R(ins, attr, dataReg, varNum, offset);

    genUpdateLife(tree);

    varDsc->SetRegNum(REG_STK);
}


//------------------------------------------------------------------------
// genSimpleReturn: Generate code for a simple return (non-struct, non-void).
//
// Arguments:
//    treeNode - The GT_RETURN/GT_RETFILT/GT_SWIFT_ERROR_RET tree node with non-struct and non-void type
//
// Return Value:
//    None
//
void CodeGen::genSimpleReturn(GenTree* treeNode)
{
    assert(treeNode->OperIs(GT_RETURN, GT_RETFILT, GT_SWIFT_ERROR_RET));
    GenTree*  op1        = treeNode->AsOp()->GetReturnValue();
    var_types targetType = treeNode->TypeGet();

    assert(targetType != TYP_STRUCT);
    assert(targetType != TYP_VOID);

    regNumber retReg = varTypeUsesFloatArgReg(treeNode) ? REG_FLOATRET : REG_INTRET;

    bool movRequired = (op1->GetRegNum() != retReg);

    if (!movRequired)
    {
        if (op1->OperGet() == GT_LCL_VAR)
        {
            GenTreeLclVarCommon* lcl            = op1->AsLclVarCommon();
            const LclVarDsc*     varDsc         = compiler->lvaGetDesc(lcl);
            bool                 isRegCandidate = varDsc->lvIsRegCandidate();
            if (isRegCandidate && ((op1->gtFlags & GTF_SPILLED) == 0))
            {
                // We may need to generate a zero-extending mov instruction to load the value from this GT_LCL_VAR

                var_types op1Type = genActualType(op1->TypeGet());
                var_types lclType = genActualType(varDsc->TypeGet());

                if (genTypeSize(op1Type) < genTypeSize(lclType))
                {
                    movRequired = true;
                }
            }
        }
    }

    // For PPC64LE, use inst_Mov to move the return value to the appropriate return register
    inst_Mov(targetType, retReg, op1->GetRegNum(), /* canSkip */ !movRequired);
}

//------------------------------------------------------------------------
// genCodeForLclVar: Produce code for a GT_LCL_VAR node.
//
// Arguments:
//    tree - the GT_LCL_VAR node
//
void CodeGen::genCodeForLclVar(GenTreeLclVar* tree)
{
    unsigned varNum = tree->GetLclNum();
    assert(varNum < compiler->lvaCount);

    LclVarDsc* varDsc         = compiler->lvaGetDesc(varNum);
    bool       isRegCandidate = varDsc->lvIsRegCandidate();

    // lcl_vars are not defs
    assert((tree->gtFlags & GTF_VAR_DEF) == 0);

    // If this is a register candidate that has been spilled, genConsumeReg() will
    // reload it at the point of use. Otherwise, if it's not in a register, we load it here.
    if (!isRegCandidate && !tree->IsMultiReg() && ((tree->gtFlags & GTF_SPILLED) == 0))
    {
        var_types targetType = varDsc->GetRegisterType(tree);

        // targetType must be a normal scalar type and not a TYP_STRUCT
        assert(targetType != TYP_STRUCT);

        instruction ins  = ins_Load(targetType);
        emitAttr    attr = emitTypeSize(targetType);

        GetEmitter()->emitIns_R_S(ins, attr, tree->GetRegNum(), varNum, 0);
        genProduceReg(tree);
    }
}

//------------------------------------------------------------------------
// genCreateAndStoreGCInfo: Create and record GC Info for the function.
//
void CodeGen::genCreateAndStoreGCInfo(unsigned            codeSize,
                                      unsigned            prologSize,
				      unsigned epilogSize DEBUGARG(void* codePtr))
{
    IAllocator*    allowZeroAlloc = new (compiler, CMK_GC) CompIAllocator(compiler->getAllocatorGC());
    GcInfoEncoder* gcInfoEncoder  = new (compiler, CMK_GC)
        GcInfoEncoder(compiler->info.compCompHnd, compiler->info.compMethodInfo, allowZeroAlloc, NOMEM);
    assert(gcInfoEncoder != nullptr);

    // Follow the code pattern of the x86 gc info encoder
    gcInfo.gcInfoBlockHdrSave(gcInfoEncoder, codeSize, prologSize);

    // We keep the call count for the second call to gcMakeRegPtrTable() below.
    unsigned callCnt = 0;

    // First we figure out the encoder ID's for the stack slots and registers.
    gcInfo.gcMakeRegPtrTable(gcInfoEncoder, codeSize, prologSize, GCInfo::MAKE_REG_PTR_MODE_ASSIGN_SLOTS, &callCnt);

    // Now we've requested all the slots we'll need; "finalize" these
    gcInfoEncoder->FinalizeSlotIds();

    // Now we can actually use those slot ID's to declare live ranges.
    gcInfo.gcMakeRegPtrTable(gcInfoEncoder, codeSize, prologSize, GCInfo::MAKE_REG_PTR_MODE_DO_WORK, &callCnt);

    if (compiler->opts.IsReversePInvoke())
    {
        unsigned reversePInvokeFrameVarNumber = compiler->lvaReversePInvokeFrameVar;
        assert(reversePInvokeFrameVarNumber != BAD_VAR_NUM);
        const LclVarDsc* reversePInvokeFrameVar = compiler->lvaGetDesc(reversePInvokeFrameVarNumber);
        gcInfoEncoder->SetReversePInvokeFrameSlot(reversePInvokeFrameVar->GetStackOffset());
    }

    gcInfoEncoder->Build();

    // GC Encoder automatically puts the GC info in the right spot
    compiler->compInfoBlkAddr = gcInfoEncoder->Emit();
    compiler->compInfoBlkSize = 0; // not exposed by the GCEncoder interface
}


//------------------------------------------------------------------------
// genRangeCheck: generate code for GT_BOUNDS_CHECK node.
//
// Arguments:
//    oper - The GT_BOUNDS_CHECK node
//
// Notes:
//    Emits an unsigned comparison (index >= length) and jumps to the
//    range-check-fail helper block if the check fails.
//
//    The CLR array bounds check is defined as:
//        throw if (uint)index >= (uint)length
//    so we always use the unsigned compare instructions cmplw/cmpld.
//
void CodeGen::genRangeCheck(GenTree* oper)
{
    noway_assert(oper->OperIs(GT_BOUNDS_CHECK));
    GenTreeBoundsChk* bndsChk = oper->AsBoundsChk();

    GenTree* arrLen   = bndsChk->GetArrayLength();
    GenTree* arrIndex = bndsChk->GetIndex();

    genConsumeRegs(arrIndex);
    genConsumeRegs(arrLen);

    regNumber indexReg  = arrIndex->GetRegNum();
    regNumber lengthReg = arrLen->GetRegNum();

#ifdef DEBUG
    var_types bndsChkType = genActualType(arrLen->TypeGet());
    var_types indexType   = genActualType(arrIndex->TypeGet());
    // Bounds checks can only be 32 or 64 bit sized comparisons.
    assert(bndsChkType == TYP_INT || bndsChkType == TYP_LONG);
    assert(indexType == TYP_INT || indexType == TYP_LONG);
#endif // DEBUG

    // Array bounds checks are ALWAYS unsigned: the CLR defines them as
    //   (uint)index >= (uint)length
    // so we must use cmplw/cmpld (unsigned) rather than cmpw/cmpd (signed).
    //
    // cmplw compares the low 32 bits of both registers as unsigned 32-bit
    // integers — correct for TYP_INT operands without any sign extension.
    // cmpld compares the full 64-bit register content — correct for TYP_LONG.
    instruction cmpIns = (genActualType(arrLen->TypeGet()) == TYP_LONG) ? INS_cmpld : INS_cmplw;
    GetEmitter()->emitIns_R_R(cmpIns, emitActualTypeSize(genActualType(arrLen->TypeGet())),
                              indexReg, lengthReg);

    // Branch if (uint)index >= (uint)length → range-check failure
    genJumpToThrowHlpBlk(EJ_ge, bndsChk->gtThrowKind, bndsChk->gtIndRngFailBB);
}

//---------------------------------------------------------------------
// genCodeForPhysReg - generate code for a GT_PHYSREG node
//
// Arguments
//    tree - the GT_PHYSREG node
//
// Return value:
//    None
//
void CodeGen::genCodeForPhysReg(GenTreePhysReg* tree)
{
    assert(tree->OperIs(GT_PHYSREG));

    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->GetRegNum();

    // If the physical source register differs from the LSRA-assigned target
    // register, emit a move.  If they are the same, no instruction is needed.
    if (targetReg != tree->gtSrcReg)
    {
        GetEmitter()->emitIns_Mov(ins_Copy(targetType), emitActualTypeSize(targetType),
                                  targetReg, tree->gtSrcReg, /* canSkip */ false);
        genTransferRegGCState(targetReg, tree->gtSrcReg);
    }

    genProduceReg(tree);
}

//---------------------------------------------------------------------
// genCodeForNullCheck - generate code for a GT_NULLCHECK node
//
// Arguments
//    tree - the GT_NULLCHECK node
//
// Return value:
//    None
//
void CodeGen::genCodeForNullCheck(GenTreeIndir* tree)
{
    assert(tree->OperIs(GT_NULLCHECK));

    genConsumeRegs(tree->gtOp1);

    // Perform a load operation to trigger a null pointer exception if the address is null
    // Use REG_R0 as a scratch register (zero register on PPC64LE)
    GetEmitter()->emitInsLoadStoreOp(ins_Load(tree->TypeGet()), emitActualTypeSize(tree), REG_R0, tree);
}

//------------------------------------------------------------------------
// genCodeForReturnTrap: Produce code for a GT_RETURNTRAP node.
//
// Arguments:
//    tree - the GT_RETURNTRAP node
//
// Notes:
//    Emits a conditional call to CORINFO_HELP_STOP_FOR_GC when the GC trap
//    flag (g_TrapReturningThreads) is non-zero.  This is required after every
//    return from native code so that a pending GC can collect before the
//    managed thread resumes.
//
//    PPC64LE instruction sequence:
//      cmpwi  data_reg, 0      ; compare trap value against zero
//      beq    skipLabel        ; if zero, GC is not waiting — skip helper
//      <genEmitHelperCall>     ; call CORINFO_HELP_STOP_FOR_GC
//    skipLabel:
//
void CodeGen::genCodeForReturnTrap(GenTreeOp* tree)
{
    assert(tree->OperGet() == GT_RETURNTRAP);

    GenTree* data = tree->gtOp1;
    genConsumeRegs(data);

    // Compare the trap value against zero using a word compare.
    // The trap flag is an int32 so EA_4BYTE / cmpwi is correct.
    GetEmitter()->emitIns_R_I(INS_cmpwi, EA_4BYTE, data->GetRegNum(), 0);

    BasicBlock* skipLabel = genCreateTempLabel();

    // Branch over the helper call if the trap is not set.
    inst_JMP(EJ_eq, skipLabel);

    // Emit the call to the EE helper that stops for GC (or other reasons).
    genEmitHelperCall(CORINFO_HELP_STOP_FOR_GC, 0, EA_UNKNOWN);

    genDefineTempLabel(skipLabel);
}
//------------------------------------------------------------------------
// genCodeForShift: Generates the code sequence for a GenTree node that
// represents a bit shift or rotate operation (<<, >>, >>>, rol, ror).
//
// Arguments:
//    tree - the bit shift node (that specifies the type of bit shift to perform).
//
// Assumptions:
//    a) All GenTrees are register allocated.
//
void CodeGen::genCodeForShift(GenTree* tree)
{
    assert(tree->OperIs(GT_LSH, GT_RSH, GT_RSZ, GT_ROR, GT_ROL));
    
    var_types   targetType = tree->TypeGet();
    genTreeOps  oper       = tree->OperGet();
    instruction ins        = INS_invalid;
    emitAttr    size       = emitActualTypeSize(targetType);
    
    GenTree* operand = tree->gtGetOp1();
    GenTree* shiftBy = tree->gtGetOp2();
    
    regNumber targetReg  = tree->GetRegNum();
    regNumber operandReg = operand->GetRegNum();
    
    // Determine if this is 32-bit or 64-bit operation
    bool is64Bit = (size == EA_8BYTE);
    
    if (shiftBy->IsCnsIntOrI())
    {
        // Immediate shift amount
        ssize_t shiftAmount = shiftBy->AsIntCon()->IconValue();
        
        // Mask shift amount (PowerPC masks automatically, but be explicit)
        shiftAmount &= (is64Bit ? 63 : 31);
        
        // Select appropriate immediate instruction
        switch (oper)
        {
            case GT_LSH:
                ins = is64Bit ? INS_sldi : INS_slwi;
                break;
                
            case GT_RSH:
                // Arithmetic right shift (sign-extending)
                ins = is64Bit ? INS_sradi : INS_srawi;
                break;
                
            case GT_RSZ:
                // Logical right shift (zero-extending)
                ins = is64Bit ? INS_srdi : INS_srwi;
                break;
                
            case GT_ROR:
                // Rotate right: PPC64LE uses rldicl/rlwinm to implement ROR.
                // rotrd rA, rS, n  →  rldicl rA, rS, (64-n)&63, 0  (64-bit)
                // rotrw rA, rS, n  →  rlwinm rA, rS, (32-n)&31, 0, 31 (32-bit)
                ins = is64Bit ? INS_rotrd : INS_rotrw;
                break;
                
            default:
                unreached();
        }
        
        // Emit: targetReg = operandReg SHIFT shiftAmount
        GetEmitter()->emitIns_R_R_I(ins, size, targetReg, operandReg, shiftAmount);
    }
    else
    {
        // Register-based shift amount
        regNumber shiftReg = shiftBy->GetRegNum();
        
        // Select appropriate register instruction
        switch (oper)
        {
            case GT_LSH:
                ins = is64Bit ? INS_sld : INS_slw;
                break;
                
            case GT_RSH:
                // Arithmetic right shift (sign-extending)
                ins = is64Bit ? INS_srad : INS_sraw;
                break;
                
            case GT_RSZ:
                // Logical right shift (zero-extending)
                ins = is64Bit ? INS_srd : INS_srw;
                break;
                
            case GT_ROR:
                // Rotate right (register): PPC64LE uses rldcl/rlwnm.
                // LowerRotate has already converted GT_ROL → GT_ROR by negating
                // the count, so the register value here is the right-rotate amount.
                // rldcl rA, rS, rB, 0  rotates left by rB bits with no mask clear,
                // which is equivalent to ROR when rB = 64 - n (already computed).
                ins = is64Bit ? INS_rldcl : INS_rlwnm;
                break;
                
            default:
                unreached();
        }
        
        // Emit: targetReg = operandReg SHIFT shiftReg
        GetEmitter()->emitIns_R_R_R(ins, size, targetReg, operandReg, shiftReg);
    }
    
    genProduceReg(tree);
}

//------------------------------------------------------------------------
// genCodeForLclAddr: Generates the code for GT_LCL_ADDR.
//
// Arguments:
//    lclAddrNode - the node.
//
void CodeGen::genCodeForLclAddr(GenTreeLclFld* lclAddrNode)
{
    assert(lclAddrNode->OperIs(GT_LCL_ADDR));

    var_types targetType = lclAddrNode->TypeGet();
    emitAttr  size       = emitTypeSize(targetType);
    regNumber targetReg  = lclAddrNode->GetRegNum();

    // Address of a local var.
    noway_assert((targetType == TYP_BYREF) || (targetType == TYP_I_IMPL));

    // PowerPC64 doesn't have LEA instruction like x86/ARM
    // We compute the address using: addi targetReg, framePointer, offset
    // The emitIns_R_S handles this by computing the stack offset and generating appropriate instructions
    
    GetEmitter()->emitIns_R_S(INS_addi, size, targetReg, lclAddrNode->GetLclNum(), lclAddrNode->GetLclOffs());

    genProduceReg(lclAddrNode);

}

//------------------------------------------------------------------------
// genTableBasedSwitch: Generate code for a switch statement based on a table of ip-relative offsets
//
// Arguments:
//    treeNode - the GT_SWITCH_TABLE node
//
void CodeGen::genTableBasedSwitch(GenTree* treeNode)
{
    genConsumeOperands(treeNode->AsOp());
    regNumber idxReg  = treeNode->AsOp()->gtOp1->GetRegNum();
    regNumber baseReg = treeNode->AsOp()->gtOp2->GetRegNum();

    regNumber tmpReg = internalRegisters.GetSingle(treeNode);

    // Block-relative approach for PPC64 (similar to x86):
    // 1. baseReg has jump table address (from genJumpTable)
    // 2. Load block-relative offset from jump table into baseReg
    // 3. Get method start address (fgFirstBB) using bcl/mflr + PC-relative
    // 4. Add block offset to method start to get target in R12 (PPC64LE ABI requirement)
    
    // Step 1: Load block-relative offset from jump table into baseReg
    // Calculate table entry address: baseReg + (idxReg << 2)
    GetEmitter()->emitIns_R_R_I(INS_sldi, EA_8BYTE, tmpReg, idxReg, 2);
    GetEmitter()->emitIns_R_R_R(INS_add, EA_PTRSIZE, tmpReg, baseReg, tmpReg);
    // Load 32-bit block-relative offset with sign extension into baseReg
    GetEmitter()->emitIns_R_R_I(INS_lwa, EA_4BYTE, baseReg, tmpReg, 0);

    // Step 2: Get method start address (fgFirstBB) using bcl/mflr + PC-relative
    // This is similar to genJumpTable but calculates offset to method start instead
    regNumber methodStartReg = tmpReg;  // Reuse tmpReg for method start address
    regNumber offsetReg = REG_R0;       // Use R0 for offset calculation
    
    // Capture current PC
    GetEmitter()->emitIns(INS_bcl);
    GetEmitter()->emitIns_R(INS_mflr, EA_PTRSIZE, methodStartReg);
    
    // Get field handle for fgFirstBB (first basic block)
    // Use a special sentinel value to indicate fgFirstBB (not a real data offset)
    // Jump table offsets are relative to fgFirstBB, not method start
    // Maximum encodable value is 0x0FFFFFFF (after left shift by 2, must be < 0x40000000)
    // We use 0x0FFFFFFE as sentinel since real jump tables will be much smaller
    const unsigned FIRST_BB_SENTINEL = 0x0FFFFFFE;
    CORINFO_FIELD_HANDLE fldHnd = compiler->eeFindJitDataOffs(FIRST_BB_SENTINEL);
    
    // Load offset from current PC to method start
    // lis offsetReg, offset@ha
    GetEmitter()->emitIns_R_C(INS_lis, EA_PTRSIZE, offsetReg, fldHnd, 0);
    // addi methodStartReg, methodStartReg, offset@l
    GetEmitter()->emitIns_R_R_C(INS_addi, EA_PTRSIZE, methodStartReg, methodStartReg, fldHnd, 0);
    // add methodStartReg, methodStartReg, offsetReg
    GetEmitter()->emitIns_R_R_R(INS_add, EA_PTRSIZE, methodStartReg, methodStartReg, offsetReg);
    
    // Step 3: Add block-relative offset to method start to get absolute target address
    // IMPORTANT: Store result in R12 as required by PPC64LE ABI for indirect branches
    // R12 = methodStartReg + baseReg (target = method_start + block_offset)
    GetEmitter()->emitIns_R_R_R(INS_add, EA_PTRSIZE, REG_R12, methodStartReg, baseReg);

    // Step 4: Jump to target (R12 contains target address per PPC64LE ABI)
    GetEmitter()->emitIns_R(INS_mtctr, EA_PTRSIZE, REG_R12);
    GetEmitter()->emitIns(INS_bctr);
}

//------------------------------------------------------------------------
// genJumpTable: Emits the jump table and loads its address using PC-relative addressing
//
// Arguments:
//    treeNode - the GT_JMPTABLE node
//
// Notes:
//    Uses PC-relative addressing to load the jump table address.
//    The jump table contains absolute addresses that will be relocated by the runtime.
//
void CodeGen::genJumpTable(GenTree* treeNode)
{
    // Use block-relative addressing for jump table on PPC64LE
    // The jump table contains offsets relative to the method start
    unsigned jmpTabBase = genEmitJumpTable(treeNode, true);  // true = use block-relative offsets
    regNumber targetReg = treeNode->GetRegNum();
    
    regNumber tmpReg = REG_R0;  // Use R0 as temporary register
    
    // Get current PC using bcl/mflr technique
    GetEmitter()->emitIns(INS_bcl);  // bcl 20, 31, $+4 - branch to next instruction
    GetEmitter()->emitIns_R(INS_mflr, EA_PTRSIZE, targetReg);  // mflr targetReg - get PC into targetReg
    
    // Calculate offset from current PC back to method start
    // We need: method_start = PC - offset_to_method_start
    // Then we can add block-relative offsets from jump table to method_start
    
    // For now, load the jump table address (we'll fix the offset calculation in emitter)
    // Get field handle for the jump table data
    CORINFO_FIELD_HANDLE fldHnd = compiler->eeFindJitDataOffs(jmpTabBase);
    
    // Load high 16 bits of offset: lis tmpReg, offset@ha
    GetEmitter()->emitIns_R_C(INS_lis, EA_PTRSIZE, tmpReg, fldHnd, 0);
    
    // Add low 16 bits of offset to targetReg: addi targetReg, targetReg, offset@l
    // This gives: targetReg = PC + offset_low
    GetEmitter()->emitIns_R_R_C(INS_addi, EA_PTRSIZE, targetReg, targetReg, fldHnd, 0);
    
    // Add high bits to get absolute jump table address
    // targetReg = targetReg + tmpReg = (PC + offset_low) + offset_high
    GetEmitter()->emitIns_R_R_R(INS_add, EA_PTRSIZE, targetReg, targetReg, tmpReg);
    
    genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// genCodeForInitBlkLoop - Generate code for an InitBlk using an inlined for-loop.
//    It's needed for cases when size is too big to unroll and we're not allowed
//    to use memset call due to atomicity requirements.
//
// Arguments:
//    initBlkNode - the GT_STORE_BLK node
//
// Code shape (PPC64LE ELFv2):
//
//    std     zeroReg, 0(dstReg)          ; zero first word + null-check dstReg
//    [if size > 8:]
//      li/lis offsetReg, <size - 8>
//    .LOOP:
//      stdx    zeroReg, dstReg, offsetReg ; *(dstReg + offsetReg) = 0
//      addi    offsetReg, offsetReg, -8
//      cmpdi   cr0, offsetReg, 0
//      bne     cr0, .LOOP
//
// The loop counts down from (size - 8) to 0, stepping by 8 bytes.
// The first 8 bytes are zeroed before the loop as a null-check; the loop
// body then covers bytes [8 .. size-8] (inclusive) working backwards.
//
void CodeGen::genCodeForInitBlkLoop(GenTreeBlk* initBlkNode)
{
    GenTree* const dstNode = initBlkNode->Addr();
    genConsumeReg(dstNode);
    const regNumber dstReg = dstNode->GetRegNum();

    // The fill value may be wrapped in a GT_INIT_VAL node (which is contained).
    // Unwrap it to get the actual fill value node that holds the allocated register.
    GenTree* zeroNode = initBlkNode->Data();
    if (zeroNode->OperIs(GT_INIT_VAL))
    {
        assert(zeroNode->isContained());
        zeroNode = zeroNode->gtGetOp1();
    }
    genConsumeReg(zeroNode);
    const regNumber zeroReg = zeroNode->GetRegNum();

    if (initBlkNode->IsVolatile())
    {
        // Issue a full memory barrier before a volatile initBlock operation.
        // PPC64LE ELFv2: use 'sync' (hwsync) for a heavyweight full barrier.
        instGen_MemoryBarrier();
    }

    const unsigned size = initBlkNode->GetLayout()->GetSize();
    assert((size >= TARGET_POINTER_SIZE) && ((size % TARGET_POINTER_SIZE) == 0));

    // The loop is reversed (counting down from size-8 to 0).
    // We zero the first word *before* entering the loop so that a null dstReg
    // causes an AV immediately rather than at "null + large_offset" on the
    // first loop iteration.
    GetEmitter()->emitIns_R_R_I(INS_std, EA_PTRSIZE, zeroReg, dstReg, 0);

    if (size > TARGET_POINTER_SIZE)
    {
        // Keep dstReg live across stores so GC can track the interior pointer.
        gcInfo.gcMarkRegPtrVal(dstReg, dstNode->TypeGet());

        const regNumber offsetReg = internalRegisters.GetSingle(initBlkNode);
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, offsetReg, size - TARGET_POINTER_SIZE);

        BasicBlock* loop = genCreateTempLabel();
        genDefineTempLabel(loop);

        // stdx  zeroReg, dstReg, offsetReg  =>  MEM[dstReg + offsetReg] = zeroReg
        GetEmitter()->emitIns_R_R_R(INS_stdx, EA_PTRSIZE, zeroReg, dstReg, offsetReg);
        // addi  offsetReg, offsetReg, -8
        GetEmitter()->emitIns_R_R_I(INS_addi, EA_PTRSIZE, offsetReg, offsetReg, -TARGET_POINTER_SIZE);
        // cmpdi cr0, offsetReg, 0
        GetEmitter()->emitIns_R_I(INS_cmpdi, EA_PTRSIZE, offsetReg, 0);
        // bne   cr0, .LOOP
        inst_JMP(EJ_ne, loop);

        gcInfo.gcMarkRegSetNpt(genRegMask(dstReg));
    }
}

//----------------------------------------------------------------------------------
// genCodeForInitBlkUnroll: Generate unrolled block initialization code.
//
// Arguments:
//    node - the GT_STORE_BLK node to generate code for
//
void CodeGen::genCodeForInitBlkUnroll(GenTreeBlk* node)
{
    assert(node->OperIs(GT_STORE_BLK));

    unsigned  dstLclNum      = BAD_VAR_NUM;
    regNumber dstAddrBaseReg = REG_NA;
    int       dstOffset      = 0;
    GenTree*  dstAddr        = node->Addr();

    if (!dstAddr->isContained())
    {
        dstAddrBaseReg = genConsumeReg(dstAddr);
    }
    else if (dstAddr->OperIsAddrMode())
    {
        assert(!dstAddr->AsAddrMode()->HasIndex());

        dstAddrBaseReg = genConsumeReg(dstAddr->AsAddrMode()->Base());
        dstOffset      = dstAddr->AsAddrMode()->Offset();
    }
    else
    {
        assert(dstAddr->OperIs(GT_LCL_ADDR));
        dstLclNum = dstAddr->AsLclVarCommon()->GetLclNum();
        dstOffset = dstAddr->AsLclVarCommon()->GetLclOffs();
    }

    GenTree* src = node->Data();

    if (src->OperIs(GT_INIT_VAL))
    {
        assert(src->isContained());
        src = src->gtGetOp1();
    }

    if (node->IsVolatile())
    {
        instGen_MemoryBarrier();
    }

    emitter* emit = GetEmitter();
    unsigned size = node->GetLayout()->GetSize();

    assert(size <= INT32_MAX);
    assert(dstOffset < INT32_MAX - static_cast<int>(size));

    regNumber srcReg;

    if (!src->isContained())
    {
        srcReg = genConsumeReg(src);
    }
    else
    {
        assert(src->IsIntegralConst(0));
        // On PPC64LE we can use R0 for zero
        srcReg = REG_R0;
    }

    // Unroll the init block using stores of decreasing size
    for (unsigned regSize = REGSIZE_BYTES; size > 0; size -= regSize, dstOffset += regSize)
    {
        while (regSize > size)
        {
            regSize /= 2;
        }

        instruction storeIns;
        emitAttr    attr;

        switch (regSize)
        {
            case 1:
                storeIns = INS_stb;
                attr     = EA_1BYTE;
                break;
            case 2:
                storeIns = INS_sth;
                attr     = EA_2BYTE;
                break;
            case 4:
                storeIns = INS_stw;
                attr     = EA_4BYTE;
                break;
            case 8:
                storeIns = INS_std;
                attr     = EA_8BYTE;
                break;
            default:
                unreached();
        }

        if (dstLclNum != BAD_VAR_NUM)
        {
            emit->emitIns_S_R(storeIns, attr, srcReg, dstLclNum, dstOffset);
        }
        else
        {
            emit->emitIns_R_R_I(storeIns, attr, srcReg, dstAddrBaseReg, dstOffset);
        }
    }
}

//------------------------------------------------------------------------
// instGen_MemoryBarrier: Generate a memory barrier instruction
//
// Arguments:
//   barrierKind - The kind of barrier to generate
//
void CodeGen::instGen_MemoryBarrier(BarrierKind barrierKind)
{
#ifdef DEBUG
    if (JitConfig.JitNoMemoryBarriers() == 1)
    {
        return;
    }
#endif // DEBUG

    // PPC64LE memory barriers:
    // Always use hwsync (heavy-weight sync) for full memory barrier
    // This ensures strongest ordering for all loads and stores
    
    GetEmitter()->emitIns(INS_hwsync);
}

//------------------------------------------------------------------------
// inst_SETCC: Generate code to set a register to 0 or 1 based on a condition.
//
// Arguments:
//   condition - The condition
//   type      - The type of the value to be produced
//   dstReg    - The destination register to be set to 1 or 0
//
void CodeGen::inst_SETCC(GenCondition condition, var_types type, regNumber dstReg)
{
    //_ASSERTE("!NYI");
    assert(varTypeIsIntegral(type));
    assert(genIsValidIntReg(dstReg));

    // PowerPC uses branchy pattern like ARM32:
    // Emit code like:
    //   bCC True      ; branch if condition is true
    //   li rD, 0      ; set register to 0 (false case)
    //   b Next        ; skip the true case
    // True:
    //   li rD, 1      ; set register to 1 (true case)
    // Next:
    //   ...

    BasicBlock* labelTrue = genCreateTempLabel();
    inst_JCC(condition, labelTrue);

    // False case: set register to 0
    GetEmitter()->emitIns_R_I(INS_li, emitActualTypeSize(type), dstReg, 0);

    BasicBlock* labelNext = genCreateTempLabel();
    GetEmitter()->emitIns_J(INS_b, labelNext);

    // True case: set register to 1
    genDefineTempLabel(labelTrue);
    GetEmitter()->emitIns_R_I(INS_li, emitActualTypeSize(type), dstReg, 1);

    genDefineTempLabel(labelNext);
}


//------------------------------------------------------------------------
// inst_JMP: Generate a jump instruction.
//
void CodeGen::inst_JMP(emitJumpKind jmp, BasicBlock* tgtBlock)
{
    assert(tgtBlock != nullptr);

    GetEmitter()->emitIns_J(emitter::emitJumpKindToIns(jmp), tgtBlock);
}



//------------------------------------------------------------------------
// genCodeForStoreBlk: Produce code for a GT_STORE_BLK node.
//
// Arguments:
//    tree - the node
//
void CodeGen::genCodeForStoreBlk(GenTreeBlk* blkOp)
{
    assert(blkOp->OperIs(GT_STORE_BLK));

    bool isCopyBlk = blkOp->OperIsCopyBlkOp();

    switch (blkOp->gtBlkOpKind)
    {
        case GenTreeBlk::BlkOpKindCpObjUnroll:
            assert(!blkOp->gtBlkOpGcUnsafe);
            genCodeForCpObj(blkOp->AsBlk());
            break;

        case GenTreeBlk::BlkOpKindLoop:
            assert(!isCopyBlk);
            genCodeForInitBlkLoop(blkOp);
            break;

        case GenTreeBlk::BlkOpKindUnroll:
            if (isCopyBlk)
            {
                if (blkOp->gtBlkOpGcUnsafe)
                {
                    GetEmitter()->emitDisableGC();
                }
                genCodeForCpBlkUnroll(blkOp);
                if (blkOp->gtBlkOpGcUnsafe)
                {
                    GetEmitter()->emitEnableGC();
                }
            }
            else
            {
                assert(!blkOp->gtBlkOpGcUnsafe);
                genCodeForInitBlkUnroll(blkOp);
            }
            break;

        case GenTreeBlk::BlkOpKindUnrollMemmove:
            // Memmove - not yet implemented for PPC64LE
            NYI_POWERPC64("genCodeForStoreBlk: BlkOpKindUnrollMemmove");
            break;

        default:
            unreached();
    }
}


//------------------------------------------------------------------------
// genCodeForLclFld: Produce code for a GT_LCL_FLD node.
//
// Arguments:
//    tree - the GT_LCL_FLD node
//
void CodeGen::genCodeForLclFld(GenTreeLclFld* tree)
{
    assert(tree->OperIs(GT_LCL_FLD));

    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->GetRegNum();
    emitter*  emit       = GetEmitter();

    NYI_IF(targetType == TYP_STRUCT, "GT_LCL_FLD: struct load local field not supported");
    assert(targetReg != REG_NA);

    unsigned offs   = tree->GetLclOffs();
    unsigned varNum = tree->GetLclNum();
    assert(varNum < compiler->lvaCount);

    // Determine the actual type to load based on the target register type
    // For HFA structs on PPC64LE, the tree type may be TYP_LONG but the register is a float register
    var_types loadType = targetType;
    
    // Check if this is an HFA struct with float register
    LclVarDsc* varDsc = compiler->lvaGetDesc(varNum);
    var_types hfaType = TYP_UNDEF;
    unsigned hfaSlots = 0;
    bool isHfaParam = false;
    
    if (genIsValidFloatReg(targetReg) && varTypeIsStruct(varDsc))
    {
        // Try to get class handle - first from lvClassHnd, then from GetLayout()
        CORINFO_CLASS_HANDLE classHnd = varDsc->lvClassHnd;
        if (classHnd == NO_CLASS_HANDLE)
        {
            ClassLayout* layout = varDsc->GetLayout();
            if (layout != nullptr)
            {
                classHnd = layout->GetClassHandle();
            }
        }
        
        if (classHnd != NO_CLASS_HANDLE && IsPpc64leHfaLikeStruct(compiler, classHnd, &hfaType, &hfaSlots))
        {
            // This is an HFA struct, use the HFA element type
            loadType = hfaType;
            isHfaParam = varDsc->lvIsParam;
            JITDUMP("[PPC64LE HFA DEBUG] genCodeForLclFld - HFA detected, overriding load type from %s to %s for V%02u+%u -> %s (hfaSlots=%u, lvIsParam=%d)\n",
                   varTypeName(targetType), varTypeName(loadType), varNum, offs, getRegName(targetReg), hfaSlots, varDsc->lvIsParam);
        }
    }

    emitAttr    attr = emitActualTypeSize(loadType);
    instruction ins  = ins_Load(loadType);
    
    // For split HFA parameters, check if this field is passed on the stack or in a register
    if (isHfaParam && varDsc->lvIsParam)
    {
        const ABIPassingInformation& abiInfo = compiler->lvaGetParameterABIInfo(varNum);
        unsigned fieldSize = (hfaType == TYP_FLOAT) ? 4 : 8;
        
        // Find which segment this field offset belongs to
        for (unsigned i = 0; i < abiInfo.NumSegments; i++)
        {
            const ABIPassingSegment& segment = abiInfo.Segment(i);
            if (segment.Offset == offs && segment.Size == fieldSize)
            {
                if (segment.IsPassedInRegister())
                {
                    // Field is passed in a register - LSRA has already assigned the correct register
                    // No load needed, just produce the register
                    JITDUMP("[PPC64LE HFA DEBUG] genCodeForLclFld - V%02u+%u already in register %s (passed in register)\n",
                           varNum, offs, getRegName(targetReg));
                    genProduceReg(tree);
                    return;
                }
                else
                {
                    // Field is on the incoming stack - load from caller's stack frame
                    int stackOffset = segment.GetStackOffset();
                    
                    // The incoming parameters are relative to the caller's SP (before our frame allocation)
                    // After frame allocation (stdu r1, -frameSize(r1)), our SP is moved down
                    // The incoming parameters are now at (current SP + frameSize + stackOffset)
                    int frameSize = genTotalFrameSize();
                    int adjustedOffset = stackOffset + frameSize;
                    
                    JITDUMP("[PPC64LE HFA DEBUG] genCodeForLclFld - Loading V%02u+%u from incoming stack: ABI offset=%d, adjusted offset=%d (frame=%d)\n",
                           varNum, offs, stackOffset, adjustedOffset, frameSize);
                    
                    // Load from incoming parameter area
                    emit->emitIns_R_AR(ins, attr, targetReg, REG_SPBASE, adjustedOffset);
                    genProduceReg(tree);
                    return;
                }
            }
        }
    }
    
    // Normal case: load from local variable's home location
    emit->emitIns_R_S(ins, attr, targetReg, varNum, offs);

    genProduceReg(tree);
}

//------------------------------------------------------------------------
// genCodeForIndexAddr: Produce code for a GT_INDEX_ADDR node.
//
// Arguments:
//    tree - the GT_INDEX_ADDR node
//
void CodeGen::genCodeForIndexAddr(GenTreeIndexAddr* node)
{
    GenTree* const base  = node->Arr();
    GenTree* const index = node->Index();

    genConsumeReg(base);
    genConsumeReg(index);

    // NOTE: `genConsumeReg` marks the consumed register as not a GC pointer, as it assumes that the input registers
    // die at the first instruction generated by the node. This is not the case for `INDEX_ADDR`, however, as the
    // base register is multiply-used. As such, we need to mark the base register as containing a GC pointer until
    // we are finished generating the code for this node.

    gcInfo.gcMarkRegPtrVal(base->GetRegNum(), base->TypeGet());
    assert(!varTypeIsGC(index->TypeGet()));

    // The index is never contained, even if it is a constant.
    assert(index->isUsedFromReg());

    regNumber baseReg  = base->GetRegNum();
    regNumber indexReg = index->GetRegNum();
    regNumber targetReg = node->GetRegNum();
    emitAttr  attr     = emitActualTypeSize(node);
    emitter*  emit     = GetEmitter();

    // Use R12 as temporary register for intermediate calculations
    // R12 is a volatile register safe to use as scratch
    const regNumber tmpReg = REG_R12;

    // Generate the bounds check if necessary.
    if (node->IsBoundsChecked())
    {
        // Load array length from the array header: tmpReg = [base + lenOffset]
        // lwz zero-extends the 32-bit length into a 64-bit register, keeping it unsigned.
        emit->emitIns_R_R_I(INS_lwz, EA_4BYTE, tmpReg, baseReg, node->gtLenOffset);

        // Array bounds checks are ALWAYS unsigned: the CLR defines them as
        //   (uint)index >= (uint)length
        // so we must use cmplw/cmpld (unsigned) rather than cmpw/cmpd (signed).
        //
        // Using signed cmpw here means that when index wraps negative — the most
        // common case being i = entry.next = -1 (end-of-chain sentinel in Dictionary)
        // — the signed comparison sees -1 < length and incorrectly passes the check,
        // causing the code to access memory before the array buffer.
        instruction cmpIns = (index->TypeGet() == TYP_LONG) ? INS_cmpld : INS_cmplw;
        emit->emitIns_R_R(cmpIns, emitActualTypeSize(index->TypeGet()), indexReg, tmpReg);

        // Branch if (uint)index >= (uint)length → range-check failure
        genJumpToThrowHlpBlk(EJ_ge, SCK_RNGCHK_FAIL, node->gtIndRngFailBB);
    }

    // Calculate: result = base + (index * elementSize) + elementOffset
    
    // Can we use a shift instruction for multiply?
    // PowerPC shift instructions can shift by 0-63 bits
    if (isPow2(node->gtElemSize) && (node->gtElemSize <= (1ULL << 63)))
    {
        DWORD scale;
        BitScanForward(&scale, node->gtElemSize);

        if (scale == 0)
        {
            // Element size is 1, no scaling needed
            // result = base + index
            emit->emitIns_R_R_R(INS_add, attr, targetReg, baseReg, indexReg);
        }
        else
        {
            // Shift index left by scale bits: tmpReg = index << scale
            // Use sldi (shift left doubleword immediate) or slwi (shift left word immediate)
            instruction shiftIns = (EA_SIZE(attr) == EA_8BYTE) ? INS_sldi : INS_slwi;
            emit->emitIns_R_R_I(shiftIns, attr, tmpReg, indexReg, scale);
            
            // Add base to scaled index: result = base + tmpReg
            emit->emitIns_R_R_R(INS_add, attr, targetReg, baseReg, tmpReg);
        }
    }
    else // Non-power-of-2 element size, use multiply
    {
        // Load element size into tmpReg
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, tmpReg, (ssize_t)node->gtElemSize);

        // Multiply: tmpReg = index * elementSize.
        // Use ppc64UseWideArith so the instruction is selected from the index
        // operand width rather than the node-result attr, matching the same
        // fix applied in genCodeForBinary.
        instruction mulIns = ppc64UseWideArith(index, index) ? INS_mulld : INS_mullw;
        emit->emitIns_R_R_R(mulIns, attr, tmpReg, indexReg, tmpReg);

        // Add base: result = base + tmpReg
        emit->emitIns_R_R_R(INS_add, attr, targetReg, baseReg, tmpReg);
    }

    // Add element offset if non-zero
    // result = result + elementOffset
    if (node->gtElemOffset != 0)
    {
        // Use addi (add immediate) for small offsets
        // PowerPC addi supports 16-bit signed immediate (-32768 to 32767)
        if (((int)node->gtElemOffset >= -32768) && ((int)node->gtElemOffset <= 32767))
        {
            emit->emitIns_R_R_I(INS_addi, attr, targetReg, targetReg, node->gtElemOffset);
        }
        else
        {
            // Large offset: load into tmpReg and add
            instGen_Set_Reg_To_Imm(EA_PTRSIZE, tmpReg, node->gtElemOffset);
            emit->emitIns_R_R_R(INS_add, attr, targetReg, targetReg, tmpReg);
        }
    }

    // Mark base register as no longer containing a GC pointer
    gcInfo.gcMarkRegSetNpt(base->gtGetRegMask());

    genProduceReg(node);
}

//------------------------------------------------------------------------
// genCall: Produce code for a GT_CALL node
//
void CodeGen::genCall(GenTreeCall* call)
{
    genCallPlaceRegArgs(call);

    // Insert a null check on "this" pointer if asked.
    if (call->NeedsNullCheck())
    {
        const regNumber regThis = genGetThisArgReg(call);

        // Load word from "this" pointer to trigger null check
        // Using lwz (load word and zero) with R0 as destination (discarded)
        GetEmitter()->emitIns_R_R_I(INS_lwz, EA_4BYTE, REG_R0, regThis, 0);
    }

    // If fast tail call, then we are done here, we just have to load the call
    // target into the right registers. We ensure in RA that target is loaded
    // into a volatile register that won't be restored by epilog sequence.
    if (call->IsFastTailCall())
    {
        GenTree* target = getCallTarget(call, nullptr);

        if (target != nullptr)
        {
            // Indirect fast tail calls materialize call target either in gtControlExpr or in gtCallAddr.
            genConsumeReg(target);
        }
#ifdef FEATURE_READYTORUN
        else if (call->IsR2ROrVirtualStubRelativeIndir())
        {
            assert((call->IsR2RRelativeIndir() && (call->gtEntryPoint.accessType == IAT_PVALUE)) ||
                   (call->IsVirtualStubRelativeIndir() && (call->gtEntryPoint.accessType == IAT_VALUE)));
            assert(call->gtControlExpr == nullptr);

            regNumber tmpReg = internalRegisters.GetSingle(call);
            // Register where we save call address in should not be overridden by epilog.
            // Note: PPC64LE doesn't have a dedicated link register constant like ARM's RBM_LR,
            // but the link register is implicitly used by branch-and-link instructions.
            assert((genRegMask(tmpReg) & RBM_INT_CALLEE_TRASH) == genRegMask(tmpReg));

            regNumber callAddrReg =
                call->IsVirtualStubRelativeIndir() ? compiler->virtualStubParamInfo->GetReg() : REG_R2R_INDIRECT_PARAM;
            GetEmitter()->emitIns_R_R_I(ins_Load(TYP_I_IMPL), emitActualTypeSize(TYP_I_IMPL), tmpReg, callAddrReg, 0);
            // We will use this again when emitting the jump in genCallInstruction in the epilog
            internalRegisters.Add(call, genRegMask(tmpReg));
        }
#endif

        return;
    }

    // For a pinvoke to unmanaged code we emit a label to clear
    // the GC pointer state before the callsite.
    // We can't utilize the typical lazy killing of GC pointers
    // at (or inside) the callsite.
    if (compiler->killGCRefs(call))
    {
        genDefineTempLabel(genCreateTempLabel());
    }

    genCallInstruction(call);

    genDefinePendingCallLabel(call);

#ifdef DEBUG
    // We should not have GC pointers in killed registers live around the call.
    // GC info for arg registers were cleared when consuming arg nodes above
    // and LSRA should ensure it for other trashed registers.
    regMaskTP killMask = RBM_CALLEE_TRASH;
    if (call->IsHelperCall())
    {
        CorInfoHelpFunc helpFunc = compiler->eeGetHelperNum(call->gtCallMethHnd);
        killMask                 = compiler->compHelperCallKillSet(helpFunc);
    }

    assert((gcInfo.gcRegGCrefSetCur & killMask) == 0);
    assert((gcInfo.gcRegByrefSetCur & killMask) == 0);
#endif // DEBUG

    var_types returnType = call->TypeGet();
    if (returnType != TYP_VOID)
    {
        regNumber returnReg;

        if (call->HasMultiRegRetVal())
        {
            const ReturnTypeDesc* pRetTypeDesc = call->GetReturnTypeDesc();
            assert(pRetTypeDesc != nullptr);
            unsigned regCount = pRetTypeDesc->GetReturnRegCount();

            // If regs allocated to call node are different from ABI return
            // regs in which the call has returned its result, move the result
            // to regs allocated to call node.
            for (unsigned i = 0; i < regCount; ++i)
            {
                var_types regType      = pRetTypeDesc->GetReturnRegType(i);
                returnReg              = pRetTypeDesc->GetABIReturnReg(i, call->GetUnmanagedCallConv());
                regNumber allocatedReg = call->GetRegNumByIdx(i);
                inst_Mov(regType, allocatedReg, returnReg, /* canSkip */ true);
            }
        }
        else
        {
            if (varTypeUsesFloatReg(returnType))
            {
                returnReg = REG_FLOATRET;
            }
            else
            {
                returnReg = REG_INTRET;
            }

            if (call->GetRegNum() != returnReg)
            {
                inst_Mov(returnType, call->GetRegNum(), returnReg, /* canSkip */ false);
            }
        }

        genProduceReg(call);
    }
}

//------------------------------------------------------------------------
// genCallInstruction - Generate instructions necessary to transfer control to the call.
//
// Arguments:
//    call - the GT_CALL node
//
// Remaks:
//   For tailcalls this function will generate a jump.
//
void CodeGen::genCallInstruction(GenTreeCall* call)
{
    // Determine return value size(s).
    const ReturnTypeDesc* pRetTypeDesc  = call->GetReturnTypeDesc();
    emitAttr              retSize       = EA_PTRSIZE;
    emitAttr              secondRetSize = EA_UNKNOWN;

    // unused values are of no interest to GC.
    if (!call->IsUnusedValue())
    {
        if (call->HasMultiRegRetVal())
        {
            retSize       = emitTypeSize(pRetTypeDesc->GetReturnRegType(0));
            secondRetSize = emitTypeSize(pRetTypeDesc->GetReturnRegType(1));
        }
        else
        {
            assert(call->gtType != TYP_STRUCT);

            if (call->gtType == TYP_REF)
            {
                retSize = EA_GCREF;
            }
            else if (call->gtType == TYP_BYREF)
            {
                retSize = EA_BYREF;
            }
        }
    }

    DebugInfo di;
    // We need to propagate the debug information to the call instruction, so we can emit
    // an IL to native mapping record for the call, to support managed return value debugging.
    // We don't want tail call helper calls that were converted from normal calls to get a record,
    // so we skip this hash table lookup logic in that case.
    if (compiler->opts.compDbgInfo && compiler->genCallSite2DebugInfoMap != nullptr && !call->IsTailCall())
    {
        (void)compiler->genCallSite2DebugInfoMap->Lookup(call, &di);
    }

    CORINFO_SIG_INFO* sigInfo = nullptr;
#ifdef DEBUG
    // Pass the call signature information down into the emitter so the emitter can associate
    // native call sites with the signatures they were generated from.
    if (!call->IsHelperCall())
    {
        sigInfo = call->callSig;
    }

    if (call->IsFastTailCall())
    {
        regMaskTP trashedByEpilog = RBM_CALLEE_SAVED;

        // The epilog may use and trash REG_GSCOOKIE_TMP_0/1. Make sure we have no
        // non-standard args that may be trash if this is a tailcall.
        if (compiler->getNeedsGSSecurityCookie())
        {
            trashedByEpilog |= genRegMask(REG_GSCOOKIE_TMP_0);
            trashedByEpilog |= genRegMask(REG_GSCOOKIE_TMP_1);
        }

        for (CallArg& arg : call->gtArgs.Args())
        {
            for (unsigned i = 0; i < arg.NewAbiInfo.NumSegments; i++)
            {
                const ABIPassingSegment& seg = arg.NewAbiInfo.Segment(i);
                if (seg.IsPassedInRegister() && ((trashedByEpilog & seg.GetRegisterMask()) != 0))
                {
                    JITDUMP("Tail call node:\n");
                    DISPTREE(call);
                    JITDUMP("Register used: %s\n", getRegName(seg.GetRegister()));
                    assert(!"Argument to tailcall may be trashed by epilog");
                }
            }
        }
    }
#endif // DEBUG
    CORINFO_METHOD_HANDLE methHnd;
    GenTree*              target = getCallTarget(call, &methHnd);

    if (target != nullptr)
    {
        // A call target can not be a contained indirection
        assert(!target->isContainedIndir());

        // For fast tailcall we have already consumed the target. We ensure in
        // RA that the target was allocated into a volatile register that will
        // not be messed up by epilog sequence.
        if (!call->IsFastTailCall())
        {
            genConsumeReg(target);
        }

        // We have already generated code for gtControlExpr evaluating it into a register.
        // Move target to R12 and call through CTR.
        //
        assert(genIsValidIntReg(target->GetRegNum()));

        regNumber targetReg = target->GetRegNum();
        
        // Move target address to R12 if it's not already there
        if (targetReg != REG_R12)
        {
            GetEmitter()->emitIns_Mov(INS_mov, EA_PTRSIZE, REG_R12, targetReg, /* canSkip */ false);
        }
        
        // Move R12 to CTR for indirect call
        GetEmitter()->emitIns_R(INS_mtctr, EA_PTRSIZE, REG_R12);

        // clang-format off
        genEmitCall(emitter::EC_INDIR_R,
                    methHnd,
                    INDEBUG_LDISASM_COMMA(sigInfo)
                    nullptr, // addr
                    retSize
                    MULTIREG_HAS_SECOND_GC_RET_ONLY_ARG(secondRetSize),
                    di,
                    REG_R12,  // Always use R12 for indirect calls
                    call->IsFastTailCall());
        // clang-format on

        // ELFv2 ABI: restore TOC pointer (r2) from our saved slot after any non-tail
        // inter-module call that may have clobbered r2.
        if (!call->IsFastTailCall())
        {
            GetEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_R2, REG_SPBASE, 24 /* R2_save_offset */);
        }
    }
    else
    {
        // If we have no target and this is a call with indirection cell then
        // we do an optimization where we load the call address directly from
        // the indirection cell instead of duplicating the tree. In BuildCall
        // we ensure that get an extra register for the purpose. Note that for
        // CFG the call might have changed to
        // CORINFO_HELP_DISPATCH_INDIRECT_CALL in which case we still have the
        // indirection cell but we should not try to optimize.
        regNumber callThroughIndirReg = REG_NA;
        if (!call->IsHelperCall(compiler, CORINFO_HELP_DISPATCH_INDIRECT_CALL))
        {
            callThroughIndirReg = getCallIndirectionCellReg(call);
        }

        if (callThroughIndirReg != REG_NA)
        {
            assert(call->IsR2ROrVirtualStubRelativeIndir());
            regNumber targetAddrReg;
            // For fast tailcalls we have already loaded the call target when processing the call node.
            if (!call->IsFastTailCall())
            {
                // For PPC64LE, allocate an internal register to load the target into.
                // Similar to ARM32 approach - we use an internal register for the load.
                targetAddrReg = internalRegisters.GetSingle(call);

                GetEmitter()->emitIns_R_R_I(ins_Load(TYP_I_IMPL), emitActualTypeSize(TYP_I_IMPL), targetAddrReg,
                                            callThroughIndirReg, 0);
            }
            else
            {
                targetAddrReg = internalRegisters.GetSingle(call);
                // Register where we save call address in should not be overridden by epilog.
                // PPC64LE uses link register implicitly for branch-and-link instructions.
                // Ensure the target register is in the callee-trash set (volatile registers).
                assert((genRegMask(targetAddrReg) & RBM_INT_CALLEE_TRASH) == genRegMask(targetAddrReg));
            }

            // We have now generated code loading the target address from the indirection cell into `targetAddrReg`.
            // Move to R12 and call through CTR.
            //
            assert(genIsValidIntReg(targetAddrReg));

            // Move target address to R12 if it's not already there
            if (targetAddrReg != REG_R12)
            {
                GetEmitter()->emitIns_Mov(INS_mov, EA_PTRSIZE, REG_R12, targetAddrReg, /* canSkip */ false);
            }
            
            // Move R12 to CTR for indirect call
            GetEmitter()->emitIns_R(INS_mtctr, EA_PTRSIZE, REG_R12);

            // clang-format off
            genEmitCall(emitter::EC_INDIR_R,
                        methHnd,
                        INDEBUG_LDISASM_COMMA(sigInfo)
                        nullptr, // addr
                        retSize
                        MULTIREG_HAS_SECOND_GC_RET_ONLY_ARG(secondRetSize),
                        di,
                        REG_R12,  // Always use R12 for indirect calls
                        call->IsFastTailCall());
            // clang-format on

            // ELFv2 ABI: restore TOC pointer (r2) from our saved slot after any non-tail
            // inter-module call that may have clobbered r2.
            if (!call->IsFastTailCall())
            {
                GetEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_R2, REG_SPBASE, 24 /* R2_save_offset */);
            }
        }
        else
        {
            // Generate a direct call to a non-virtual user defined or helper method
            assert(call->IsHelperCall() || (call->gtCallType == CT_USER_FUNC));

            void* addr = nullptr;
#ifdef FEATURE_READYTORUN
            if (call->gtEntryPoint.addr != NULL)
            {
                assert(call->gtEntryPoint.accessType == IAT_VALUE);
                addr = call->gtEntryPoint.addr;
            }
            else
#endif // FEATURE_READYTORUN
                if (call->IsHelperCall())
                {
                    CorInfoHelpFunc helperNum = compiler->eeGetHelperNum(methHnd);
                    noway_assert(helperNum != CORINFO_HELP_UNDEF);

                    void* pAddr = nullptr;
                    addr        = compiler->compGetHelperFtn(helperNum, (void**)&pAddr);
                    assert(pAddr == nullptr);
                }
                else
                {
                    // Direct call to a non-virtual user function.
                    addr = call->gtDirectCallAddress;
                }

            assert(addr != nullptr);

            // PowerPC64 bl has a 24-bit signed offset (±32MB range). Because the final
            // code layout is not known at instruction-selection time, we cannot reliably
            // determine whether a direct bl is reachable. Always emit the full 64-bit
            // address materialisation into r12 followed by an indirect call through CTR.
            // This is correct for all call distances.
            uint64_t targetAddr = (uint64_t)addr;

            // lis r12, target@highest  (bits 63:48)
            GetEmitter()->emitIns_R_I(INS_lis,  EA_8BYTE, REG_R12, (targetAddr >> 48) & 0xFFFF);
            // ori r12, r12, target@higher  (bits 47:32)
            GetEmitter()->emitIns_R_I(INS_ori,  EA_8BYTE, REG_R12, (targetAddr >> 32) & 0xFFFF);
            // sldi r12, r12, 32
            GetEmitter()->emitIns_R_I(INS_sldi, EA_8BYTE, REG_R12, 32);
            // oris r12, r12, target@h  (bits 31:16)
            GetEmitter()->emitIns_R_I(INS_oris, EA_8BYTE, REG_R12, (targetAddr >> 16) & 0xFFFF);
            // ori r12, r12, target@l  (bits 15:0)
            GetEmitter()->emitIns_R_I(INS_ori,  EA_8BYTE, REG_R12, targetAddr & 0xFFFF);
            // mtctr r12
            GetEmitter()->emitIns_R(INS_mtctr, EA_8BYTE, REG_R12);

            // Indirect call through CTR
            // clang-format off
            genEmitCall(emitter::EC_INDIR_R,
                        methHnd,
                        INDEBUG_LDISASM_COMMA(sigInfo)
                        nullptr,  // addr is nullptr for indirect calls
                        retSize
                        MULTIREG_HAS_SECOND_GC_RET_ONLY_ARG(secondRetSize),
                        di,
                        REG_R12,  // ireg - call through CTR
                        call->IsFastTailCall());
            // clang-format on

            // ELFv2 ABI: restore TOC pointer (r2) from our saved slot after any non-tail
            // inter-module call that may have clobbered r2.
            if (!call->IsFastTailCall())
            {
                GetEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_R2, REG_SPBASE, 24 /* R2_save_offset */);
            }
        }
    }
}

//------------------------------------------------------------------------
// genJmpPlaceVarArgs:
//   Generate code to place all varargs correctly for a JMP.
//
void CodeGen::genJmpPlaceVarArgs()
{
    //_ASSERTE("!NYI");
    abort();
}

//------------------------------------------------------------------------
// genGetVolatileLdStIns: Determine the most efficient instruction to perform a
//    volatile load or store and whether an explicit barrier is required or not.
//
// Arguments:
//    currentIns   - the current instruction to perform load/store
//    targetReg    - the target register
//    indir        - the indirection node representing the volatile load/store
//    needsBarrier - OUT parameter. Set to true if an explicit memory barrier is required.
//
// Return Value:
//    instruction to perform the volatile load/store with.
//
instruction CodeGen::genGetVolatileLdStIns(instruction   currentIns,
					regNumber     targetReg,
					GenTreeIndir* indir,
					bool*         needsBarrier)
{
    //_ASSERTE("!NYI");
    abort();
}

//------------------------------------------------------------------------
// genCodeForIndir: Produce code for a GT_IND node.
//
// Arguments:
//    tree - the GT_IND node
//
void CodeGen::genCodeForIndir(GenTreeIndir* tree)
{
    assert(tree->OperIs(GT_IND));

#ifdef FEATURE_SIMD
    if (tree->TypeGet() == TYP_SIMD12)
    {
	abort();
    }
#endif

    var_types   type      = tree->TypeGet();
    instruction ins       = ins_Load(type);
    regNumber   targetReg = tree->GetRegNum();

    genConsumeAddress(tree->Addr());

    if (tree->IsVolatile())
    {
	// Issue a full memory barrier before a volatile load
	// PowerPC64: hwsync provides a full memory barrier
	instGen(INS_hwsync);
    }

    GetEmitter()->emitInsLoadStoreOp(ins, emitActualTypeSize(type), targetReg, tree);

    genProduceReg(tree);
}

//------------------------------------------------------------------------
// genCodeForStoreInd: Produce code for a GT_STOREIND node.
//
// Arguments:
//    tree - the GT_STOREIND node
//
void CodeGen::genCodeForStoreInd(GenTreeStoreInd* tree)
{
#ifdef FEATURE_SIMD
    // Storing Vector3 of size 12 bytes through indirection
    if (tree->TypeGet() == TYP_SIMD12)
    {
        abort(); // NYI for PPC64LE
    }
#endif // FEATURE_SIMD

    GenTree* data = tree->Data();
    GenTree* addr = tree->Addr();

    // For now, we don't handle GC write barriers - just do normal stores
    // TODO: Implement GC write barrier support when genEmitHelperCall is ready
    GCInfo::WriteBarrierForm writeBarrierForm = gcInfo.gcIsWriteBarrierCandidate(tree);
    if (writeBarrierForm != GCInfo::WBF_NoBarrier)
    {
        // Write barrier needed but not yet implemented
        // For now, just do a normal store (this may cause GC issues in some scenarios)
        // TODO: Implement proper write barrier support
    }

    // Normal store path
    // We must consume the operands in the proper execution order,
    // so that liveness is updated appropriately.
    genConsumeAddress(addr);

    if (!data->isContained())
    {
        genConsumeRegs(data);
    }

    regNumber dataReg;
    if (data->isContainedIntOrIImmed())
    {
        assert(data->IsIntegralConst(0));
        dataReg = REG_R0; // Use R0 as zero register on PPC64LE
    }
    else // data is not contained, so evaluate it into a register
    {
        assert(!data->isContained());
        dataReg = data->GetRegNum();
    }

    var_types   type = tree->TypeGet();
    instruction ins  = ins_Store(type);

    if (tree->IsVolatile())
    {
	// Issue a full memory barrier before a volatile store
	// PowerPC64: hwsync provides a full memory barrier to ensure
	// all previous memory operations complete before the store
	instGen(INS_hwsync);
    }

    GetEmitter()->emitInsLoadStoreOp(ins, emitActualTypeSize(type), dataReg, tree);

    if (tree->IsVolatile())
    {
        // Issue a load barrier after a volatile store
        // lwsync is a lighter-weight sync that orders loads
        instGen(INS_lwsync);
    }
}

void CodeGen::genEHCatchRet(BasicBlock* block)
{
    // Load the address of the continuation point (target block) into the integer return register
    // This is used when returning from a catch handler
    // PowerPC64 uses a 3-instruction sequence: bcl + mflr + addi
    // to load the PC-relative address of the target label
    GetEmitter()->emitIns_R_L(INS_addi, EA_PTRSIZE, block->GetTarget(), REG_INTRET);
}


// The following classes
//   - InitBlockUnrollHelper
//   - CopyBlockUnrollHelper
// encapsulate algorithms that produce instruction sequences for inlined equivalents of memset() and memcpy() functions.
//
// Each class has a private template function that accepts an "InstructionStream" as a template class argument:
//   - InitBlockUnrollHelper::UnrollInitBlock<InstructionStream>(startDstOffset, byteCount, initValue)
//   - CopyBlockUnrollHelper::UnrollCopyBlock<InstructionStream>(startSrcOffset, startDstOffset, byteCount)
//
// The design goal is to separate optimization approaches implemented by the algorithms
// from the target platform specific details.
//
// InstructionStream is a "stream" of load/store instructions (i.e. ldr/ldp/str/stp) that represents an instruction
// sequence that will initialize a memory region with some value or copy values from one memory region to another.
//
// As far as UnrollInitBlock and UnrollCopyBlock concerned, InstructionStream implements the following class member
// functions:
//   - LoadPairRegs(offset, regSizeBytes)
//   - StorePairRegs(offset, regSizeBytes)
//   - LoadReg(offset, regSizeBytes)
//   - StoreReg(offset, regSizeBytes)
//
// There are three implementations of InstructionStream:
//   - CountingStream that counts how many instructions were pushed out of the stream
//   - VerifyingStream that validates that all the instructions in the stream are encodable on Arm64
//   - ProducingStream that maps the function to corresponding emitter functions
//
// The idea behind the design is that decision regarding what instruction sequence to emit
// (scalar instructions vs. SIMD instructions) is made by execution an algorithm producing an instruction sequence
// while counting the number of produced instructions and verifying that all the instructions are encodable.
//
// For example, using SIMD instructions might produce a shorter sequence but require "spilling" a value of a starting
// address
// to an integer register (due to stricter offset alignment rules for 16-byte wide SIMD instructions).
// This the CodeGen can take this fact into account before emitting an instruction sequence.
//
// Alternative design might have had VerifyingStream and ProducingStream fused into one class
// that would allow to undo an instruction if the sequence is not fully encodable.

#if 0
class CountingStream
{
public:
    CountingStream()
    {
	instrCount = 0;
    }

    void LoadPairRegs(int offset, unsigned regSizeBytes)
    {
	instrCount++;
    }

    void StorePairRegs(int offset, unsigned regSizeBytes)
    {
	instrCount++;
    }

    void LoadReg(int offset, unsigned regSizeBytes)
    {
	instrCount++;
    }

    void StoreReg(int offset, unsigned regSizeBytes)
    {
	instrCount++;
    }

    unsigned InstructionCount() const
    {
	return instrCount;
    }

private:
    unsigned instrCount;
};

class VerifyingStream
{
public:
    VerifyingStream()
    {
	canEncodeAllLoads  = true;
	canEncodeAllStores = true;
    }

    void LoadPairRegs(int offset, unsigned regSizeBytes)
    {
	canEncodeAllLoads = canEncodeAllLoads && emitter::canEncodeLoadOrStorePairOffset(offset, EA_SIZE(regSizeBytes));
    }

    void StorePairRegs(int offset, unsigned regSizeBytes)
    {
	canEncodeAllStores =
	canEncodeAllStores && emitter::canEncodeLoadOrStorePairOffset(offset, EA_SIZE(regSizeBytes));
    }

    void LoadReg(int offset, unsigned regSizeBytes)
    {
	canEncodeAllLoads =
	canEncodeAllLoads && emitter::emitIns_valid_imm_for_ldst_offset(offset, EA_SIZE(regSizeBytes));
    }

    void StoreReg(int offset, unsigned regSizeBytes)
    {
	canEncodeAllStores =
	canEncodeAllStores && emitter::emitIns_valid_imm_for_ldst_offset(offset, EA_SIZE(regSizeBytes));
    }

    bool CanEncodeAllLoads() const
    {
	return canEncodeAllLoads;
    }

    bool CanEncodeAllStores() const
    {
	return canEncodeAllStores;
    }

private:
    bool canEncodeAllLoads;
    bool canEncodeAllStores;
};

#endif

//------------------------------------------------------------------------
// genCodeForCpObj: Generate code for CpObj nodes to copy structs that have interleaved
//                  GC pointers. This will generate a sequence of loads/stores for each slot.
//                  For non-GC slots, we will use ld/std instructions. For GC slots, we will
//                  call the CORINFO_HELP_ASSIGN_BYREF helper.
//
// Arguments:
//    cpObjNode - the GT_STORE_BLK node with GC pointers
//
// Notes:
//    Based on ARM64 implementation, adapted for PPC64LE instructions.
//
void CodeGen::genCodeForCpObj(GenTreeBlk* cpObjNode)
{
    GenTree*  dstAddr       = cpObjNode->Addr();
    GenTree*  source        = cpObjNode->Data();
    var_types srcAddrType   = TYP_BYREF;
    bool      sourceIsLocal = false;

    assert(source->isContained());
    if (source->gtOper == GT_IND)
    {
        GenTree* srcAddr = source->gtGetOp1();
        assert(!srcAddr->isContained());
        srcAddrType = srcAddr->TypeGet();
    }
    else
    {
        noway_assert(source->IsLocal());
        sourceIsLocal = true;
    }

    bool dstOnStack =
        dstAddr->gtSkipReloadOrCopy()->OperIs(GT_LCL_ADDR) || cpObjNode->GetLayout()->IsStackOnly(compiler);

#ifdef DEBUG
    assert(!dstAddr->isContained());

    // This GenTree node has data about GC pointers, this means we're dealing
    // with CpObj.
    assert(cpObjNode->GetLayout()->HasGCPtr());
#endif // DEBUG

    // Consume the operands and get them into the right registers.
    // They may now contain gc pointers (depending on their type; gcMarkRegPtrVal will "do the right thing").
    genConsumeBlockOp(cpObjNode, REG_WRITE_BARRIER_DST_BYREF, REG_WRITE_BARRIER_SRC_BYREF, REG_NA);
    gcInfo.gcMarkRegPtrVal(REG_WRITE_BARRIER_SRC_BYREF, srcAddrType);
    gcInfo.gcMarkRegPtrVal(REG_WRITE_BARRIER_DST_BYREF, dstAddr->TypeGet());

    ClassLayout* layout = cpObjNode->GetLayout();
    unsigned     slots  = layout->GetSlotCount();

    // Temp register(s) used to perform the sequence of loads and stores.
    regNumber tmpReg  = internalRegisters.Extract(cpObjNode, RBM_ALLINT);
    regNumber tmpReg2 = REG_NA;

    assert(genIsValidIntReg(tmpReg));
    assert(tmpReg != REG_WRITE_BARRIER_SRC_BYREF);
    assert(tmpReg != REG_WRITE_BARRIER_DST_BYREF);

    if (slots > 1)
    {
        tmpReg2 = internalRegisters.Extract(cpObjNode, RBM_ALLINT);
        assert(tmpReg2 != tmpReg);
        assert(genIsValidIntReg(tmpReg2));
        assert(tmpReg2 != REG_WRITE_BARRIER_DST_BYREF);
        assert(tmpReg2 != REG_WRITE_BARRIER_SRC_BYREF);
    }

    if (cpObjNode->IsVolatile())
    {
        // issue a full memory barrier before a volatile CpObj operation
        instGen_MemoryBarrier();
    }

    emitter* emit = GetEmitter();

    // If we can prove it's on the stack we don't need to use the write barrier.
    if (dstOnStack)
    {
        unsigned i = 0;
        // Check if two or more remaining slots and use two ld/std pairs
        while (i < slots - 1)
        {
            emitAttr attr0 = emitTypeSize(layout->GetGCPtrType(i + 0));
            emitAttr attr1 = emitTypeSize(layout->GetGCPtrType(i + 1));

            // Load two slots
            emit->emitIns_R_R_I(INS_ld, attr0, tmpReg, REG_WRITE_BARRIER_SRC_BYREF, 0);
            emit->emitIns_R_R_I(INS_ld, attr1, tmpReg2, REG_WRITE_BARRIER_SRC_BYREF, TARGET_POINTER_SIZE);
            // Advance source pointer
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, REG_WRITE_BARRIER_SRC_BYREF, REG_WRITE_BARRIER_SRC_BYREF,
                                2 * TARGET_POINTER_SIZE);
            // Store two slots
            emit->emitIns_R_R_I(INS_std, attr0, tmpReg, REG_WRITE_BARRIER_DST_BYREF, 0);
            emit->emitIns_R_R_I(INS_std, attr1, tmpReg2, REG_WRITE_BARRIER_DST_BYREF, TARGET_POINTER_SIZE);
            // Advance destination pointer
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, REG_WRITE_BARRIER_DST_BYREF, REG_WRITE_BARRIER_DST_BYREF,
                                2 * TARGET_POINTER_SIZE);
            i += 2;
        }

        // Use a ld/std pair for the last remainder
        if (i < slots)
        {
            emitAttr attr0 = emitTypeSize(layout->GetGCPtrType(i + 0));

            emit->emitIns_R_R_I(INS_ld, attr0, tmpReg, REG_WRITE_BARRIER_SRC_BYREF, 0);
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, REG_WRITE_BARRIER_SRC_BYREF, REG_WRITE_BARRIER_SRC_BYREF,
                                TARGET_POINTER_SIZE);
            emit->emitIns_R_R_I(INS_std, attr0, tmpReg, REG_WRITE_BARRIER_DST_BYREF, 0);
            emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, REG_WRITE_BARRIER_DST_BYREF, REG_WRITE_BARRIER_DST_BYREF,
                                TARGET_POINTER_SIZE);
        }
    }
    else
    {
        unsigned gcPtrCount = cpObjNode->GetLayout()->GetGCPtrCount();

        unsigned i = 0;
        while (i < slots)
        {
            if (!layout->IsGCPtr(i))
            {
                // How many continuous non-gc slots do we have?
                unsigned nonGcSlots = 0;
                do
                {
                    nonGcSlots++;
                    i++;
                } while ((i < slots) && !layout->IsGCPtr(i));

                const regNumber srcReg = REG_WRITE_BARRIER_SRC_BYREF;
                const regNumber dstReg = REG_WRITE_BARRIER_DST_BYREF;
                while (nonGcSlots > 0)
                {
                    // Copy at least two slots at a time
                    if (nonGcSlots >= 2)
                    {
                        nonGcSlots -= 2;
                        emit->emitIns_R_R_I(INS_ld, EA_8BYTE, tmpReg, srcReg, 0);
                        emit->emitIns_R_R_I(INS_ld, EA_8BYTE, tmpReg2, srcReg, TARGET_POINTER_SIZE);
                        emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, srcReg, srcReg, 2 * TARGET_POINTER_SIZE);
                        emit->emitIns_R_R_I(INS_std, EA_8BYTE, tmpReg, dstReg, 0);
                        emit->emitIns_R_R_I(INS_std, EA_8BYTE, tmpReg2, dstReg, TARGET_POINTER_SIZE);
                        emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, dstReg, dstReg, 2 * TARGET_POINTER_SIZE);
                    }
                    else
                    {
                        nonGcSlots--;
                        emit->emitIns_R_R_I(INS_ld, EA_8BYTE, tmpReg, srcReg, 0);
                        emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, srcReg, srcReg, TARGET_POINTER_SIZE);
                        emit->emitIns_R_R_I(INS_std, EA_8BYTE, tmpReg, dstReg, 0);
                        emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, dstReg, dstReg, TARGET_POINTER_SIZE);
                    }
                }
            }
            else
            {
                // In the case of a GC-Pointer we'll call the ByRef write barrier helper
                genEmitHelperCall(CORINFO_HELP_ASSIGN_BYREF, 0, EA_PTRSIZE);
                gcPtrCount--;
                i++;
            }
        }
        assert(gcPtrCount == 0);
    }

    if (cpObjNode->IsVolatile())
    {
        // issue a load barrier after a volatile CpObj operation
        instGen_MemoryBarrier(BARRIER_LOAD_ONLY);
    }

    // Clear the gcInfo for REG_WRITE_BARRIER_SRC_BYREF and REG_WRITE_BARRIER_DST_BYREF.
    // While we normally update GC info prior to the last instruction that uses them,
    // these actually live into the helper call.
    gcInfo.gcMarkRegSetNpt(RBM_WRITE_BARRIER_SRC_BYREF | RBM_WRITE_BARRIER_DST_BYREF);
}

//----------------------------------------------------------------------------------
// genCodeForCpBlkUnroll: Generate unrolled block copy code.
//
// Arguments:
//    node - the GT_STORE_BLK node to generate code for
//
void CodeGen::genCodeForCpBlkUnroll(GenTreeBlk* node)
{
    assert(node->OperIs(GT_STORE_BLK));

    unsigned  dstLclNum      = BAD_VAR_NUM;
    regNumber dstAddrBaseReg = REG_NA;
    int       dstOffset      = 0;
    GenTree*  dstAddr        = node->Addr();

    if (!dstAddr->isContained())
    {
        dstAddrBaseReg = genConsumeReg(dstAddr);
    }
    else if (dstAddr->OperIsAddrMode())
    {
        assert(!dstAddr->AsAddrMode()->HasIndex());

        dstAddrBaseReg = genConsumeReg(dstAddr->AsAddrMode()->Base());
        dstOffset      = dstAddr->AsAddrMode()->Offset();
    }
    else
    {
        assert(dstAddr->OperIs(GT_LCL_ADDR));
        dstLclNum = dstAddr->AsLclVarCommon()->GetLclNum();
        dstOffset = dstAddr->AsLclVarCommon()->GetLclOffs();
    }

    unsigned  srcLclNum      = BAD_VAR_NUM;
    regNumber srcAddrBaseReg = REG_NA;
    int       srcOffset      = 0;
    GenTree*  src            = node->Data();

    assert(src->isContained());

    if (src->OperIs(GT_LCL_VAR, GT_LCL_FLD))
    {
        srcLclNum = src->AsLclVarCommon()->GetLclNum();
        srcOffset = src->AsLclVarCommon()->GetLclOffs();
    }
    else
    {
        assert(src->OperIs(GT_IND));
        GenTree* srcAddr = src->AsIndir()->Addr();

        if (!srcAddr->isContained())
        {
            srcAddrBaseReg = genConsumeReg(srcAddr);
        }
        else if (srcAddr->OperIsAddrMode())
        {
            srcAddrBaseReg = genConsumeReg(srcAddr->AsAddrMode()->Base());
            srcOffset      = srcAddr->AsAddrMode()->Offset();
        }
        else
        {
            assert(srcAddr->OperIs(GT_LCL_ADDR));
            srcLclNum = srcAddr->AsLclVarCommon()->GetLclNum();
            srcOffset = srcAddr->AsLclVarCommon()->GetLclOffs();
        }
    }

    if (node->IsVolatile())
    {
        // issue a full memory barrier before a volatile CpBlk operation
        instGen_MemoryBarrier();
    }

    emitter* emit = GetEmitter();
    unsigned size = node->GetLayout()->GetSize();

    assert(size <= INT32_MAX);
    assert(srcOffset < INT32_MAX - static_cast<int>(size));
    assert(dstOffset < INT32_MAX - static_cast<int>(size));

    const regNumber tempReg = internalRegisters.Extract(node, RBM_ALLINT);

    for (unsigned regSize = REGSIZE_BYTES; size > 0; size -= regSize, srcOffset += regSize, dstOffset += regSize)
    {
        while (regSize > size)
        {
            regSize /= 2;
        }

        instruction loadIns;
        instruction storeIns;
        emitAttr    attr;

        switch (regSize)
        {
            case 1:
                loadIns  = INS_lbz;
                storeIns = INS_stb;
                attr     = EA_1BYTE;
                break;
            case 2:
                loadIns  = INS_lhz;
                storeIns = INS_sth;
                attr     = EA_2BYTE;
                break;
            case 4:
                loadIns  = INS_lwz;
                storeIns = INS_stw;
                attr     = EA_4BYTE;
                break;
            case 8:
                loadIns  = INS_ld;
                storeIns = INS_std;
                attr     = EA_8BYTE;
                break;
            default:
                unreached();
        }

        if (srcLclNum != BAD_VAR_NUM)
        {
            emit->emitIns_R_S(loadIns, attr, tempReg, srcLclNum, srcOffset);
        }
        else
        {
            emit->emitIns_R_R_I(loadIns, attr, tempReg, srcAddrBaseReg, srcOffset);
        }

        if (dstLclNum != BAD_VAR_NUM)
        {
            emit->emitIns_S_R(storeIns, attr, tempReg, dstLclNum, dstOffset);
        }
        else
        {
            emit->emitIns_R_R_I(storeIns, attr, tempReg, dstAddrBaseReg, dstOffset);
        }
    }

    if (node->IsVolatile())
    {
        // issue a full memory barrier after a volatile CpBlk operation
        instGen_MemoryBarrier();
    }
}


//------------------------------------------------------------------------
// genCodeForMemmove: Perform an unrolled memmove. The idea that we can
//    ignore the fact that src and dst might overlap if we save the whole
//    src to temp regs in advance, e.g. for memmove(dst: x1, src: x0, len: 30):
//
//       ldr   q16, [x0]
//       ldr   q17, [x0, #0x0E]
//       str   q16, [x1]
//       str   q17, [x1, #0x0E]
//
// Arguments:
//    tree - GenTreeBlk node
//
void CodeGen::genCodeForMemmove(GenTreeBlk* tree)
{
    //_ASSERTE("!NYI");
    abort();
}
    

// clang-format off
const CodeGen::GenConditionDesc CodeGen::GenConditionDesc::map[32]
{
    { },       // NONE  (index 0)
    { },       // 1     (index 1)
    { EJ_lt }, // SLT   (index 2) - Signed Less Than
    { EJ_le }, // SLE   (index 3) - Signed Less or Equal
    { EJ_ge }, // SGE   (index 4) - Signed Greater or Equal
    { EJ_gt }, // SGT   (index 5) - Signed Greater Than
    { },       // S     (index 6) - Sign bit set (not used on PPC)
    { },       // NS    (index 7) - Sign bit not set (not used on PPC)

    { EJ_eq }, // EQ    (index 8) - Equal
    { EJ_ne }, // NE    (index 9) - Not Equal ← YOUR TEST USES THIS!
    { EJ_lt }, // ULT   (index 10) - Unsigned Less Than
    { EJ_le }, // ULE   (index 11) - Unsigned Less or Equal
    { EJ_ge }, // UGE   (index 12) - Unsigned Greater or Equal
    { EJ_gt }, // UGT   (index 13) - Unsigned Greater Than
    { },       // C     (index 14) - Carry (not used on PPC)
    { },       // NC    (index 15) - No Carry (not used on PPC)

    { EJ_eq }, // FEQ   (index 16) - Float Equal
    { EJ_ne }, // FNE   (index 17) - Float Not Equal
    { EJ_lt }, // FLT   (index 18) - Float Less Than
    { EJ_le }, // FLE   (index 19) - Float Less or Equal
    { EJ_ge }, // FGE   (index 20) - Float Greater or Equal
    { EJ_gt }, // FGT   (index 21) - Float Greater Than
    { },       // O     (index 22) - Overflow (not used on PPC)
    { },       // NO    (index 23) - No Overflow (not used on PPC)

    { EJ_eq, GT_OR, EJ_eq },  // FEQU  (index 24) - Float Equal Unordered
    { EJ_ne },                // FNEU  (index 25) - Float Not Equal Unordered
    { EJ_lt },                // FLTU  (index 26) - Float Less Than Unordered
    { EJ_le },                // FLEU  (index 27) - Float Less or Equal Unordered
    { EJ_ge },                // FGEU  (index 28) - Float Greater or Equal Unordered
    { EJ_gt },                // FGTU  (index 29) - Float Greater Than Unordered
    { },       // P     (index 30) - Parity (not used on PPC)
    { },       // NP    (index 31) - No Parity (not used on PPC)
};
// clang-format on

/*****************************************************************************
 *
 *  Generates code for a function epilog.
 *
 *  Please consult the "debugger team notification" comment in genFnProlog().
 */

void CodeGen::genFnEpilog(BasicBlock* block)
{
    assert(block != nullptr);

    ScopedSetVariable<bool> _setGeneratingEpilog(&compiler->compGeneratingEpilog, true);

    regMaskTP regsToRestoreMask = regSet.rsGetModifiedCalleeSavedRegsMask();

    int totalFrameSize = genTotalFrameSize();

    // Must exactly match the localFrameSize calculation in genPushCalleeSavedRegisters.
    const int LINKAGE_AREA_SIZE    = 32;
    const int PARAM_SAVE_AREA_SIZE = 64;
    int paramSaveArea = PARAM_SAVE_AREA_SIZE;
    if (compiler->info.compArgsCount > 0 && compiler->compArgSize > PARAM_SAVE_AREA_SIZE)
    {
        paramSaveArea = compiler->compArgSize;
    }
    int localFrameSize = LINKAGE_AREA_SIZE + paramSaveArea + compiler->compLclFrameSize;

    if (compiler->lvaPSPSym != BAD_VAR_NUM)
    {
        localFrameSize -= TARGET_POINTER_SIZE;
    }

    if ((compiler->lvaMonAcquired != BAD_VAR_NUM) && !compiler->opts.IsOSR())
    {
        localFrameSize -= TARGET_POINTER_SIZE;
    }

    constexpr int FP_backchain_save_offset = -8;
    constexpr int LR_save_offset = 16;
    constexpr int R2_save_offset = 24;

    emitter* emit = GetEmitter();
    int      offset;

    regMaskTP maskRestoreRegsFloat = regsToRestoreMask & RBM_ALLFLOAT;
    regMaskTP maskRestoreRegsInt   = regsToRestoreMask & RBM_INT_CALLEE_SAVED;

    offset = localFrameSize;
    for (int regNum = REG_R14; regNum <= REG_R31; regNum++)
    {
        regNumber reg     = (regNumber)regNum;
        regMaskTP regMask = genRegMask(reg);

        if ((maskRestoreRegsInt & regMask) != RBM_NONE)
        {
            offset += REGSIZE_BYTES;
        }
    }

    compiler->unwindBegEpilog();

    for (int regNum = REG_F31; regNum >= REG_F14; regNum--)
    {
        regNumber reg     = (regNumber)regNum;
        regMaskTP regMask = genRegMask(reg);

        if ((maskRestoreRegsFloat & regMask) != RBM_NONE)
        {
            offset -= REGSIZE_BYTES;
            emit->emitIns_R_R_I(INS_lfd, EA_8BYTE, reg, REG_SPBASE, offset);
            compiler->unwindSaveReg(reg, offset);
        }
    }

    for (int regNum = REG_R31; regNum >= REG_R14; regNum--)
    {
        regNumber reg     = (regNumber)regNum;
        regMaskTP regMask = genRegMask(reg);

        if ((maskRestoreRegsInt & regMask) != RBM_NONE)
        {
            offset -= REGSIZE_BYTES;
            emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, reg, REG_SPBASE, offset);
            compiler->unwindSaveReg(reg, offset);
        }
    }

    // Restore r31 and r1. For an ordinary frame, restore r31 while r1 still
    // denotes the fixed callee SP, so its unwind offset is non-negative.
    if (compiler->compLocallocUsed)
    {
        // ld r1, 0(r1): r1 = caller_SP (ELFv2 backchain written by genLclHeap at 0(new_r1))
        // The backchain word holds the value of caller_SP, so a single load is sufficient.
        emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, 0);
        compiler->unwindAllocStack(totalFrameSize);

        // Localloc unwind semantics are handled separately. Preserve the
        // existing machine-code restore from caller_SP - 8 for now.
        emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_FP, REG_SPBASE, FP_backchain_save_offset);
        compiler->unwindSaveReg(REG_FP, FP_backchain_save_offset);
    }
    else
    {
        const int FP_save_offset = totalFrameSize + FP_backchain_save_offset;
        assert(FP_save_offset >= 0);

        emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_FP, REG_SPBASE, FP_save_offset);
        compiler->unwindSaveReg(REG_FP, FP_save_offset);

        emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, totalFrameSize);
        compiler->unwindAllocStack(totalFrameSize);
    }

    // r1 is now caller_SP.
    emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_R0, REG_SPBASE, LR_save_offset);
    compiler->unwindSaveReg(REG_R0, LR_save_offset);

    emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_R2, REG_SPBASE, R2_save_offset);
    compiler->unwindSaveReg(REG_R2, R2_save_offset);

    emit->emitIns_R(INS_mtlr, EA_PTRSIZE, REG_R0);
    emit->emitIns(INS_blr);
    compiler->unwindReturn(REG_R0);

    compiler->unwindEndEpilog();
}

//------------------------------------------------------------------------
// genPushCalleeSavedRegisters: Push any callee-saved registers we have used.
//
// Arguments (arm64):
//    initReg        - A scratch register (that gets set to zero on some platforms).
//    pInitRegZeroed - OUT parameter. *pInitRegZeroed is set to 'true' if this method sets initReg register to zero,
//                     'false' if initReg was set to a non-zero value, and left unchanged if initReg was not touched.
//
void CodeGen::genPushCalleeSavedRegisters()
{
    assert(compiler->compGeneratingProlog);

    regMaskTP rsPushRegs = regSet.rsGetModifiedCalleeSavedRegsMask();

#if ETW_EBP_FRAMED
    if (!isFramePointerUsed() && regSet.rsRegsModified(RBM_FPBASE))
    {
        noway_assert(!"Used register RBM_FPBASE as a scratch register!");
    }
#endif

    // PPC64LE currently always uses the frame pointer in the same style as the
    // simpler fixed-frame LoongArch64/RISC-V64 implementations.
    assert(isFramePointerUsed());

    regSet.rsMaskCalleeSaved = rsPushRegs;

#ifdef DEBUG
    JITDUMP("Frame info. #outsz=%d; #framesz=%d; LclFrameSize=%d;\n", unsigned(compiler->lvaOutgoingArgSpaceSize),
            genTotalFrameSize(), compiler->compLclFrameSize);

    if (compiler->compCalleeRegsPushed != genCountBits(regSet.rsMaskCalleeSaved))
    {
        printf("Error: unexpected number of callee-saved registers to push. Expected: %d. Got: %d ",
               compiler->compCalleeRegsPushed, genCountBits(rsPushRegs));
        dspRegMask(rsPushRegs);
        printf("\n");
        assert(compiler->compCalleeRegsPushed == genCountBits(rsPushRegs | RBM_FPBASE));
    }

    if (verbose)
    {
        regMaskTP maskSaveRegsFloat = rsPushRegs & RBM_ALLFLOAT;
        regMaskTP maskSaveRegsInt   = rsPushRegs & ~maskSaveRegsFloat;
        printf("Save float regs: ");
        dspRegMask(maskSaveRegsFloat);
        printf("\n");
        printf("Save int   regs: ");
        dspRegMask(maskSaveRegsInt);
        printf("\n");
    }
#endif // DEBUG

    int totalFrameSize = genTotalFrameSize();
    
    // Calculate localFrameSize using same logic as genTotalFrameSize()
    // PPC64LE ELFv2 ABI: 32 (linkage) + parameter save area + compLclFrameSize
    const int LINKAGE_AREA_SIZE = 32;
    const int PARAM_SAVE_AREA_SIZE = 64;
    
    int paramSaveArea = PARAM_SAVE_AREA_SIZE;
    if (compiler->info.compArgsCount > 0 && compiler->compArgSize > PARAM_SAVE_AREA_SIZE)
    {
        paramSaveArea = compiler->compArgSize;
    }
    
    int localFrameSize = LINKAGE_AREA_SIZE + paramSaveArea + compiler->compLclFrameSize;

    if (compiler->lvaPSPSym != BAD_VAR_NUM)
    {
        localFrameSize -= TARGET_POINTER_SIZE;
    }

    if ((compiler->lvaMonAcquired != BAD_VAR_NUM) && !compiler->opts.IsOSR())
    {
        localFrameSize -= TARGET_POINTER_SIZE;
    }

#ifdef DEBUG
    if (compiler->opts.disAsm)
    {
        printf("Frame info. #outsz=%d; #framesz=%d; lcl=%d\n", unsigned(compiler->lvaOutgoingArgSpaceSize),
               genTotalFrameSize(), localFrameSize);
    }
#endif

    constexpr int FP_backchain_save_offset = -8;
    constexpr int LR_save_offset           = 16;
    constexpr int R2_save_offset           = 24;

    GetEmitter()->emitIns_R_R_I(INS_std, EA_PTRSIZE, REG_R2, REG_SPBASE, R2_save_offset);
    compiler->unwindSaveReg(REG_R2, R2_save_offset);

    GetEmitter()->emitIns_R(INS_mflr, EA_PTRSIZE, REG_R0);
    GetEmitter()->emitIns_R_R_I(INS_std, EA_PTRSIZE, REG_R0, REG_SPBASE, LR_save_offset);
    compiler->unwindSaveReg(REG_R0, LR_save_offset);

    // Save the ABI linkage-area entries first, then allocate the full frame.
    // Save the incoming r31 relative to the established callee SP before
    // overwriting r31 with the current frame pointer. Then save the remaining
    // modified callee-saved registers in ascending register order.
    GetEmitter()->emitIns_R_R_I(INS_stdu, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, -totalFrameSize);
    compiler->unwindAllocStack(totalFrameSize);

    // Save the incoming r31 at caller_SP - 8, expressed relative to the
    // established callee SP so the unwind offset is non-negative.
    const int FP_save_offset = totalFrameSize + FP_backchain_save_offset;
    assert(FP_save_offset >= 0);
    GetEmitter()->emitIns_R_R_I(INS_std, EA_PTRSIZE, REG_FP, REG_SPBASE, FP_save_offset);
    compiler->unwindSaveReg(REG_FP, FP_save_offset);

    GetEmitter()->emitIns_Mov(INS_mov, EA_PTRSIZE, REG_FP, REG_SPBASE, /* canSkip */ false);

    int offset = localFrameSize;

    regMaskTP maskSaveRegsFloat = rsPushRegs & RBM_ALLFLOAT;
    regMaskTP maskSaveRegsInt   = rsPushRegs & RBM_INT_CALLEE_SAVED;

    for (int regNum = REG_R14; regNum <= REG_R31; regNum++)
    {
        regNumber reg     = (regNumber)regNum;
        regMaskTP regMask = genRegMask(reg);

        if ((maskSaveRegsInt & regMask) != RBM_NONE)
        {
            GetEmitter()->emitIns_R_R_I(INS_std, EA_PTRSIZE, reg, REG_SPBASE, offset);
            compiler->unwindSaveReg(reg, offset);
            offset += REGSIZE_BYTES;
        }
    }

    for (int regNum = REG_F14; regNum <= REG_F31; regNum++)
    {
        regNumber reg     = (regNumber)regNum;
        regMaskTP regMask = genRegMask(reg);

        if ((maskSaveRegsFloat & regMask) != RBM_NONE)
        {
            GetEmitter()->emitIns_R_R_I(INS_stfd, EA_8BYTE, reg, REG_SPBASE, offset);
            compiler->unwindSaveReg(reg, offset);
            offset += REGSIZE_BYTES;
        }
    }

    JITDUMP("    frame pointer offset from SP=0\n");
    compiler->unwindSetFrameReg(REG_FPBASE, 0);

    if (compiler->info.compIsVarArgs)
    {
        JITDUMP("    compIsVarArgs=true\n");
        NYI_POWERPC64("genPushCalleeSavedRegisters does not yet support compIsVarArgs");
    }
}

//------------------------------------------------------------------------
// genInstrWithConstant:   we will typically generate one instruction
//
//    ins  reg1, reg2, imm
//
// However the imm might not fit as a directly encodable immediate,
// when it doesn't fit we generate extra instruction(s) that sets up
// the 'regTmp' with the proper immediate value.
//
//     mov  regTmp, imm
//     ins  reg1, reg2, regTmp
//
// Arguments:
//    ins                 - instruction
//    attr                - operation size and GC attribute
//    reg1, reg2          - first and second register operands
//    imm                 - immediate value (third operand when it fits)
//    tmpReg              - temp register to use when the 'imm' doesn't fit. Can be REG_NA
//                          if caller knows for certain the constant will fit.
//    inUnwindRegion      - true if we are in a prolog/epilog region with unwind codes.
//                          Default: false.
//
// Return Value:
//    returns true if the immediate was small enough to be encoded inside instruction. If not,
//    returns false meaning the immediate was too large and tmpReg was used and modified.
//
// Notes:
//    PowerPC64 D-form instructions (ld, std, addi, etc.) use 16-bit signed immediates.
//    For ld/std, the immediate must be a multiple of 4 (DS-form, 14-bit field).
//    If the immediate doesn't fit, we load it into tmpReg and use indexed addressing.
//
bool CodeGen::genInstrWithConstant(instruction ins,
				   emitAttr    attr,
				   regNumber   reg1,
				   regNumber   reg2,
				   ssize_t     imm,
				   regNumber   tmpReg,
				   bool        inUnwindRegion /* = false */)
{
    bool immFitsInIns = false;
    emitAttr size = EA_SIZE(attr);

    // reg1 is usually a dest register
    // reg2 is always source register
    assert(tmpReg != reg2); // tmpReg cannot match any source register

    // Check if immediate fits in instruction encoding
    switch (ins)
    {
        case INS_addi:
            // addi uses 16-bit signed immediate (SIMM field)
            immFitsInIns = (imm >= -32768 && imm <= 32767);
            break;

        case INS_std:
        case INS_stw:
        case INS_sth:
        case INS_stb:
        case INS_stfd:
        case INS_stfs:
            // reg1 is a source register for store instructions
            assert(tmpReg != reg1); // tmpReg cannot match source register
            FALLTHROUGH;

        case INS_ld:
        case INS_lwz:
        case INS_lhz:
        case INS_lbz:
        case INS_lfd:
        case INS_lfs:
            // Load/store instructions use 16-bit signed immediate
            // For ld/std (DS-form), immediate must be multiple of 4 (uses 14-bit field)
            if (ins == INS_ld || ins == INS_std)
            {
                immFitsInIns = ((imm >= -32768) && (imm <= 32764) && ((imm & 3) == 0));
            }
            else
            {
                // Other loads/stores use full 16-bit signed immediate (D-form)
                immFitsInIns = (imm >= -32768 && imm <= 32767);
            }
            break;

        default:
            assert(!"Unexpected instruction in genInstrWithConstant");
            break;
    }

    if (immFitsInIns)
    {
        // Generate a single instruction that encodes the immediate directly
        GetEmitter()->emitIns_R_R_I(ins, attr, reg1, reg2, imm);
    }
    else
    {
        // Caller can specify REG_NA for tmpReg when it "knows" the immediate will always fit
        assert(tmpReg != REG_NA);

        // Generate multiple instructions:
        // 1. Load the immediate into tmpReg
        // 2. Use indexed addressing (X-form instruction)

        // Load immediate into tmpReg using li/lis/ori sequence
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, tmpReg, imm);
        regSet.verifyRegUsed(tmpReg);

        // When we are in an unwind code region, record the extra instructions
        if (inUnwindRegion)
        {
            compiler->unwindPadding();
        }

        // Convert to indexed form and use three-register encoding
        instruction insIndexed = ins;
        switch (ins)
        {
            case INS_addi:
                // addi rd, ra, imm -> add rd, ra, tmpReg
                insIndexed = INS_add;
                break;
            case INS_ld:
                insIndexed = INS_ldx;
                break;
            case INS_std:
                insIndexed = INS_stdx;
                break;
            case INS_lwz:
                insIndexed = INS_lwzx;
                break;
            case INS_stw:
                insIndexed = INS_stwx;
                break;
            case INS_lhz:
                insIndexed = INS_lhzx;
                break;
            case INS_sth:
                insIndexed = INS_sthx;
                break;
            case INS_lbz:
                insIndexed = INS_lbzx;
                break;
            case INS_stb:
                insIndexed = INS_stbx;
                break;
            case INS_lfd:
                insIndexed = INS_lfdx;
                break;
            case INS_stfd:
                insIndexed = INS_stfdx;
                break;
            case INS_lfs:
                insIndexed = INS_lfsx;
                break;
            case INS_stfs:
                insIndexed = INS_stfsx;
                break;
            default:
                assert(!"Unexpected instruction for indexed form");
                break;
        }

        // Generate indexed instruction: insIndexed reg1, reg2, tmpReg
        GetEmitter()->emitIns_R_R_R(insIndexed, attr, reg1, reg2, tmpReg);
    }

    return immFitsInIns;
}

//---------------------------------------------------------------------
// genCallerSPtoFPdelta - return the offset from Caller-SP to the frame pointer.
// This number is going to be negative, since the Caller-SP is at a higher
// address than the frame pointer.
//
// There must be a frame pointer to call this function!
//
// Notes:
//    PowerPC64 frame layout (after genPushCalleeSavedRegisters):
//    - FP points to the bottom of the allocated frame (same as SP after stdu)
//    - Total frame was allocated with: stdu sp, sp, -totalFrameSize
//    - So FP is totalFrameSize bytes below Caller's SP
//
int CodeGenInterface::genCallerSPtoFPdelta() const
{
    assert(isFramePointerUsed());
    
    // FP = Caller-SP - totalFrameSize
    // So delta = -totalFrameSize
    int callerSPtoFPdelta = genCallerSPtoInitialSPdelta() + genSPtoFPdelta();
    
    assert(callerSPtoFPdelta <= 0);
    return callerSPtoFPdelta;
}

//---------------------------------------------------------------------
// genCallerSPtoInitialSPdelta - return the offset from Caller-SP to Initial SP.
//
// This number will be negative.
//
// Notes:
//    PowerPC64: After stdu sp, sp, -totalFrameSize, Initial-SP is totalFrameSize
//    bytes below Caller-SP.
//
int CodeGenInterface::genCallerSPtoInitialSPdelta() const
{
    // Initial-SP = Caller-SP - totalFrameSize
    int callerSPtoSPdelta = -genTotalFrameSize();
    
    assert(callerSPtoSPdelta <= 0);
    return callerSPtoSPdelta;
}

//---------------------------------------------------------------------
// genSPtoFPdelta - return offset from the stack pointer (Initial-SP) to the frame pointer.
//
// Notes:
//    PowerPC64: FP is set equal to SP after frame allocation (line 3239 in genPushCalleeSavedRegisters)
//    So FP = SP, meaning the delta is 0.
//
int CodeGenInterface::genSPtoFPdelta() const
{
    assert(isFramePointerUsed());
    
    // In PowerPC64, after "stdu sp, sp, -totalFrameSize" and "mr fp, sp",
    // FP points to the same location as SP (bottom of the frame).
    // Therefore, SP to FP delta is 0.
    return 0;
}

//---------------------------------------------------------------------
// genTotalFrameSize - return the total size of the stack frame, including local size,
// callee-saved register size, etc.
//
// Return value:
//    Total frame size
//
// Notes:
//    PPC64LE ELFv2 ABI requires:
//    - Linkage area: 32 bytes (mandatory)
//    - Parameter save area: 64 bytes (8 registers * 8 bytes) for incoming register parameters
//      (only if function has parameters - this is where callee can spill r3-r10)
//    - Local variables and spills: compLclFrameSize (includes outgoing arg space)
//    - Callee-saved registers: compCalleeRegsPushed * 8
//
//    Note: compArgSize tracks incoming parameter size. If we have incoming parameters,
//    we need the parameter save area. If compLclFrameSize already accounts for this
//    (e.g., includes space for parameters), we should not double-count.
//

int CodeGenInterface::genTotalFrameSize() const
{
    assert(!IsUninitialized(compiler->compCalleeRegsPushed));

    // PPC64LE ELFv2 ABI frame layout:
    // - 32 bytes: Linkage area (back chain, CR save, LR save, TOC save, etc.)
    // - Parameter save area: Space for incoming parameters
    //   * Minimum 64 bytes (for r3-r10) if function has parameters
    //   * If compArgSize > 64, use compArgSize (parameters spilled to stack)
    // - compLclFrameSize: Local variables + outgoing argument space
    // - 16 bytes: Temporary storage (8 bytes for GT_CNS_DBL + 8 bytes for frame pointer)
    // - compCalleeRegsPushed * 8: Callee-saved registers
    
    const int LINKAGE_AREA_SIZE = 32;
    const int PARAM_SAVE_AREA_SIZE = 64;  // Minimum: 8 registers * 8 bytes
    const int TEMP_STORAGE_SIZE = 16;     // 8 bytes for GT_CNS_DBL + 8 bytes for FP
    
    // Calculate parameter save area size
    // PPC64LE ELFv2 ABI requires parameter save area to always be allocated
    // (minimum 64 bytes for 8 register parameters r3-r10)
    int paramSaveArea = PARAM_SAVE_AREA_SIZE;
    
    // If we have parameters and compArgSize > 64, use compArgSize
    // (this means we have stack parameters beyond the register parameters)
    if (compiler->info.compArgsCount > 0 && compiler->compArgSize > PARAM_SAVE_AREA_SIZE)
    {
        paramSaveArea = compiler->compArgSize;
    }
    
    int totalFrameSize = LINKAGE_AREA_SIZE +
                         paramSaveArea +
                         compiler->compCalleeRegsPushed * REGSIZE_BYTES +
                         compiler->compLclFrameSize +
                         TEMP_STORAGE_SIZE;

    assert(totalFrameSize >= 0);

    totalFrameSize =
        static_cast<int>(roundUp(static_cast<unsigned>(totalFrameSize), STACK_ALIGN));

    assert((totalFrameSize % STACK_ALIGN) == 0);
    return totalFrameSize;
}

//-----------------------------------------------------------------------------------
// genProfilingLeaveCallback: Generate the profiling function leave or tailcall callback.
// Technically, this is not part of the epilog; it is called when we are generating code for a GT_RETURN node.
//
// Arguments:
//     helper - which helper to call. Either CORINFO_HELP_PROF_FCN_LEAVE or CORINFO_HELP_PROF_FCN_TAILCALL
//
// Return Value:
//     None
//
void CodeGen::genProfilingLeaveCallback(unsigned helper)
{
    //_ASSERTE("!NYI");
    abort();
}

//  move an immediate value into an integer register
void CodeGen::instGen_Set_Reg_To_Imm(emitAttr       size,
                                     regNumber      reg,                                     ssize_t        imm,
                                     insFlags flags DEBUGARG(size_t targetHandle) DEBUGARG(GenTreeFlags gtFlags))
{
    // reg cannot be a FP register
    assert(!genIsValidFloatReg(reg));

    if (!compiler->opts.compReloc)
    {
        size = EA_SIZE(size); // Strip any Reloc flags from size if we aren't doing relocs
    }

    if (EA_IS_RELOC(size))
    {
        abort();
    }
    else if (imm == 0)
    {
        // Zero: li reg, 0
	GetEmitter()->emitIns_R_I(INS_li, size, reg, 0, INS_OPTS_NONE, INS_SCALABLE_OPTS_NONE DEBUGARG(targetHandle) DEBUGARG(gtFlags));
    }
    else if (GetEmitter()->emitIns_valid_imm_for_li(imm))
    {
	// 16-bit signed immediate: li reg, imm
	GetEmitter()->emitIns_R_I(INS_li, size, reg, imm, INS_OPTS_NONE, INS_SCALABLE_OPTS_NONE DEBUGARG(targetHandle) DEBUGARG(gtFlags));
    }
    else
    {
        // For larger immediates, use multiple instructions.
        // Cast to uint64_t before shifting to guarantee unsigned (zero-filling) right shifts
        // on all compilers.  Using signed ssize_t arithmetic would produce arithmetic
        // (sign-filling) shifts for negative immediates, corrupting every chunk above bit 31
        // and causing the JIT disassembler to print garbage values (e.g. 0xCDCD / 52685 for
        // all five instructions when the address happens to have the same pattern in each
        // 16-bit lane).
        const uint64_t uimm = static_cast<uint64_t>(imm);

        if (size == EA_4BYTE)
        {
            // 32-bit immediate: lis rD, imm@h  +  ori rD, rD, imm@l
            // Both chunks are masked to 16 bits so that idcCnsVal stores only the
            // value that will actually be encoded in the instruction field.
            GetEmitter()->emitIns_R_I(INS_lis, size, reg, (ssize_t)((uimm >> 16) & 0xffff), INS_OPTS_NONE, INS_SCALABLE_OPTS_NONE DEBUGARG(targetHandle) DEBUGARG(gtFlags));
            GetEmitter()->emitIns_R_I(INS_ori, size, reg, (ssize_t)(uimm & 0xffff),          INS_OPTS_NONE, INS_SCALABLE_OPTS_NONE DEBUGARG(targetHandle) DEBUGARG(gtFlags));
        }
        else // EA_8BYTE — five-instruction sequence
        {
            // lis  rD, imm@highest  (bits 63:48)
            GetEmitter()->emitIns_R_I(INS_lis,  size, reg, (ssize_t)((uimm >> 48) & 0xffff), INS_OPTS_NONE, INS_SCALABLE_OPTS_NONE DEBUGARG(targetHandle) DEBUGARG(gtFlags));
            // ori  rD, rD, imm@higher  (bits 47:32)
            GetEmitter()->emitIns_R_I(INS_ori,  size, reg, (ssize_t)((uimm >> 32) & 0xffff), INS_OPTS_NONE, INS_SCALABLE_OPTS_NONE DEBUGARG(targetHandle) DEBUGARG(gtFlags));
            // sldi rD, rD, 32  — slide upper half into bits 63:32
            GetEmitter()->emitIns_R_I(INS_sldi, size, reg, 32,                                INS_OPTS_NONE, INS_SCALABLE_OPTS_NONE DEBUGARG(targetHandle) DEBUGARG(gtFlags));
            // oris rD, rD, imm@h  (bits 31:16)
            GetEmitter()->emitIns_R_I(INS_oris, size, reg, (ssize_t)((uimm >> 16) & 0xffff), INS_OPTS_NONE, INS_SCALABLE_OPTS_NONE DEBUGARG(targetHandle) DEBUGARG(gtFlags));
            // ori  rD, rD, imm@l  (bits 15:0)
            GetEmitter()->emitIns_R_I(INS_ori,  size, reg, (ssize_t)(uimm & 0xffff),          INS_OPTS_NONE, INS_SCALABLE_OPTS_NONE DEBUGARG(targetHandle) DEBUGARG(gtFlags));
        }
    }
}

//-----------------------------------------------------------------------------------
// genProfilingEnterCallback: Generate the profiling function enter callback.
//
// Arguments:
//     initReg        - register to use as scratch register
//     pInitRegZeroed - OUT parameter. *pInitRegZeroed set to 'false' if 'initReg' is
//                      set to non-zero value after this call.
//
// Return Value:
//     None
//
void CodeGen::genProfilingEnterCallback(regNumber initReg, bool* pInitRegZeroed)
{
    //_ASSERTE("!NYI");
    return;

}

/*****************************************************************************
 *  Emit a call to a helper function.
 *
 */

//------------------------------------------------------------------------
// genEmitHelperCall: Generate code to call a runtime helper function
//
// Arguments:
//    helper          - The helper function to call
//    argSize         - Size of arguments in bytes
//    retSize         - Size of return value
//    callTargetReg   - Register to use for indirect call (REG_NA = use default)
//
// Notes:
//    Simplified version for PowerPC64LE that doesn't use emitIns_R_AI
//    (which doesn't exist in the PowerPC emitter yet).
//
void CodeGen::genEmitHelperCall(unsigned helper, int argSize, emitAttr retSize, regNumber callTargetReg /*= REG_NA */)
{
    void* addr  = nullptr;
    void* pAddr = nullptr;

    addr = compiler->compGetHelperFtn((CorInfoHelpFunc)helper, &pAddr);

    // PowerPC64 bl has a 24-bit signed offset (±32MB range). To avoid range issues,
    // always materialize the full 64-bit address and use indirect call through CTR.
    
    regNumber tempReg = callTargetReg;
    if (tempReg == REG_NA)
    {
        // Use REG_R12 as the default call target register
        tempReg = REG_R12;
    }

    regMaskTP callTargetMask = genRegMask(tempReg);
    regMaskTP callKillSet    = compiler->compHelperCallKillSet((CorInfoHelpFunc)helper);

    // assert that all registers in callTargetMask are in the callKillSet
    noway_assert((callTargetMask & callKillSet) == callTargetMask);

    if (addr == nullptr)
    {
        // This is an indirect call to a runtime helper.
        // Load the helper function address from the indirection table.
        
        // Load the 64-bit address of the helper table entry
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, tempReg, (ssize_t)pAddr);
        
        // Load the actual function pointer from the helper table
        // ld tempReg, 0(tempReg)
        GetEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, tempReg, tempReg, 0);
    }
    else
    {
        // Direct call to helper - materialize the full 64-bit address
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, tempReg, (ssize_t)addr);
    }
    
    // Move target address to R12 if it's not already there
    if (tempReg != REG_R12)
    {
        GetEmitter()->emitIns_Mov(INS_mov, EA_PTRSIZE, REG_R12, tempReg, /* canSkip */ false);
    }
    
    // Move R12 to CTR for indirect call
    GetEmitter()->emitIns_R(INS_mtctr, EA_PTRSIZE, REG_R12);
    
    regSet.verifyRegUsed(REG_R12);

    // Emit the actual call instruction (indirect through CTR)
    GetEmitter()->emitIns_Call(emitter::EC_INDIR_R, compiler->eeFindHelper(helper), INDEBUG_LDISASM_COMMA(nullptr) nullptr, argSize,
                               retSize, EA_UNKNOWN, gcInfo.gcVarPtrSetCur, gcInfo.gcRegGCrefSetCur,
                               gcInfo.gcRegByrefSetCur, DebugInfo(), /* IL offset */
                               REG_R12,                              /* ireg - always use R12 */
                               REG_NA, 0, 0,                         /* xreg, xmul, disp */
                               false                                 /* isJump */
    );

    // ELFv2 ABI: restore TOC pointer (r2) from our saved slot after the helper call.
    // Helper functions live in the runtime and may use a different TOC from the JIT'd code.
    GetEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_R2, REG_SPBASE, 24 /* R2_save_offset */);

    // Mark all registers that are killed by this helper call
    regMaskTP killMask = compiler->compHelperCallKillSet((CorInfoHelpFunc)helper);
    regSet.verifyRegistersUsed(killMask);
}

//------------------------------------------------------------------------
// genAllocLclFrame: Probe the stack.
//
// Notes:
//      This only does the probing; allocating the frame is done when callee-saved registers are saved.
//      This is done before anything has been pushed. The previous frame might have a large outgoing argument
//      space that has been allocated, but the lowest addresses have not been touched. Our frame setup might
//      not touch up to the first 504 bytes. This means we could miss a guard page. On Linux (where PPC64LE runs),
//      there is only one guard page by default, so we need to be very careful. We do an extra probe if we might
//      not have probed recently enough. That is, if a call and prolog establishment might lead to missing a page.
//
// Arguments:
//      frameSize         - the size of the stack frame being allocated.
//      initReg           - register to use as a scratch register.
//      pInitRegZeroed    - OUT parameter. *pInitRegZeroed is set to 'false' if and only if
//                          this call sets 'initReg' to a non-zero value. Otherwise, it is unchanged.
//      maskArgRegsLiveIn - incoming argument registers that are currently live.
//
// Return value:
//      None
//
void CodeGen::genAllocLclFrame(unsigned frameSize, regNumber initReg, bool* pInitRegZeroed, regMaskTP maskArgRegsLiveIn)
{
    assert(compiler->compGeneratingProlog);

    if (frameSize == 0)
    {
        return;
    }

    const target_size_t pageSize = compiler->eeGetPageSize();

    // What offset from the final SP was the last probe? If we haven't probed almost a complete page, and
    // if the next action on the stack might subtract from SP first, before touching the current SP, then
    // we do one more probe at the very bottom. This is especially important on Linux (where PPC64LE runs)
    // which has only one guard page by default. Note that we probe here for PPC64LE, but we don't alter SP.
    target_size_t lastTouchDelta = 0;

    assert(!compiler->info.compPublishStubParam || (REG_SECRET_STUB_PARAM != initReg));

    if (frameSize < pageSize)
    {
        lastTouchDelta = frameSize;
    }
    else if (frameSize < 3 * pageSize)
    {
        // The probing loop in "else"-case below would require at least 6 instructions (and more if
        // 'frameSize' or 'pageSize' cannot be encoded with immediate instructions).
        // Hence for frames that are smaller than 3 * PAGE_SIZE the JIT inlines the following probing code
        // to decrease code size. This is a code size optimization heuristic, not related to the number of guard pages.
        // TODO-PPC64: The probing mechanisms should be replaced by a call to stack probe helper
        // as it is done on other platforms.

        lastTouchDelta = frameSize;

        for (target_size_t probeOffset = pageSize; probeOffset <= frameSize; probeOffset += pageSize)
        {
            // Generate:
            //    li initReg, -probeOffset
            //    lwzx r0, sp, initReg      // Load word indexed (probe the stack)
            // On PPC64LE, we use lwz with indexed addressing to probe the stack

            instGen_Set_Reg_To_Imm(EA_PTRSIZE, initReg, -(ssize_t)probeOffset);
            // Use lwz (load word and zero) with base+index addressing to probe
            // lwzx rD, rA, rB loads from address (rA + rB)
            // We load into r0 (which is a scratch register) from (sp + initReg)
            GetEmitter()->emitIns_R_R_I(INS_lwz, EA_4BYTE, REG_R0, REG_SPBASE, -(ssize_t)probeOffset);
            regSet.verifyRegUsed(initReg);
            *pInitRegZeroed = false; // The initReg does not contain zero

            lastTouchDelta -= pageSize;
        }

        assert(lastTouchDelta == frameSize % pageSize);
        compiler->unwindPadding();
    }
    else
    {
        // Emit the following sequence to 'tickle' the pages. Note it is important that stack pointer not change
        // until this is complete since the tickles could cause a stack overflow, and we need to be able to crawl
        // the stack afterward (which means the stack pointer needs to be known).
        // This is critical on Linux where there is only one guard page.

        regMaskTP availMask = RBM_ALLINT & (regSet.rsGetModifiedRegsMask() | ~RBM_INT_CALLEE_SAVED);
        availMask &= ~maskArgRegsLiveIn;   // Remove all of the incoming argument registers as they are currently live
        availMask &= ~genRegMask(initReg); // Remove the pre-calculated initReg

        regNumber rOffset = initReg;
        regNumber rLimit;
        regMaskTP tempMask;

        // We pick the next lowest register number for rLimit
        noway_assert(availMask != RBM_NONE);
        tempMask = genFindLowestBit(availMask);
        rLimit   = genRegNumFromMask(tempMask);

        // Generate:
        //
        //      li rOffset, -pageSize
        //      li rLimit, -frameSize
        // loop:
        //      lwz r0, 0(sp + rOffset)    // Probe the stack
        //      addi rOffset, rOffset, -pageSize
        //      cmpd rLimit, rOffset
        //      ble loop                   // If rLimit <= rOffset, we need to probe this rOffset

        noway_assert((ssize_t)(int)frameSize == (ssize_t)frameSize); // make sure framesize safely fits within an int

        instGen_Set_Reg_To_Imm(EA_PTRSIZE, rOffset, -(ssize_t)pageSize);
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, rLimit, -(ssize_t)frameSize);

        // There's a "virtual" label here. But we can't create a label in the prolog, so we use the magic
        // `emitIns_J` with a negative `instrCount` to branch back a specific number of instructions.

        // lwz r0, 0(sp + rOffset) - probe the stack
        GetEmitter()->emitIns_R_R_I(INS_lwz, EA_4BYTE, REG_R0, REG_SPBASE, 0); // This will need rOffset added
        // addi rOffset, rOffset, -pageSize
        GetEmitter()->emitIns_R_R_I(INS_addi, EA_PTRSIZE, rOffset, rOffset, -(ssize_t)pageSize);
        // cmpd rLimit, rOffset
        GetEmitter()->emitIns_R_R(INS_cmpd, EA_PTRSIZE, rLimit, rOffset);
        // ble loop (branch if less than or equal)
        // Branch back 4 instructions to create the probing loop
        // The -4 means: branch back to the lwz instruction (4 instructions ago: lwz, addi, cmpd, ble)
        GetEmitter()->emitIns_J(INS_ble, NULL, -4);

        *pInitRegZeroed = false; // The initReg does not contain zero

        compiler->unwindPadding();

        lastTouchDelta = frameSize % pageSize;
    }

    if (lastTouchDelta + STACK_PROBE_BOUNDARY_THRESHOLD_BYTES > pageSize)
    {
        assert(lastTouchDelta + STACK_PROBE_BOUNDARY_THRESHOLD_BYTES < 2 * pageSize);
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, initReg, -(ssize_t)frameSize);
        // lwz r0, 0(sp + initReg) - final probe
        GetEmitter()->emitIns_R_R_I(INS_lwz, EA_4BYTE, REG_R0, REG_SPBASE, -(ssize_t)frameSize);
        compiler->unwindPadding();

        regSet.verifyRegUsed(initReg);
        *pInitRegZeroed = false; // The initReg does not contain zero
    }
}


//------------------------------------------------------------------------
// genIntToFloatCast: Generate code to cast an int/long to float/double
//
// Arguments:
//    treeNode - The GT_CAST node
//
// Return Value:
//    None.
//
// Assumptions:
//    Cast is a non-overflow conversion.
//    The treeNode must have an assigned register.
//    SrcType= int32/uint32/int64/uint64 and DstType=float/double.
//
void CodeGen::genIntToFloatCast(GenTree* treeNode)
{
    // Casting from int/long to float/double on PowerPC64 requires:
    // 1. Store integer value to stack
    // 2. Load from stack into FP register using lfd
    // 3. Convert using fcfid/fcfids/fcfidu/fcfidus
    
    assert(treeNode->OperGet() == GT_CAST);

    GenTree*  op1       = treeNode->AsOp()->gtOp1;
    var_types dstType   = treeNode->CastToType();
    var_types srcType   = genActualType(op1->TypeGet());
    regNumber targetReg = treeNode->GetRegNum();
    regNumber srcReg    = genConsumeReg(op1);
    bool      isUnsigned = treeNode->IsUnsigned(); // Check the GTF_UNSIGNED flag on the cast node

    assert(varTypeIsFloating(dstType));
    assert(varTypeIsIntegral(srcType));
    assert(genIsValidFloatReg(targetReg));
    assert(genIsValidIntReg(srcReg));

    // Get internal FP register allocated by LSRA
    regNumber tmpFpReg = internalRegisters.GetSingle(treeNode, RBM_ALLFLOAT);
    assert(genIsValidFloatReg(tmpFpReg));

    // Calculate stack offset for temporary storage
    // Use space at top of local frame to avoid corrupting backchain at 0(r1)
    int tmpOffset = genTotalFrameSize() - 16;

    // Step 1: Handle 32-bit to 64-bit extension if needed
    regNumber extendedReg = srcReg;
    if (varTypeIsInt(srcType))
    {
        if (isUnsigned)
        {
            // Zero-extend 32-bit unsigned to 64-bit
            // Clear upper 32 bits by using clrldi (Clear Left Double Immediate)
            // clrldi rA, rS, n  clears the leftmost n bits
            // We want to clear the upper 32 bits, so n = 32
            // This is encoded as rldicl rA, rS, 0, 32
            // For now, use a mask operation: andi. can only handle 16-bit immediates
            // So we'll use a different approach: store 32-bit, load 64-bit zero-extended

            // Store as 32-bit word
            GetEmitter()->emitIns_R_R_I(INS_stw, EA_4BYTE, srcReg, REG_SPBASE, tmpOffset);
            // Load as 64-bit doubleword (zero-extends automatically)
            GetEmitter()->emitIns_R_R_I(INS_lwz, EA_4BYTE, srcReg, REG_SPBASE, tmpOffset);
            // Now srcReg contains the zero-extended 64-bit value
        }
        else
        {
            // Sign-extend 32-bit signed to 64-bit
            GetEmitter()->emitIns_R_R(INS_extsw, EA_8BYTE, srcReg, srcReg);
        }
    }

    // Store the 64-bit integer to stack
    GetEmitter()->emitIns_R_R_I(INS_std, EA_8BYTE, srcReg, REG_SPBASE, tmpOffset);

    // Step 2: Load from stack into FP register using lfd
    // This loads the bit pattern without conversion
    GetEmitter()->emitIns_R_R_I(INS_lfd, EA_8BYTE, tmpFpReg, REG_SPBASE, tmpOffset);

    // Step 3: Convert integer to float/double
    instruction convertIns;
    
    if (isUnsigned)
    {
        // Unsigned conversion
        if (dstType == TYP_FLOAT)
        {
            convertIns = INS_fcfidus; // Convert unsigned int to single-precision
        }
        else
        {
            convertIns = INS_fcfidu;  // Convert unsigned int to double-precision
        }
    }
    else
    {
        // Signed conversion
        if (dstType == TYP_FLOAT)
        {
            convertIns = INS_fcfids;  // Convert signed int to single-precision
        }
        else
        {
            convertIns = INS_fcfid;   // Convert signed int to double-precision
        }
    }

    // Perform the conversion: targetReg = convert(tmpFpReg)
    GetEmitter()->emitIns_R_R(convertIns, emitActualTypeSize(dstType), targetReg, tmpFpReg);

    genProduceReg(treeNode);
}

//-----------------------------------------------------------------------------
// genZeroInitFrameUsingBlockInit: architecture-specific helper for genZeroInitFrame in the case
// `genUseBlockInit` is set.
//
// Arguments:
//    untrLclHi      - (Untracked locals High-Offset)  The upper bound offset at which the zero init
//                                                     code will end initializing memory (not inclusive).
//    untrLclLo      - (Untracked locals Low-Offset)   The lower bound at which the zero init code will
//                                                     start zero initializing memory.
//    initReg        - A scratch register (that gets set to zero on some platforms).
//    pInitRegZeroed - OUT parameter. *pInitRegZeroed is set to 'true' if this method sets initReg register to zero,
//                     'false' if initReg was set to a non-zero value, and left unchanged if initReg was not touched.
//
void CodeGen::genZeroInitFrameUsingBlockInit(int untrLclHi, int untrLclLo, regNumber initReg, bool* pInitRegZeroed)
{
    assert(compiler->compGeneratingProlog);
    assert(untrLclHi > untrLclLo);

    int bytesToWrite = untrLclHi - untrLclLo;
    
    // Use initReg to hold zero
    instGen_Set_Reg_To_Imm(EA_PTRSIZE, initReg, 0);
    *pInitRegZeroed = true;
    
    // Get frame pointer
    regNumber fpReg = genFramePointerReg();
    
    // Simple loop: store zero in 8-byte chunks
    int offset = untrLclLo;
    while (offset < untrLclHi)
    {
        // std initReg, offset(fpReg)  - Store doubleword
        GetEmitter()->emitIns_R_R_I(INS_std, EA_8BYTE, initReg, fpReg, offset);
        offset += 8;
    }
    
    // Handle remaining bytes if not 8-byte aligned
    int remaining = untrLclHi - offset;
    if (remaining > 0)
    {
        if (remaining >= 4)
        {
            // stw initReg, offset(fpReg)  - Store word (4 bytes)
            GetEmitter()->emitIns_R_R_I(INS_stw, EA_4BYTE, initReg, fpReg, offset);
            offset += 4;
            remaining -= 4;
        }
        if (remaining > 0)
        {
            // sth or stb for remaining 1-3 bytes
            // For simplicity, just store word (may write extra bytes but safe)
            GetEmitter()->emitIns_R_R_I(INS_stw, EA_4BYTE, initReg, fpReg, offset);
        }
    }
}


// clang-format off
/*****************************************************************************
 *
 *  Generates code for an EH funclet prolog.
 *
 *  Funclets have the following incoming arguments:
 *
 *      catch:          x0 = the exception object that was caught (see GT_CATCH_ARG)
 *      filter:         x0 = the exception object to filter (see GT_CATCH_ARG), x1 = CallerSP of the containing function
 *      finally/fault:  none
 *
 *  Funclets set the following registers on exit:
 *
 *      catch:          x0 = the address at which execution should resume (see BBJ_EHCATCHRET)
 *      filter:         x0 = non-zero if the handler should handle the exception, zero otherwise (see GT_RETFILT)
 *      finally/fault:  none
 *
 *  The ARM64 funclet prolog sequence is one of the following (Note: #framesz is total funclet frame size,
 *  including everything; #outsz is outgoing argument space. #framesz must be a multiple of 16):
 *
 *  Frame type 1:
 *     For #outsz == 0 and #framesz <= 512:
 *     stp fp,lr,[sp,-#framesz]!    ; establish the frame (predecrement by #framesz), save FP/LR
 *     stp x19,x20,[sp,#xxx]        ; save callee-saved registers, as necessary
 *
 *  The funclet frame is thus:
 *
 *      |                       |
 *      |-----------------------|
 *      |  incoming arguments   |
 *      +=======================+ <---- Caller's SP
 *      |      OSR padding      | // If required
 *      |-----------------------|
 *      |  Varargs regs space   | // Only for varargs main functions; 64 bytes
 *      |-----------------------|
 *      |Callee saved registers | // multiple of 8 bytes
 *      |-----------------------|
 *      |    MonitorAcquired    | // 8 bytes; for synchronized methods
 *      |-----------------------|
 *      |        PSP slot       | // 8 bytes (omitted in NativeAOT ABI)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the whole frame 16 byte aligned.
 *      |-----------------------|
 *      |      Saved FP, LR     | // 16 bytes
 *      |-----------------------| <---- Ambient SP
 *      |       |               |
 *      ~       | Stack grows   ~
 *      |       | downward      |
 *              V
 *
 *  Frame type 2:
 *     For #outsz != 0 and #framesz <= 512:
 *     sub sp,sp,#framesz           ; establish the frame
 *     stp fp,lr,[sp,#outsz]        ; save FP/LR.
 *     stp x19,x20,[sp,#xxx]        ; save callee-saved registers, as necessary
 *
 *  The funclet frame is thus:
 *
 *      |                       |
 *      |-----------------------|
 *      |  incoming arguments   |
 *      +=======================+ <---- Caller's SP
 *      |      OSR padding      | // If required
 *      |-----------------------|
 *      |  Varargs regs space   | // Only for varargs main functions; 64 bytes
 *      |-----------------------|
 *      |Callee saved registers | // multiple of 8 bytes
 *      |-----------------------|
 *      |    MonitorAcquired    | // 8 bytes; for synchronized methods
 *      |-----------------------|
 *      |        PSP slot       | // 8 bytes (omitted in NativeAOT ABI)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the whole frame 16 byte aligned.
 *      |-----------------------|
 *      |      Saved FP, LR     | // 16 bytes
 *      |-----------------------|
 *      |   Outgoing arg space  | // multiple of 8 bytes
 *      |-----------------------| <---- Ambient SP
 *      |       |               |
 *      ~       | Stack grows   ~
 *      |       | downward      |
 *              V
 *
 *  Frame type 3:
 *     For #framesz > 512:
 *     stp fp,lr,[sp,- (#framesz - #outsz)]!    ; establish the frame, save FP/LR
 *                                              ; note that it is guaranteed here that (#framesz - #outsz) <= 240
 *     stp x19,x20,[sp,#xxx]                    ; save callee-saved registers, as necessary
 *     sub sp,sp,#outsz                         ; create space for outgoing argument space
 *
 *  The funclet frame is thus:
 *
 *      |                       |
 *      |-----------------------|
 *      |  incoming arguments   |
 *      +=======================+ <---- Caller's SP
 *      |      OSR padding      | // If required
 *      |-----------------------|
 *      |  Varargs regs space   | // Only for varargs main functions; 64 bytes
 *      |-----------------------|
 *      |Callee saved registers | // multiple of 8 bytes
 *      |-----------------------|
 *      |    MonitorAcquired    | // 8 bytes; for synchronized methods
 *      |-----------------------|
 *      |        PSP slot       | // 8 bytes (omitted in NativeAOT ABI)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the first SP subtraction 16 byte aligned
 *      |-----------------------|
 *      |      Saved FP, LR     | // 16 bytes <-- SP after first adjustment (points at saved FP)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the whole frame 16 byte aligned (specifically, to 16-byte align the outgoing argument space).
 *      |-----------------------|
 *      |   Outgoing arg space  | // multiple of 8 bytes
 *      |-----------------------| <---- Ambient SP (SP after second adjustment)
 *      |       |               |
 *      ~       | Stack grows   ~
 *      |       | downward      |
 *              V
 *
 * Both #1 and #2 only change SP once. That means that there will be a maximum of one alignment slot needed. For the general case, #3,
 * it is possible that we will need to add alignment to both changes to SP, leading to 16 bytes of alignment. Remember that the stack
 * pointer needs to be 16 byte aligned at all times. The size of the PSP slot plus callee-saved registers space is a maximum of 240 bytes:
 *
 *     FP,LR registers
 *     10 int callee-saved register x19-x28
 *     8 float callee-saved registers v8-v15
 *     8 saved integer argument registers x0-x7, if varargs function
 *     1 PSP slot
 *     1 alignment slot or monitor acquired slot
 *     == 30 slots * 8 bytes = 240 bytes.
 *
 * The outgoing argument size, however, can be very large, if we call a function that takes a large number of
 * arguments (note that we currently use the same outgoing argument space size in the funclet as for the main
 * function, even if the funclet doesn't have any calls, or has a much smaller, or larger, maximum number of
 * outgoing arguments for any call). In that case, we need to 16-byte align the initial change to SP, before
 * saving off the callee-saved registers and establishing the PSPsym, so we can use the limited immediate offset
 * encodings we have available, before doing another 16-byte aligned SP adjustment to create the outgoing argument
 * space. Both changes to SP might need to add alignment padding.
 *
 * In addition to the above "standard" frames, we also need to support a frame where the saved FP/LR are at the
 * highest addresses. This is to match the frame layout (specifically, callee-saved registers including FP/LR
 * and the PSPSym) that is used in the main function when a GS cookie is required due to the use of localloc.
 * (Note that localloc cannot be used in a funclet.) In these variants, not only has the position of FP/LR
 * changed, but where the alignment padding is placed has also changed.
 *
 *  Frame type 4 (variant of frame types 1 and 2):
 *     For #framesz <= 512:
 *     sub sp,sp,#framesz           ; establish the frame
 *     stp x19,x20,[sp,#xxx]        ; save callee-saved registers, as necessary
 *     stp fp,lr,[sp,#yyy]          ; save FP/LR.
 *     ; write PSPSym
 *
 *  The "#framesz <= 512" condition ensures that after we've established the frame, we can use "stp" with its
 *  maximum allowed offset (504) to save the callee-saved register at the highest address.
 *
 *  We use "sub" instead of folding it into the next instruction as a predecrement, as we need to write PSPSym
 *  at the bottom of the stack, and there might also be an alignment padding slot.
 *
 *  The funclet frame is thus:
 *
 *      |                       |
 *      |-----------------------|
 *      |  incoming arguments   |
 *      +=======================+ <---- Caller's SP
 *      |      OSR padding      | // If required
 *      |-----------------------|
 *      |  Varargs regs space   | // Only for varargs main functions; 64 bytes
 *      |-----------------------|
 *      |      Saved LR         | // 8 bytes
 *      |-----------------------|
 *      |      Saved FP         | // 8 bytes
 *      |-----------------------|
 *      |Callee saved registers | // multiple of 8 bytes
 *      |-----------------------|
 *      |    MonitorAcquired    | // 8 bytes; for synchronized methods
 *      |-----------------------|
 *      |        PSP slot       | // 8 bytes (omitted in NativeAOT ABI)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the whole frame 16 byte aligned.
 *      |-----------------------|
 *      |   Outgoing arg space  | // multiple of 8 bytes (optional; if #outsz > 0)
 *      |-----------------------| <---- Ambient SP
 *      |       |               |
 *      ~       | Stack grows   ~
 *      |       | downward      |
 *              V
 *
 *  Frame type 5 (variant of frame type 3):
 *     For #framesz > 512:
 *     sub sp,sp,(#framesz - #outsz) ; establish part of the frame. Note that it is guaranteed here that (#framesz - #outsz) <= 240
 *     stp x19,x20,[sp,#xxx]        ; save callee-saved registers, as necessary
 *     stp fp,lr,[sp,#yyy]          ; save FP/LR.
 *     sub sp,sp,#outsz             ; create space for outgoing argument space
 *     ; write PSPSym
 *
 *  For large frames with "#framesz > 512", we must do one SP adjustment first, after which we can save callee-saved
 *  registers with up to the maximum "stp" offset of 504. Then, we can establish the rest of the frame (namely, the
 *  space for the outgoing argument space).
 *
 *  The funclet frame is thus:
 *
 *      |                       |
 *      |-----------------------|
 *      |  incoming arguments   |
 *      +=======================+ <---- Caller's SP
 *      |      OSR padding      | // If required
 *      |-----------------------|
 *      |  Varargs regs space   | // Only for varargs main functions; 64 bytes
 *      |-----------------------|
 *      |      Saved LR         | // 8 bytes
 *      |-----------------------|
 *      |      Saved FP         | // 8 bytes
 *      |-----------------------|
 *      |Callee saved registers | // multiple of 8 bytes
 *      |-----------------------|
 *      |    MonitorAcquired    | // 8 bytes; for synchronized methods
 *      |-----------------------|
 *      |        PSP slot       | // 8 bytes (omitted in NativeAOT ABI)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the first SP subtraction 16 byte aligned <-- SP after first adjustment (points at alignment padding or PSP slot)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the whole frame 16 byte aligned (specifically, to 16-byte align the outgoing argument space).
 *      |-----------------------|
 *      |   Outgoing arg space  | // multiple of 8 bytes
 *      |-----------------------| <---- Ambient SP (SP after second adjustment)
 *      |       |               |
 *      ~       | Stack grows   ~
 *      |       | downward      |
 *              V
 *
 * Note that in this case we might have 16 bytes of alignment that is adjacent. This is because we are doing 2 SP
 * subtractions, and each one must be aligned up to 16 bytes.
 *
 * Note that in all cases, the PSPSym is in exactly the same position with respect to Caller-SP, and that location is the same relative to Caller-SP
 * as in the main function.
 *
 * Funclets do not have varargs arguments. However, because the PSPSym must exist at the same offset from Caller-SP as in the main function, we
 * must add buffer space for the saved varargs argument registers here, if the main function did the same.
 *
 *     ; After this header, fill the PSP slot, for use by the VM (it gets reported with the GC info), or by code generation of nested filters.
 *     ; This is not part of the "OS prolog"; it has no associated unwind data, and is not reversed in the funclet epilog.
 *
 *     if (this is a filter funclet)
 *     {
 *          // x1 on entry to a filter funclet is CallerSP of the containing function:
 *          // either the main function, or the funclet for a handler that this filter is dynamically nested within.
 *          // Note that a filter can be dynamically nested within a funclet even if it is not statically within
 *          // a funclet. Consider:
 *          //
 *          //    try {
 *          //        try {
 *          //            throw new Exception();
 *          //        } catch(Exception) {
 *          //            throw new Exception();     // The exception thrown here ...
 *          //        }
 *          //    } filter {                         // ... will be processed here, while the "catch" funclet frame is still on the stack
 *          //    } filter-handler {
 *          //    }
 *          //
 *          // Because of this, we need a PSP in the main function anytime a filter funclet doesn't know whether the enclosing frame will
 *          // be a funclet or main function. We won't know any time there is a filter protecting nested EH. To simplify, we just always
 *          // create a main function PSP for any function with a filter.
 *
 *          ldr x1, [x1, #CallerSP_to_PSP_slot_delta]  ; Load the CallerSP of the main function (stored in the PSP of the dynamically containing funclet or function)
 *          str x1, [sp, #SP_to_PSP_slot_delta]        ; store the PSP
 *          add fp, x1, #Function_CallerSP_to_FP_delta ; re-establish the frame pointer
 *     }
 *     else
 *     {
 *          // This is NOT a filter funclet. The VM re-establishes the frame pointer on entry.
 *          // TODO-ARM64-CQ: if VM set x1 to CallerSP on entry, like for filters, we could save an instruction.
 *
 *          add x3, fp, #Function_FP_to_CallerSP_delta  ; compute the CallerSP, given the frame pointer. x3 is scratch.
 *          str x3, [sp, #SP_to_PSP_slot_delta]         ; store the PSP
 *     }
 *
 *  An example epilog sequence is then:
 *
 *     add sp,sp,#outsz             ; if any outgoing argument space
 *     ...                          ; restore callee-saved registers
 *     ldp x19,x20,[sp,#xxx]
 *     ldp fp,lr,[sp],#framesz
 *     ret lr
 *
 * See CodeGen::genPushCalleeSavedRegisters() for a description of the main function frame layout.
 * See Compiler::lvaAssignVirtualFrameOffsetsToLocals() for calculation of main frame local variable offsets.
 */
// clang-format on

void CodeGen::genFuncletProlog(BasicBlock* block)
{
#ifdef DEBUG
    if (verbose)
    {
        printf("*************** In genFuncletProlog()\n");
    }
#endif

    assert(block != nullptr);
    assert(block->HasFlag(BBF_FUNCLET_BEG));

    ScopedSetVariable<bool> _setGeneratingProlog(&compiler->compGeneratingProlog, true);

    gcInfo.gcResetForBB();

    compiler->unwindBegProlog();

    bool isFilter  = (block->bbCatchTyp == BBCT_FILTER);

    regMaskTP maskArgRegsLiveIn;
    if (isFilter)
    {
        // On entry to a filter: r3 = exception object, r4 = CallerSP of the containing function
        maskArgRegsLiveIn = RBM_R3 | RBM_R4;
    }
    else if ((block->bbCatchTyp == BBCT_FINALLY) || (block->bbCatchTyp == BBCT_FAULT))
    {
        maskArgRegsLiveIn = RBM_NONE;
    }
    else
    {
        // catch: r3 = exception object
        maskArgRegsLiveIn = RBM_R3;
    }

    // --------------------------------------------------------------------------
    // Build the funclet frame.  The layout is identical to the main function's
    // ELFv2 frame:
    //
    //   caller_SP  (= funclet entry r1)
    //     +16(caller_SP) : saved LR
    //     +24(caller_SP) : saved R2/TOC
    //   ---  stdu atomically writes back-chain and decrements r1 ---
    //     funclet_SP + frameSize - 8 : saved incoming FP (r31)
    //   [funclet_SP + fiSpDelta + frameSize - 1] .. [funclet_SP + fiSP_to_CalleeSaved_delta]
    //                  : callee-saved int regs r14-r31 (ascending order)
    //                  : callee-saved float regs f14-f31 (ascending order)
    //   [funclet_SP + fiSP_to_PSP_slot_delta]
    //                  : PSP slot (written after unwindEndProlog)
    // --------------------------------------------------------------------------

    // These are the same ABI-fixed offsets used by the main prolog/epilog.
    constexpr int FP_backchain_save_offset = -8;   // relative to caller-SP
    constexpr int LR_save_offset           = 16;   // relative to caller-SP
    constexpr int R2_save_offset           = 24;   // relative to caller-SP

    emitter* emit = GetEmitter();
    int      funcletFrameSize = -genFuncletInfo.fiSpDelta; // positive value
    assert(funcletFrameSize > 0);
    assert((funcletFrameSize % STACK_ALIGN) == 0);

    regMaskTP maskSaveRegsInt   = genFuncletInfo.fiSaveRegs & RBM_INT_CALLEE_SAVED;
    regMaskTP maskSaveRegsFloat = genFuncletInfo.fiSaveRegs & RBM_ALLFLOAT;

    // --- Save ABI linkage-area entries before allocating the frame ---
    emit->emitIns_R_R_I(INS_std, EA_PTRSIZE, REG_R2, REG_SPBASE, R2_save_offset);
    compiler->unwindSaveReg(REG_R2, R2_save_offset);

    emit->emitIns_R(INS_mflr, EA_PTRSIZE, REG_R0);
    emit->emitIns_R_R_I(INS_std, EA_PTRSIZE, REG_R0, REG_SPBASE, LR_save_offset);
    compiler->unwindSaveReg(REG_R0, LR_save_offset);

    // --- Allocate the frame: stdu writes the back-chain and updates r1 ---
    emit->emitIns_R_R_I(INS_stdu, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, -funcletFrameSize);
    compiler->unwindAllocStack(funcletFrameSize);

    // Save incoming r31 at caller_SP - 8, expressed relative to funclet SP.
    const int FP_save_offset = funcletFrameSize + FP_backchain_save_offset;
    assert(FP_save_offset >= 0);
    emit->emitIns_R_R_I(INS_std, EA_PTRSIZE, REG_FP, REG_SPBASE, FP_save_offset);
    compiler->unwindSaveReg(REG_FP, FP_save_offset);

    // --- Establish FP = SP (bottom of frame) ---
    emit->emitIns_Mov(INS_mov, EA_PTRSIZE, REG_FP, REG_SPBASE, /* canSkip */ false);

    // --- Save callee-saved integer registers (r14-r31, ascending) ---
    int offset = genFuncletInfo.fiSP_to_CalleeSaved_delta;
    for (int regNum = REG_R14; regNum <= REG_R31; regNum++)
    {
        regNumber reg     = (regNumber)regNum;
        regMaskTP regMask = genRegMask(reg);
        if ((maskSaveRegsInt & regMask) != RBM_NONE)
        {
            emit->emitIns_R_R_I(INS_std, EA_PTRSIZE, reg, REG_SPBASE, offset);
            compiler->unwindSaveReg(reg, offset);
            offset += REGSIZE_BYTES;
        }
    }

    // --- Save callee-saved float registers (f14-f31, ascending) ---
    for (int regNum = REG_F14; regNum <= REG_F31; regNum++)
    {
        regNumber reg     = (regNumber)regNum;
        regMaskTP regMask = genRegMask(reg);
        if ((maskSaveRegsFloat & regMask) != RBM_NONE)
        {
            emit->emitIns_R_R_I(INS_stfd, EA_8BYTE, reg, REG_SPBASE, offset);
            compiler->unwindSaveReg(reg, offset);
            offset += REGSIZE_BYTES;
        }
    }

    compiler->unwindSetFrameReg(REG_FPBASE, 0);

    // This is the end of the OS-reported prolog for unwinding purposes.
    compiler->unwindEndProlog();

    // --- Set up PSPSym (not part of OS prolog; not reversed in epilog) ---
    // If there is no PSPSym (NativeAOT ABI) we are done.
    if (compiler->lvaPSPSym != BAD_VAR_NUM)
    {
        if (isFilter)
        {
            // On filter entry r4 = CallerSP of the dynamically-containing function/funclet.
            // Load the "real" CallerSP of the main function from the PSP stored in that frame,
            // then store it to our own PSP slot and re-establish FP.
            // r5 is used as scratch (it is volatile and not live on entry to a filter).
            genInstrWithConstant(INS_ld,   EA_PTRSIZE, REG_R4, REG_R4, genFuncletInfo.fiCallerSP_to_PSP_slot_delta,
                                 REG_R5, false);
            regSet.verifyRegUsed(REG_R4);

            genInstrWithConstant(INS_std,  EA_PTRSIZE, REG_R4, REG_SPBASE, genFuncletInfo.fiSP_to_PSP_slot_delta,
                                 REG_R5, false);

            // re-establish FP: fp = CallerSP + Function_CallerSP_to_FP_delta
            genInstrWithConstant(INS_addi, EA_PTRSIZE, REG_FPBASE, REG_R4,
                                 genFuncletInfo.fiFunction_CallerSP_to_FP_delta, REG_R5, false);
        }
        else
        {
            // Non-filter: the VM has re-established FP on entry.
            // Compute CallerSP from FP using the delta captured at compile time,
            // then store it into our PSP slot.
            // r5 is scratch (volatile, not live on funclet entry).
            genInstrWithConstant(INS_addi, EA_PTRSIZE, REG_R5, REG_FPBASE,
                                 -genFuncletInfo.fiFunction_CallerSP_to_FP_delta, REG_R0, false);
            regSet.verifyRegUsed(REG_R5);

            genInstrWithConstant(INS_std,  EA_PTRSIZE, REG_R5, REG_SPBASE, genFuncletInfo.fiSP_to_PSP_slot_delta,
                                 REG_R0, false);
        }
    }
}

/*****************************************************************************
 *
 *  Generates code for an EH funclet epilog.
 *
 *  See the description of frame shapes at genFuncletProlog().
 */

void CodeGen::genFuncletEpilog()
{
#ifdef DEBUG
    if (verbose)
    {
        printf("*************** In genFuncletEpilog()\n");
    }
#endif

    ScopedSetVariable<bool> _setGeneratingEpilog(&compiler->compGeneratingEpilog, true);

    compiler->unwindBegEpilog();

    // These are the same ABI-fixed offsets used by the main prolog/epilog.
    constexpr int FP_backchain_save_offset = -8;   // relative to caller-SP
    constexpr int LR_save_offset           = 16;   // relative to caller-SP
    constexpr int R2_save_offset           = 24;   // relative to caller-SP

    emitter* emit = GetEmitter();
    int      funcletFrameSize = -genFuncletInfo.fiSpDelta; // positive value
    assert(funcletFrameSize > 0);

    regMaskTP maskRestoreRegsInt   = genFuncletInfo.fiSaveRegs & RBM_INT_CALLEE_SAVED;
    regMaskTP maskRestoreRegsFloat = genFuncletInfo.fiSaveRegs & RBM_ALLFLOAT;

    // --- Restore callee-saved float registers (f31 down to f14, reverse of save) ---
    // First, advance 'offset' past the int saves to find where floats begin.
    int offset = genFuncletInfo.fiSP_to_CalleeSaved_delta;
    for (int regNum = REG_R14; regNum <= REG_R31; regNum++)
    {
        if ((maskRestoreRegsInt & genRegMask((regNumber)regNum)) != RBM_NONE)
        {
            offset += REGSIZE_BYTES;
        }
    }
    // 'offset' now points just past the last int save = start of float saves.
    // Restore floats highest-numbered first (matching save order in reverse).
    for (int regNum = REG_F31; regNum >= REG_F14; regNum--)
    {
        regNumber reg     = (regNumber)regNum;
        regMaskTP regMask = genRegMask(reg);
        if ((maskRestoreRegsFloat & regMask) != RBM_NONE)
        {
            offset -= REGSIZE_BYTES;
            emit->emitIns_R_R_I(INS_lfd, EA_8BYTE, reg, REG_SPBASE, offset);
            compiler->unwindSaveReg(reg, offset);
        }
    }

    // --- Restore callee-saved integer registers (r31 down to r14) ---
    for (int regNum = REG_R31; regNum >= REG_R14; regNum--)
    {
        regNumber reg     = (regNumber)regNum;
        regMaskTP regMask = genRegMask(reg);
        if ((maskRestoreRegsInt & regMask) != RBM_NONE)
        {
            offset -= REGSIZE_BYTES;
            emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, reg, REG_SPBASE, offset);
            compiler->unwindSaveReg(reg, offset);
        }
    }

    // Restore incoming r31 while r1 still denotes the fixed funclet SP.
    const int FP_save_offset = funcletFrameSize + FP_backchain_save_offset;
    assert(FP_save_offset >= 0);
    emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_FP, REG_SPBASE, FP_save_offset);
    compiler->unwindSaveReg(REG_FP, FP_save_offset);

    // --- Deallocate the frame: r1 = caller-SP ---
    emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, funcletFrameSize);
    compiler->unwindAllocStack(funcletFrameSize);

    // --- Restore LR and R2 from the caller's linkage area ---
    emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_R0, REG_SPBASE, LR_save_offset);
    compiler->unwindSaveReg(REG_R0, LR_save_offset);

    emit->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_R2, REG_SPBASE, R2_save_offset);
    compiler->unwindSaveReg(REG_R2, R2_save_offset);

    emit->emitIns_R(INS_mtlr, EA_PTRSIZE, REG_R0);
    emit->emitIns(INS_blr);
    compiler->unwindReturn(REG_R0);

    compiler->unwindEndEpilog();
}

//------------------------------------------------------------------------
// genFloatToIntCast: Generate code to cast float/double to int/long
//
// Arguments:
//    treeNode - The GT_CAST node
//
// Return Value:
//    None.
//
// Assumptions:
//    Cast is a non-overflow conversion.
//    The treeNode must have an assigned register.
//    SrcType=float/double and DstType= int32/uint32/int64/uint64
//
void CodeGen::genFloatToIntCast(GenTree* treeNode)
{
    assert(treeNode->OperGet() == GT_CAST);
    
    GenTree*  op1     = treeNode->AsOp()->gtOp1;
    var_types dstType = treeNode->CastToType();
    var_types srcType = op1->TypeGet();
   
    assert(varTypeIsFloating(srcType));
    assert(varTypeIsIntegral(dstType));
    
    regNumber srcReg = genConsumeReg(op1);
    regNumber dstReg = treeNode->GetRegNum();
   
    assert(genIsValidFloatReg(srcReg));
    assert(genIsValidIntReg(dstReg));
    
    emitter* emit = GetEmitter();
    
    // PowerPC64 float-to-int conversion requires:
    // 1. Convert float to integer in FP register
    // 2. Store FP register to stack
    // 3. Load from stack to integer register
    
    instruction convertIns;
    bool isUnsigned = varTypeIsUnsigned(dstType);
    bool is64Bit = (genTypeSize(dstType) == 8);
    
    // Select appropriate conversion instruction
    if (is64Bit)
    {
        convertIns = isUnsigned ? INS_fctiduz : INS_fctidz;
    }
    else
    {
        convertIns = isUnsigned ? INS_fctiwuz : INS_fctiwz;
    }
    
    // Use a temporary FP register for the converted value
    regNumber tempFpReg = internalRegisters.GetSingle(treeNode, RBM_ALLFLOAT);
    
    // Convert float to integer (result in FP register)
    emit->emitIns_R_R(convertIns, EA_8BYTE, tempFpReg, srcReg);
    
    // Store FP register to stack and load to integer register
    // PowerPC64 cannot directly move from FP to GPR, must use stack
    // Use a safe offset that doesn't corrupt the stack frame header (backchain at 0, CR at 8, LR at 16, TOC at 24)
    int tmpOffset = genTotalFrameSize() - 16;  // Use space at end of frame for temporary storage
    
    // Step 1: Store the FP register (with converted integer) to stack
    emit->emitIns_R_R_I(INS_stfd, EA_8BYTE, tempFpReg, REG_SPBASE, tmpOffset);
    
    // Step 2: Load from stack to integer register
    if (is64Bit)
    {
        // Load 64-bit value
        emit->emitIns_R_R_I(INS_ld, EA_8BYTE, dstReg, REG_SPBASE, tmpOffset);
    }
    else
    {
        // Load 32-bit value from stack
        // In little-endian, the 32-bit result is at offset 0
        emit->emitIns_R_R_I(INS_lwz, EA_4BYTE, dstReg, REG_SPBASE, tmpOffset);
    }
    
    genProduceReg(treeNode);
}

/*****************************************************************************
 *
 *  Capture the information used to generate the funclet prologs and epilogs.
 *  Note that all funclet prologs are identical, and all funclet epilogs are
 *  identical (per type: filters are identical, and non-filters are identical).
 *  Thus, we compute the data used for these just once.
 *
 *  See genFuncletProlog() for more information about the prolog/epilog sequences.
 */

void CodeGen::genCaptureFuncletPrologEpilogInfo()
{
    if (!compiler->ehAnyFunclets())
    {
        return;
    }

    assert(isFramePointerUsed());
    // The frame size and offsets must be finalized.
    assert(compiler->lvaDoneFrameLayout == Compiler::FINAL_FRAME_LAYOUT);

    regMaskTP rsMaskSaveRegs = regSet.rsMaskCalleeSaved;
    // Note: on PPC64LE, FP (r31) is saved at the fixed ABI back-chain slot
    // (-8 from caller-SP) before stdu, so it is NOT tracked in rsMaskCalleeSaved.
    // The callee-save loops in the prolog/epilog cover r14-r30 and f14-f31 only.
    assert((rsMaskSaveRegs & RBM_FPBASE) == 0);

    // -----------------------------------------------------------------------
    // Funclet frame layout (ELFv2 ABI):
    //
    //   caller-SP  (= funclet entry r1)
    //     -8(caller-SP)  : saved FP (r31)   ]  written before stdu into the
    //     +16(caller-SP) : saved LR          ]  caller's linkage area
    //     +24(caller-SP) : saved R2/TOC      ]
    //   funclet-SP  (= caller-SP - funcletFrameSize)
    //     +0  .. +31            : 32-byte ELFv2 mandatory linkage area
    //     +32 .. +32+saveBytes-1: callee-saved int regs r14-r30 (ascending)
    //                             then callee-saved float regs f14-f31 (ascending)
    //     +pspSlotOffset        : PSP slot (8 bytes, if present) — at the same
    //                             caller-SP-relative offset as in the main function
    //     ..alignment padding..
    //   caller-SP  (= funclet-SP + funcletFrameSize)
    //
    // PPC64LE has no negative-SP addressing.  All slot offsets are positive
    // SP-relative.  The PSP slot's position in the funclet frame is driven by
    // the main function's PSPSym caller-SP-relative offset so the VM can find
    // it at the same location from both frames.
    // -----------------------------------------------------------------------

    // OSR: pad so PSPSym sits at the same caller-relative offset as in Tier0.
    int osrPad = 0;
    if (compiler->opts.IsOSR())
    {
        osrPad -= compiler->info.compPatchpointInfo->TotalFrameSize();
        assert((osrPad % STACK_ALIGN) == 0);
    }

    // Delta from caller-SP to FP in the main function (negative).
    genFuncletInfo.fiFunction_CallerSP_to_FP_delta = genCallerSPtoFPdelta() + osrPad;

    // Callee-saved registers start right above the 32-byte linkage area.
    const int FUNCLET_LINKAGE_AREA           = 32;
    genFuncletInfo.fiSP_to_CalleeSaved_delta = FUNCLET_LINKAGE_AREA;

    const int saveBytes = (int)(genCountBits(rsMaskSaveRegs) * REGSIZE_BYTES);

    // Minimum frame content: linkage area + callee-saved registers.
    int funcletFrameSize = FUNCLET_LINKAGE_AREA + saveBytes;

    // The PSP slot must sit at the same caller-SP-relative offset as in the main
    // function.  Read that offset directly from the main frame's lclvar layout so
    // there is a single source of truth and the assert at the bottom always holds.
    //
    // fiCallerSP_to_PSP_slot_delta  < 0  (PSP is below caller-SP)
    // fiSP_to_PSP_slot_delta        > 0  (PSP is above funclet-SP)
    //   fiSP_to_PSP_slot_delta = funcletFrameSize + fiCallerSP_to_PSP_slot_delta
    //                          = funcletFrameSize - |callerSP_to_PSP|
    //
    // The funclet frame must be at least large enough to hold this slot, so we
    // size it to ensure fiSP_to_PSP_slot_delta >= funcletFrameSize - funcletFrameSize
    // is positive after alignment.

    if (compiler->lvaPSPSym != BAD_VAR_NUM)
    {
        // Authoritative caller-SP-relative offset of the PSP slot, set by
        // lvaAssignVirtualFrameOffsetsToLocals / lvaFixVirtualFrameOffsets.
        const int callerSP_to_PSP = compiler->lvaGetCallerSPRelativeOffset(compiler->lvaPSPSym);
        assert(callerSP_to_PSP <= 0);

        // The funclet frame must be tall enough that pspSlotOffset is positive.
        // Minimum required size = -callerSP_to_PSP + 8 (for the slot itself) + osrPad,
        // but we always round up to STACK_ALIGN anyway.
        funcletFrameSize = max(funcletFrameSize, -callerSP_to_PSP - osrPad);
        funcletFrameSize = (int)roundUp((unsigned)funcletFrameSize, STACK_ALIGN);

        genFuncletInfo.fiSpDelta                    = -funcletFrameSize;
        genFuncletInfo.fiSaveRegs                   = rsMaskSaveRegs;
        genFuncletInfo.fiCallerSP_to_PSP_slot_delta = callerSP_to_PSP;
        // PSP slot offset from funclet-SP: always positive on PPC64LE.
        genFuncletInfo.fiSP_to_PSP_slot_delta       = funcletFrameSize + callerSP_to_PSP;
    }
    else
    {
        funcletFrameSize = (int)roundUp((unsigned)funcletFrameSize, STACK_ALIGN);

        genFuncletInfo.fiSpDelta                    = -funcletFrameSize;
        genFuncletInfo.fiSaveRegs                   = rsMaskSaveRegs;
        genFuncletInfo.fiCallerSP_to_PSP_slot_delta = 0;
        genFuncletInfo.fiSP_to_PSP_slot_delta       = 0;
    }

#ifdef DEBUG
    if (verbose)
    {
        printf("\n");
        printf("Funclet prolog / epilog info\n");
        printf("                         Save regs: ");
        dspRegMask(genFuncletInfo.fiSaveRegs);
        printf("\n");
        if (compiler->opts.IsOSR())
        {
            printf("                           OSR Pad: %d\n", osrPad);
        }
        printf("   fiFunction_CallerSP_to_FP_delta: %d\n", genFuncletInfo.fiFunction_CallerSP_to_FP_delta);
        printf("         fiSP_to_CalleeSaved_delta: %d\n", genFuncletInfo.fiSP_to_CalleeSaved_delta);
        printf("            fiSP_to_PSP_slot_delta: %d\n", genFuncletInfo.fiSP_to_PSP_slot_delta);
        printf("      fiCallerSP_to_PSP_slot_delta: %d\n", genFuncletInfo.fiCallerSP_to_PSP_slot_delta);
        printf("                         fiSpDelta: %d\n", genFuncletInfo.fiSpDelta);
    }

    assert(genFuncletInfo.fiSP_to_CalleeSaved_delta >= 0);
    assert(genFuncletInfo.fiSP_to_PSP_slot_delta >= 0);
    assert(genFuncletInfo.fiCallerSP_to_PSP_slot_delta <= 0);
    assert(genFuncletInfo.fiSpDelta < 0);
    assert(((-genFuncletInfo.fiSpDelta) % STACK_ALIGN) == 0);

    if (compiler->lvaPSPSym != BAD_VAR_NUM)
    {
        assert(genFuncletInfo.fiCallerSP_to_PSP_slot_delta ==
               compiler->lvaGetCallerSPRelativeOffset(compiler->lvaPSPSym)); // same offset used in main function and funclet!
    }
#endif // DEBUG
}

void CodeGen::genSetPSPSym(regNumber initReg, bool* pInitRegZeroed)
{    
    //_ASSERTE("!NYI");
    assert(compiler->compGeneratingProlog);

    if (compiler->lvaPSPSym == BAD_VAR_NUM)
    {
        return;  // No PSPSym needed for this function
    }

    noway_assert(isFramePointerUsed()); // We need an explicit frame pointer

    // Calculate the offset from current SP to caller's SP
    int SPtoCallerSPdelta = -genCallerSPtoInitialSPdelta();

    if (compiler->opts.IsOSR())
    {
        SPtoCallerSPdelta += compiler->info.compPatchpointInfo->TotalFrameSize();
    }

    // Use initReg as scratch register
    regNumber regTmp = initReg;
    *pInitRegZeroed = false;

    // Calculate caller's SP: addi regTmp, SP, SPtoCallerSPdelta
    GetEmitter()->emitIns_R_R_I(INS_addi, EA_PTRSIZE, regTmp, REG_SPBASE, SPtoCallerSPdelta);

    // Store it to PSPSym local variable: std regTmp, [FP + PSPSym_offset]
    GetEmitter()->emitIns_S_R(INS_std, EA_PTRSIZE, regTmp, compiler->lvaPSPSym, 0);
}

//------------------------------------------------------------------------
// genLeaInstruction: Produce code for a GT_LEA node (Load Effective Address).
//
// Arguments:
//    lea - the GT_LEA node
//
// Notes:
//    PowerPC doesn't have a direct LEA instruction like x86/x64.
//    We need to compute: base + (index * scale) + offset
//    using add and shift instructions.
//

void CodeGen::genLeaInstruction(GenTreeAddrMode* lea)
{
    emitter* emit = GetEmitter();
    GenTree* base  = lea->Base();
    GenTree* index = lea->Index();
    unsigned scale = lea->GetScale();
    int      offset = lea->Offset();
    regNumber targetReg = lea->GetRegNum();

    // Consume the operands
    if (base != nullptr)
    {
        genConsumeReg(base);
    }
    if (index != nullptr)
    {
        genConsumeReg(index);
    }

    // PowerPC LEA computation strategy:
    // 1. If we have an index with scale, compute: index << log2(scale)
    // 2. Add base (if present)
    // 3. Add offset (if present)

    regNumber resultReg = targetReg;

    if (index != nullptr && scale > 1)
    {
        // Need to scale the index: index << log2(scale)
        unsigned shift = genLog2(scale);
        regNumber indexReg = index->GetRegNum();

        if (base == nullptr && offset == 0)
        {
            // Just scaled index: targetReg = index << shift
            emit->emitIns_R_R_I(INS_sldi, EA_PTRSIZE, targetReg, indexReg, shift);
            resultReg = targetReg;
        }
        else
        {
            // Need to use internal register for scaled index
            regNumber tempReg = internalRegisters.GetSingle(lea);
            emit->emitIns_R_R_I(INS_sldi, EA_PTRSIZE, tempReg, indexReg, shift);

            if (base != nullptr)
            {
                // Add base: targetReg = base + (index << shift)
                emit->emitIns_R_R_R(INS_add, EA_PTRSIZE, targetReg, base->GetRegNum(), tempReg);
                resultReg = targetReg;
            }
            else
            {
                // No base, just move scaled index to target
                emit->emitIns_Mov(INS_mov, EA_PTRSIZE, targetReg, tempReg, /* canSkip */ false);
                resultReg = targetReg;
            }
        }
    }
    else if (index != nullptr)
    {
        // Index with scale == 1
        regNumber indexReg = index->GetRegNum();

        if (base != nullptr)
        {
            // targetReg = base + index
            emit->emitIns_R_R_R(INS_add, EA_PTRSIZE, targetReg, base->GetRegNum(), indexReg);
            resultReg = targetReg;
        }
        else
        {
            // Just index, move to target
            emit->emitIns_Mov(INS_mov, EA_PTRSIZE, targetReg, indexReg, /* canSkip */ false);
            resultReg = targetReg;
        }
    }
    else if (base != nullptr)
    {
        // Just base, possibly with offset
        if (offset == 0)
        {
            // Just move base to target
            emit->emitIns_Mov(INS_mov, EA_PTRSIZE, targetReg, base->GetRegNum(), /* canSkip */ false);
            resultReg = targetReg;
        }
        else
        {
            // Will add offset below
            resultReg = base->GetRegNum();
        }
    }
    else
    {
        // Just offset (constant address)
        assert(offset != 0);
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, targetReg, offset);
        genProduceReg(lea);
        return;
    }

    // Add offset if present and not yet handled
    if (offset != 0 && !(base != nullptr && index == nullptr && offset == 0))
    {
        // PowerPC addi instruction uses 16-bit signed immediate
        if ((offset >= -32768) && (offset <= 32767))
        {
            // Offset fits in immediate
            if (resultReg == targetReg)
            {
                // Add to target: targetReg = targetReg + offset
                emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, targetReg, targetReg, offset);
            }
            else
            {
                // Add to result and store in target: targetReg = resultReg + offset
                emit->emitIns_R_R_I(INS_addi, EA_PTRSIZE, targetReg, resultReg, offset);
            }
        }
        else
        {
            // Offset doesn't fit, need to use internal register
            regNumber tempReg = internalRegisters.GetSingle(lea);
            instGen_Set_Reg_To_Imm(EA_PTRSIZE, tempReg, offset);

            if (resultReg == targetReg)
            {
                // Add to target: targetReg = targetReg + tempReg
                emit->emitIns_R_R_R(INS_add, EA_PTRSIZE, targetReg, targetReg, tempReg);
            }
            else
            {
                // Add to result: targetReg = resultReg + tempReg
                emit->emitIns_R_R_R(INS_add, EA_PTRSIZE, targetReg, resultReg, tempReg);
            }
        }
    }

    genProduceReg(lea);
}

#ifdef FEATURE_SIMD
//------------------------------------------------------------------------
// genSIMDSplitReturn: Generates code for returning a fixed-size SIMD type that lives
//                     in a single register, but is returned in multiple registers.
//
// Arguments:
//    src         - The source of the return
//    retTypeDesc - The return type descriptor.
//
void CodeGen::genSIMDSplitReturn(GenTree* src, ReturnTypeDesc* retTypeDesc)
{
    //_ASSERTE("!NYI");
    abort();
}
#endif // FEATURE_SIMD
    

//------------------------------------------------------------------------
// genIntCastOverflowCheck: Generate overflow checking code for an integer cast.
//
// Arguments:
//    cast - The GT_CAST node
//    desc - The cast description
//    reg  - The register containing the value to check
//
void CodeGen::genIntCastOverflowCheck(GenTreeCast* cast, const GenIntCastDesc& desc, regNumber reg)
{
    emitter* emit = GetEmitter();
    
    switch (desc.CheckKind())
    {
        case GenIntCastDesc::CHECK_POSITIVE:
            // Check if value >= 0
            emit->emitIns_R_I(INS_cmpdi, EA_ATTR(desc.CheckSrcSize()), reg, 0);
            genJumpToThrowHlpBlk(EJ_lt, SCK_OVERFLOW);
            break;

#ifdef TARGET_64BIT
        case GenIntCastDesc::CHECK_UINT_RANGE:
        {
            // Check if value fits in unsigned 32-bit range (upper 32 bits must be zero)
            // Use a temporary register to test upper bits
            regNumber tempReg = internalRegisters.GetSingle(cast);
            // Shift right 32 bits and check if result is zero
            emit->emitIns_R_R_I(INS_sldi, EA_8BYTE, tempReg, reg, 32);
            emit->emitIns_R_I(INS_cmpdi, EA_8BYTE, tempReg, 0);
            genJumpToThrowHlpBlk(EJ_ne, SCK_OVERFLOW);
            break;
        }

        case GenIntCastDesc::CHECK_POSITIVE_INT_RANGE:
        {
            // Check if value fits in signed 32-bit range (0 to 0x7FFFFFFF)
            regNumber tempReg = internalRegisters.GetSingle(cast);
            // Check upper 33 bits are zero
            emit->emitIns_R_R_I(INS_sldi, EA_8BYTE, tempReg, reg, 33);
            emit->emitIns_R_I(INS_cmpdi, EA_8BYTE, tempReg, 0);
            genJumpToThrowHlpBlk(EJ_ne, SCK_OVERFLOW);
            break;
        }

        case GenIntCastDesc::CHECK_INT_RANGE:
        {
            // Check if value fits in signed 32-bit range (INT32_MIN to INT32_MAX)
            // Sign extend from 32-bit and compare with original
            regNumber tempReg = internalRegisters.GetSingle(cast);
            emit->emitIns_R_R(INS_extsw, EA_8BYTE, tempReg, reg);
            emit->emitIns_R_R(INS_cmpd, EA_8BYTE, reg, tempReg);
            genJumpToThrowHlpBlk(EJ_ne, SCK_OVERFLOW);
            break;
        }
#endif

        default:
        {
            assert(desc.CheckKind() == GenIntCastDesc::CHECK_SMALL_INT_RANGE);
            const int castMaxValue = desc.CheckSmallIntMax();
            const int castMinValue = desc.CheckSmallIntMin();

            // Check upper bound
            emit->emitIns_R_I(INS_cmpdi, EA_ATTR(desc.CheckSrcSize()), reg, castMaxValue);
            genJumpToThrowHlpBlk(EJ_gt, SCK_OVERFLOW);

            // Check lower bound if not zero
            if (castMinValue != 0)
            {
                emit->emitIns_R_I(INS_cmpdi, EA_ATTR(desc.CheckSrcSize()), reg, castMinValue);
                genJumpToThrowHlpBlk(EJ_lt, SCK_OVERFLOW);
            }
            break;
        }
    }
}

//------------------------------------------------------------------------
// genIntToIntCast: Generate code for an integer cast, with or without overflow check.
//
// Arguments:
//    cast - The GT_CAST node
//
// Assumptions:
//    Neither the source nor target type can be a floating point type.
//
void CodeGen::genIntToIntCast(GenTreeCast* cast)
{
    genConsumeRegs(cast->CastOp());

    emitter*        emit    = GetEmitter();
    var_types       dstType = cast->CastToType();
    var_types       srcType = genActualType(cast->CastOp()->TypeGet());
    const regNumber srcReg  = cast->CastOp()->GetRegNum();
    const regNumber dstReg  = cast->GetRegNum();

    assert(genIsValidIntReg(srcReg));
    assert(genIsValidIntReg(dstReg));

    GenIntCastDesc desc(cast);

    if (desc.CheckKind() != GenIntCastDesc::CHECK_NONE)
    {
        genIntCastOverflowCheck(cast, desc, srcReg);
    }

    if ((desc.ExtendKind() != GenIntCastDesc::COPY) || (srcReg != dstReg))
    {
        instruction ins;

        switch (desc.ExtendKind())
        {
            case GenIntCastDesc::ZERO_EXTEND_SMALL_INT:
                if (desc.ExtendSrcSize() == 1)
                {
                    // Zero extend byte: AND with 0xFF
                    emit->emitIns_R_R_I(INS_andi, EA_PTRSIZE, dstReg, srcReg, 0xFF);
                }
                else
                {
                    // Zero extend halfword: AND with 0xFFFF
                    emit->emitIns_R_R_I(INS_andi, EA_PTRSIZE, dstReg, srcReg, 0xFFFF);
                }
                break;

            case GenIntCastDesc::SIGN_EXTEND_SMALL_INT:
                ins = (desc.ExtendSrcSize() == 1) ? INS_extsb : INS_extsh;
                emit->emitIns_R_R(ins, EA_PTRSIZE, dstReg, srcReg);
                break;

#ifdef TARGET_64BIT
            case GenIntCastDesc::ZERO_EXTEND_INT:
                // Zero extend 32-bit to 64-bit: clear upper 32 bits
                // Use rotate and mask or shift operations
                emit->emitIns_R_R_I(INS_sldi, EA_8BYTE, dstReg, srcReg, 32);
                emit->emitIns_R_R_I(INS_srdi, EA_8BYTE, dstReg, dstReg, 32);
                break;

            case GenIntCastDesc::SIGN_EXTEND_INT:
                emit->emitIns_R_R(INS_extsw, EA_8BYTE, dstReg, srcReg);
                break;
#endif

            case GenIntCastDesc::COPY:
                if (srcReg != dstReg)
                {
                    emit->emitIns_Mov(INS_mov, EA_ATTR(desc.ExtendSrcSize()), dstReg, srcReg, /* canSkip */ false);
                }
                break;

            case GenIntCastDesc::LOAD_ZERO_EXTEND_SMALL_INT:
            case GenIntCastDesc::LOAD_SIGN_EXTEND_SMALL_INT:
            case GenIntCastDesc::LOAD_ZERO_EXTEND_INT:
            case GenIntCastDesc::LOAD_SIGN_EXTEND_INT:
            case GenIntCastDesc::LOAD_SOURCE:
                // These are handled by containment - should not reach here
                unreached();
                break;

            default:
                unreached();
        }
    }

    genProduceReg(cast);
}
 
//------------------------------------------------------------------------
// genFloatToFloatCast: Generate code for a cast between float and double
//
// Arguments:
//    treeNode - The GT_CAST node
//
// Return Value:
//    None.
//
// Assumptions:
//    Cast is a non-overflow conversion.
//    The treeNode must have an assigned register.
//    The cast is between float and double.
//
void CodeGen::genFloatToFloatCast(GenTree* treeNode)
{
    // float <--> double conversions are always non-overflow ones
    assert(treeNode->OperGet() == GT_CAST);
    assert(!treeNode->gtOverflow());

    regNumber targetReg = treeNode->GetRegNum();
    assert(genIsValidFloatReg(targetReg));

    GenTree* op1 = treeNode->AsOp()->gtOp1;
    assert(!op1->isContained());                  // Cannot be contained
    assert(genIsValidFloatReg(op1->GetRegNum())); // Must be a valid float reg.

    var_types dstType = treeNode->CastToType();
    var_types srcType = op1->TypeGet();
    assert(varTypeIsFloating(srcType) && varTypeIsFloating(dstType));

    genConsumeOperands(treeNode->AsOp());

    // treeNode must be a reg
    assert(!treeNode->isContained());

    if (srcType != dstType)
    {
        if (srcType == TYP_FLOAT)
        {
            // Float to Double: no explicit conversion needed in PowerPC64
            // Just move to double register (already in correct format)
            if (treeNode->GetRegNum() != op1->GetRegNum())
            {
                GetEmitter()->emitIns_R_R(INS_fmr, EA_8BYTE, treeNode->GetRegNum(), op1->GetRegNum());
            }
        }
        else
        {
            // Double to Float: use frsp (round to single precision)
            GetEmitter()->emitIns_R_R(INS_frsp, EA_4BYTE, treeNode->GetRegNum(), op1->GetRegNum());
        }
    }
    else if (treeNode->GetRegNum() != op1->GetRegNum())
    {
        // Same type cast - just move
        GetEmitter()->emitIns_R_R(INS_fmr, emitActualTypeSize(treeNode), treeNode->GetRegNum(), op1->GetRegNum());
    }

    genProduceReg(treeNode);
}


/*
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                           End Prolog / Epilog                             XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

BasicBlock* CodeGen::genCallFinally(BasicBlock* block)
{
    assert(block->KindIs(BBJ_CALLFINALLY));

    BasicBlock* const nextBlock = block->Next();

    // Generate a call to the finally, like this:
    //      ld/mr       r3, [fp + offset] / sp    // Load r3 with PSPSym, or sp if PSPSym is not used
    //      bl          finally-funclet
    //      b           finally-return             // Only for non-retless finally calls
    // The 'b' can be a NOP if we're going to the next block.

    if (compiler->lvaPSPSym != BAD_VAR_NUM)
    {
        GetEmitter()->emitIns_R_S(INS_ld, EA_PTRSIZE, REG_R3, compiler->lvaPSPSym, 0);
    }
    else
    {
        GetEmitter()->emitIns_Mov(INS_mov, EA_PTRSIZE, REG_R3, REG_SPBASE, /* canSkip */ false);
    }

    if (block->HasFlag(BBF_RETLESS_CALL))
    {
        // Load target block address into R12 using PC-relative sequence (bcl + mflr + addi)
        // Then call via CTR to avoid ±32MB range limitation of direct bl
        GetEmitter()->emitIns_R_L(INS_addi, EA_PTRSIZE, block->GetTarget(), REG_R12);
        GetEmitter()->emitIns_R(INS_mtctr, EA_PTRSIZE, REG_R12);
        GetEmitter()->emitIns(INS_bctrl);

        // We have a retless call, and the last instruction generated was a call.
        // If the next block is in a different EH region (or is the end of the code
        // block), then we need to generate a breakpoint here (since it will never
        // get executed) to get proper unwind behavior.

        if ((nextBlock == nullptr) || !BasicBlock::sameEHRegion(block, nextBlock))
        {
            instGen(INS_trap); // This should never get executed (PPC64 trap instruction)
        }

        return block;
    }
    else
    {
        // Because of the way the flowgraph is connected, the liveness info for this one instruction
        // after the call is not (can not be) correct in cases where a variable has a last use in the
        // handler.  So turn off GC reporting once we execute the call and reenable after the jmp/nop
        GetEmitter()->emitDisableGC();
        
        // Load target block address into R12 using PC-relative sequence (bcl + mflr + addi)
        // Then call via CTR to avoid ±32MB range limitation of direct bl
        GetEmitter()->emitIns_R_L(INS_addi, EA_PTRSIZE, block->GetTarget(), REG_R12);
        GetEmitter()->emitIns_R(INS_mtctr, EA_PTRSIZE, REG_R12);
        GetEmitter()->emitIns(INS_bctrl);

        // Now go to where the finally funclet needs to return to.
        BasicBlock* const finallyContinuation = nextBlock->GetFinallyContinuation();
        if (nextBlock->NextIs(finallyContinuation) && !compiler->fgInDifferentRegions(nextBlock, finallyContinuation))
        {
            // Fall-through.
            // TODO-PPC64-CQ: Can we get rid of this instruction, and just have the call return directly
            // to the next instruction? This would depend on stack walking from within the finally
            // handler working without this instruction being in this special EH region.
            instGen(INS_nop);
        }
        else
        {
            inst_JMP(EJ_jmp, finallyContinuation);
        }

        GetEmitter()->emitEnableGC();

        return nextBlock;
    }
}




#endif // TARGET_POWERPC64
