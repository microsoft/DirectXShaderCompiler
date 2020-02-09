///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilResourceProperites.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilResourceProperties.h"
#include "llvm/IR/Constant.h"
#include "dxc/DXIL/DxilShaderModel.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
using namespace llvm;

namespace hlsl {
namespace resource_helper {
// Resource Class and Resource Kind is used as seperate parameter, other fileds
// are saved in constant.
// The constant is as struct with int32 fields.
// ShaderModel 6.6 has 2 fileds.
Constant *getAsConstant(const DxilResourceProperties &RP, Type *Ty,
                        const ShaderModel &) {
  StructType *ST = cast<StructType>(Ty);
  switch (ST->getNumElements()) {
  case 2: {
    Constant *RawDwords[] = {
        ConstantInt::get(ST->getElementType(0), RP.RawDword0),
        ConstantInt::get(ST->getElementType(1), RP.RawDword0)};
    return ConstantStruct::get(ST, RawDwords);
  } break;
  default:
    return nullptr;
    break;
  }
  return nullptr;
}

DxilResourceProperties loadFromConstant(const Constant &C,
                                        DXIL::ResourceClass RC,
                                        DXIL::ResourceKind RK, Type *Ty,
                                        const ShaderModel &) {
  DxilResourceProperties RP;
  RP.Class = RC;
  RP.Kind = RK;
  // Ty Should match C.getType().
  StructType *ST = cast<StructType>(Ty);
  switch (ST->getNumElements()) {
  case 2: {
    const ConstantStruct *CS = cast<ConstantStruct>(&C);
    const Constant *RawDword0 = CS->getOperand(0);
    const Constant *RawDword1 = CS->getOperand(1);
    RP.RawDword0 = cast<ConstantInt>(RawDword0)->getLimitedValue();
    RP.RawDword1 = cast<ConstantInt>(RawDword1)->getLimitedValue();
  } break;
  default:
    RP.Class = DXIL::ResourceClass::Invalid;
    break;
  }
  return RP;
}
} // namespace resource_helper
} // namespace hlsl
