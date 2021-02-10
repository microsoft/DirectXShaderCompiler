///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PixPassHelpers.cpp														 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilOperations.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace PIXPassHelpers
{
    bool IsAllocateRayQueryInstruction(llvm::Value* Val) {
        if (llvm::Instruction* Inst = llvm::dyn_cast<llvm::Instruction>(Val)) {
            return hlsl::OP::IsDxilOpFuncCallInst(Inst, hlsl::OP::OpCode::AllocateRayQuery);
        }
        return false;
    }
}