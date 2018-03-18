//===-- DxilSimplify.cpp - Fold dxil intrinsics into constants -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

// simplify dxil op like mad 0, a, b->b.

#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilModule.h"

using namespace llvm;
using namespace hlsl;

namespace {
DXIL::OpCode GetOpcode(Value *opArg) {
  if (ConstantInt *ci = dyn_cast<ConstantInt>(opArg)) {
    uint64_t opcode = ci->getLimitedValue();
    if (opcode < static_cast<uint64_t>(OP::OpCode::NumOpCodes)) {
      return static_cast<OP::OpCode>(opcode);
    }
  }
  return DXIL::OpCode::NumOpCodes;
}
} // namespace

/// \brief Given a function and set of arguments, see if we can fold the
/// result as dxil operation.
///
/// If this call could not be simplified returns null.
Value *llvm::SimplifyDxil(Function *F, ArrayRef<Value *> Args,
                          const DataLayout &DL, const TargetLibraryInfo *TLI,
                          const DominatorTree *DT, AssumptionCache *AC,
                          const Instruction *CxtI) {
  if (!F->getParent()->HasDxilModule()) {
    assert(!OP::IsDxilOpFunc(F) && "dx.op function with no dxil module?");
    return nullptr;
  }

  // Lookup opcode class in dxil module. Set default value to invalid class.
  OP::OpCodeClass opClass = OP::OpCodeClass::NumOpClasses;
  const bool found =
      F->getParent()->GetDxilModule().GetOP()->GetOpCodeClass(F, opClass);
  if (!found)
    return nullptr;

  DXIL::OpCode opcode = GetOpcode(Args[0]);
  if (opcode == DXIL::OpCode::NumOpCodes)
    return nullptr;

  switch (opcode) {
  default:
    return nullptr;
  case DXIL::OpCode::FMad: {
    Value *op0 = Args[DXIL::OperandIndex::kBinarySrc0OpIdx];
    Value *op2 = Args[DXIL::OperandIndex::kTrinarySrc2OpIdx];
    Constant *zero = ConstantFP::get(op0->getType(), 0);
    if (op0 == zero)
      return op2;
    Value *op1 = Args[DXIL::OperandIndex::kBinarySrc1OpIdx];
    if (op1 == zero)
      return op2;
    return nullptr;
  } break;
  case DXIL::OpCode::IMad:
  case DXIL::OpCode::UMad: {
    Value *op0 = Args[DXIL::OperandIndex::kBinarySrc0OpIdx];
    Value *op2 = Args[DXIL::OperandIndex::kTrinarySrc2OpIdx];
    Constant *zero = ConstantInt::get(op0->getType(), 0);
    if (op0 == zero)
      return op2;
    Value *op1 = Args[DXIL::OperandIndex::kBinarySrc1OpIdx];
    if (op1 == zero)
      return op2;
    return nullptr;
  } break;
  }
}