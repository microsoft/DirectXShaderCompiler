///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PixPassHelpers.cpp														 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilResourceBase.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace PIXPassHelpers
{
    bool IsAllocateRayQueryInstruction(llvm::Value* Val) {
        if (llvm::Instruction* Inst = llvm::dyn_cast<llvm::Instruction>(Val)) {
            if (Inst->getOpcode() == llvm::Instruction::OtherOps::Call) {
                if (Inst->getNumOperands() > 0) {
                    if (auto* asInt =
                        llvm::cast_or_null<llvm::ConstantInt>(Inst->getOperand(0))) {
                        if (asInt->getZExtValue() ==
                            (uint64_t)hlsl::DXIL::OpCode::AllocateRayQuery) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
}