///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilWaveMatrix.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilWaveMatrix.h"
#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/Support/Global.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace hlsl {

DxilWaveMatrixProperties
wavemat_helper::LoadInfoFromConstant(llvm::Constant *C) {
  DXASSERT(!isa<ConstantAggregateZero>(C),
           "otherwise, DxilWaveMatrixProperties has invalid value");
  const ConstantStruct *CS = cast<ConstantStruct>(C);
  DXASSERT(CS->getType()->getNumElements() == 4,
           "otherwise, struct is not expected layout");
  DxilWaveMatrixProperties info;
  info.kind = (DXIL::WaveMatrixKind)cast<ConstantInt>(CS->getOperand(0))
                  ->getLimitedValue();
  info.compType = (DXIL::ComponentType)cast<ConstantInt>(CS->getOperand(1))
                      ->getLimitedValue();
  info.dimM = (uint32_t)cast<ConstantInt>(CS->getOperand(2))->getLimitedValue();
  info.dimN = (uint32_t)cast<ConstantInt>(CS->getOperand(3))->getLimitedValue();
  return info;
}

Constant *
wavemat_helper::GetInfoConstantFromWaveMatPtr(llvm::Value *waveMatPtr) {
  DXASSERT_NOMSG(isa<AllocaInst>(waveMatPtr));
  for (auto *U : waveMatPtr->users()) {
    Instruction *I = cast<Instruction>(U);
    DxilInst_WaveMatrix_Annotate annotate(I);
    if (annotate) {
      DXASSERT_NOMSG(isa<Constant>(annotate.get_waveMatProps()));
      return cast<Constant>(annotate.get_waveMatProps());
    }
  }
  return nullptr;
}

DxilWaveMatrixProperties
wavemat_helper::GetInfoFromWaveMatPtr(llvm::Value *waveMatPtr) {
  Constant *infoC = wavemat_helper::GetInfoConstantFromWaveMatPtr(waveMatPtr);
  DXASSERT(infoC, "otherwise, no WaveMatAnnotate call found for ptr");
  return wavemat_helper::LoadInfoFromConstant(infoC);
}

llvm::Constant *
wavemat_helper::GetAsConstant(const DxilWaveMatrixProperties &info,
                              llvm::StructType *infoTy) {
  LLVMContext &Ctx = infoTy->getContext();
  IntegerType *i8Ty = IntegerType::get(Ctx, 8);
  IntegerType *i32Ty = IntegerType::get(Ctx, 32);
  return ConstantStruct::get(cast<StructType>(infoTy),
                             {ConstantInt::get(i8Ty, (unsigned)info.kind),
                              ConstantInt::get(i8Ty, (unsigned)info.compType),
                              ConstantInt::get(i32Ty, (unsigned)info.dimM),
                              ConstantInt::get(i32Ty, (unsigned)info.dimN)});
}

} // namespace hlsl
