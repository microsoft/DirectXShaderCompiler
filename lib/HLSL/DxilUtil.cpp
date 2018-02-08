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
#include "dxc/HLSL/DxilModule.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"

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

bool HasDynamicIndexing(Value *V) {
  for (auto User : V->users()) {
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(User)) {
      for (auto Idx = GEP->idx_begin(); Idx != GEP->idx_end(); ++Idx) {
        if (!isa<ConstantInt>(Idx))
          return true;
      }
    }
  }
  return false;
}

unsigned
GetLegacyCBufferFieldElementSize(DxilFieldAnnotation &fieldAnnotation,
                                           llvm::Type *Ty,
                                           DxilTypeSystem &typeSys) {

  while (isa<ArrayType>(Ty)) {
    Ty = Ty->getArrayElementType();
  }

  // Bytes.
  CompType compType = fieldAnnotation.GetCompType();
  unsigned compSize = compType.Is64Bit() ? 8 : compType.Is16Bit() && !typeSys.UseMinPrecision() ? 2 : 4;
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

void PrintDiagnosticHandler(const llvm::DiagnosticInfo &DI, void *Context) {
  DiagnosticPrinter *printer = reinterpret_cast<DiagnosticPrinter *>(Context);
  DI.print(*printer);
}

std::unique_ptr<llvm::Module> LoadModuleFromBitcode(llvm::MemoryBuffer *MB,
  llvm::LLVMContext &Ctx,
  std::string &DiagStr) {
  raw_string_ostream DiagStream(DiagStr);
  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  Ctx.setDiagnosticHandler(PrintDiagnosticHandler, &DiagPrinter, true);
  ErrorOr<std::unique_ptr<llvm::Module>> pModule(
    llvm::parseBitcodeFile(MB->getMemBufferRef(), Ctx));
  if (std::error_code ec = pModule.getError()) {
    return nullptr;
  }
  return std::unique_ptr<llvm::Module>(pModule.get().release());
}

std::unique_ptr<llvm::Module> LoadModuleFromBitcode(llvm::StringRef BC,
  llvm::LLVMContext &Ctx,
  std::string &DiagStr) {
  std::unique_ptr<llvm::MemoryBuffer> pBitcodeBuf(
    llvm::MemoryBuffer::getMemBuffer(BC, "", false));
  return LoadModuleFromBitcode(pBitcodeBuf.get(), Ctx, DiagStr);
}

static const StringRef kResourceMapErrorMsg =
    "local resource not guaranteed to map to unique global resource.";
void EmitResMappingError(Instruction *Res) {
  const DebugLoc &DL = Res->getDebugLoc();
  if (DL.get()) {
    Res->getContext().emitError("line:" + std::to_string(DL.getLine()) +
                                " col:" + std::to_string(DL.getCol()) + " " +
                                Twine(kResourceMapErrorMsg));
  } else {
    Res->getContext().emitError(Twine(kResourceMapErrorMsg) +
                                " With /Zi to show more information.");
  }
}

Value *SelectOnOperation(llvm::Instruction *Inst, unsigned operandIdx) {
  Instruction *prototype = Inst;
  for (unsigned i = 0; i < prototype->getNumOperands(); i++) {
    if (i == operandIdx)
      continue;
    if (!isa<Constant>(prototype->getOperand(i)))
      return nullptr;
  }
  Value *V = prototype->getOperand(operandIdx);
  if (SelectInst *SI = dyn_cast<SelectInst>(V)) {
    IRBuilder<> Builder(SI);
    Instruction *trueClone = Inst->clone();
    trueClone->setOperand(operandIdx, SI->getTrueValue());
    Builder.Insert(trueClone);
    Instruction *falseClone = Inst->clone();
    falseClone->setOperand(operandIdx, SI->getFalseValue());
    Builder.Insert(falseClone);
    Value *newSel =
        Builder.CreateSelect(SI->getCondition(), trueClone, falseClone);
    return newSel;
  }

  if (PHINode *Phi = dyn_cast<PHINode>(V)) {
    Type *Ty = Inst->getType();
    unsigned numOperands = Phi->getNumOperands();
    IRBuilder<> Builder(Phi);
    PHINode *newPhi = Builder.CreatePHI(Ty, numOperands);
    for (unsigned i = 0; i < numOperands; i++) {
      BasicBlock *b = Phi->getIncomingBlock(i);
      Value *V = Phi->getIncomingValue(i);
      Instruction *iClone = Inst->clone();
      IRBuilder<> iBuilder(b->getTerminator()->getPrevNode());
      iClone->setOperand(operandIdx, V);
      iBuilder.Insert(iClone);
      newPhi->addIncoming(iClone, b);
    }
    return newPhi;
  }
  return nullptr;
}
}
}
