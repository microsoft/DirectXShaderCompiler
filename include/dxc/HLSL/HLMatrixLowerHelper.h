///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLMatrixLowerHelper.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file provides helper functions to lower high level matrix.           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "llvm/IR/IRBuilder.h"

namespace llvm {
  class Type;
  class Value;
  template<typename T>
  class ArrayRef;
}

namespace hlsl {

namespace HLMatrixLower {
// TODO: use type annotation.
bool IsMatrixType(llvm::Type *Ty);
// Translate matrix type to vector type.
llvm::Type *LowerMatrixType(llvm::Type *Ty);
// TODO: use type annotation.
llvm::Type *GetMatrixInfo(llvm::Type *Ty, unsigned &col, unsigned &row);
// TODO: use type annotation.
bool IsMatrixArrayPointer(llvm::Type *Ty);
// Translate matrix array pointer type to vector array pointer type.
llvm::Type *LowerMatrixArrayPointer(llvm::Type *Ty);

llvm::Value *BuildMatrix(llvm::Type *EltTy, unsigned col, unsigned row,
                   bool colMajor, llvm::ArrayRef<llvm::Value *> elts,
                   llvm::IRBuilder<> &Builder);

} // namespace HLMatrixLower

} // namespace hlsl