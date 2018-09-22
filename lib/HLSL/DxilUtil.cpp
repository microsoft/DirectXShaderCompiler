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
#include "dxc/HLSL/HLModule.h"
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
#include "dxc/Support/Global.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"

using namespace llvm;
using namespace hlsl;

namespace hlsl {

namespace dxilutil {

const char ManglingPrefix[] = "\01?";
const char EntryPrefix[] = "dx.entry.";

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

StringRef DemangleFunctionName(StringRef name) {
  if (!name.startswith(ManglingPrefix)) {
    // Name isn't mangled.
    return name;
  }

  size_t nameEnd = name.find_first_of("@");
  DXASSERT(nameEnd != StringRef::npos, "else Name isn't mangled but has \01?");

  return name.substr(2, nameEnd - 2);
}

std::string ReplaceFunctionName(StringRef originalName, StringRef newName) {
  if (originalName.startswith(ManglingPrefix)) {
    return (Twine(ManglingPrefix) + newName +
      originalName.substr(originalName.find_first_of('@'))).str();
  } else if (originalName.startswith(EntryPrefix)) {
    return (Twine(EntryPrefix) + newName).str();
  }
  return newName.str();
}

// From AsmWriter.cpp
// PrintEscapedString - Print each character of the specified string, escaping
// it if it is not printable or if it is an escape char.
void PrintEscapedString(StringRef Name, raw_ostream &Out) {
  for (unsigned i = 0, e = Name.size(); i != e; ++i) {
    unsigned char C = Name[i];
    if (isprint(C) && C != '\\' && C != '"')
      Out << C;
    else
      Out << '\\' << hexdigit(C >> 4) << hexdigit(C & 0x0F);
  }
}

void PrintUnescapedString(StringRef Name, raw_ostream &Out) {
  for (unsigned i = 0, e = Name.size(); i != e; ++i) {
    unsigned char C = Name[i];
    if (C == '\\') {
      C = Name[++i];
      unsigned value = hexDigitValue(C);
      if (value != -1U) {
        C = (unsigned char)value;
        unsigned value2 = hexDigitValue(Name[i+1]);
        assert(value2 != -1U && "otherwise, not a two digit hex escape");
        if (value2 != -1U) {
          C = (C << 4) + (unsigned char)value2;
          ++i;
        }
      } // else, the next character (in C) should be the escaped character
    }
    Out << C;
  }
}

std::unique_ptr<llvm::Module> LoadModuleFromBitcode(llvm::MemoryBuffer *MB,
  llvm::LLVMContext &Ctx,
  std::string &DiagStr) {
  raw_string_ostream DiagStream(DiagStr);
  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  LLVMContext::DiagnosticHandlerTy OrigHandler = Ctx.getDiagnosticHandler();
  void *OrigContext = Ctx.getDiagnosticContext();
  Ctx.setDiagnosticHandler(PrintDiagnosticHandler, &DiagPrinter, true);
  ErrorOr<std::unique_ptr<llvm::Module>> pModule(
    llvm::parseBitcodeFile(MB->getMemBufferRef(), Ctx));
  Ctx.setDiagnosticHandler(OrigHandler, OrigContext);
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

// If we don't have debug location and this is select/phi,
// try recursing users to find instruction with debug info.
// Only recurse phi/select and limit depth to prevent doing
// too much work if no debug location found.
static bool EmitErrorOnInstructionFollowPhiSelect(
    Instruction *I, StringRef Msg, unsigned depth=0) {
  if (depth > 4)
    return false;
  if (I->getDebugLoc().get()) {
    EmitErrorOnInstruction(I, Msg);
    return true;
  }
  if (isa<PHINode>(I) || isa<SelectInst>(I)) {
    for (auto U : I->users())
      if (Instruction *UI = dyn_cast<Instruction>(U))
        if (EmitErrorOnInstructionFollowPhiSelect(UI, Msg, depth+1))
          return true;
  }
  return false;
}

void EmitErrorOnInstruction(Instruction *I, StringRef Msg) {
  const DebugLoc &DL = I->getDebugLoc();
  if (DL.get()) {
    std::string locString;
    raw_string_ostream os(locString);
    DL.print(os);
    I->getContext().emitError(os.str() + ": " + Twine(Msg));
    return;
  } else if (isa<PHINode>(I) || isa<SelectInst>(I)) {
    if (EmitErrorOnInstructionFollowPhiSelect(I, Msg))
      return;
  }

  I->getContext().emitError(Twine(Msg) + " Use /Zi for source location.");
}

const StringRef kResourceMapErrorMsg =
    "local resource not guaranteed to map to unique global resource.";
void EmitResMappingError(Instruction *Res) {
  EmitErrorOnInstruction(Res, kResourceMapErrorMsg);
}

void CollectSelect(llvm::Instruction *Inst,
                   std::unordered_set<llvm::Instruction *> &selectSet) {
  unsigned startOpIdx = 0;
  // Skip Cond for Select.
  if (isa<SelectInst>(Inst)) {
    startOpIdx = 1;
  } else if (!isa<PHINode>(Inst)) {
    // Only check phi and select here.
    return;
  }
  // Already add.
  if (selectSet.count(Inst))
    return;

  selectSet.insert(Inst);

  // Scan operand to add node which is phi/select.
  unsigned numOperands = Inst->getNumOperands();
  for (unsigned i = startOpIdx; i < numOperands; i++) {
    Value *V = Inst->getOperand(i);
    if (Instruction *I = dyn_cast<Instruction>(V)) {
      CollectSelect(I, selectSet);
    }
  }
}

Value *MergeSelectOnSameValue(Instruction *SelInst, unsigned startOpIdx,
                            unsigned numOperands) {
  Value *op0 = nullptr;
  for (unsigned i = startOpIdx; i < numOperands; i++) {
    Value *op = SelInst->getOperand(i);
    if (i == startOpIdx) {
      op0 = op;
    } else {
      if (op0 != op)
        return nullptr;
    }
  }
  if (op0) {
    SelInst->replaceAllUsesWith(op0);
    SelInst->eraseFromParent();
  }
  return op0;
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

llvm::Instruction *SkipAllocas(llvm::Instruction *I) {
  // Step past any allocas:
  while (I && isa<AllocaInst>(I))
    I = I->getNextNode();
  return I;
}
llvm::Instruction *FindAllocaInsertionPt(llvm::Instruction* I) {
  Function *F = I->getParent()->getParent();
  if (F)
    return F->getEntryBlock().getFirstInsertionPt();
  else // BB with no parent function
    return I->getParent()->getFirstInsertionPt();
}
llvm::Instruction *FindAllocaInsertionPt(llvm::Function* F) {
  return F->getEntryBlock().getFirstInsertionPt();
}
llvm::Instruction *FirstNonAllocaInsertionPt(llvm::Instruction* I) {
  return SkipAllocas(FindAllocaInsertionPt(I));
}
llvm::Instruction *FirstNonAllocaInsertionPt(llvm::BasicBlock* BB) {
  return SkipAllocas(
    BB->getFirstInsertionPt());
}
llvm::Instruction *FirstNonAllocaInsertionPt(llvm::Function* F) {
  return SkipAllocas(
    F->getEntryBlock().getFirstInsertionPt());
}

bool ContainsHLSLObjectType(llvm::Type *Ty) {
  // Unwrap pointer/array
  while (llvm::isa<llvm::PointerType>(Ty))
    Ty = llvm::cast<llvm::PointerType>(Ty)->getPointerElementType();
  while (llvm::isa<llvm::ArrayType>(Ty))
    Ty = llvm::cast<llvm::ArrayType>(Ty)->getArrayElementType();

  if (llvm::StructType *ST = llvm::dyn_cast<llvm::StructType>(Ty)) {
    if (ST->getName().startswith("dx.types."))
      return true;
    // TODO: How is this suppoed to check for Input/OutputPatch types if
    // these have already been eliminated in function arguments during CG?
    if (HLModule::IsHLSLObjectType(Ty))
      return true;
    // Otherwise, recurse elements of UDT
    for (auto ETy : ST->elements()) {
      if (ContainsHLSLObjectType(ETy))
        return true;
    }
  }
  return false;
}

// Based on the implementation available in LLVM's trunk:
// http://llvm.org/doxygen/Constants_8cpp_source.html#l02734
bool IsSplat(llvm::ConstantDataVector *cdv) {
  const char *Base = cdv->getRawDataValues().data();

  // Compare elements 1+ to the 0'th element.
  unsigned EltSize = cdv->getElementByteSize();
  for (unsigned i = 1, e = cdv->getNumElements(); i != e; ++i)
    if (memcmp(Base, Base + i * EltSize, EltSize))
      return false;

  return true;
}

}
}
