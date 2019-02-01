///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLMatrixType.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/HLMatrixType.h"
#include "dxc/Support/Global.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

using namespace llvm;
using namespace hlsl;

HLMatrixType::HLMatrixType(Type *RegReprElemTy, unsigned NumRows, unsigned NumColumns)
  : RegReprElemTy(RegReprElemTy), NumRows(NumRows), NumColumns(NumColumns) {
  DXASSERT(RegReprElemTy != nullptr && (RegReprElemTy->isIntegerTy() || RegReprElemTy->isFloatingPointTy()),
    "Invalid matrix element type.");
  DXASSERT(NumRows >= 1 && NumRows <= 4 && NumColumns >= 1 && NumColumns <= 4,
    "Invalid matrix dimensions.");
}

Type *HLMatrixType::getElementType(bool MemRepr) const {
  // Bool i1s become i32s
  return MemRepr && RegReprElemTy->isIntegerTy(1)
    ? IntegerType::get(RegReprElemTy->getContext(), 32)
    : RegReprElemTy;
}

VectorType *HLMatrixType::getLoweredVectorType(bool MemRepr) const {
  return VectorType::get(getElementType(MemRepr), getNumElements());
}

Value *HLMatrixType::emitLoweredVectorMemToReg(Value *VecVal, IRBuilder<> &Builder) const {
  DXASSERT(VecVal->getType() == getLoweredVectorType(true), "Lowered matrix type mismatch.");
  if (RegReprElemTy->isIntegerTy(1)) {
    VecVal = Builder.CreateICmpNE(VecVal, Constant::getNullValue(VecVal->getType()), "tobool");
  }
  return VecVal;
}

Value *HLMatrixType::emitLoweredVectorRegToMem(Value *VecVal, IRBuilder<> &Builder) const {
  DXASSERT(VecVal->getType() == getLoweredVectorTypeForReg(), "Lowered matrix type mismatch.");
  if (RegReprElemTy->isIntegerTy(1)) {
    VecVal = Builder.CreateZExt(VecVal, getLoweredVectorTypeForMem(), "frombool");
  }
  return VecVal;
}

Value *HLMatrixType::emitLoweredVectorLoad(Value *VecPtr, IRBuilder<> &Builder) const {
  return emitLoweredVectorMemToReg(Builder.CreateLoad(VecPtr), Builder);
}

StoreInst *HLMatrixType::emitLoweredVectorStore(Value *VecVal, Value *VecPtr, IRBuilder<> &Builder) const {
  return Builder.CreateStore(emitLoweredVectorRegToMem(VecVal, Builder), VecPtr);
}

Value *HLMatrixType::emitLoweredVectorRowToCol(Value *VecVal, IRBuilder<> &Builder) const {
  DXASSERT(VecVal->getType() == getLoweredVectorTypeForReg(), "Lowered matrix type mismatch.");
  if (NumRows == 1 || NumColumns == 1) return VecVal;

  SmallVector<int, 16> ShuffleIndices;
  for (unsigned ColIdx = 0; ColIdx < NumColumns; ++ColIdx)
    for (unsigned RowIdx = 0; RowIdx < NumRows; ++RowIdx)
      ShuffleIndices.emplace_back(RowIdx * NumColumns + ColIdx);
  return Builder.CreateShuffleVector(VecVal, VecVal, ShuffleIndices, "row2col");
}

Value *HLMatrixType::emitLoweredVectorColToRow(Value *VecVal, IRBuilder<> &Builder) const {
  DXASSERT(VecVal->getType() == getLoweredVectorTypeForReg(), "Lowered matrix type mismatch.");
  if (NumRows == 1 || NumColumns == 1) return VecVal;

  SmallVector<int, 16> ShuffleIndices;
  for (unsigned RowIdx = 0; RowIdx < NumRows; ++RowIdx)
    for (unsigned ColIdx = 0; ColIdx < NumColumns; ++ColIdx)
      ShuffleIndices.emplace_back(RowIdx * NumColumns + ColIdx);
  return Builder.CreateShuffleVector(VecVal, VecVal, ShuffleIndices, "col2row");
}

bool HLMatrixType::isa(Type *Ty) {
  StructType *StructTy = llvm::dyn_cast<StructType>(Ty);
  return StructTy != nullptr && StructTy->getName().startswith(StructNamePrefix);
}

bool HLMatrixType::isMatrixPtr(Type *Ty) {
  PointerType *PtrTy = llvm::dyn_cast<PointerType>(Ty);
  return PtrTy && isa(PtrTy->getElementType());
}

bool HLMatrixType::isMatrixArrayPtr(Type *Ty) {
  PointerType *PtrTy = llvm::dyn_cast<PointerType>(Ty);
  if (PtrTy == nullptr) return false;
  ArrayType *ArrayTy = llvm::dyn_cast<ArrayType>(PtrTy->getElementType());
  if (ArrayTy == nullptr) return false;
  while (ArrayType *NestedArrayTy = llvm::dyn_cast<ArrayType>(ArrayTy->getElementType()))
    ArrayTy = NestedArrayTy;
  return isa(ArrayTy->getElementType());
}

bool HLMatrixType::isMatrixPtrOrArrayPtr(Type *Ty) {
  PointerType *PtrTy = llvm::dyn_cast<PointerType>(Ty);
  if (PtrTy == nullptr) return false;
  Ty = PtrTy->getElementType();
  while (ArrayType *ArrayTy = llvm::dyn_cast<ArrayType>(Ty))
    Ty = Ty->getArrayElementType();
  return isa(Ty);
}

bool HLMatrixType::isMatrixOrPtrOrArrayPtr(Type *Ty) {
  if (PointerType *PtrTy = llvm::dyn_cast<PointerType>(Ty)) Ty = PtrTy->getElementType();
  while (ArrayType *ArrayTy = llvm::dyn_cast<ArrayType>(Ty)) Ty = ArrayTy->getElementType();
  return isa(Ty);
}

HLMatrixType HLMatrixType::cast(Type *Ty) {
  DXASSERT_NOMSG(isa(Ty));
  StructType *StructTy = llvm::cast<StructType>(Ty);
  DXASSERT_NOMSG(Ty->getNumContainedTypes() == 1);
  ArrayType *RowArrayTy = llvm::cast<ArrayType>(StructTy->getElementType(0));
  DXASSERT_NOMSG(RowArrayTy->getNumElements() >= 1 && RowArrayTy->getNumElements() <= 4);
  VectorType *RowTy = llvm::cast<VectorType>(RowArrayTy->getElementType());
  DXASSERT_NOMSG(RowTy->getNumElements() >= 1 && RowTy->getNumElements() <= 4);
  return HLMatrixType(RowTy->getElementType(), RowArrayTy->getNumElements(), RowTy->getNumElements());
}

HLMatrixType HLMatrixType::dyn_cast(Type *Ty) {
  return isa(Ty) ? cast(Ty) : HLMatrixType();
}