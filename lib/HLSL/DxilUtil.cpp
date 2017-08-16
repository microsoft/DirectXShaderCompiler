///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilUtil.cpp                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Dxil helper functions.                                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


#include "llvm/IR/GlobalVariable.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/DxilUtil.h"
#include "llvm/IR/Module.h"

using namespace llvm;
using namespace hlsl;

namespace hlsl {

namespace dxilutil {

Type *GetArrayEltTy(Type *Ty) {
  if (isa<PointerType>(Ty))
    Ty = Ty->getPointerElementType();
  while (isa<ArrayType>(Ty)) {
    Ty = Ty->getArrayElementType();
  }
  return Ty;
}

unsigned
GetLegacyCBufferFieldElementSize(DxilFieldAnnotation &fieldAnnotation,
                                           llvm::Type *Ty,
                                           DxilTypeSystem &typeSys, bool useStrictPrecision) {
  while (isa<ArrayType>(Ty)) {
    Ty = Ty->getArrayElementType();
  }

  // Bytes.
  CompType compType = fieldAnnotation.GetCompType();
  unsigned compSize = compType.Is64Bit() ? 8 : compType.Is16Bit() && useStrictPrecision ? 2 : 4;
  unsigned fieldSize = compSize;
  if (Ty->isVectorTy()) {
    fieldSize *= Ty->getVectorNumElements();
  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    DxilStructAnnotation *EltAnnotation = typeSys.GetStructAnnotation(ST);
    if (EltAnnotation) {
      fieldSize = EltAnnotation->GetCBufferSize();
    } else {
      // Calculate size when don't have annotation.
      if (fieldAnnotation.HasMatrixAnnotation()) {
        const DxilMatrixAnnotation &matAnnotation =
            fieldAnnotation.GetMatrixAnnotation();
        unsigned rows = matAnnotation.Rows;
        unsigned cols = matAnnotation.Cols;
        if (matAnnotation.Orientation == MatrixOrientation::ColumnMajor) {
          rows = cols;
          cols = matAnnotation.Rows;
        } else if (matAnnotation.Orientation != MatrixOrientation::RowMajor) {
          // Invalid matrix orientation.
          fieldSize = 0;
        }
        fieldSize = (rows - 1) * 16 + cols * 4;
      } else {
        // Cannot find struct annotation.
        fieldSize = 0;
      }
    }
  }
  return fieldSize;
}

bool IsStaticGlobal(GlobalVariable *GV) {
  return GV->getLinkage() == GlobalValue::LinkageTypes::InternalLinkage &&
         GV->getType()->getPointerAddressSpace() == DXIL::kDefaultAddrSpace;
}

bool IsSharedMemoryGlobal(llvm::GlobalVariable *GV) {
  return GV->getType()->getPointerAddressSpace() == DXIL::kTGSMAddrSpace;
}

bool RemoveUnusedFunctions(Module &M, Function *EntryFunc,
                           Function *PatchConstantFunc, bool IsLib) {
  std::vector<Function *> deadList;
  for (auto &F : M.functions()) {
    if (&F == EntryFunc || &F == PatchConstantFunc)
      continue;
    if (F.isDeclaration() || !IsLib) {
      if (F.user_empty())
        deadList.emplace_back(&F);
    }
  }
  bool bUpdated = deadList.size();
  for (Function *F : deadList)
    F->eraseFromParent();
  return bUpdated;
}
}

}