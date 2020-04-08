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


#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/Support/Global.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/IRBuilder.h"

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
  // Note: the DiagStr is not used.
  auto pModule = llvm::parseBitcodeFile(MB->getMemBufferRef(), Ctx);
  if (!pModule) {
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


DIGlobalVariable *FindGlobalVariableDebugInfo(GlobalVariable *GV,
                                              DebugInfoFinder &DbgInfoFinder) {
  struct GlobalFinder {
    GlobalVariable *GV;
    bool operator()(llvm::DIGlobalVariable *const arg) const {
      return arg->getVariable() == GV;
    }
  };
  GlobalFinder F = {GV};
  DebugInfoFinder::global_variable_iterator Found =
      std::find_if(DbgInfoFinder.global_variables().begin(),
                   DbgInfoFinder.global_variables().end(), F);
  if (Found != DbgInfoFinder.global_variables().end()) {
    return *Found;
  }
  return nullptr;
}

std::string FormatMessageAtLocation(const DebugLoc &DL, const Twine& Msg) {
  std::string locString;
  raw_string_ostream os(locString);
  DL.print(os);
  os << ": " << Msg;
  return os.str();
}

std::string FormatMessageInSubProgram(DISubprogram *DISP, const Twine& Msg) {
  std::string locString;
  raw_string_ostream os(locString);

  auto *Scope = cast<DIScope>(DISP->getScope());
  os << Scope->getFilename();
  os << ':' << DISP->getLine();
  os << ": " << Msg;
  return os.str();
}

std::string FormatMessageInVariable(DIVariable *DIV, const Twine& Msg) {
  std::string locString;
  raw_string_ostream os(locString);

  auto *Scope = cast<DIScope>(DIV->getScope());
  os << Scope->getFilename();
  os << ':' << DIV->getLine();
  os << ": " << Msg;
  return os.str();
}

Twine FormatMessageWithoutLocation(const Twine& Msg) {
  return Msg + " Use /Zi for source location.";
}

static void EmitWarningOrErrorOnInstruction(Instruction *I, Twine Msg,
                                            bool bWarning);

// If we don't have debug location and this is select/phi,
// try recursing users to find instruction with debug info.
// Only recurse phi/select and limit depth to prevent doing
// too much work if no debug location found.
static bool EmitWarningOrErrorOnInstructionFollowPhiSelect(Instruction *I,
                                                           Twine Msg,
                                                           bool bWarning,
                                                           unsigned depth = 0) {
  if (depth > 4)
    return false;
  if (I->getDebugLoc().get()) {
    EmitWarningOrErrorOnInstruction(I, Msg, bWarning);
    return true;
  }
  if (isa<PHINode>(I) || isa<SelectInst>(I)) {
    for (auto U : I->users())
      if (Instruction *UI = dyn_cast<Instruction>(U))
        if (EmitWarningOrErrorOnInstructionFollowPhiSelect(UI, Msg, bWarning,
                                                           depth + 1))
          return true;
  }
  return false;
}

static void EmitWarningOrErrorOnInstruction(Instruction *I, Twine Msg,
                                            bool bWarning) {
  const DebugLoc &DL = I->getDebugLoc();
  if (DL.get()) {
    if (bWarning)
      I->getContext().emitWarning(FormatMessageAtLocation(DL, Msg));
    else
      I->getContext().emitError(FormatMessageAtLocation(DL, Msg));
    return;
  } else if (isa<PHINode>(I) || isa<SelectInst>(I)) {
    if (EmitWarningOrErrorOnInstructionFollowPhiSelect(I, Msg, bWarning))
      return;
  }

  if (bWarning)
    I->getContext().emitWarning(FormatMessageWithoutLocation(Msg));
  else
    I->getContext().emitError(FormatMessageWithoutLocation(Msg));
}

void EmitErrorOnInstruction(Instruction *I, Twine Msg) {
  EmitWarningOrErrorOnInstruction(I, Msg, /*bWarning*/false);
}

void EmitWarningOnInstruction(Instruction *I, Twine Msg) {
  EmitWarningOrErrorOnInstruction(I, Msg, /*bWarning*/true);
}

static void EmitWarningOrErrorOnFunction(Function *F, Twine Msg,
                                         bool bWarning) {
  DISubprogram *DISP = getDISubprogram(F);
  if (DISP) {
    if (bWarning)
      F->getContext().emitWarning(FormatMessageInSubProgram(DISP, Msg));
    else
      F->getContext().emitError(FormatMessageInSubProgram(DISP, Msg));
    return;
  }

  if (bWarning)
    F->getContext().emitWarning(FormatMessageWithoutLocation(Msg));
  else
    F->getContext().emitError(FormatMessageWithoutLocation(Msg));
}

void EmitErrorOnFunction(Function *F, Twine Msg) {
  EmitWarningOrErrorOnFunction(F, Msg, /*bWarning*/false);
}

void EmitWarningOnFunction(Function *F, Twine Msg) {
  EmitWarningOrErrorOnFunction(F, Msg, /*bWarning*/true);
}

static void EmitWarningOrErrorOnGlobalVariable(GlobalVariable *GV,
                                               Twine Msg, bool bWarning) {
  DIVariable *DIV = nullptr;
  if (GV)
    DIV = FindGlobalVariableDebugInfo(GV, GV->getParent()->GetDxilModule().GetOrCreateDebugInfoFinder());
  if (DIV) {
    if (bWarning)
      GV->getContext().emitWarning(FormatMessageInVariable(DIV, Msg));
    else
      GV->getContext().emitError(FormatMessageInVariable(DIV, Msg));
    return;
  }

  if (bWarning)
    GV->getContext().emitWarning(FormatMessageWithoutLocation(Msg));
  else
    GV->getContext().emitError(FormatMessageWithoutLocation(Msg));
}

void EmitErrorOnGlobalVariable(GlobalVariable *GV, Twine Msg) {
  EmitWarningOrErrorOnGlobalVariable(GV, Msg, /*bWarning*/false);
}

void EmitWarningOnGlobalVariable(GlobalVariable *GV, Twine Msg) {
  EmitWarningOrErrorOnGlobalVariable(GV, Msg, /*bWarning*/true);
}


const char *kResourceMapErrorMsg =
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

bool SimplifyTrivialPHIs(BasicBlock *BB) {
  bool Changed = false;
  SmallVector<Instruction *, 16> Removed;
  for (Instruction &I : *BB) {
    PHINode *PN = dyn_cast<PHINode>(&I);
    if (!PN)
      continue;

    if (PN->getNumIncomingValues() == 1) {
      Value *V = PN->getIncomingValue(0);
      PN->replaceAllUsesWith(V);
      Removed.push_back(PN);
      Changed = true;
    }
  }

  for (Instruction *I : Removed)
    I->eraseFromParent();

  return Changed;
}

static DbgValueInst *FindDbgValueInst(Value *Val) {
  if (auto *ValAsMD = LocalAsMetadata::getIfExists(Val)) {
    if (auto *ValMDAsVal = MetadataAsValue::getIfExists(Val->getContext(), ValAsMD)) {
      for (User *ValMDUser : ValMDAsVal->users()) {
        if (DbgValueInst *DbgValInst = dyn_cast<DbgValueInst>(ValMDUser))
          return DbgValInst;
      }
    }
  }
  return nullptr;
}

void MigrateDebugValue(Value *Old, Value *New) {
  DbgValueInst *DbgValInst = FindDbgValueInst(Old);
  if (DbgValInst == nullptr) return;
  
  DbgValInst->setOperand(0, MetadataAsValue::get(New->getContext(), ValueAsMetadata::get(New)));

  // Move the dbg value after the new instruction
  if (Instruction *NewInst = dyn_cast<Instruction>(New)) {
    if (NewInst->getNextNode() != DbgValInst) {
      DbgValInst->removeFromParent();
      DbgValInst->insertAfter(NewInst);
    }
  }
}

// Propagates any llvm.dbg.value instruction for a given vector
// to the elements that were used to create it through a series
// of insertelement instructions.
//
// This is used after lowering a vector-returning intrinsic.
// If we just keep the debug info on the recomposed vector,
// we will lose it when we break it apart again during later
// optimization stages.
void TryScatterDebugValueToVectorElements(Value *Val) {
  if (!isa<InsertElementInst>(Val) || !Val->getType()->isVectorTy()) return;

  DbgValueInst *VecDbgValInst = FindDbgValueInst(Val);
  if (VecDbgValInst == nullptr) return;

  Type *ElemTy = Val->getType()->getVectorElementType();
  DIBuilder DbgInfoBuilder(*VecDbgValInst->getModule());
  unsigned ElemSizeInBits = VecDbgValInst->getModule()->getDataLayout().getTypeSizeInBits(ElemTy);

  DIExpression *ParentBitPiece = VecDbgValInst->getExpression();
  if (ParentBitPiece != nullptr && !ParentBitPiece->isBitPiece())
    ParentBitPiece = nullptr;

  while (InsertElementInst *InsertElt = dyn_cast<InsertElementInst>(Val)) {
    Value *NewElt = InsertElt->getOperand(1);
    unsigned EltIdx = static_cast<unsigned>(cast<ConstantInt>(InsertElt->getOperand(2))->getLimitedValue());
    unsigned OffsetInBits = EltIdx * ElemSizeInBits;

    if (ParentBitPiece) {
      assert(OffsetInBits + ElemSizeInBits <= ParentBitPiece->getBitPieceSize()
        && "Nested bit piece expression exceeds bounds of its parent.");
      OffsetInBits += ParentBitPiece->getBitPieceOffset();
    }

    DIExpression *DIExpr = DbgInfoBuilder.createBitPieceExpression(OffsetInBits, ElemSizeInBits);
    // Offset is basically unused and deprecated in later LLVM versions.
    // Emit it as zero otherwise later versions of the bitcode reader will drop the intrinsic.
    DbgInfoBuilder.insertDbgValueIntrinsic(NewElt, /* Offset */ 0, VecDbgValInst->getVariable(),
      DIExpr, VecDbgValInst->getDebugLoc(), InsertElt);
    Val = InsertElt->getOperand(0);
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

llvm::Instruction *SkipAllocas(llvm::Instruction *I) {
  // Step past any allocas:
  while (I && (isa<AllocaInst>(I) || isa<DbgInfoIntrinsic>(I)))
    I = I->getNextNode();
  return I;
}
llvm::Instruction *FindAllocaInsertionPt(llvm::BasicBlock* BB) {
  return &*BB->getFirstInsertionPt();
}
llvm::Instruction *FindAllocaInsertionPt(llvm::Function* F) {
  return FindAllocaInsertionPt(&F->getEntryBlock());
}
llvm::Instruction *FindAllocaInsertionPt(llvm::Instruction* I) {
  Function *F = I->getParent()->getParent();
  if (F)
    return FindAllocaInsertionPt(F);
  else // BB with no parent function
    return FindAllocaInsertionPt(I->getParent());
}
llvm::Instruction *FirstNonAllocaInsertionPt(llvm::Instruction* I) {
  return SkipAllocas(FindAllocaInsertionPt(I));
}
llvm::Instruction *FirstNonAllocaInsertionPt(llvm::BasicBlock* BB) {
  return SkipAllocas(FindAllocaInsertionPt(BB));
}
llvm::Instruction *FirstNonAllocaInsertionPt(llvm::Function* F) {
  return SkipAllocas(FindAllocaInsertionPt(F));
}

static bool ConsumePrefix(StringRef &Str, StringRef Prefix) {
  if (!Str.startswith(Prefix)) return false;
  Str = Str.substr(Prefix.size());
  return true;
}

bool IsResourceSingleComponent(Type *Ty) {
  if (llvm::ArrayType *arrType = llvm::dyn_cast<llvm::ArrayType>(Ty)) {
    if (arrType->getArrayNumElements() > 1) {
      return false;
    }
    return IsResourceSingleComponent(arrType->getArrayElementType());
  } else if (llvm::StructType *structType =
                 llvm::dyn_cast<llvm::StructType>(Ty)) {
    if (structType->getStructNumElements() > 1) {
      return false;
    }
    return IsResourceSingleComponent(structType->getStructElementType(0));
  } else if (llvm::VectorType *vectorType =
                 llvm::dyn_cast<llvm::VectorType>(Ty)) {
    if (vectorType->getNumElements() > 1) {
      return false;
    }
    return IsResourceSingleComponent(vectorType->getVectorElementType());
  }
  return true;
}

bool IsHLSLResourceType(llvm::Type *Ty) {
  if (llvm::StructType *ST = dyn_cast<llvm::StructType>(Ty)) {
    if (!ST->hasName())
      return false;
    StringRef name = ST->getName();
    ConsumePrefix(name, "class.");
    ConsumePrefix(name, "struct.");

    if (name == "SamplerState")
      return true;
    if (name == "SamplerComparisonState")
      return true;

    if (name.startswith("AppendStructuredBuffer<"))
      return true;
    if (name.startswith("ConsumeStructuredBuffer<"))
      return true;

    if (name.startswith("ConstantBuffer<"))
      return true;

    if (name == "RaytracingAccelerationStructure")
      return true;

    if (ConsumePrefix(name, "FeedbackTexture2D")) {
      ConsumePrefix(name, "Array");
      return name.startswith("<");
    }

    ConsumePrefix(name, "RasterizerOrdered");
    ConsumePrefix(name, "RW");
    if (name == "ByteAddressBuffer")
      return true;

    if (name.startswith("Buffer<"))
      return true;
    if (name.startswith("StructuredBuffer<"))
      return true;

    if (ConsumePrefix(name, "Texture")) {
      if (name.startswith("1D<"))
        return true;
      if (name.startswith("1DArray<"))
        return true;
      if (name.startswith("2D<"))
        return true;
      if (name.startswith("2DArray<"))
        return true;
      if (name.startswith("3D<"))
        return true;
      if (name.startswith("Cube<"))
        return true;
      if (name.startswith("CubeArray<"))
        return true;
      if (name.startswith("2DMS<"))
        return true;
      if (name.startswith("2DMSArray<"))
        return true;
      return false;
    }
  }
  return false;
}

bool IsHLSLObjectType(llvm::Type *Ty) {
  if (llvm::StructType *ST = dyn_cast<llvm::StructType>(Ty)) {
    if (!ST->hasName()) {
      return false;
    }

    StringRef name = ST->getName();
    // TODO: don't check names.
    if (name.startswith("dx.types.wave_t"))
      return true;

    if (name.endswith("_slice_type"))
      return false;

    if (IsHLSLResourceType(Ty))
      return true;

    ConsumePrefix(name, "class.");
    ConsumePrefix(name, "struct.");

    if (name.startswith("TriangleStream<"))
      return true;
    if (name.startswith("PointStream<"))
      return true;
    if (name.startswith("LineStream<"))
      return true;
  }
  return false;
}

bool IsHLSLRayQueryType(llvm::Type *Ty) {
  if (llvm::StructType *ST = dyn_cast<llvm::StructType>(Ty)) {
    if (!ST->hasName())
      return false;
    StringRef name = ST->getName();
    // TODO: don't check names.
    ConsumePrefix(name, "class.");
    if (name.startswith("RayQuery<"))
      return true;
  }
  return false;
}

bool IsHLSLResourceDescType(llvm::Type *Ty) {
  if (llvm::StructType *ST = dyn_cast<llvm::StructType>(Ty)) {
    if (!ST->hasName())
      return false;
    StringRef name = ST->getName();

    // TODO: don't check names.
    if (name == ("struct..Resource"))
      return true;
  }
  return false;
}

bool IsIntegerOrFloatingPointType(llvm::Type *Ty) {
  return Ty->isIntegerTy() || Ty->isFloatingPointTy();
}

bool ContainsHLSLObjectType(llvm::Type *Ty) {
  // Unwrap pointer/array
  while (llvm::isa<llvm::PointerType>(Ty))
    Ty = llvm::cast<llvm::PointerType>(Ty)->getPointerElementType();
  while (llvm::isa<llvm::ArrayType>(Ty))
    Ty = llvm::cast<llvm::ArrayType>(Ty)->getArrayElementType();

  if (llvm::StructType *ST = llvm::dyn_cast<llvm::StructType>(Ty)) {
    if (ST->hasName() && ST->getName().startswith("dx.types."))
      return true;
    // TODO: How is this suppoed to check for Input/OutputPatch types if
    // these have already been eliminated in function arguments during CG?
    if (IsHLSLObjectType(Ty))
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

llvm::Type* StripArrayTypes(llvm::Type *Ty, llvm::SmallVectorImpl<unsigned> *OuterToInnerLengths) {
  DXASSERT_NOMSG(Ty);
  while (Ty->isArrayTy()) {
    if (OuterToInnerLengths) {
      OuterToInnerLengths->push_back(Ty->getArrayNumElements());
    }
    Ty = Ty->getArrayElementType();
  }
  return Ty;
}
llvm::Type* WrapInArrayTypes(llvm::Type *Ty, llvm::ArrayRef<unsigned> OuterToInnerLengths) {
  DXASSERT_NOMSG(Ty);
  for (auto it = OuterToInnerLengths.rbegin(), E = OuterToInnerLengths.rend(); it != E; ++it) {
    Ty = ArrayType::get(Ty, *it);
  }
  return Ty;
}

namespace {
// Create { v0, v1 } from { v0.lo, v0.hi, v1.lo, v1.hi }
void Make64bitResultForLoad(Type *EltTy, ArrayRef<Value *> resultElts32,
                            unsigned size, MutableArrayRef<Value *> resultElts,
                            hlsl::OP *hlslOP, IRBuilder<> &Builder) {
  Type *i64Ty = Builder.getInt64Ty();
  Type *doubleTy = Builder.getDoubleTy();
  if (EltTy == doubleTy) {
    Function *makeDouble =
        hlslOP->GetOpFunc(DXIL::OpCode::MakeDouble, doubleTy);
    Value *makeDoubleOpArg =
        Builder.getInt32((unsigned)DXIL::OpCode::MakeDouble);
    for (unsigned i = 0; i < size; i++) {
      Value *lo = resultElts32[2 * i];
      Value *hi = resultElts32[2 * i + 1];
      Value *V = Builder.CreateCall(makeDouble, {makeDoubleOpArg, lo, hi});
      resultElts[i] = V;
    }
  } else {
    for (unsigned i = 0; i < size; i++) {
      Value *lo = resultElts32[2 * i];
      Value *hi = resultElts32[2 * i + 1];
      lo = Builder.CreateZExt(lo, i64Ty);
      hi = Builder.CreateZExt(hi, i64Ty);
      hi = Builder.CreateShl(hi, 32);
      resultElts[i] = Builder.CreateOr(lo, hi);
    }
  }
}

// Split { v0, v1 } to { v0.lo, v0.hi, v1.lo, v1.hi }
void Split64bitValForStore(Type *EltTy, ArrayRef<Value *> vals, unsigned size,
                           MutableArrayRef<Value *> vals32, hlsl::OP *hlslOP,
                           IRBuilder<> &Builder) {
  Type *i32Ty = Builder.getInt32Ty();
  Type *doubleTy = Builder.getDoubleTy();
  Value *undefI32 = UndefValue::get(i32Ty);

  if (EltTy == doubleTy) {
    Function *dToU = hlslOP->GetOpFunc(DXIL::OpCode::SplitDouble, doubleTy);
    Value *dToUOpArg = Builder.getInt32((unsigned)DXIL::OpCode::SplitDouble);
    for (unsigned i = 0; i < size; i++) {
      if (isa<UndefValue>(vals[i])) {
        vals32[2 * i] = undefI32;
        vals32[2 * i + 1] = undefI32;
      } else {
        Value *retVal = Builder.CreateCall(dToU, {dToUOpArg, vals[i]});
        Value *lo = Builder.CreateExtractValue(retVal, 0);
        Value *hi = Builder.CreateExtractValue(retVal, 1);
        vals32[2 * i] = lo;
        vals32[2 * i + 1] = hi;
      }
    }
  } else {
    for (unsigned i = 0; i < size; i++) {
      if (isa<UndefValue>(vals[i])) {
        vals32[2 * i] = undefI32;
        vals32[2 * i + 1] = undefI32;
      } else {
        Value *lo = Builder.CreateTrunc(vals[i], i32Ty);
        Value *hi = Builder.CreateLShr(vals[i], 32);
        hi = Builder.CreateTrunc(hi, i32Ty);
        vals32[2 * i] = lo;
        vals32[2 * i + 1] = hi;
      }
    }
  }
}
}

llvm::CallInst *TranslateCallRawBufferLoadToBufferLoad(
    llvm::CallInst *CI, llvm::Function *newFunction, hlsl::OP *op) {
  IRBuilder<> Builder(CI);
  SmallVector<Value *, 4> args;
  args.emplace_back(op->GetI32Const((unsigned)DXIL::OpCode::BufferLoad));
  for (unsigned i = 1; i < 4; ++i) {
    args.emplace_back(CI->getArgOperand(i));
  }
  CallInst *newCall = Builder.CreateCall(newFunction, args);
  return newCall;
}

void ReplaceRawBufferLoadWithBufferLoad(
    llvm::Function *F, hlsl::OP *op) {
  Type *RTy = F->getReturnType();
  if (StructType *STy = dyn_cast<StructType>(RTy)) {
    Type *ETy = STy->getElementType(0);
    Function *newFunction = op->GetOpFunc(hlsl::DXIL::OpCode::BufferLoad, ETy);
    for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
      User *user = *(U++);
      if (CallInst *CI = dyn_cast<CallInst>(user)) {
        CallInst *newCall = TranslateCallRawBufferLoadToBufferLoad(CI, newFunction, op);
        CI->replaceAllUsesWith(newCall);
        CI->eraseFromParent();
      } else {
        DXASSERT(false, "function can only be used with call instructions.");
      }
    }
  } else {
    DXASSERT(false, "RawBufferLoad should return struct type.");
  }
}


llvm::CallInst *TranslateCallRawBufferStoreToBufferStore(
    llvm::CallInst *CI, llvm::Function *newFunction, hlsl::OP *op) {
  IRBuilder<> Builder(CI);
  SmallVector<Value *, 4> args;
  args.emplace_back(op->GetI32Const((unsigned)DXIL::OpCode::BufferStore));
  for (unsigned i = 1; i < 9; ++i) {
    args.emplace_back(CI->getArgOperand(i));
  }
  CallInst *newCall = Builder.CreateCall(newFunction, args);
  return newCall;
}

void ReplaceRawBufferStoreWithBufferStore(llvm::Function *F, hlsl::OP *op) {
  DXASSERT(F->getReturnType()->isVoidTy(), "rawBufferStore should return a void type.");
  Type *ETy = F->getFunctionType()->getParamType(4); // value
  Function *newFunction = op->GetOpFunc(hlsl::DXIL::OpCode::BufferStore, ETy);
  for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
    User *user = *(U++);
    if (CallInst *CI = dyn_cast<CallInst>(user)) {
      TranslateCallRawBufferStoreToBufferStore(CI, newFunction, op);
      CI->eraseFromParent();
    }
    else {
      DXASSERT(false, "function can only be used with call instructions.");
    }
  }
}


void ReplaceRawBufferLoad64Bit(llvm::Function *F, llvm::Type *EltTy, hlsl::OP *hlslOP) {
  Function *bufLd = hlslOP->GetOpFunc(DXIL::OpCode::RawBufferLoad,
                                      Type::getInt32Ty(hlslOP->GetCtx()));
  for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
    User *user = *(U++);
    if (CallInst *CI = dyn_cast<CallInst>(user)) {
      IRBuilder<> Builder(CI);
      SmallVector<Value *, 4> args(CI->arg_operands());

      Value *offset = CI->getArgOperand(
          DXIL::OperandIndex::kRawBufferLoadElementOffsetOpIdx);

      unsigned size = 0;
      bool bNeedStatus = false;
      for (User *U : CI->users()) {
        ExtractValueInst *Elt = cast<ExtractValueInst>(U);
        DXASSERT(Elt->getNumIndices() == 1, "else invalid use for resRet");
        unsigned idx = Elt->getIndices()[0];
        if (idx == 4) {
          bNeedStatus = true;
        } else {
          size = std::max(size, idx+1);
        }
      }
      unsigned maskHi = 0;
      unsigned maskLo = 0;
      switch (size) {
      case 1:
        maskLo = 3;
        break;
      case 2:
        maskLo = 0xf;
        break;
      case 3:
        maskLo = 0xf;
        maskHi = 3;
        break;
      case 4:
        maskLo = 0xf;
        maskHi = 0xf;
        break;
      }

      args[DXIL::OperandIndex::kRawBufferLoadMaskOpIdx] =
          Builder.getInt8(maskLo);
      Value *resultElts[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
      CallInst *newLd = Builder.CreateCall(bufLd, args);

      Value *resultElts32[8];
      unsigned eltBase = 0;
      for (unsigned i = 0; i < size; i++) {
        if (i == 2) {
          // Update offset 4 by 4 bytes.
          if (isa<UndefValue>(offset)) {
            // [RW]ByteAddressBuffer has undef element offset -> update index
            Value *index = CI->getArgOperand(DXIL::OperandIndex::kRawBufferLoadIndexOpIdx);
            args[DXIL::OperandIndex::kRawBufferLoadIndexOpIdx] =
              Builder.CreateAdd(index, Builder.getInt32(4 * 4));
          }
          else {
            // [RW]StructuredBuffer -> update element offset
            args[DXIL::OperandIndex::kRawBufferLoadElementOffsetOpIdx] =
              Builder.CreateAdd(offset, Builder.getInt32(4 * 4));
          }
          args[DXIL::OperandIndex::kRawBufferLoadMaskOpIdx] =
              Builder.getInt8(maskHi);
          newLd = Builder.CreateCall(bufLd, args);
          eltBase = 4;
        }
        unsigned resBase = 2 * i;
        resultElts32[resBase] =
            Builder.CreateExtractValue(newLd, resBase - eltBase);
        resultElts32[resBase + 1] =
            Builder.CreateExtractValue(newLd, resBase + 1 - eltBase);
      }

      Make64bitResultForLoad(EltTy, resultElts32, size, resultElts, hlslOP, Builder);
      if (bNeedStatus) {
        resultElts[4] = Builder.CreateExtractValue(newLd, 4);
      }
      for (auto it = CI->user_begin(); it != CI->user_end(); ) {
        ExtractValueInst *Elt = cast<ExtractValueInst>(*(it++));
        DXASSERT(Elt->getNumIndices() == 1, "else invalid use for resRet");
        unsigned idx = Elt->getIndices()[0];
        if (!Elt->user_empty()) {
          Value *newElt = resultElts[idx];
          Elt->replaceAllUsesWith(newElt);
        }
        Elt->eraseFromParent();
      }

      CI->eraseFromParent();
    } else {
      DXASSERT(false, "function can only be used with call instructions.");
    }
  }
}

void ReplaceRawBufferStore64Bit(llvm::Function *F, llvm::Type *ETy, hlsl::OP *hlslOP) {
  Function *newFunction = hlslOP->GetOpFunc(hlsl::DXIL::OpCode::RawBufferStore,
                                            Type::getInt32Ty(hlslOP->GetCtx()));
  for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
    User *user = *(U++);
    if (CallInst *CI = dyn_cast<CallInst>(user)) {
      IRBuilder<> Builder(CI);
      SmallVector<Value *, 4> args(CI->arg_operands());
      Value *vals[4] = {
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreVal0OpIdx),
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreVal1OpIdx),
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreVal2OpIdx),
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreVal3OpIdx)};
      ConstantInt *cMask = cast<ConstantInt>(
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreMaskOpIdx));
      Value *undefI32 = UndefValue::get(Builder.getInt32Ty());
      Value *vals32[8] = {undefI32, undefI32, undefI32, undefI32,
                          undefI32, undefI32, undefI32, undefI32};

      unsigned maskLo = 0;
      unsigned maskHi = 0;
      unsigned size = 0;
      unsigned mask = cMask->getLimitedValue();
      switch (mask) {
      case 1:
        maskLo = 3;
        size = 1;
        break;
      case 3:
        maskLo = 15;
        size = 2;
        break;
      case 7:
        maskLo = 15;
        maskHi = 3;
        size = 3;
        break;
      case 15:
        maskLo = 15;
        maskHi = 15;
        size = 4;
        break;
      default:
        DXASSERT(0, "invalid mask");
      }

      Split64bitValForStore(ETy, vals, size, vals32, hlslOP, Builder);
      args[DXIL::OperandIndex::kRawBufferStoreMaskOpIdx] =
          Builder.getInt8(maskLo);
      args[DXIL::OperandIndex::kRawBufferStoreVal0OpIdx] = vals32[0];
      args[DXIL::OperandIndex::kRawBufferStoreVal1OpIdx] = vals32[1];
      args[DXIL::OperandIndex::kRawBufferStoreVal2OpIdx] = vals32[2];
      args[DXIL::OperandIndex::kRawBufferStoreVal3OpIdx] = vals32[3];

      Builder.CreateCall(newFunction, args);

      if (maskHi) {
        // Update offset 4 by 4 bytes.
        Value *offset = args[DXIL::OperandIndex::kBufferStoreCoord1OpIdx];
        if (isa<UndefValue>(offset)) {
          // [RW]ByteAddressBuffer has element offset == undef -> update index instead
          Value *index = args[DXIL::OperandIndex::kBufferStoreCoord0OpIdx];
          index = Builder.CreateAdd(index, Builder.getInt32(4 * 4));
          args[DXIL::OperandIndex::kRawBufferStoreIndexOpIdx] = index;
        }
        else {
          // [RW]StructuredBuffer -> update element offset
          offset = Builder.CreateAdd(offset, Builder.getInt32(4 * 4));
          args[DXIL::OperandIndex::kRawBufferStoreElementOffsetOpIdx] = offset;
        }

        args[DXIL::OperandIndex::kRawBufferStoreMaskOpIdx] =
            Builder.getInt8(maskHi);
        args[DXIL::OperandIndex::kRawBufferStoreVal0OpIdx] = vals32[4];
        args[DXIL::OperandIndex::kRawBufferStoreVal1OpIdx] = vals32[5];
        args[DXIL::OperandIndex::kRawBufferStoreVal2OpIdx] = vals32[6];
        args[DXIL::OperandIndex::kRawBufferStoreVal3OpIdx] = vals32[7];

        Builder.CreateCall(newFunction, args);
      }
      CI->eraseFromParent();
    } else {
      DXASSERT(false, "function can only be used with call instructions.");
    }
  }
}

}
}

///////////////////////////////////////////////////////////////////////////////

namespace {
class DxilLoadMetadata : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilLoadMetadata () : ModulePass(ID) {}

  const char *getPassName() const override { return "HLSL load DxilModule from metadata"; }

  bool runOnModule(Module &M) override {
    if (!M.HasDxilModule()) {
      (void)M.GetOrCreateDxilModule();
      return true;
    }

    return false;
  }
};
}

char DxilLoadMetadata::ID = 0;

ModulePass *llvm::createDxilLoadMetadataPass() {
  return new DxilLoadMetadata();
}

INITIALIZE_PASS(DxilLoadMetadata, "hlsl-dxilload", "HLSL load DxilModule from metadata", false, false)
