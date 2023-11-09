///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilWaveMatrix.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// WaveMatrix related types and helpers.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "DxilConstants.h"

namespace llvm {
class Value;
class Constant;
class Type;
class StructType;
} // namespace llvm

namespace hlsl {

struct DxilWaveMatrixProperties {
  DXIL::WaveMatrixKind kind;
  DXIL::ComponentType compType;
  unsigned dimM, dimN;

  DxilWaveMatrixProperties()
      : kind(DXIL::WaveMatrixKind::NumKinds),
        compType(DXIL::ComponentType::Invalid), dimM(0), dimN(0) {}
  bool isValid() const { return kind < DXIL::WaveMatrixKind::NumKinds; }
  bool operator==(const DxilWaveMatrixProperties &other) {
    return kind == other.kind && compType == other.compType &&
           dimM == other.dimM && dimN == other.dimN;
  }
  bool operator!=(const DxilWaveMatrixProperties &other) {
    return !(*this == other);
  }
};

namespace wavemat_helper {

DxilWaveMatrixProperties LoadInfoFromConstant(llvm::Constant *C);
llvm::Constant *GetInfoConstantFromWaveMatPtr(llvm::Value *waveMatPtr);
DxilWaveMatrixProperties GetInfoFromWaveMatPtr(llvm::Value *waveMatPtr);
llvm::Constant *GetAsConstant(const DxilWaveMatrixProperties &info,
                              llvm::StructType *infoTy);

} // namespace wavemat_helper

} // namespace hlsl
