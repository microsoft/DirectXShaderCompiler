///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLMatrixLowerHelper.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
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

class DxilFieldAnnotation;
class DxilTypeSystem;

namespace HLMatrixLower {
// TODO: use type annotation.
bool IsMatrixType(llvm::Type *Ty);
DxilFieldAnnotation *FindAnnotationFromMatUser(llvm::Value *Mat,
                                               DxilTypeSystem &typeSys);
// Translate matrix type to vector type.
llvm::Type *LowerMatrixType(llvm::Type *Ty);
// TODO: use type annotation.
llvm::Type *GetMatrixInfo(llvm::Type *Ty, unsigned &col, unsigned &row);
// TODO: use type annotation.
bool IsMatrixArrayPointer(llvm::Type *Ty);
// Translate matrix array pointer type to vector array pointer type.
llvm::Type *LowerMatrixArrayPointer(llvm::Type *Ty);

llvm::Value *BuildVector(llvm::Type *EltTy, unsigned size,
                         llvm::ArrayRef<llvm::Value *> elts,
                         llvm::IRBuilder<> &Builder);
// For case like mat[i][j].
// IdxList is [i][0], [i][1], [i][2],[i][3].
// Idx is j.
// return [i][j] not mat[i][j] because resource ptr and temp ptr need different
// code gen.
llvm::Value *
LowerGEPOnMatIndexListToIndex(llvm::GetElementPtrInst *GEP,
                              llvm::ArrayRef<llvm::Value *> IdxList);
unsigned GetColMajorIdx(unsigned r, unsigned c, unsigned row);
unsigned GetRowMajorIdx(unsigned r, unsigned c, unsigned col);
} // namespace HLMatrixLower

} // namespace hlsl