///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLMatrixLowerPass.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// HLMatrixLowerPass implementation.                                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/HLMatrixLowerPass.h"
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HLSL/HLMatrixType.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/Support/Global.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/DXIL/DxilModule.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_set>
#include <vector>

using namespace llvm;
using namespace hlsl;
using namespace hlsl::HLMatrixLower;
namespace hlsl {
namespace HLMatrixLower {

// If user is function call, return param annotation to get matrix major.
DxilFieldAnnotation *FindAnnotationFromMatUser(Value *Mat,
                                               DxilTypeSystem &typeSys) {
  for (User *U : Mat->users()) {
    if (CallInst *CI = dyn_cast<CallInst>(U)) {
      Function *F = CI->getCalledFunction();
      if (DxilFunctionAnnotation *Anno = typeSys.GetFunctionAnnotation(F)) {
        for (unsigned i = 0; i < CI->getNumArgOperands(); i++) {
          if (CI->getArgOperand(i) == Mat) {
            return &Anno->GetParameterAnnotation(i);
          }
        }
      }
    }
  }
  return nullptr;
}

// Translate matrix type to vector type.
Type *LowerMatrixType(Type *Ty, bool forMem) {
  // Only translate matrix type and function type which use matrix type.
  // Not translate struct has matrix or matrix pointer.
  // Struct should be flattened before.
  // Pointer could cover by matldst which use vector as value type.
  if (FunctionType *FT = dyn_cast<FunctionType>(Ty)) {
    Type *RetTy = LowerMatrixType(FT->getReturnType());
    SmallVector<Type *, 4> params;
    for (Type *param : FT->params()) {
      params.emplace_back(LowerMatrixType(param));
    }
    return FunctionType::get(RetTy, params, false);
  } else if (dxilutil::IsHLSLMatrixType(Ty)) {
    unsigned row, col;
    Type *EltTy = GetMatrixInfo(Ty, col, row);
    if (forMem && EltTy->isIntegerTy(1))
      EltTy = Type::getInt32Ty(Ty->getContext());
    return VectorType::get(EltTy, row * col);
  } else {
    return Ty;
  }
}


Type *GetMatrixInfo(Type *Ty, unsigned &col, unsigned &row) {
  DXASSERT(dxilutil::IsHLSLMatrixType(Ty), "not matrix type");
  StructType *ST = cast<StructType>(Ty);
  Type *EltTy = ST->getElementType(0);
  Type *RowTy = EltTy->getArrayElementType();
  row = EltTy->getArrayNumElements();
  col = RowTy->getVectorNumElements();
  return RowTy->getVectorElementType();
}

bool IsMatrixArrayPointer(llvm::Type *Ty) {
  if (!Ty->isPointerTy())
    return false;
  Ty = Ty->getPointerElementType();
  if (!Ty->isArrayTy())
    return false;
  while (Ty->isArrayTy())
    Ty = Ty->getArrayElementType();
  return dxilutil::IsHLSLMatrixType(Ty);
}
Type *LowerMatrixArrayPointer(Type *Ty, bool forMem) {
  unsigned addrSpace = Ty->getPointerAddressSpace();
  Ty = Ty->getPointerElementType();
  std::vector<unsigned> arraySizeList;
  while (Ty->isArrayTy()) {
    arraySizeList.push_back(Ty->getArrayNumElements());
    Ty = Ty->getArrayElementType();
  }
  Ty = LowerMatrixType(Ty, forMem);

  for (auto arraySize = arraySizeList.rbegin();
       arraySize != arraySizeList.rend(); arraySize++)
    Ty = ArrayType::get(Ty, *arraySize);
  return PointerType::get(Ty, addrSpace);
}

Value *BuildVector(Type *EltTy, unsigned size, ArrayRef<llvm::Value *> elts,
  IRBuilder<> &Builder) {
  Value *Vec = UndefValue::get(VectorType::get(EltTy, size));
  for (unsigned i = 0; i < size; i++)
    Vec = Builder.CreateInsertElement(Vec, elts[i], i);
  return Vec;
}

llvm::Value *VecMatrixMemToReg(llvm::Value *VecVal, llvm::Type *MatType,
  llvm::IRBuilder<> &Builder)
{
  llvm::Type *VecMatRegTy = HLMatrixLower::LowerMatrixType(MatType, /*forMem*/false);
  if (VecVal->getType() == VecMatRegTy) {
    return VecVal;
  }

  DXASSERT(VecMatRegTy->getVectorElementType()->isIntegerTy(1),
    "Vector matrix mem to reg type mismatch should only happen for bools.");
  llvm::Type *VecMatMemTy = HLMatrixLower::LowerMatrixType(MatType, /*forMem*/true);
  return Builder.CreateICmpNE(VecVal, Constant::getNullValue(VecMatMemTy));
}

llvm::Value *VecMatrixRegToMem(llvm::Value* VecVal, llvm::Type *MatType,
  llvm::IRBuilder<> &Builder)
{
  llvm::Type *VecMatMemTy = HLMatrixLower::LowerMatrixType(MatType, /*forMem*/true);
  if (VecVal->getType() == VecMatMemTy) {
    return VecVal;
  }

  DXASSERT(VecVal->getType()->getVectorElementType()->isIntegerTy(1),
    "Vector matrix reg to mem type mismatch should only happen for bools.");
  return Builder.CreateZExt(VecVal, VecMatMemTy);
}

llvm::Instruction *CreateVecMatrixLoad(
  llvm::Value *VecPtr, llvm::Type *MatType, llvm::IRBuilder<> &Builder)
{
  llvm::Instruction *VecVal = Builder.CreateLoad(VecPtr);
  return cast<llvm::Instruction>(VecMatrixMemToReg(VecVal, MatType, Builder));
}

llvm::Instruction *CreateVecMatrixStore(llvm::Value* VecVal, llvm::Value *VecPtr,
  llvm::Type *MatType, llvm::IRBuilder<> &Builder)
{
  llvm::Type *VecMatMemTy = HLMatrixLower::LowerMatrixType(MatType, /*forMem*/true);
  if (VecVal->getType() == VecMatMemTy) {
    return Builder.CreateStore(VecVal, VecPtr);
  }

  // We need to convert to the memory representation, and we want to return
  // the conversion instruction rather than the store since that's what
  // accepts the register-typed i1 values.

  // Do not use VecMatrixRegToMem as it may constant fold the conversion
  // instruction, which is what we want to return.
  DXASSERT(VecVal->getType()->getVectorElementType()->isIntegerTy(1),
    "Vector matrix reg to mem type mismatch should only happen for bools.");

  llvm::Instruction *ConvInst = Builder.Insert(new ZExtInst(VecVal, VecMatMemTy));
  Builder.CreateStore(ConvInst, VecPtr);
  return ConvInst;
}

Value *LowerGEPOnMatIndexListToIndex(
    llvm::GetElementPtrInst *GEP, ArrayRef<Value *> IdxList) {
  IRBuilder<> Builder(GEP);
  Value *zero = Builder.getInt32(0);
  DXASSERT(GEP->getNumIndices() == 2, "must have 2 level");
  Value *baseIdx = (GEP->idx_begin())->get();
  DXASSERT_LOCALVAR(baseIdx, baseIdx == zero, "base index must be 0");
  Value *Idx = (GEP->idx_begin() + 1)->get();

  if (ConstantInt *immIdx = dyn_cast<ConstantInt>(Idx)) {
    return IdxList[immIdx->getSExtValue()];
  } else {
    IRBuilder<> AllocaBuilder(
        GEP->getParent()->getParent()->getEntryBlock().getFirstInsertionPt());
    unsigned size = IdxList.size();
    // Store idxList to temp array.
    ArrayType *AT = ArrayType::get(IdxList[0]->getType(), size);
    Value *tempArray = AllocaBuilder.CreateAlloca(AT);

    for (unsigned i = 0; i < size; i++) {
      Value *EltPtr = Builder.CreateGEP(tempArray, {zero, Builder.getInt32(i)});
      Builder.CreateStore(IdxList[i], EltPtr);
    }
    // Load the idx.
    Value *GEPOffset = Builder.CreateGEP(tempArray, {zero, Idx});
    return Builder.CreateLoad(GEPOffset);
  }
}


unsigned GetColMajorIdx(unsigned r, unsigned c, unsigned row) {
  return c * row + r;
}
unsigned GetRowMajorIdx(unsigned r, unsigned c, unsigned col) {
  return r * col + c;
}

} // namespace HLMatrixLower
} // namespace hlsl

namespace {

// Creates and manages a set of overloaded functions keyed on the function type,
// and which should be destroyed when the pool gets out of scope.
class TempOverloadPool {
public:
  TempOverloadPool(llvm::Module &Module, const char* BaseName)
    : Module(Module), BaseName(BaseName) {}
  ~TempOverloadPool() { clear(); }

  Function *get(FunctionType *Ty);
  bool contains(FunctionType *Ty) { return Funcs.count(Ty) != 0; }
  bool contains(Function *Func);
  void clear();

private:
  llvm::Module &Module;
  const char* BaseName;
  llvm::DenseMap<FunctionType*, Function*> Funcs;
};

Function *TempOverloadPool::get(FunctionType *Ty) {
  auto It = Funcs.find(Ty);
  if (It != Funcs.end()) return It->second;

  std::string MangledName;
  raw_string_ostream MangledNameStream(MangledName);
  MangledNameStream << BaseName;
  MangledNameStream << '.';
  Ty->print(MangledNameStream);
  MangledNameStream.flush();

  Function* Func = cast<Function>(Module.getOrInsertFunction(MangledName, Ty));
  Funcs.insert(std::make_pair(Ty, Func));
  return Func;
}

bool TempOverloadPool::contains(Function *Func) {
  auto It = Funcs.find(Func->getFunctionType());
  return It != Funcs.end() && It->second == Func;
}

void TempOverloadPool::clear() {
  for (auto Entry : Funcs) {
    DXASSERT(Entry.second->use_empty(), "Temporary function still used during pool destruction.");
    Entry.second->removeFromParent();
  }
  Funcs.clear();
}

class HLMatrixLowerPass : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit HLMatrixLowerPass() : ModulePass(ID) {}

  const char *getPassName() const override { return "HL matrix lower"; }
  bool runOnModule(Module &M) override;

private:
  void runOnFunction(Function &Func);
  void runOnGlobal(GlobalVariable *GV);
  void runOnGlobalMatrixArray(GlobalVariable *GV);

  std::vector<Instruction*> getMatrixInstructions(Function &Func);
  static Type *getLoweredType(Type *Ty, bool MemRepr = false);
  Value *getLoweredByValOperand(Value *Val, IRBuilder<> &Builder);
  Value *tryGetLoweredMatrixOrArrayPtrOperandNoGep(Value *MatOrArrayPtr, IRBuilder<> &Builder);
  Value *tryGetLoweredMatrixPtrOperand(Value *MatPtr, IRBuilder<> &Builder);
  void replaceAllUsesByLoweredValue(Instruction *MatInst, Value *VecVal);

  Value *lowerInstruction(Instruction *MatInst);
  AllocaInst *lowerAlloca(AllocaInst *MatAlloca);
  Value *lowerHLOperation(CallInst *Call, HLOpcodeGroup OpcodeGroup);
  Value *lowerHLIntrinsic(CallInst *Call, IntrinsicOp Opcode);
  Value *lowerHLMulIntrinsic(Value* Lhs, Value *Rhs, bool Unsigned, IRBuilder<> &Builder);
  Value *lowerHLTransposeIntrinsic(Value *MatVal, IRBuilder<> &Builder);
  Value *lowerHLDeterminantIntrinsic(Value *MatVal, IRBuilder<> &Builder);
  Value *lowerHLUnaryOperation(Value *MatVal, HLUnaryOpcode Opcode, IRBuilder<> &Builder);
  Value *lowerHLBinaryOperation(Value *Lhs, Value *Rhs, HLBinaryOpcode Opcode, IRBuilder<> &Builder);
  Value *lowerHLLoadStore(CallInst *Call, HLMatLoadStoreOpcode Opcode);
  Value *lowerHLLoad(Value *MatPtr, bool RowMajor, IRBuilder<> &Builder);
  Value *lowerHLStore(Value *MatVal, Value *MatPtr, bool RowMajor, bool Return, IRBuilder<> &Builder);
  Value *lowerHLCast(Value *Src, Type *DstTy, HLCastOpcode Opcode, IRBuilder<> &Builder);
  Value *lowerHLSubscript(CallInst *Call, HLSubscriptOpcode Opcode);
  Value *lowerHLMatElementSubscript(CallInst *Call, bool RowMajor);
  Value *lowerHLMatSubscript(CallInst *Call, bool RowMajor);
  Value *lowerHLMatConstantSubscript(CallInst *Call, Value *MatPtr, SmallVectorImpl<int> &ElemIndices);
  Value *lowerHLMatDynamicSubscript(CallInst *Call, Value *MatPtr, SmallVectorImpl<Value*> &ElemIndices);
  Value *lowerHLInit(CallInst *Call);

  Instruction *MatCastToVec(CallInst *CI);
  Instruction *MatLdStToVec(CallInst *CI);
  Instruction *MatSubscriptToVec(CallInst *CI);
  Instruction *MatFrExpToVec(CallInst *CI);
  Instruction *MatIntrinsicToVec(CallInst *CI);
  Instruction *TrivialMatUnOpToVec(CallInst *CI);
  // Replace matVal with vecVal on matUseInst.
  void TrivialMatUnOpReplace(Value *matVal, Value *vecVal,
                            CallInst *matUseInst);
  Instruction *TrivialMatBinOpToVec(CallInst *CI);
  // Replace matVal with vecVal on matUseInst.
  void TrivialMatBinOpReplace(Value *matVal, Value *vecVal,
                             CallInst *matUseInst);
  // Replace matVal with vecVal on mulInst.
  void TranslateMatMatMul(Value *matVal, Value *vecVal,
                          CallInst *mulInst, bool isSigned);
  void TranslateMatVecMul(Value *matVal, Value *vecVal,
                          CallInst *mulInst, bool isSigned);
  void TranslateVecMatMul(Value *matVal, Value *vecVal,
                          CallInst *mulInst, bool isSigned);
  void TranslateMul(Value *matVal, Value *vecVal, CallInst *mulInst,
                    bool isSigned);
  // Replace matVal with vecVal on transposeInst.
  void TranslateMatTranspose(Value *matVal, Value *vecVal,
                             CallInst *transposeInst);
  void TranslateMatDeterminant(Value *matVal, Value *vecVal,
                             CallInst *determinantInst);
  void MatIntrinsicReplace(Value *matVal, Value *vecVal,
                           CallInst *matUseInst);
  // Replace matVal with vecVal on castInst.
  void TranslateMatMatCast(Value *matVal, Value *vecVal,
                           CallInst *castInst);
  void TranslateMatToOtherCast(Value *matVal, Value *vecVal,
                               CallInst *castInst);
  void TranslateMatCast(Value *matVal, Value *vecVal,
                        CallInst *castInst);
  void TranslateMatMajorCast(Value *matVal, Value *vecVal,
                        CallInst *castInst, bool rowToCol, bool transpose);
  // Replace matVal with vecVal in matSubscript
  void TranslateMatSubscript(Value *matVal, Value *vecVal,
                             CallInst *matSubInst);
  // Replace matInitInst using matToVecMap
  void TranslateMatInit(CallInst *matInitInst);
  // Replace matSelectInst using matToVecMap
  void TranslateMatSelect(CallInst *matSelectInst);
  // Replace matVal with vecVal on matInitInst.
  void TranslateMatArrayGEP(Value *matVal, Value *vecVal,
                            GetElementPtrInst *matGEP);
  void TranslateMatLoadStoreOnGlobal(Value *matGlobal, ArrayRef<Value *>vecGlobals,
                             CallInst *matLdStInst);
  void TranslateMatLoadStoreOnGlobal(GlobalVariable *matGlobal, GlobalVariable *vecGlobal,
                             CallInst *matLdStInst);
  void TranslateMatSubscriptOnGlobalPtr(CallInst *matSubInst, Value *vecPtr);
  void TranslateMatLoadStoreOnGlobalPtr(CallInst *matLdStInst, Value *vecPtr);

  // Get new matrix value corresponding to vecVal
  Value *GetMatrixForVec(Value *vecVal, Type *matTy);

  // Translate library function input/output to preserve function signatures
  void TranslateArgForLibFunc(CallInst *CI);
  void TranslateArgsForLibFunc(Function &F);

  // Replace matVal with vecVal on matUseInst.
  void TrivialMatReplace(Value *matVal, Value *vecVal,
                        CallInst *matUseInst);
  // Lower a matrix type instruction to a vector type instruction.
  void lowerToVec(Instruction *matInst);
  // Lower users of a matrix type instruction.
  void replaceMatWithVec(Value *matVal, Value *vecVal);
  // Translate user library function call arguments
  void castMatrixArgs(Value *matVal, Value *vecVal, CallInst *CI);
  // Translate mat inst which need all operands ready.
  void finalMatTranslation(Value *matVal);

  // For most matrix insturction users, it will only have one matrix use.
  // Use vector so save deadInsts because vector is cheap.
  void AddToDeadInsts(Instruction *I) { m_deadInsts.emplace_back(I); }
  // In case instruction has more than one matrix use.
  // Use AddToDeadInstsWithDups to make sure it's not add to deadInsts more than once.
  void AddToDeadInstsWithDups(Instruction *I) {
    if (m_inDeadInstsSet.count(I) == 0) {
      // Only add to deadInsts when it's not inside m_inDeadInstsSet.
      m_inDeadInstsSet.insert(I);
      AddToDeadInsts(I);
    }
  }
  // Delete dead insts in m_deadInsts.
  void DeleteDeadInsts();

private:
  Module *m_pModule;
  HLModule *m_pHLModule;
  bool m_HasDbgInfo;

  // Pools for the translation stubs
  TempOverloadPool *m_matToVecStubs = nullptr;
  TempOverloadPool *m_vecToMatStubs = nullptr;

  std::vector<Instruction *> m_deadInsts;
  // For instruction like matrix array init.
  // May use more than 1 matrix alloca inst.
  // This set is here to avoid put it into deadInsts more than once.
  std::unordered_set<Instruction *> m_inDeadInstsSet;
  // Map from matrix value to its vector version.
  DenseMap<Value *, Value *> matToVecMap;
  // Map from new vector version to matrix version needed by user call or return.
  DenseMap<Value *, Value *> vecToMatMap;
};
}

char HLMatrixLowerPass::ID = 0;

ModulePass *llvm::createHLMatrixLowerPass() { return new HLMatrixLowerPass(); }

INITIALIZE_PASS(HLMatrixLowerPass, "hlmatrixlower", "HLSL High-Level Matrix Lower", false, false)

static Instruction *CreateTypeCast(HLCastOpcode castOp, Type *toTy, Value *src,
                                   IRBuilder<> Builder) {
  Type *srcTy = src->getType();

  // Conversions between equivalent types are no-ops,
  // even between signed/unsigned variants.
  if (srcTy == toTy) return cast<Instruction>(src);

  bool fromUnsigned = castOp == HLCastOpcode::FromUnsignedCast ||
                      castOp == HLCastOpcode::UnsignedUnsignedCast;
  bool toUnsigned = castOp == HLCastOpcode::ToUnsignedCast ||
                    castOp == HLCastOpcode::UnsignedUnsignedCast;

  // Conversions to bools are comparisons
  if (toTy->getScalarSizeInBits() == 1) {
    // fcmp une is what regular clang uses in C++ for (bool)f;
    return cast<Instruction>(srcTy->isIntOrIntVectorTy()
      ? Builder.CreateICmpNE(src, llvm::Constant::getNullValue(srcTy), "tobool")
      : Builder.CreateFCmpUNE(src, llvm::Constant::getNullValue(srcTy), "tobool"));
  }

  // Cast necessary
  auto CastOp = static_cast<Instruction::CastOps>(HLModule::GetNumericCastOp(
    srcTy, fromUnsigned, toTy, toUnsigned));
  return cast<Instruction>(Builder.CreateCast(CastOp, src, toTy));
}

Instruction *HLMatrixLowerPass::MatCastToVec(CallInst *CI) {
  IRBuilder<> Builder(CI);
  Value *op = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  HLCastOpcode opcode = static_cast<HLCastOpcode>(GetHLOpcode(CI));

  bool ToMat = dxilutil::IsHLSLMatrixType(CI->getType());
  bool FromMat = dxilutil::IsHLSLMatrixType(op->getType());
  if (ToMat && !FromMat) {
    // Translate OtherToMat here.
    // Rest will translated when replace.
    unsigned col, row;
    Type *EltTy = GetMatrixInfo(CI->getType(), col, row);
    unsigned toSize = col * row;
    Instruction *sizeCast = nullptr;
    Type *FromTy = op->getType();
    Type *I32Ty = IntegerType::get(FromTy->getContext(), 32);
    if (FromTy->isVectorTy()) {
      std::vector<Constant *> MaskVec(toSize);
      for (size_t i = 0; i != toSize; ++i)
        MaskVec[i] = ConstantInt::get(I32Ty, i);

      Value *castMask = ConstantVector::get(MaskVec);

      sizeCast = new ShuffleVectorInst(op, op, castMask);
      Builder.Insert(sizeCast);

    } else {
      op = Builder.CreateInsertElement(
          UndefValue::get(VectorType::get(FromTy, 1)), op, (uint64_t)0);
      Constant *zero = ConstantInt::get(I32Ty, 0);
      std::vector<Constant *> MaskVec(toSize, zero);
      Value *castMask = ConstantVector::get(MaskVec);

      sizeCast = new ShuffleVectorInst(op, op, castMask);
      Builder.Insert(sizeCast);
    }
    Instruction *typeCast = sizeCast;
    if (EltTy != FromTy->getScalarType()) {
      typeCast = CreateTypeCast(opcode, VectorType::get(EltTy, toSize),
                                sizeCast, Builder);
    }
    return typeCast;
  } else if (FromMat && ToMat) {
    if (isa<Argument>(op)) {
      // Cast From mat to mat for arugment.
      IRBuilder<> Builder(CI);

      // Here only lower the return type to vector.
      Type *RetTy = LowerMatrixType(CI->getType());
      SmallVector<Type *, 4> params;
      for (Value *operand : CI->arg_operands()) {
        params.emplace_back(operand->getType());
      }

      Type *FT = FunctionType::get(RetTy, params, false);

      HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
      unsigned opcode = GetHLOpcode(CI);

      Function *vecF = GetOrCreateHLFunction(*m_pModule, cast<FunctionType>(FT),
                                             group, opcode);

      SmallVector<Value *, 4> argList;
      for (Value *arg : CI->arg_operands()) {
        argList.emplace_back(arg);
      }

      return Builder.CreateCall(vecF, argList);
    }
  }

  return MatIntrinsicToVec(CI);
}

// Return GEP if value is Matrix resulting GEP from UDT alloca
// UDT alloca must be there for library function args
static GetElementPtrInst *GetIfMatrixGEPOfUDTAlloca(Value *V) {
  if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
    if (dxilutil::IsHLSLMatrixType(GEP->getResultElementType())) {
      Value *ptr = GEP->getPointerOperand();
      if (AllocaInst *AI = dyn_cast<AllocaInst>(ptr)) {
        Type *ATy = AI->getAllocatedType();
        if (ATy->isStructTy() && !dxilutil::IsHLSLMatrixType(ATy)) {
          return GEP;
        }
      }
    }
  }
  return nullptr;
}

// Return GEP if value is Matrix resulting GEP from UDT argument of
// none-graphics functions.
static GetElementPtrInst *GetIfMatrixGEPOfUDTArg(Value *V, HLModule &HM) {
  if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
    if (dxilutil::IsHLSLMatrixType(GEP->getResultElementType())) {
      Value *ptr = GEP->getPointerOperand();
      if (Argument *Arg = dyn_cast<Argument>(ptr)) {
        if (!HM.IsGraphicsShader(Arg->getParent()))
          return GEP;
      }
    }
  }
  return nullptr;
}

Instruction *HLMatrixLowerPass::MatLdStToVec(CallInst *CI) {
  IRBuilder<> Builder(CI);
  unsigned opcode = GetHLOpcode(CI);
  HLMatLoadStoreOpcode matOpcode = static_cast<HLMatLoadStoreOpcode>(opcode);
  Instruction *result = nullptr;
  switch (matOpcode) {
  case HLMatLoadStoreOpcode::ColMatLoad:
  case HLMatLoadStoreOpcode::RowMatLoad: {
    Value *matPtr = CI->getArgOperand(HLOperandIndex::kMatLoadPtrOpIdx);
    if (isa<AllocaInst>(matPtr) || GetIfMatrixGEPOfUDTAlloca(matPtr) ||
        GetIfMatrixGEPOfUDTArg(matPtr, *m_pHLModule)) {
      Value *vecPtr = matToVecMap[cast<Instruction>(matPtr)];
      result = CreateVecMatrixLoad(vecPtr, matPtr->getType()->getPointerElementType(), Builder);
    } else
      result = MatIntrinsicToVec(CI);
  } break;
  case HLMatLoadStoreOpcode::ColMatStore:
  case HLMatLoadStoreOpcode::RowMatStore: {
    Value *matPtr = CI->getArgOperand(HLOperandIndex::kMatStoreDstPtrOpIdx);
    if (isa<AllocaInst>(matPtr) || GetIfMatrixGEPOfUDTAlloca(matPtr) ||
        GetIfMatrixGEPOfUDTArg(matPtr, *m_pHLModule)) {
      Value *vecPtr = matToVecMap[cast<Instruction>(matPtr)];
      Value *matVal = CI->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);
      Value *vecVal = UndefValue::get(HLMatrixLower::LowerMatrixType(matVal->getType()));
      result = CreateVecMatrixStore(vecVal, vecPtr, matVal->getType(), Builder);
    } else
      result = MatIntrinsicToVec(CI);
  } break;
  }
  return result;
}

Instruction *HLMatrixLowerPass::MatSubscriptToVec(CallInst *CI) {
  IRBuilder<> Builder(CI);
  Value *matPtr = CI->getArgOperand(HLOperandIndex::kMatSubscriptMatOpIdx);
  if (isa<AllocaInst>(matPtr)) {
    // Here just create a new matSub call which use vec ptr.
    // Later in TranslateMatSubscript will do the real translation.
    std::vector<Value *> args(CI->getNumArgOperands());
    for (unsigned i = 0; i < CI->getNumArgOperands(); i++) {
      args[i] = CI->getArgOperand(i);
    }
    // Change mat ptr into vec ptr.
    args[HLOperandIndex::kMatSubscriptMatOpIdx] =
        matToVecMap[cast<Instruction>(matPtr)];
    std::vector<Type *> paramTyList(CI->getNumArgOperands());
    for (unsigned i = 0; i < CI->getNumArgOperands(); i++) {
      paramTyList[i] = args[i]->getType();
    }

    FunctionType *funcTy = FunctionType::get(CI->getType(), paramTyList, false);
    unsigned opcode = GetHLOpcode(CI);
    Function *opFunc = GetOrCreateHLFunction(*m_pModule, funcTy, HLOpcodeGroup::HLSubscript, opcode);
    return Builder.CreateCall(opFunc, args);
  } else
    return MatIntrinsicToVec(CI);
}

Instruction *HLMatrixLowerPass::MatFrExpToVec(CallInst *CI) {
  IRBuilder<> Builder(CI);
  FunctionType *FT = CI->getCalledFunction()->getFunctionType();
  Type *RetTy = LowerMatrixType(FT->getReturnType());
  SmallVector<Type *, 4> params;
  for (Type *param : FT->params()) {
    if (!param->isPointerTy()) {
      params.emplace_back(LowerMatrixType(param));
    } else {
      // Lower pointer type for frexp.
      Type *EltTy = LowerMatrixType(param->getPointerElementType());
      params.emplace_back(
          PointerType::get(EltTy, param->getPointerAddressSpace()));
    }
  }

  Type *VecFT = FunctionType::get(RetTy, params, false);

  HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
  Function *vecF =
      GetOrCreateHLFunction(*m_pModule, cast<FunctionType>(VecFT), group,
                            static_cast<unsigned>(IntrinsicOp::IOP_frexp));

  SmallVector<Value *, 4> argList;
  auto paramTyIt = params.begin();
  for (Value *arg : CI->arg_operands()) {
    Type *Ty = arg->getType();
    Type *ParamTy = *(paramTyIt++);

    if (Ty != ParamTy)
      argList.emplace_back(UndefValue::get(ParamTy));
    else
      argList.emplace_back(arg);
  }

  return Builder.CreateCall(vecF, argList);
}

Instruction *HLMatrixLowerPass::MatIntrinsicToVec(CallInst *CI) {
  IRBuilder<> Builder(CI);
  unsigned opcode = GetHLOpcode(CI);

  if (opcode == static_cast<unsigned>(IntrinsicOp::IOP_frexp))
    return MatFrExpToVec(CI);

  Type *FT = LowerMatrixType(CI->getCalledFunction()->getFunctionType());

  HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());

  Function *vecF = GetOrCreateHLFunction(*m_pModule, cast<FunctionType>(FT), group, opcode);

  SmallVector<Value *, 4> argList;
  for (Value *arg : CI->arg_operands()) {
    Type *Ty = arg->getType();
    if (dxilutil::IsHLSLMatrixType(Ty)) {
      argList.emplace_back(UndefValue::get(LowerMatrixType(Ty)));
    } else
      argList.emplace_back(arg);
  }

  return Builder.CreateCall(vecF, argList);
}

Instruction *HLMatrixLowerPass::TrivialMatUnOpToVec(CallInst *CI) {
  Type *ResultTy = LowerMatrixType(CI->getType());
  UndefValue *tmp = UndefValue::get(ResultTy);
  IRBuilder<> Builder(CI);
  HLUnaryOpcode opcode = static_cast<HLUnaryOpcode>(GetHLOpcode(CI));
  bool isFloat = ResultTy->getVectorElementType()->isFloatingPointTy();

  Constant *one = isFloat
    ? ConstantFP::get(ResultTy->getVectorElementType(), 1)
    : ConstantInt::get(ResultTy->getVectorElementType(), 1);
  Constant *oneVec = ConstantVector::getSplat(ResultTy->getVectorNumElements(), one);

  Instruction *Result = nullptr;
  switch (opcode) {
  case HLUnaryOpcode::Plus: {
    // This is actually a no-op, but the structure of the code here requires
    // that we create an instruction.
    Constant *zero = Constant::getNullValue(ResultTy);
    if (isFloat)
      Result = BinaryOperator::CreateFAdd(tmp, zero);
    else
      Result = BinaryOperator::CreateAdd(tmp, zero);
  } break;
  case HLUnaryOpcode::Minus: {
    Constant *zero = Constant::getNullValue(ResultTy);
    if (isFloat)
      Result = BinaryOperator::CreateFSub(zero, tmp);
    else
      Result = BinaryOperator::CreateSub(zero, tmp);
  } break;
  case HLUnaryOpcode::LNot: {
    Constant *zero = Constant::getNullValue(ResultTy);
    if (isFloat)
      Result = CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_UEQ, tmp, zero);
    else
      Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_EQ, tmp, zero);
  } break;
  case HLUnaryOpcode::Not: {
    Constant *allOneBits = Constant::getAllOnesValue(ResultTy);
    Result = BinaryOperator::CreateXor(tmp, allOneBits);
  } break;
  case HLUnaryOpcode::PostInc:
  case HLUnaryOpcode::PreInc:
    if (isFloat)
      Result = BinaryOperator::CreateFAdd(tmp, oneVec);
    else
      Result = BinaryOperator::CreateAdd(tmp, oneVec);
    break;
  case HLUnaryOpcode::PostDec:
  case HLUnaryOpcode::PreDec:
    if (isFloat)
      Result = BinaryOperator::CreateFSub(tmp, oneVec);
    else
      Result = BinaryOperator::CreateSub(tmp, oneVec);
    break;
  default:
    DXASSERT(0, "not implement");
    return nullptr;
  }
  Builder.Insert(Result);
  return Result;
}

Instruction *HLMatrixLowerPass::TrivialMatBinOpToVec(CallInst *CI) {
  Type *ResultTy = LowerMatrixType(CI->getType());
  IRBuilder<> Builder(CI);
  HLBinaryOpcode opcode = static_cast<HLBinaryOpcode>(GetHLOpcode(CI));
  Type *OpTy = LowerMatrixType(
      CI->getOperand(HLOperandIndex::kBinaryOpSrc0Idx)->getType());
  UndefValue *tmp = UndefValue::get(OpTy);
  bool isFloat = OpTy->getVectorElementType()->isFloatingPointTy();

  Instruction *Result = nullptr;

  switch (opcode) {
  case HLBinaryOpcode::Add:
    if (isFloat)
      Result = BinaryOperator::CreateFAdd(tmp, tmp);
    else
      Result = BinaryOperator::CreateAdd(tmp, tmp);
    break;
  case HLBinaryOpcode::Sub:
    if (isFloat)
      Result = BinaryOperator::CreateFSub(tmp, tmp);
    else
      Result = BinaryOperator::CreateSub(tmp, tmp);
    break;
  case HLBinaryOpcode::Mul:
    if (isFloat)
      Result = BinaryOperator::CreateFMul(tmp, tmp);
    else
      Result = BinaryOperator::CreateMul(tmp, tmp);
    break;
  case HLBinaryOpcode::Div:
    if (isFloat)
      Result = BinaryOperator::CreateFDiv(tmp, tmp);
    else
      Result = BinaryOperator::CreateSDiv(tmp, tmp);
    break;
  case HLBinaryOpcode::Rem:
    if (isFloat)
      Result = BinaryOperator::CreateFRem(tmp, tmp);
    else
      Result = BinaryOperator::CreateSRem(tmp, tmp);
    break;
  case HLBinaryOpcode::And:
    Result = BinaryOperator::CreateAnd(tmp, tmp);
    break;
  case HLBinaryOpcode::Or:
    Result = BinaryOperator::CreateOr(tmp, tmp);
    break;
  case HLBinaryOpcode::Xor:
    Result = BinaryOperator::CreateXor(tmp, tmp);
    break;
  case HLBinaryOpcode::Shl: {
    Value *op1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
    DXASSERT_LOCALVAR(op1, dxilutil::IsHLSLMatrixType(op1->getType()),
                      "must be matrix type here");
    Result = BinaryOperator::CreateShl(tmp, tmp);
  } break;
  case HLBinaryOpcode::Shr: {
    Value *op1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
    DXASSERT_LOCALVAR(op1, dxilutil::IsHLSLMatrixType(op1->getType()),
                      "must be matrix type here");
    Result = BinaryOperator::CreateAShr(tmp, tmp);
  } break;
  case HLBinaryOpcode::LT:
    if (isFloat)
      Result = CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_OLT, tmp, tmp);
    else
      Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_SLT, tmp, tmp);
    break;
  case HLBinaryOpcode::GT:
    if (isFloat)
      Result = CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_OGT, tmp, tmp);
    else
      Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_SGT, tmp, tmp);
    break;
  case HLBinaryOpcode::LE:
    if (isFloat)
      Result = CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_OLE, tmp, tmp);
    else
      Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_SLE, tmp, tmp);
    break;
  case HLBinaryOpcode::GE:
    if (isFloat)
      Result = CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_OGE, tmp, tmp);
    else
      Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_SGE, tmp, tmp);
    break;
  case HLBinaryOpcode::EQ:
    if (isFloat)
      Result = CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_OEQ, tmp, tmp);
    else
      Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_EQ, tmp, tmp);
    break;
  case HLBinaryOpcode::NE:
    if (isFloat)
      Result = CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_ONE, tmp, tmp);
    else
      Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_NE, tmp, tmp);
    break;
  case HLBinaryOpcode::UDiv:
    Result = BinaryOperator::CreateUDiv(tmp, tmp);
    break;
  case HLBinaryOpcode::URem:
    Result = BinaryOperator::CreateURem(tmp, tmp);
    break;
  case HLBinaryOpcode::UShr: {
    Value *op1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
    DXASSERT_LOCALVAR(op1, dxilutil::IsHLSLMatrixType(op1->getType()),
                      "must be matrix type here");
    Result = BinaryOperator::CreateLShr(tmp, tmp);
  } break;
  case HLBinaryOpcode::ULT:
    Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_ULT, tmp, tmp);
    break;
  case HLBinaryOpcode::UGT:
    Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_UGT, tmp, tmp);
    break;
  case HLBinaryOpcode::ULE:
    Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_ULE, tmp, tmp);
    break;
  case HLBinaryOpcode::UGE:
    Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_UGE, tmp, tmp);
    break;
  case HLBinaryOpcode::LAnd:
  case HLBinaryOpcode::LOr: {
    Value *vecZero = Constant::getNullValue(ResultTy);
    Instruction *cmpL;
    if (isFloat)
      cmpL = CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_ONE, tmp, vecZero);
    else
      cmpL = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_NE, tmp, vecZero);
    Builder.Insert(cmpL);

    Instruction *cmpR;
    if (isFloat)
      cmpR =
          CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_ONE, tmp, vecZero);
    else
      cmpR = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_NE, tmp, vecZero);
    Builder.Insert(cmpR);

    // How to map l, r back? Need check opcode
    if (opcode == HLBinaryOpcode::LOr)
      Result = BinaryOperator::CreateOr(cmpL, cmpR);
    else
      Result = BinaryOperator::CreateAnd(cmpL, cmpR);
    break;
  }
  default:
    DXASSERT(0, "not implement");
    return nullptr;
  }
  Builder.Insert(Result);
  return Result;
}

// Create BitCast if ptr, otherwise, create alloca of new type, write to bitcast of alloca, and return load from alloca
// If bOrigAllocaTy is true: create alloca of old type instead, write to alloca, and return load from bitcast of alloca
static Instruction *BitCastValueOrPtr(Value* V, Instruction *Insert, Type *Ty, bool bOrigAllocaTy = false, const Twine &Name = "") {
  IRBuilder<> Builder(Insert);
  if (Ty->isPointerTy()) {
    // If pointer, we can bitcast directly
    return cast<Instruction>(Builder.CreateBitCast(V, Ty, Name));
  } else {
    // If value, we have to alloca, store to bitcast ptr, and load
    IRBuilder<> AllocaBuilder(dxilutil::FindAllocaInsertionPt(Insert));
    Type *allocaTy = bOrigAllocaTy ? V->getType() : Ty;
    Type *otherTy = bOrigAllocaTy ? Ty : V->getType();
    Instruction *allocaInst = AllocaBuilder.CreateAlloca(allocaTy);
    Instruction *bitCast = cast<Instruction>(Builder.CreateBitCast(allocaInst, otherTy->getPointerTo()));
    Builder.CreateStore(V, bOrigAllocaTy ? allocaInst : bitCast);
    return Builder.CreateLoad(bOrigAllocaTy ? bitCast : allocaInst, Name);
  }
}

void HLMatrixLowerPass::lowerToVec(Instruction *matInst) {
  Value *vecVal = nullptr;
  
  if (CallInst *CI = dyn_cast<CallInst>(matInst)) {
    hlsl::HLOpcodeGroup group =
        hlsl::GetHLOpcodeGroupByName(CI->getCalledFunction());
    switch (group) {
    case HLOpcodeGroup::HLIntrinsic: {
      vecVal = MatIntrinsicToVec(CI);
    } break;
    case HLOpcodeGroup::HLSelect: {
      vecVal = MatIntrinsicToVec(CI);
    } break;
    case HLOpcodeGroup::HLBinOp: {
      vecVal = TrivialMatBinOpToVec(CI);
    } break;
    case HLOpcodeGroup::HLUnOp: {
      vecVal = TrivialMatUnOpToVec(CI);
    } break;
    case HLOpcodeGroup::HLCast: {
      vecVal = MatCastToVec(CI);
    } break;
    case HLOpcodeGroup::HLInit: {
      vecVal = MatIntrinsicToVec(CI);
    } break;
    case HLOpcodeGroup::HLMatLoadStore: {
      vecVal = MatLdStToVec(CI);
    } break;
    case HLOpcodeGroup::HLSubscript: {
      vecVal = MatSubscriptToVec(CI);
    } break;
    case HLOpcodeGroup::NotHL: {
      // Translate user function return
      vecVal = BitCastValueOrPtr( matInst,
                                  matInst->getNextNode(),
                                  HLMatrixLower::LowerMatrixType(matInst->getType()),
                                  /*bOrigAllocaTy*/ false,
                                  matInst->getName());
      // matrix equivalent of this new vector will be the original, retained user call
      vecToMatMap[vecVal] = matInst;
    } break;
    default:
      DXASSERT(0, "invalid inst");
    }
  } else if (AllocaInst *AI = dyn_cast<AllocaInst>(matInst)) {
    Type *Ty = AI->getAllocatedType();
    Type *matTy = Ty;
    
    IRBuilder<> AllocaBuilder(AI);
    if (Ty->isArrayTy()) {
      Type *vecTy = HLMatrixLower::LowerMatrixArrayPointer(AI->getType(), /*forMem*/ true);
      vecTy = vecTy->getPointerElementType();
      vecVal = AllocaBuilder.CreateAlloca(vecTy, nullptr, AI->getName());
    } else {
      Type *vecTy = HLMatrixLower::LowerMatrixType(matTy, /*forMem*/ true);
      vecVal = AllocaBuilder.CreateAlloca(vecTy, nullptr, AI->getName());
    }
    // Update debug info.
    DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(AI);
    if (DDI) {
      LLVMContext &Context = AI->getContext();
      Value *DDIVar = MetadataAsValue::get(Context, DDI->getRawVariable());
      Value *DDIExp = MetadataAsValue::get(Context, DDI->getRawExpression());
      Value *VMD = MetadataAsValue::get(Context, ValueAsMetadata::get(vecVal));
      IRBuilder<> debugBuilder(DDI);
      debugBuilder.CreateCall(DDI->getCalledFunction(), {VMD, DDIVar, DDIExp});
    }

    if (HLModule::HasPreciseAttributeWithMetadata(AI))
      HLModule::MarkPreciseAttributeWithMetadata(cast<Instruction>(vecVal));

  } else if (GetIfMatrixGEPOfUDTAlloca(matInst) ||
             GetIfMatrixGEPOfUDTArg(matInst, *m_pHLModule)) {
    // If GEP from alloca of non-matrix UDT, bitcast
    IRBuilder<> Builder(matInst->getNextNode());
    vecVal = Builder.CreateBitCast(matInst,
      HLMatrixLower::LowerMatrixType(
        matInst->getType()->getPointerElementType() )->getPointerTo());
    // matrix equivalent of this new vector will be the original, retained GEP
    vecToMatMap[vecVal] = matInst;
  } else {
    DXASSERT(0, "invalid inst");
  }
  if (vecVal) {
    matToVecMap[matInst] = vecVal;
  }
}

// Replace matInst with vecVal on matUseInst.
void HLMatrixLowerPass::TrivialMatUnOpReplace(Value *matVal,
                                             Value *vecVal,
                                             CallInst *matUseInst) {
  (void)(matVal); // Unused
  HLUnaryOpcode opcode = static_cast<HLUnaryOpcode>(GetHLOpcode(matUseInst));
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[matUseInst]);
  switch (opcode) {
  case HLUnaryOpcode::Plus: // add(x, 0)
    // Ideally we'd get completely rid of the instruction for +mat,
    // but matToVecMap needs to point to some instruction.
  case HLUnaryOpcode::Not: // xor(x, -1)
  case HLUnaryOpcode::LNot: // cmpeq(x, 0)
  case HLUnaryOpcode::PostInc:
  case HLUnaryOpcode::PreInc:
  case HLUnaryOpcode::PostDec:
  case HLUnaryOpcode::PreDec:
    vecUseInst->setOperand(0, vecVal);
    break;
  case HLUnaryOpcode::Minus: // sub(0, x)
    vecUseInst->setOperand(1, vecVal);
    break;
  case HLUnaryOpcode::Invalid:
  case HLUnaryOpcode::NumOfUO:
    DXASSERT(false, "Unexpected HL unary opcode.");
    break;
  }
}

// Replace matInst with vecVal on matUseInst.
void HLMatrixLowerPass::TrivialMatBinOpReplace(Value *matVal,
                                              Value *vecVal,
                                              CallInst *matUseInst) {
  HLBinaryOpcode opcode = static_cast<HLBinaryOpcode>(GetHLOpcode(matUseInst));
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[matUseInst]);

  if (opcode != HLBinaryOpcode::LAnd && opcode != HLBinaryOpcode::LOr) {
    if (matUseInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx) == matVal)
      vecUseInst->setOperand(0, vecVal);
    if (matUseInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx) == matVal)
      vecUseInst->setOperand(1, vecVal);
  } else {
    if (matUseInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx) ==
      matVal) {
      Instruction *vecCmp = cast<Instruction>(vecUseInst->getOperand(0));
      vecCmp->setOperand(0, vecVal);
    }
    if (matUseInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx) ==
      matVal) {
      Instruction *vecCmp = cast<Instruction>(vecUseInst->getOperand(1));
      vecCmp->setOperand(0, vecVal);
    }
  }
}

static Function *GetOrCreateMadIntrinsic(Type *Ty, Type *opcodeTy, IntrinsicOp madOp, Module &M) {
  llvm::FunctionType *MadFuncTy =
      llvm::FunctionType::get(Ty, { opcodeTy, Ty, Ty, Ty}, false);

  Function *MAD =
      GetOrCreateHLFunction(M, MadFuncTy, HLOpcodeGroup::HLIntrinsic,
                            (unsigned)madOp);
  return MAD;
}

void HLMatrixLowerPass::TranslateMatMatMul(Value *matVal,
                                           Value *vecVal,
                                           CallInst *mulInst, bool isSigned) {
  (void)(matVal); // Unused; retrieved from matToVecMap directly
  DXASSERT(matToVecMap.count(mulInst), "must have vec version");
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[mulInst]);
  // Already translated.
  if (!isa<CallInst>(vecUseInst))
    return;
  Value *LVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *RVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);

  unsigned col, row;
  Type *EltTy = GetMatrixInfo(LVal->getType(), col, row);
  unsigned rCol, rRow;
  GetMatrixInfo(RVal->getType(), rCol, rRow);
  DXASSERT_NOMSG(col == rRow);

  bool isFloat = EltTy->isFloatingPointTy();

  Value *retVal = llvm::UndefValue::get(LowerMatrixType(mulInst->getType()));
  IRBuilder<> Builder(vecUseInst);

  Value *lMat = matToVecMap[cast<Instruction>(LVal)];
  Value *rMat = matToVecMap[cast<Instruction>(RVal)];

  auto CreateOneEltMul = [&](unsigned r, unsigned lc, unsigned c) -> Value * {
    unsigned lMatIdx = HLMatrixLower::GetRowMajorIdx(r, lc, col);
    unsigned rMatIdx = HLMatrixLower::GetRowMajorIdx(lc, c, rCol);
    Value *lMatElt = Builder.CreateExtractElement(lMat, lMatIdx);
    Value *rMatElt = Builder.CreateExtractElement(rMat, rMatIdx);
    return isFloat ? Builder.CreateFMul(lMatElt, rMatElt)
                   : Builder.CreateMul(lMatElt, rMatElt);
  };

  IntrinsicOp madOp = isSigned ? IntrinsicOp::IOP_mad : IntrinsicOp::IOP_umad;
  Type *opcodeTy = Builder.getInt32Ty();
  Function *Mad = GetOrCreateMadIntrinsic(EltTy, opcodeTy, madOp,
                                          *m_pHLModule->GetModule());
  Value *madOpArg = Builder.getInt32((unsigned)madOp);

  auto CreateOneEltMad = [&](unsigned r, unsigned lc, unsigned c,
                             Value *acc) -> Value * {
    unsigned lMatIdx = HLMatrixLower::GetRowMajorIdx(r, lc, col);
    unsigned rMatIdx = HLMatrixLower::GetRowMajorIdx(lc, c, rCol);
    Value *lMatElt = Builder.CreateExtractElement(lMat, lMatIdx);
    Value *rMatElt = Builder.CreateExtractElement(rMat, rMatIdx);
    return Builder.CreateCall(Mad, {madOpArg, lMatElt, rMatElt, acc});
  };

  for (unsigned r = 0; r < row; r++) {
    for (unsigned c = 0; c < rCol; c++) {
      unsigned lc = 0;
      Value *tmpVal = CreateOneEltMul(r, lc, c);

      for (lc = 1; lc < col; lc++) {
        tmpVal = CreateOneEltMad(r, lc, c, tmpVal);
      }
      unsigned matIdx = HLMatrixLower::GetRowMajorIdx(r, c, rCol);
      retVal = Builder.CreateInsertElement(retVal, tmpVal, matIdx);
    }
  }

  Instruction *matmatMul = cast<Instruction>(retVal);
  // Replace vec transpose function call with shuf.
  vecUseInst->replaceAllUsesWith(matmatMul);
  AddToDeadInsts(vecUseInst);
  matToVecMap[mulInst] = matmatMul;
}

void HLMatrixLowerPass::TranslateMatVecMul(Value *matVal,
                                           Value *vecVal,
                                           CallInst *mulInst, bool isSigned) {
  // matInst should == mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *RVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);

  unsigned col, row;
  Type *EltTy = GetMatrixInfo(matVal->getType(), col, row);
  DXASSERT_NOMSG(RVal->getType()->getVectorNumElements() == col);

  bool isFloat = EltTy->isFloatingPointTy();

  Value *retVal = llvm::UndefValue::get(mulInst->getType());
  IRBuilder<> Builder(mulInst);

  Value *vec = RVal;
  Value *mat = vecVal; // vec version of matInst;

  IntrinsicOp madOp = isSigned ? IntrinsicOp::IOP_mad : IntrinsicOp::IOP_umad;
  Type *opcodeTy = Builder.getInt32Ty();
  Function *Mad = GetOrCreateMadIntrinsic(EltTy, opcodeTy, madOp,
                                          *m_pHLModule->GetModule());
  Value *madOpArg = Builder.getInt32((unsigned)madOp);

  auto CreateOneEltMad = [&](unsigned r, unsigned c, Value *acc) -> Value * {
    Value *vecElt = Builder.CreateExtractElement(vec, c);
    uint32_t matIdx = HLMatrixLower::GetRowMajorIdx(r, c, col);
    Value *matElt = Builder.CreateExtractElement(mat, matIdx);
    return Builder.CreateCall(Mad, {madOpArg, vecElt, matElt, acc});
  };

  for (unsigned r = 0; r < row; r++) {
    unsigned c = 0;
    Value *vecElt = Builder.CreateExtractElement(vec, c);
    uint32_t matIdx = HLMatrixLower::GetRowMajorIdx(r, c, col);
    Value *matElt = Builder.CreateExtractElement(mat, matIdx);

    Value *tmpVal = isFloat ? Builder.CreateFMul(vecElt, matElt)
                            : Builder.CreateMul(vecElt, matElt);

    for (c = 1; c < col; c++) {
      tmpVal = CreateOneEltMad(r, c, tmpVal);
    }

    retVal = Builder.CreateInsertElement(retVal, tmpVal, r);
  }

  mulInst->replaceAllUsesWith(retVal);
  AddToDeadInsts(mulInst);
}

void HLMatrixLowerPass::TranslateVecMatMul(Value *matVal,
                                           Value *vecVal,
                                           CallInst *mulInst, bool isSigned) {
  Value *LVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  // matVal should == mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  Value *RVal = vecVal;

  unsigned col, row;
  Type *EltTy = GetMatrixInfo(matVal->getType(), col, row);
  DXASSERT_NOMSG(LVal->getType()->getVectorNumElements() == row);

  bool isFloat = EltTy->isFloatingPointTy();

  Value *retVal = llvm::UndefValue::get(mulInst->getType());
  IRBuilder<> Builder(mulInst);

  Value *vec = LVal;
  Value *mat = RVal;

  IntrinsicOp madOp = isSigned ? IntrinsicOp::IOP_mad : IntrinsicOp::IOP_umad;
  Type *opcodeTy = Builder.getInt32Ty();
  Function *Mad = GetOrCreateMadIntrinsic(EltTy, opcodeTy, madOp,
                                          *m_pHLModule->GetModule());
  Value *madOpArg = Builder.getInt32((unsigned)madOp);

  auto CreateOneEltMad = [&](unsigned r, unsigned c, Value *acc) -> Value * {
    Value *vecElt = Builder.CreateExtractElement(vec, r);
    uint32_t matIdx = HLMatrixLower::GetRowMajorIdx(r, c, col);
    Value *matElt = Builder.CreateExtractElement(mat, matIdx);
    return Builder.CreateCall(Mad, {madOpArg, vecElt, matElt, acc});
  };

  for (unsigned c = 0; c < col; c++) {
    unsigned r = 0;
    Value *vecElt = Builder.CreateExtractElement(vec, r);
    uint32_t matIdx = HLMatrixLower::GetRowMajorIdx(r, c, col);
    Value *matElt = Builder.CreateExtractElement(mat, matIdx);

    Value *tmpVal = isFloat ? Builder.CreateFMul(vecElt, matElt)
                            : Builder.CreateMul(vecElt, matElt);

    for (r = 1; r < row; r++) {
      tmpVal = CreateOneEltMad(r, c, tmpVal);
    }

    retVal = Builder.CreateInsertElement(retVal, tmpVal, c);
  }

  mulInst->replaceAllUsesWith(retVal);
  AddToDeadInsts(mulInst);
}

void HLMatrixLowerPass::TranslateMul(Value *matVal, Value *vecVal,
                                     CallInst *mulInst, bool isSigned) {
  Value *LVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *RVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);

  bool LMat = dxilutil::IsHLSLMatrixType(LVal->getType());
  bool RMat = dxilutil::IsHLSLMatrixType(RVal->getType());
  if (LMat && RMat) {
    TranslateMatMatMul(matVal, vecVal, mulInst, isSigned);
  } else if (LMat) {
    TranslateMatVecMul(matVal, vecVal, mulInst, isSigned);
  } else {
    TranslateVecMatMul(matVal, vecVal, mulInst, isSigned);
  }
}

void HLMatrixLowerPass::TranslateMatTranspose(Value *matVal,
                                              Value *vecVal,
                                              CallInst *transposeInst) {
  // Matrix value is row major, transpose is cast it to col major.
  TranslateMatMajorCast(matVal, vecVal, transposeInst,
      /*bRowToCol*/ true, /*bTranspose*/ true);
}

static Value *Determinant2x2(Value *m00, Value *m01, Value *m10, Value *m11,
                             IRBuilder<> &Builder) {
  Value *mul0 = Builder.CreateFMul(m00, m11);
  Value *mul1 = Builder.CreateFMul(m01, m10);
  return Builder.CreateFSub(mul0, mul1);
}

static Value *Determinant3x3(Value *m00, Value *m01, Value *m02,
                             Value *m10, Value *m11, Value *m12,
                             Value *m20, Value *m21, Value *m22,
                             IRBuilder<> &Builder) {
  Value *deter00 = Determinant2x2(m11, m12, m21, m22, Builder);
  Value *deter01 = Determinant2x2(m10, m12, m20, m22, Builder);
  Value *deter02 = Determinant2x2(m10, m11, m20, m21, Builder);
  deter00 = Builder.CreateFMul(m00, deter00);
  deter01 = Builder.CreateFMul(m01, deter01);
  deter02 = Builder.CreateFMul(m02, deter02);
  Value *result = Builder.CreateFSub(deter00, deter01);
  result = Builder.CreateFAdd(result, deter02);
  return result;
}

static Value *Determinant4x4(Value *m00, Value *m01, Value *m02, Value *m03,
                             Value *m10, Value *m11, Value *m12, Value *m13,
                             Value *m20, Value *m21, Value *m22, Value *m23,
                             Value *m30, Value *m31, Value *m32, Value *m33,
                             IRBuilder<> &Builder) {
  Value *deter00 = Determinant3x3(m11, m12, m13, m21, m22, m23, m31, m32, m33, Builder);
  Value *deter01 = Determinant3x3(m10, m12, m13, m20, m22, m23, m30, m32, m33, Builder);
  Value *deter02 = Determinant3x3(m10, m11, m13, m20, m21, m23, m30, m31, m33, Builder);
  Value *deter03 = Determinant3x3(m10, m11, m12, m20, m21, m22, m30, m31, m32, Builder);
  deter00 = Builder.CreateFMul(m00, deter00);
  deter01 = Builder.CreateFMul(m01, deter01);
  deter02 = Builder.CreateFMul(m02, deter02);
  deter03 = Builder.CreateFMul(m03, deter03);
  Value *result = Builder.CreateFSub(deter00, deter01);
  result = Builder.CreateFAdd(result, deter02);
  result = Builder.CreateFSub(result, deter03);
  return result;
}


void HLMatrixLowerPass::TranslateMatDeterminant(Value *matVal, Value *vecVal,
    CallInst *determinantInst) {
  unsigned row, col;
  GetMatrixInfo(matVal->getType(), col, row);
  IRBuilder<> Builder(determinantInst);
  // when row == 1, result is vecVal.
  Value *Result = vecVal;
  if (row == 2) {
    Value *m00 = Builder.CreateExtractElement(vecVal, (uint64_t)0);
    Value *m01 = Builder.CreateExtractElement(vecVal, 1);
    Value *m10 = Builder.CreateExtractElement(vecVal, 2);
    Value *m11 = Builder.CreateExtractElement(vecVal, 3);
    Result = Determinant2x2(m00, m01, m10, m11, Builder);
  }
  else if (row == 3) {
    Value *m00 = Builder.CreateExtractElement(vecVal, (uint64_t)0);
    Value *m01 = Builder.CreateExtractElement(vecVal, 1);
    Value *m02 = Builder.CreateExtractElement(vecVal, 2);
    Value *m10 = Builder.CreateExtractElement(vecVal, 3);
    Value *m11 = Builder.CreateExtractElement(vecVal, 4);
    Value *m12 = Builder.CreateExtractElement(vecVal, 5);
    Value *m20 = Builder.CreateExtractElement(vecVal, 6);
    Value *m21 = Builder.CreateExtractElement(vecVal, 7);
    Value *m22 = Builder.CreateExtractElement(vecVal, 8);
    Result = Determinant3x3(m00, m01, m02, 
                            m10, m11, m12, 
                            m20, m21, m22, Builder);
  }
  else if (row == 4) {
    Value *m00 = Builder.CreateExtractElement(vecVal, (uint64_t)0);
    Value *m01 = Builder.CreateExtractElement(vecVal, 1);
    Value *m02 = Builder.CreateExtractElement(vecVal, 2);
    Value *m03 = Builder.CreateExtractElement(vecVal, 3);

    Value *m10 = Builder.CreateExtractElement(vecVal, 4);
    Value *m11 = Builder.CreateExtractElement(vecVal, 5);
    Value *m12 = Builder.CreateExtractElement(vecVal, 6);
    Value *m13 = Builder.CreateExtractElement(vecVal, 7);

    Value *m20 = Builder.CreateExtractElement(vecVal, 8);
    Value *m21 = Builder.CreateExtractElement(vecVal, 9);
    Value *m22 = Builder.CreateExtractElement(vecVal, 10);
    Value *m23 = Builder.CreateExtractElement(vecVal, 11);

    Value *m30 = Builder.CreateExtractElement(vecVal, 12);
    Value *m31 = Builder.CreateExtractElement(vecVal, 13);
    Value *m32 = Builder.CreateExtractElement(vecVal, 14);
    Value *m33 = Builder.CreateExtractElement(vecVal, 15);

    Result = Determinant4x4(m00, m01, m02, m03,
                            m10, m11, m12, m13,
                            m20, m21, m22, m23,
                            m30, m31, m32, m33,
                            Builder);
  } else {
    DXASSERT(row == 1, "invalid matrix type");
    Result = Builder.CreateExtractElement(Result, (uint64_t)0);
  }
  determinantInst->replaceAllUsesWith(Result);
  AddToDeadInsts(determinantInst);
}

void HLMatrixLowerPass::TrivialMatReplace(Value *matVal,
                                         Value *vecVal,
                                         CallInst *matUseInst) {
  CallInst *vecUseInst = cast<CallInst>(matToVecMap[matUseInst]);

  for (unsigned i = 0; i < matUseInst->getNumArgOperands(); i++)
    if (matUseInst->getArgOperand(i) == matVal) {
      vecUseInst->setArgOperand(i, vecVal);
    }
}

static Instruction *CreateTransposeShuffle(IRBuilder<> &Builder, Value *vecVal, unsigned toRows, unsigned toCols) {
  SmallVector<int, 16> castMask(toCols * toRows);
  unsigned idx = 0;
  for (unsigned r = 0; r < toRows; r++)
    for (unsigned c = 0; c < toCols; c++)
      castMask[idx++] = c * toRows + r;
  return cast<Instruction>(
    Builder.CreateShuffleVector(vecVal, vecVal, castMask));
}

void HLMatrixLowerPass::TranslateMatMajorCast(Value *matVal,
                                              Value *vecVal,
                                              CallInst *castInst,
                                              bool bRowToCol,
                                              bool bTranspose) {
  unsigned col, row;
  if (!bTranspose) {
    GetMatrixInfo(castInst->getType(), col, row);
    DXASSERT(castInst->getType() == matVal->getType(), "type must match");
  } else {
    unsigned castCol, castRow;
    Type *castTy = GetMatrixInfo(castInst->getType(), castCol, castRow);
    unsigned srcCol, srcRow;
    Type *srcTy = GetMatrixInfo(matVal->getType(), srcCol, srcRow);
    DXASSERT_LOCALVAR((castTy == srcTy), srcTy == castTy, "type must match");
    DXASSERT(castCol == srcRow && castRow == srcCol, "col row must match");
    col = srcCol;
    row = srcRow;
  }

  DXASSERT(matToVecMap.count(castInst), "must have vec version");
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[castInst]);
  // Create before vecUseInst to prevent instructions being inserted after uses.
  IRBuilder<> Builder(vecUseInst);

  if (bRowToCol)
    std::swap(row, col);
  Instruction *vecCast = CreateTransposeShuffle(Builder, vecVal, row, col);

  // Replace vec cast function call with vecCast.
  vecUseInst->replaceAllUsesWith(vecCast);
  AddToDeadInsts(vecUseInst);
  matToVecMap[castInst] = vecCast;
}

void HLMatrixLowerPass::TranslateMatMatCast(Value *matVal,
                                            Value *vecVal,
                                            CallInst *castInst) {
  unsigned toCol, toRow;
  Type *ToEltTy = GetMatrixInfo(castInst->getType(), toCol, toRow);
  unsigned fromCol, fromRow;
  Type *FromEltTy = GetMatrixInfo(matVal->getType(), fromCol, fromRow);
  unsigned fromSize = fromCol * fromRow;
  unsigned toSize = toCol * toRow;
  DXASSERT(fromSize >= toSize, "cannot extend matrix");
  DXASSERT(matToVecMap.count(castInst), "must have vec version");
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[castInst]);

  IRBuilder<> Builder(vecUseInst);
  Instruction *vecCast = nullptr;

  HLCastOpcode opcode = static_cast<HLCastOpcode>(GetHLOpcode(castInst));

  if (fromSize == toSize) {
    vecCast = CreateTypeCast(opcode, VectorType::get(ToEltTy, toSize), vecVal,
                             Builder);
  } else {
    // shuf first
    std::vector<int> castMask(toCol * toRow);
    unsigned idx = 0;
    for (unsigned r = 0; r < toRow; r++)
      for (unsigned c = 0; c < toCol; c++) {
        unsigned matIdx = HLMatrixLower::GetRowMajorIdx(r, c, fromCol);
        castMask[idx++] = matIdx;
      }

    Instruction *shuf = cast<Instruction>(
        Builder.CreateShuffleVector(vecVal, vecVal, castMask));

    if (ToEltTy != FromEltTy)
      vecCast = CreateTypeCast(opcode, VectorType::get(ToEltTy, toSize), shuf,
                               Builder);
    else
      vecCast = shuf;
  }
  // Replace vec cast function call with vecCast.
  vecUseInst->replaceAllUsesWith(vecCast);
  AddToDeadInsts(vecUseInst);
  matToVecMap[castInst] = vecCast;
}

void HLMatrixLowerPass::TranslateMatToOtherCast(Value *matVal,
                                                Value *vecVal,
                                                CallInst *castInst) {
  unsigned col, row;
  Type *EltTy = GetMatrixInfo(matVal->getType(), col, row);
  unsigned fromSize = col * row;

  IRBuilder<> Builder(castInst);
  Value *sizeCast = nullptr;

  HLCastOpcode opcode = static_cast<HLCastOpcode>(GetHLOpcode(castInst));

  Type *ToTy = castInst->getType();
  if (ToTy->isVectorTy()) {
    unsigned toSize = ToTy->getVectorNumElements();
    if (fromSize != toSize) {
      std::vector<int> castMask(fromSize);
      for (unsigned c = 0; c < toSize; c++)
        castMask[c] = c;

      sizeCast = Builder.CreateShuffleVector(vecVal, vecVal, castMask);
    } else
      sizeCast = vecVal;
  } else {
    DXASSERT(ToTy->isSingleValueType(), "must scalar here");
    sizeCast = Builder.CreateExtractElement(vecVal, (uint64_t)0);
  }

  Value *typeCast = sizeCast;
  if (EltTy != ToTy->getScalarType()) {
    typeCast = CreateTypeCast(opcode, ToTy, typeCast, Builder);
  }
  // Replace cast function call with typeCast.
  castInst->replaceAllUsesWith(typeCast);
  AddToDeadInsts(castInst);
}

void HLMatrixLowerPass::TranslateMatCast(Value *matVal,
                                         Value *vecVal,
                                         CallInst *castInst) {
  HLCastOpcode opcode = static_cast<HLCastOpcode>(GetHLOpcode(castInst));
  if (opcode == HLCastOpcode::ColMatrixToRowMatrix ||
      opcode == HLCastOpcode::RowMatrixToColMatrix) {
    TranslateMatMajorCast(matVal, vecVal, castInst,
                          opcode == HLCastOpcode::RowMatrixToColMatrix,
                          /*bTranspose*/false);
  } else {
    bool ToMat = dxilutil::IsHLSLMatrixType(castInst->getType());
    bool FromMat = dxilutil::IsHLSLMatrixType(matVal->getType());
    if (ToMat && FromMat) {
      TranslateMatMatCast(matVal, vecVal, castInst);
    } else if (FromMat)
      TranslateMatToOtherCast(matVal, vecVal, castInst);
    else {
      DXASSERT(0, "Not translate as user of matInst");
    }
  }
}

void HLMatrixLowerPass::MatIntrinsicReplace(Value *matVal,
                                            Value *vecVal,
                                            CallInst *matUseInst) {
  IRBuilder<> Builder(matUseInst);
  IntrinsicOp opcode = static_cast<IntrinsicOp>(GetHLOpcode(matUseInst));
  switch (opcode) {
  case IntrinsicOp::IOP_umul:
    TranslateMul(matVal, vecVal, matUseInst, /*isSigned*/false);
    break;
  case IntrinsicOp::IOP_mul:
    TranslateMul(matVal, vecVal, matUseInst, /*isSigned*/true);
    break;
  case IntrinsicOp::IOP_transpose:
    TranslateMatTranspose(matVal, vecVal, matUseInst);
    break;
  case IntrinsicOp::IOP_determinant:
    TranslateMatDeterminant(matVal, vecVal, matUseInst);
    break;
  default:
    CallInst *useInst = matUseInst;
    if (matToVecMap.count(matUseInst))
      useInst = cast<CallInst>(matToVecMap[matUseInst]);
    for (unsigned i = 0; i < useInst->getNumArgOperands(); i++) {
      if (matUseInst->getArgOperand(i) == matVal)
        useInst->setArgOperand(i, vecVal);
    }
    break;
  }
}

void HLMatrixLowerPass::TranslateMatSubscript(Value *matVal, Value *vecVal,
                                              CallInst *matSubInst) {
  unsigned opcode = GetHLOpcode(matSubInst);
  HLSubscriptOpcode matOpcode = static_cast<HLSubscriptOpcode>(opcode);
  assert(matOpcode != HLSubscriptOpcode::DefaultSubscript &&
         "matrix don't use default subscript");

  Type *matType = matVal->getType()->getPointerElementType();
  unsigned col, row;
  Type *EltTy = HLMatrixLower::GetMatrixInfo(matType, col, row);

  bool isElement = (matOpcode == HLSubscriptOpcode::ColMatElement) |
                   (matOpcode == HLSubscriptOpcode::RowMatElement);
  Value *mask =
      matSubInst->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx);

  if (isElement) {
    Type *resultType = matSubInst->getType()->getPointerElementType();
    unsigned resultSize = 1;
    if (resultType->isVectorTy())
      resultSize = resultType->getVectorNumElements();

    std::vector<int> shufMask(resultSize);
    Constant *EltIdxs = cast<Constant>(mask);
    for (unsigned i = 0; i < resultSize; i++) {
      shufMask[i] =
          cast<ConstantInt>(EltIdxs->getAggregateElement(i))->getLimitedValue();
    }

    for (Value::use_iterator CallUI = matSubInst->use_begin(),
                             CallE = matSubInst->use_end();
         CallUI != CallE;) {
      Use &CallUse = *CallUI++;
      Instruction *CallUser = cast<Instruction>(CallUse.getUser());
      IRBuilder<> Builder(CallUser);
      Value *vecLd = Builder.CreateLoad(vecVal);
      if (LoadInst *ld = dyn_cast<LoadInst>(CallUser)) {
        if (resultSize > 1) {
          Value *shuf = Builder.CreateShuffleVector(vecLd, vecLd, shufMask);
          ld->replaceAllUsesWith(shuf);
        } else {
          Value *elt = Builder.CreateExtractElement(vecLd, shufMask[0]);
          ld->replaceAllUsesWith(elt);
        }
      } else if (StoreInst *st = dyn_cast<StoreInst>(CallUser)) {
        Value *val = st->getValueOperand();
        if (resultSize > 1) {
          for (unsigned i = 0; i < shufMask.size(); i++) {
            unsigned idx = shufMask[i];
            Value *valElt = Builder.CreateExtractElement(val, i);
            vecLd = Builder.CreateInsertElement(vecLd, valElt, idx);
          }
          Builder.CreateStore(vecLd, vecVal);
        } else {
          vecLd = Builder.CreateInsertElement(vecLd, val, shufMask[0]);
          Builder.CreateStore(vecLd, vecVal);
        }
      } else {
        DXASSERT(0, "matrix element should only used by load/store.");
      }
      AddToDeadInsts(CallUser);
    }
  } else {
    // Subscript.
    // Return a row.
    // Use insertElement and extractElement.
    ArrayType *AT = ArrayType::get(EltTy, col*row);

    IRBuilder<> AllocaBuilder(
        matSubInst->getParent()->getParent()->getEntryBlock().getFirstInsertionPt());
    Value *tempArray = AllocaBuilder.CreateAlloca(AT);
    Value *zero = AllocaBuilder.getInt32(0);
    bool isDynamicIndexing = !isa<ConstantInt>(mask);
    SmallVector<Value *, 4> idxList;
    for (unsigned i = 0; i < col; i++) {
      idxList.emplace_back(
          matSubInst->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx + i));
    }

    for (Value::use_iterator CallUI = matSubInst->use_begin(),
                             CallE = matSubInst->use_end();
         CallUI != CallE;) {
      Use &CallUse = *CallUI++;
      Instruction *CallUser = cast<Instruction>(CallUse.getUser());
      IRBuilder<> Builder(CallUser);
      Value *vecLd = Builder.CreateLoad(vecVal);
      if (LoadInst *ld = dyn_cast<LoadInst>(CallUser)) {
        Value *sub = UndefValue::get(ld->getType());
        if (!isDynamicIndexing) {
          for (unsigned i = 0; i < col; i++) {
            Value *matIdx = idxList[i];
            Value *valElt = Builder.CreateExtractElement(vecLd, matIdx);
            sub = Builder.CreateInsertElement(sub, valElt, i);
          }
        } else {
          // Copy vec to array.
          for (unsigned int i = 0; i < row*col; i++) {
            Value *Elt =
                Builder.CreateExtractElement(vecLd, Builder.getInt32(i));
            Value *Ptr = Builder.CreateInBoundsGEP(tempArray,
                                                   {zero, Builder.getInt32(i)});
            Builder.CreateStore(Elt, Ptr);
          }
          for (unsigned i = 0; i < col; i++) {
            Value *matIdx = idxList[i];
            Value *Ptr = Builder.CreateGEP(tempArray, { zero, matIdx});
            Value *valElt = Builder.CreateLoad(Ptr);
            sub = Builder.CreateInsertElement(sub, valElt, i);
          }
        }
        ld->replaceAllUsesWith(sub);
      } else if (StoreInst *st = dyn_cast<StoreInst>(CallUser)) {
        Value *val = st->getValueOperand();
        if (!isDynamicIndexing) {
          for (unsigned i = 0; i < col; i++) {
            Value *matIdx = idxList[i];
            Value *valElt = Builder.CreateExtractElement(val, i);
            vecLd = Builder.CreateInsertElement(vecLd, valElt, matIdx);
          }
        } else {
          // Copy vec to array.
          for (unsigned int i = 0; i < row * col; i++) {
            Value *Elt =
                Builder.CreateExtractElement(vecLd, Builder.getInt32(i));
            Value *Ptr = Builder.CreateInBoundsGEP(tempArray,
                                                   {zero, Builder.getInt32(i)});
            Builder.CreateStore(Elt, Ptr);
          }
          // Update array.
          for (unsigned i = 0; i < col; i++) {
            Value *matIdx = idxList[i];
            Value *Ptr = Builder.CreateGEP(tempArray, { zero, matIdx});
            Value *valElt = Builder.CreateExtractElement(val, i);
            Builder.CreateStore(valElt, Ptr);
          }
          // Copy array to vec.
          for (unsigned int i = 0; i < row * col; i++) {
            Value *Ptr = Builder.CreateInBoundsGEP(tempArray,
                                                   {zero, Builder.getInt32(i)});
            Value *Elt = Builder.CreateLoad(Ptr);
            vecLd = Builder.CreateInsertElement(vecLd, Elt, i);
          }
        }
        Builder.CreateStore(vecLd, vecVal);
      } else if (GetElementPtrInst *GEP =
                     dyn_cast<GetElementPtrInst>(CallUser)) {
        Value *GEPOffset = HLMatrixLower::LowerGEPOnMatIndexListToIndex(GEP, idxList);
        Value *NewGEP = Builder.CreateGEP(vecVal, {zero, GEPOffset});
        GEP->replaceAllUsesWith(NewGEP);
      } else {
        DXASSERT(0, "matrix subscript should only used by load/store.");
      }
      AddToDeadInsts(CallUser);
    }
  }
  // Check vec version.
  DXASSERT(matToVecMap.count(matSubInst) == 0, "should not have vec version");
  // All the user should have been removed.
  matSubInst->replaceAllUsesWith(UndefValue::get(matSubInst->getType()));
  AddToDeadInsts(matSubInst);
}

void HLMatrixLowerPass::TranslateMatLoadStoreOnGlobal(
    Value *matGlobal, ArrayRef<Value *> vecGlobals,
    CallInst *matLdStInst) {
  // No dynamic indexing on matrix, flatten matrix to scalars.
  // vecGlobals already in correct major.
  Type *matType = matGlobal->getType()->getPointerElementType();
  unsigned col, row;
  HLMatrixLower::GetMatrixInfo(matType, col, row);
  Type *vecType = HLMatrixLower::LowerMatrixType(matType);

  IRBuilder<> Builder(matLdStInst);

  HLMatLoadStoreOpcode opcode = static_cast<HLMatLoadStoreOpcode>(GetHLOpcode(matLdStInst));
  switch (opcode) {
  case HLMatLoadStoreOpcode::ColMatLoad:
  case HLMatLoadStoreOpcode::RowMatLoad: {
    Value *Result = UndefValue::get(vecType);
    for (unsigned matIdx = 0; matIdx < col * row; matIdx++) {
      Value *Elt = Builder.CreateLoad(vecGlobals[matIdx]);
      Result = Builder.CreateInsertElement(Result, Elt, matIdx);
    }
    matLdStInst->replaceAllUsesWith(Result);
  } break;
  case HLMatLoadStoreOpcode::ColMatStore:
  case HLMatLoadStoreOpcode::RowMatStore: {
    Value *Val = matLdStInst->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);
    for (unsigned matIdx = 0; matIdx < col * row; matIdx++) {
      Value *Elt = Builder.CreateExtractElement(Val, matIdx);
      Builder.CreateStore(Elt, vecGlobals[matIdx]);
    }
  } break;
  }
}

void HLMatrixLowerPass::TranslateMatLoadStoreOnGlobal(GlobalVariable *matGlobal,
                                                      GlobalVariable *scalarArrayGlobal,
                                                      CallInst *matLdStInst) {
  // vecGlobals already in correct major.
  HLMatLoadStoreOpcode opcode =
      static_cast<HLMatLoadStoreOpcode>(GetHLOpcode(matLdStInst));
  switch (opcode) {
  case HLMatLoadStoreOpcode::ColMatLoad:
  case HLMatLoadStoreOpcode::RowMatLoad: {
    IRBuilder<> Builder(matLdStInst);
    Type *matTy = matGlobal->getType()->getPointerElementType();
    unsigned col, row;
    Type *EltTy = HLMatrixLower::GetMatrixInfo(matTy, col, row);
    Value *zeroIdx = Builder.getInt32(0);

    std::vector<Value *> matElts(col * row);

    for (unsigned matIdx = 0; matIdx < col * row; matIdx++) {
      Value *GEP = Builder.CreateInBoundsGEP(
          scalarArrayGlobal, {zeroIdx, Builder.getInt32(matIdx)});
      matElts[matIdx] = Builder.CreateLoad(GEP);
    }

    Value *newVec =
        HLMatrixLower::BuildVector(EltTy, col * row, matElts, Builder);
    matLdStInst->replaceAllUsesWith(newVec);
    matLdStInst->eraseFromParent();
  } break;
  case HLMatLoadStoreOpcode::ColMatStore:
  case HLMatLoadStoreOpcode::RowMatStore: {
    Value *Val = matLdStInst->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);

    IRBuilder<> Builder(matLdStInst);
    Type *matTy = matGlobal->getType()->getPointerElementType();
    unsigned col, row;
    HLMatrixLower::GetMatrixInfo(matTy, col, row);
    Value *zeroIdx = Builder.getInt32(0);

    std::vector<Value *> matElts(col * row);

    for (unsigned matIdx = 0; matIdx < col * row; matIdx++) {
      Value *GEP = Builder.CreateInBoundsGEP(
          scalarArrayGlobal, {zeroIdx, Builder.getInt32(matIdx)});
      Value *Elt = Builder.CreateExtractElement(Val, matIdx);
      Builder.CreateStore(Elt, GEP);
    }

    matLdStInst->eraseFromParent();
  } break;
  }
}
void HLMatrixLowerPass::TranslateMatSubscriptOnGlobalPtr(
    CallInst *matSubInst, Value *vecPtr) {
  Value *basePtr =
      matSubInst->getArgOperand(HLOperandIndex::kMatSubscriptMatOpIdx);
  Value *idx = matSubInst->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx);
  IRBuilder<> subBuilder(matSubInst);
  Value *zeroIdx = subBuilder.getInt32(0);

  HLSubscriptOpcode opcode =
      static_cast<HLSubscriptOpcode>(GetHLOpcode(matSubInst));

  Type *matTy = basePtr->getType()->getPointerElementType();
  unsigned col, row;
  HLMatrixLower::GetMatrixInfo(matTy, col, row);

  std::vector<Value *> idxList;
  switch (opcode) {
  case HLSubscriptOpcode::ColMatSubscript:
  case HLSubscriptOpcode::RowMatSubscript: {
    // Just use index created in EmitHLSLMatrixSubscript.
    for (unsigned c = 0; c < col; c++) {
      Value *matIdx =
          matSubInst->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx + c);
      idxList.emplace_back(matIdx);
    }
  } break;
  case HLSubscriptOpcode::RowMatElement:
  case HLSubscriptOpcode::ColMatElement: {
    Type *resultType = matSubInst->getType()->getPointerElementType();
    unsigned resultSize = 1;
    if (resultType->isVectorTy())
      resultSize = resultType->getVectorNumElements();
    // Just use index created in EmitHLSLMatrixElement.
    Constant *EltIdxs = cast<Constant>(idx);
    for (unsigned i = 0; i < resultSize; i++) {
      Value *matIdx = EltIdxs->getAggregateElement(i);
      idxList.emplace_back(matIdx);
    }
  } break;
  default:
    DXASSERT(0, "invalid operation");
    break;
  }

  // Cannot generate vector pointer
  // Replace all uses with scalar pointers.
  if (!matSubInst->getType()->getPointerElementType()->isVectorTy()) {
    DXASSERT(idxList.size() == 1, "Expected a single matrix element index if the result is not a vector");
    Value *Ptr =
      subBuilder.CreateInBoundsGEP(vecPtr, { zeroIdx, idxList[0] });
    matSubInst->replaceAllUsesWith(Ptr);
  } else {
    // Split the use of CI with Ptrs.
    for (auto U = matSubInst->user_begin(); U != matSubInst->user_end();) {
      Instruction *subsUser = cast<Instruction>(*(U++));
      IRBuilder<> userBuilder(subsUser);
      if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(subsUser)) {
        Value *IndexPtr =
            HLMatrixLower::LowerGEPOnMatIndexListToIndex(GEP, idxList);
        Value *Ptr = userBuilder.CreateInBoundsGEP(vecPtr,
                                                   {zeroIdx, IndexPtr});
        for (auto gepU = GEP->user_begin(); gepU != GEP->user_end();) {
          Instruction *gepUser = cast<Instruction>(*(gepU++));
          IRBuilder<> gepUserBuilder(gepUser);
          if (StoreInst *stUser = dyn_cast<StoreInst>(gepUser)) {
            Value *subData = stUser->getValueOperand();
            gepUserBuilder.CreateStore(subData, Ptr);
            stUser->eraseFromParent();
          } else if (LoadInst *ldUser = dyn_cast<LoadInst>(gepUser)) {
            Value *subData = gepUserBuilder.CreateLoad(Ptr);
            ldUser->replaceAllUsesWith(subData);
            ldUser->eraseFromParent();
          } else {
            AddrSpaceCastInst *Cast = cast<AddrSpaceCastInst>(gepUser);
            Cast->setOperand(0, Ptr);
          }
        }
        GEP->eraseFromParent();
      } else if (StoreInst *stUser = dyn_cast<StoreInst>(subsUser)) {
        Value *val = stUser->getValueOperand();
        for (unsigned i = 0; i < idxList.size(); i++) {
          Value *Elt = userBuilder.CreateExtractElement(val, i);
          Value *Ptr = userBuilder.CreateInBoundsGEP(vecPtr,
                                                     {zeroIdx, idxList[i]});
          userBuilder.CreateStore(Elt, Ptr);
        }
        stUser->eraseFromParent();
      } else {

        Value *ldVal =
            UndefValue::get(matSubInst->getType()->getPointerElementType());
        for (unsigned i = 0; i < idxList.size(); i++) {
          Value *Ptr = userBuilder.CreateInBoundsGEP(vecPtr,
                                                     {zeroIdx, idxList[i]});
          Value *Elt = userBuilder.CreateLoad(Ptr);
          ldVal = userBuilder.CreateInsertElement(ldVal, Elt, i);
        }
        // Must be load here.
        LoadInst *ldUser = cast<LoadInst>(subsUser);
        ldUser->replaceAllUsesWith(ldVal);
        ldUser->eraseFromParent();
      }
    }
  }
  matSubInst->eraseFromParent();
}

void HLMatrixLowerPass::TranslateMatLoadStoreOnGlobalPtr(
    CallInst *matLdStInst, Value *vecPtr) {
  // Just translate into vector here.
  // DynamicIndexingVectorToArray will change it to scalar array.
  IRBuilder<> Builder(matLdStInst);
  unsigned opcode = hlsl::GetHLOpcode(matLdStInst);
  HLMatLoadStoreOpcode matLdStOp = static_cast<HLMatLoadStoreOpcode>(opcode);
  switch (matLdStOp) {
  case HLMatLoadStoreOpcode::ColMatLoad:
  case HLMatLoadStoreOpcode::RowMatLoad: {
    // Load as vector.
    Value *newLoad = Builder.CreateLoad(vecPtr);

    matLdStInst->replaceAllUsesWith(newLoad);
    matLdStInst->eraseFromParent();
  } break;
  case HLMatLoadStoreOpcode::ColMatStore:
  case HLMatLoadStoreOpcode::RowMatStore: {
    // Change value to vector array, then store.
    Value *Val = matLdStInst->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);

    Value *vecArrayGep = vecPtr;
    Builder.CreateStore(Val, vecArrayGep);
    matLdStInst->eraseFromParent();
  } break;
  default:
    DXASSERT(0, "invalid operation");
    break;
  }
}

// Flatten values inside init list to scalar elements.
static void IterateInitList(MutableArrayRef<Value *> elts, unsigned &idx,
                            Value *val,
                            DenseMap<Value *, Value *> &matToVecMap,
                            IRBuilder<> &Builder) {
  Type *valTy = val->getType();

  if (valTy->isPointerTy()) {
    if (HLMatrixLower::IsMatrixArrayPointer(valTy)) {
      if (matToVecMap.count(cast<Instruction>(val))) {
        val = matToVecMap[cast<Instruction>(val)];
      } else {
        // Convert to vec array with bitcast.
        Type *vecArrayPtrTy = HLMatrixLower::LowerMatrixArrayPointer(valTy);
        val = Builder.CreateBitCast(val, vecArrayPtrTy);
      }
    }
    Type *valEltTy = val->getType()->getPointerElementType();
    if (valEltTy->isVectorTy() || dxilutil::IsHLSLMatrixType(valEltTy) ||
        valEltTy->isSingleValueType()) {
      Value *ldVal = Builder.CreateLoad(val);
      IterateInitList(elts, idx, ldVal, matToVecMap, Builder);
    } else {
      Type *i32Ty = Type::getInt32Ty(valTy->getContext());
      Value *zero = ConstantInt::get(i32Ty, 0);
      if (ArrayType *AT = dyn_cast<ArrayType>(valEltTy)) {
        for (unsigned i = 0; i < AT->getArrayNumElements(); i++) {
          Value *gepIdx = ConstantInt::get(i32Ty, i);
          Value *EltPtr = Builder.CreateInBoundsGEP(val, {zero, gepIdx});
          IterateInitList(elts, idx, EltPtr, matToVecMap, Builder);
        }
      } else {
        // Struct.
        StructType *ST = cast<StructType>(valEltTy);
        for (unsigned i = 0; i < ST->getNumElements(); i++) {
          Value *gepIdx = ConstantInt::get(i32Ty, i);
          Value *EltPtr = Builder.CreateInBoundsGEP(val, {zero, gepIdx});
          IterateInitList(elts, idx, EltPtr, matToVecMap, Builder);
        }
      }
    }
  } else if (dxilutil::IsHLSLMatrixType(valTy)) {
    unsigned col, row;
    HLMatrixLower::GetMatrixInfo(valTy, col, row);
    unsigned matSize = col * row;
    val = matToVecMap[cast<Instruction>(val)];
    // temp matrix all row major
    for (unsigned i = 0; i < matSize; i++) {
      Value *Elt = Builder.CreateExtractElement(val, i);
      elts[idx + i] = Elt;
    }
    idx += matSize;
  } else {
    if (valTy->isVectorTy()) {
      unsigned vecSize = valTy->getVectorNumElements();
      for (unsigned i = 0; i < vecSize; i++) {
        Value *Elt = Builder.CreateExtractElement(val, i);
        elts[idx + i] = Elt;
      }
      idx += vecSize;
    } else {
      DXASSERT(valTy->isSingleValueType(), "must be single value type here");
      elts[idx++] = val;
    }
  }
}

void HLMatrixLowerPass::TranslateMatInit(CallInst *matInitInst) {
  // Array matrix init will be translated in TranslateMatArrayInitReplace.
  if (matInitInst->getType()->isVoidTy())
    return;

  DXASSERT(matToVecMap.count(matInitInst), "must have vec version");
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[matInitInst]);
  IRBuilder<> Builder(vecUseInst);
  unsigned col, row;
  Type *EltTy = GetMatrixInfo(matInitInst->getType(), col, row);

  Type *vecTy = VectorType::get(EltTy, col * row);
  unsigned vecSize = vecTy->getVectorNumElements();
  unsigned idx = 0;
  std::vector<Value *> elts(vecSize);
  // Skip opcode arg.
  for (unsigned i = 1; i < matInitInst->getNumArgOperands(); i++) {
    Value *val = matInitInst->getArgOperand(i);

    IterateInitList(elts, idx, val, matToVecMap, Builder);
  }

  Value *newInit = UndefValue::get(vecTy);
  // InitList is row major, the result is row major too.
  for (unsigned i=0;i< col * row;i++) {
      Constant *vecIdx = Builder.getInt32(i);
      newInit = InsertElementInst::Create(newInit, elts[i], vecIdx);
      Builder.Insert(cast<Instruction>(newInit));
  }

  // Replace matInit function call with matInitInst.
  vecUseInst->replaceAllUsesWith(newInit);
  AddToDeadInsts(vecUseInst);
  matToVecMap[matInitInst] = newInit;
}

void HLMatrixLowerPass::TranslateMatSelect(CallInst *matSelectInst) {
  unsigned col, row;
  Type *EltTy = GetMatrixInfo(matSelectInst->getType(), col, row);

  Type *vecTy = VectorType::get(EltTy, col * row);
  unsigned vecSize = vecTy->getVectorNumElements();

  CallInst *vecUseInst = cast<CallInst>(matToVecMap[matSelectInst]);
  Instruction *LHS = cast<Instruction>(matSelectInst->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx));
  Instruction *RHS = cast<Instruction>(matSelectInst->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx));

  IRBuilder<> Builder(vecUseInst);

  Value *Cond = vecUseInst->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  bool isVecCond = Cond->getType()->isVectorTy();
  if (isVecCond) {
    Instruction *MatCond = cast<Instruction>(
        matSelectInst->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx));
    DXASSERT_NOMSG(matToVecMap.count(MatCond));
    Cond = matToVecMap[MatCond];
  }
  DXASSERT_NOMSG(matToVecMap.count(LHS));
  Value *VLHS = matToVecMap[LHS];
  DXASSERT_NOMSG(matToVecMap.count(RHS));
  Value *VRHS = matToVecMap[RHS];

  Value *VecSelect = UndefValue::get(vecTy);
  for (unsigned i = 0; i < vecSize; i++) {
    llvm::Value *EltCond = Cond;
    if (isVecCond)
      EltCond = Builder.CreateExtractElement(Cond, i);
    llvm::Value *EltL = Builder.CreateExtractElement(VLHS, i);
    llvm::Value *EltR = Builder.CreateExtractElement(VRHS, i);
    llvm::Value *EltSelect = Builder.CreateSelect(EltCond, EltL, EltR);
    VecSelect = Builder.CreateInsertElement(VecSelect, EltSelect, i);
  }
  AddToDeadInsts(vecUseInst);
  vecUseInst->replaceAllUsesWith(VecSelect);
  matToVecMap[matSelectInst] = VecSelect;
}

void HLMatrixLowerPass::TranslateMatArrayGEP(Value *matInst,
                                             Value *vecVal,
                                             GetElementPtrInst *matGEP) {
  SmallVector<Value *, 4> idxList(matGEP->idx_begin(), matGEP->idx_end());

  IRBuilder<> GEPBuilder(matGEP);
  Value *newGEP = GEPBuilder.CreateInBoundsGEP(vecVal, idxList);
  // Only used by mat subscript and mat ld/st.
  for (Value::user_iterator user = matGEP->user_begin();
       user != matGEP->user_end();) {
    Instruction *useInst = cast<Instruction>(*(user++));
    IRBuilder<> Builder(useInst);
    // Skip return here.
    if (isa<ReturnInst>(useInst))
      continue;
    if (CallInst *useCall = dyn_cast<CallInst>(useInst)) {
      // Function call.
      hlsl::HLOpcodeGroup group =
          hlsl::GetHLOpcodeGroupByName(useCall->getCalledFunction());
      switch (group) {
      case HLOpcodeGroup::HLMatLoadStore: {
        unsigned opcode = GetHLOpcode(useCall);
        HLMatLoadStoreOpcode matOpcode =
            static_cast<HLMatLoadStoreOpcode>(opcode);
        switch (matOpcode) {
        case HLMatLoadStoreOpcode::ColMatLoad:
        case HLMatLoadStoreOpcode::RowMatLoad: {
          // Skip the vector version.
          if (useCall->getType()->isVectorTy())
            continue;
          Type *matTy = useCall->getType();
          Value *newLd = CreateVecMatrixLoad(newGEP, matTy, Builder);
          DXASSERT(matToVecMap.count(useCall), "must have vec version");
          Value *oldLd = matToVecMap[useCall];
          // Delete the oldLd.
          AddToDeadInsts(cast<Instruction>(oldLd));
          oldLd->replaceAllUsesWith(newLd);
          matToVecMap[useCall] = newLd;
        } break;
        case HLMatLoadStoreOpcode::ColMatStore:
        case HLMatLoadStoreOpcode::RowMatStore: {
          Value *vecPtr = newGEP;
          
          Value *matVal = useCall->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);
          // Skip the vector version.
          if (matVal->getType()->isVectorTy()) {
            AddToDeadInsts(useCall);
            continue;
          }

          Instruction *matInst = cast<Instruction>(matVal);

          DXASSERT(matToVecMap.count(matInst), "must have vec version");
          Value *vecVal = matToVecMap[matInst];
          CreateVecMatrixStore(vecVal, vecPtr, matVal->getType(), Builder);
        } break;
        }
      } break;
      case HLOpcodeGroup::HLSubscript: {
        TranslateMatSubscript(matGEP, newGEP, useCall);
      } break;
      default:
        DXASSERT(0, "invalid operation");
        break;
      }
    } else if (dyn_cast<BitCastInst>(useInst)) {
      // Just replace the src with vec version.
      useInst->setOperand(0, newGEP);
    } else {
      // Must be GEP.
      GetElementPtrInst *GEP = cast<GetElementPtrInst>(useInst);
      TranslateMatArrayGEP(matGEP, cast<Instruction>(newGEP), GEP);
    }
  }
  AddToDeadInsts(matGEP);
}

Value *HLMatrixLowerPass::GetMatrixForVec(Value *vecVal, Type *matTy) {
  Value *newMatVal = nullptr;
  if (vecToMatMap.count(vecVal)) {
    newMatVal = vecToMatMap[vecVal];
  } else {
    // create conversion instructions if necessary, caching result for subsequent replacements.
    // do so right after the vecVal def so it's available to all potential uses.
    newMatVal = BitCastValueOrPtr(vecVal,
      cast<Instruction>(vecVal)->getNextNode(), // vecVal must be instruction
      matTy,
      /*bOrigAllocaTy*/true,
      vecVal->getName());
    vecToMatMap[vecVal] = newMatVal;
  }
  return newMatVal;
}

void HLMatrixLowerPass::replaceMatWithVec(Value *matVal,
                                          Value *vecVal) {
  for (Value::user_iterator user = matVal->user_begin();
       user != matVal->user_end();) {
    Instruction *useInst = cast<Instruction>(*(user++));
    // User must be function call.
    if (CallInst *useCall = dyn_cast<CallInst>(useInst)) {
      hlsl::HLOpcodeGroup group =
          hlsl::GetHLOpcodeGroupByName(useCall->getCalledFunction());
      switch (group) {
      case HLOpcodeGroup::HLIntrinsic: {
        if (CallInst *matCI = dyn_cast<CallInst>(matVal)) {
          MatIntrinsicReplace(matCI, vecVal, useCall);
        } else {
          IntrinsicOp opcode = static_cast<IntrinsicOp>(GetHLOpcode(useCall));
          if (opcode == IntrinsicOp::MOP_Append) {
            // Replace matrix with vector representation and update intrinsic signature
            // We don't care about matrix orientation here, since that will need to be
            // taken into account anyways when generating the store output calls.
            SmallVector<Value *, 4> flatArgs;
            SmallVector<Type *, 4> flatParamTys;
            for (Value *arg : useCall->arg_operands()) {
              Value *flagArg = arg == matVal ? vecVal : arg;
              flatArgs.emplace_back(arg == matVal ? vecVal : arg);
              flatParamTys.emplace_back(flagArg->getType());
            }

            // Don't need flat return type for Append.
            FunctionType *flatFuncTy =
              FunctionType::get(useInst->getType(), flatParamTys, false);
            Function *flatF = GetOrCreateHLFunction(*m_pModule, flatFuncTy, group, static_cast<unsigned int>(opcode));
            
            // Append returns void, so the old call should have no users
            DXASSERT(useInst->getType()->isVoidTy(), "Unexpected MOP_Append intrinsic return type");
            DXASSERT(useInst->use_empty(), "Unexpected users of MOP_Append intrinsic return value");
            IRBuilder<> Builder(useCall);
            Builder.CreateCall(flatF, flatArgs);
            AddToDeadInsts(useCall);
          }
          else if (opcode == IntrinsicOp::IOP_frexp) {
            // NOTE: because out param use copy out semantic, so the operand of
            // out must be temp alloca.
            DXASSERT(isa<AllocaInst>(matVal), "else invalid mat ptr for frexp");
            auto it = matToVecMap.find(useCall);
            DXASSERT(it != matToVecMap.end(),
              "else fail to create vec version of useCall");
            CallInst *vecUseInst = cast<CallInst>(it->second);

            for (unsigned i = 0; i < vecUseInst->getNumArgOperands(); i++) {
              if (useCall->getArgOperand(i) == matVal) {
                vecUseInst->setArgOperand(i, vecVal);
              }
            }
          } else {
            DXASSERT(false, "Unexpected matrix user intrinsic.");
          }
        }
      } break;
      case HLOpcodeGroup::HLSelect: {
        MatIntrinsicReplace(matVal, vecVal, useCall);
      } break;
      case HLOpcodeGroup::HLBinOp: {
        TrivialMatBinOpReplace(matVal, vecVal, useCall);
      } break;
      case HLOpcodeGroup::HLUnOp: {
        TrivialMatUnOpReplace(matVal, vecVal, useCall);
      } break;
      case HLOpcodeGroup::HLCast: {
        TranslateMatCast(matVal, vecVal, useCall);
      } break;
      case HLOpcodeGroup::HLMatLoadStore: {
        DXASSERT(matToVecMap.count(useCall), "must have vec version");
        Value *vecUser = matToVecMap[useCall];
        if (isa<AllocaInst>(matVal) || GetIfMatrixGEPOfUDTAlloca(matVal) ||
            GetIfMatrixGEPOfUDTArg(matVal, *m_pHLModule)) {
          // Load Already translated in lowerToVec.
          // Store val operand will be set by the val use.
          // Do nothing here.
        } else if (StoreInst *stInst = dyn_cast<StoreInst>(vecUser)) {
          DXASSERT(vecVal->getType() == stInst->getValueOperand()->getType(),
            "Mismatched vector matrix store value types.");
          stInst->setOperand(0, vecVal);
        } else if (ZExtInst *zextInst = dyn_cast<ZExtInst>(vecUser)) {
          // This happens when storing bool matrices,
          // which must first undergo conversion from i1's to i32's.
          DXASSERT(vecVal->getType() == zextInst->getOperand(0)->getType(),
            "Mismatched vector matrix store value types.");
          zextInst->setOperand(0, vecVal);
        } else
          TrivialMatReplace(matVal, vecVal, useCall);

      } break;
      case HLOpcodeGroup::HLSubscript: {
        if (AllocaInst *AI = dyn_cast<AllocaInst>(vecVal))
          TranslateMatSubscript(matVal, vecVal, useCall);
        else if (BitCastInst *BCI = dyn_cast<BitCastInst>(vecVal))
          TranslateMatSubscript(matVal, vecVal, useCall);
        else
          TrivialMatReplace(matVal, vecVal, useCall);

      } break;
      case HLOpcodeGroup::HLInit: {
        DXASSERT(!isa<AllocaInst>(matVal), "array of matrix init should lowered in StoreInitListToDestPtr at CGHLSLMS.cpp");
        TranslateMatInit(useCall);
      } break;
      case HLOpcodeGroup::NotHL: {
        castMatrixArgs(matVal, vecVal, useCall);
      } break;
      case HLOpcodeGroup::HLExtIntrinsic:
      case HLOpcodeGroup::HLCreateHandle:
      case HLOpcodeGroup::NumOfHLOps:
      // No vector equivalents for these ops.
        break;
      }
    } else if (dyn_cast<BitCastInst>(useInst)) {
      // Just replace the src with vec version.
      if (useInst != vecVal)
        useInst->setOperand(0, vecVal);
    } else if (ReturnInst *RI = dyn_cast<ReturnInst>(useInst)) {
      Value *newMatVal = GetMatrixForVec(vecVal, matVal->getType());
      RI->setOperand(0, newMatVal);
    } else if (isa<StoreInst>(useInst)) {
      DXASSERT(vecToMatMap.count(vecVal) && vecToMatMap[vecVal] == matVal, "matrix store should only be used with preserved matrix values");
    } else {
      // Must be GEP on mat array alloca.
      GetElementPtrInst *GEP = cast<GetElementPtrInst>(useInst);
      AllocaInst *AI = cast<AllocaInst>(matVal);
      TranslateMatArrayGEP(AI, vecVal, GEP);
    }
  }
}

void HLMatrixLowerPass::castMatrixArgs(Value *matVal, Value *vecVal, CallInst *CI) {
  // translate user function parameters as necessary
  Type *Ty = matVal->getType();
  if (Ty->isPointerTy()) {
    IRBuilder<> Builder(CI);
    Value *newMatVal = Builder.CreateBitCast(vecVal, Ty);
    CI->replaceUsesOfWith(matVal, newMatVal);
  } else {
    Value *newMatVal = GetMatrixForVec(vecVal, Ty);
    CI->replaceUsesOfWith(matVal, newMatVal);
  }
}

void HLMatrixLowerPass::finalMatTranslation(Value *matVal) {
  // Translate matInit.
  if (CallInst *CI = dyn_cast<CallInst>(matVal)) {
    hlsl::HLOpcodeGroup group =
        hlsl::GetHLOpcodeGroupByName(CI->getCalledFunction());
    switch (group) {
    case HLOpcodeGroup::HLInit: {
      TranslateMatInit(CI);
    } break;
    case HLOpcodeGroup::HLSelect: {
      TranslateMatSelect(CI);
    } break;
    default:
      // Skip group already translated.
      break;
    }
  }
}

void HLMatrixLowerPass::DeleteDeadInsts() {
  while (!m_deadInsts.empty()) {
    Instruction *Inst = m_deadInsts.back();
    m_deadInsts.pop_back();

    DXASSERT_NOMSG(Inst->use_empty());
    for (Value *Operand : Inst->operand_values()) {
      Instruction *OperandInst = dyn_cast<Instruction>(Operand);
      if (OperandInst && ++OperandInst->user_begin() == OperandInst->user_end()) {
        // We were its only user, erase recursively
        DXASSERT_NOMSG(*OperandInst->user_begin() == Inst);
        m_deadInsts.emplace_back(OperandInst);
      }
    }

    Inst->eraseFromParent();
  }

  //// Delete the matrix version insts.
  //for (Instruction *deadInst : m_deadInsts) {
  //  // Replace with undef and remove it.
  //  deadInst->replaceAllUsesWith(UndefValue::get(deadInst->getType()));
  //  deadInst->eraseFromParent();
  //}
  //m_deadInsts.clear();
  //m_inDeadInstsSet.clear();
}

static bool OnlyUsedByMatrixLdSt(Value *V) {
  bool onlyLdSt = true;
  for (User *user : V->users()) {
    if (isa<Constant>(user) && user->use_empty())
      continue;

    CallInst *CI = cast<CallInst>(user);
    if (GetHLOpcodeGroupByName(CI->getCalledFunction()) ==
        HLOpcodeGroup::HLMatLoadStore)
      continue;

    onlyLdSt = false;
    break;
  }
  return onlyLdSt;
}

static Constant *LowerMatrixArrayConst(Constant *MA, Type *ResultTy) {
  if (ArrayType *AT = dyn_cast<ArrayType>(ResultTy)) {
    std::vector<Constant *> Elts;
    Type *EltResultTy = AT->getElementType();
    for (unsigned i = 0; i < AT->getNumElements(); i++) {
      Constant *Elt =
          LowerMatrixArrayConst(MA->getAggregateElement(i), EltResultTy);
      Elts.emplace_back(Elt);
    }
    return ConstantArray::get(AT, Elts);
  } else {
    // Cast float[row][col] -> float< row * col>.
    // Get float[row][col] from the struct.
    Constant *rows = MA->getAggregateElement((unsigned)0);
    ArrayType *RowAT = cast<ArrayType>(rows->getType());
    std::vector<Constant *> Elts;
    for (unsigned r=0;r<RowAT->getArrayNumElements();r++) {
      Constant *row = rows->getAggregateElement(r);
      VectorType *VT = cast<VectorType>(row->getType());
      for (unsigned c = 0; c < VT->getVectorNumElements(); c++) {
        Elts.emplace_back(row->getAggregateElement(c));
      }
    }
    return ConstantVector::get(Elts);
  }
}

void HLMatrixLowerPass::runOnGlobalMatrixArray(GlobalVariable *GV) {
  // Lower to array of vector array like float[row * col].
  // It's follow the major of decl.
  // DynamicIndexingVectorToArray will change it to scalar array.
  Type *Ty = GV->getType()->getPointerElementType();
  std::vector<unsigned> arraySizeList;
  while (Ty->isArrayTy()) {
    arraySizeList.push_back(Ty->getArrayNumElements());
    Ty = Ty->getArrayElementType();
  }
  unsigned row, col;
  Type *EltTy = GetMatrixInfo(Ty, col, row);
  Ty = VectorType::get(EltTy, col * row);

  for (auto arraySize = arraySizeList.rbegin();
       arraySize != arraySizeList.rend(); arraySize++)
    Ty = ArrayType::get(Ty, *arraySize);

  Type *VecArrayTy = Ty;
  Constant *InitVal = nullptr;
  if (GV->hasInitializer()) {
    Constant *OldInitVal = GV->getInitializer();
    InitVal = isa<UndefValue>(OldInitVal)
      ? UndefValue::get(VecArrayTy)
      : LowerMatrixArrayConst(OldInitVal, cast<ArrayType>(VecArrayTy));
  }

  bool isConst = GV->isConstant();
  GlobalVariable::ThreadLocalMode TLMode = GV->getThreadLocalMode();
  unsigned AddressSpace = GV->getType()->getAddressSpace();
  GlobalValue::LinkageTypes linkage = GV->getLinkage();

  Module *M = GV->getParent();
  GlobalVariable *VecGV =
      new llvm::GlobalVariable(*M, VecArrayTy, /*IsConstant*/ isConst, linkage,
                               /*InitVal*/ InitVal, GV->getName() + ".v",
                               /*InsertBefore*/ nullptr, TLMode, AddressSpace);
  // Add debug info.
  if (m_HasDbgInfo) {
    DebugInfoFinder &Finder = m_pHLModule->GetOrCreateDebugInfoFinder();
    HLModule::UpdateGlobalVariableDebugInfo(GV, Finder, VecGV);
  }

  for (User *U : GV->users()) {
    Value *VecGEP = nullptr;
    // Must be GEP or GEPOperator.
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(U)) {
      IRBuilder<> Builder(GEP);
      SmallVector<Value *, 4> idxList(GEP->idx_begin(), GEP->idx_end());
      VecGEP = Builder.CreateInBoundsGEP(VecGV, idxList);
      AddToDeadInsts(GEP);
    } else {
      GEPOperator *GEPOP = cast<GEPOperator>(U);
      IRBuilder<> Builder(GV->getContext());
      SmallVector<Value *, 4> idxList(GEPOP->idx_begin(), GEPOP->idx_end());
      VecGEP = Builder.CreateInBoundsGEP(VecGV, idxList);
    }

    for (auto user = U->user_begin(); user != U->user_end();) {
      CallInst *CI = cast<CallInst>(*(user++));
      HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
      if (group == HLOpcodeGroup::HLMatLoadStore) {
        TranslateMatLoadStoreOnGlobalPtr(CI, VecGEP);
      } else if (group == HLOpcodeGroup::HLSubscript) {
        TranslateMatSubscriptOnGlobalPtr(CI, VecGEP);
      } else {
        DXASSERT(0, "invalid operation");
      }
    }
  }

  DeleteDeadInsts();
  GV->removeDeadConstantUsers();
  GV->eraseFromParent();
}

static void FlattenMatConst(Constant *M, std::vector<Constant *> &Elts) {
  unsigned row, col;
  Type *EltTy = HLMatrixLower::GetMatrixInfo(M->getType(), col, row);
  if (isa<UndefValue>(M)) {
    Constant *Elt = UndefValue::get(EltTy);
    for (unsigned i=0;i<col*row;i++)
      Elts.emplace_back(Elt);
  } else {
    M = M->getAggregateElement((unsigned)0);
    // Initializer is already in correct major.
    // Just read it here.
    // The type is vector<element, col>[row].
    for (unsigned r = 0; r < row; r++) {
      Constant *C = M->getAggregateElement(r);
      for (unsigned c = 0; c < col; c++) {
        Elts.emplace_back(C->getAggregateElement(c));
      }
    }
  }
}

bool HLMatrixLowerPass::runOnModule(Module &M) {
  TempOverloadPool matToVecStubs(M, "hlmatrixlower.mat2vec");
  TempOverloadPool vecToMatStubs(M, "hlmatrixlower.vec2mat");

  m_pModule = &M;
  m_pHLModule = &m_pModule->GetOrCreateHLModule();
  // Load up debug information, to cross-reference values and the instructions
  // used to load them.
  m_HasDbgInfo = getDebugMetadataVersionFromModule(M) != 0;
  m_matToVecStubs = &matToVecStubs;
  m_vecToMatStubs = &vecToMatStubs;

  for (Function &F : M.functions()) {
    if (F.isDeclaration()) continue;
    runOnFunction(F);
  }

  //std::vector<GlobalVariable*> staticGVs;
  //for (GlobalVariable &GV : M.globals()) {
  //  if (dxilutil::IsStaticGlobal(&GV) ||
  //    dxilutil::IsSharedMemoryGlobal(&GV)) {
  //    staticGVs.emplace_back(&GV);
  //  }
  //}

  //for (GlobalVariable *GV : staticGVs)
  //  runOnGlobal(GV);

  m_pModule = nullptr;
  m_pHLModule = nullptr;
  m_matToVecStubs = nullptr;
  m_vecToMatStubs = nullptr;

  return true;
}

void HLMatrixLowerPass::runOnGlobal(GlobalVariable *GV) {
  if (HLMatrixLower::IsMatrixArrayPointer(GV->getType())) {
    runOnGlobalMatrixArray(GV);
    return;
  }

  Type *Ty = GV->getType()->getPointerElementType();
  if (!dxilutil::IsHLSLMatrixType(Ty))
    return;

  bool onlyLdSt = OnlyUsedByMatrixLdSt(GV);

  bool isConst = GV->isConstant();

  Type *vecTy = HLMatrixLower::LowerMatrixType(Ty);
  Module *M = GV->getParent();
  const DataLayout &DL = M->getDataLayout();

  std::vector<Constant *> Elts;
  // Lower to vector or array for scalar matrix.
  // Make it col major so don't need shuffle when load/store.
  FlattenMatConst(GV->getInitializer(), Elts);

  if (onlyLdSt) {
    Type *EltTy = vecTy->getVectorElementType();
    unsigned vecSize = vecTy->getVectorNumElements();
    std::vector<Value *> vecGlobals(vecSize);

    GlobalVariable::ThreadLocalMode TLMode = GV->getThreadLocalMode();
    unsigned AddressSpace = GV->getType()->getAddressSpace();
    GlobalValue::LinkageTypes linkage = GV->getLinkage();
    unsigned debugOffset = 0;
    unsigned size = DL.getTypeAllocSizeInBits(EltTy);
    unsigned align = DL.getPrefTypeAlignment(EltTy);
    for (int i = 0, e = vecSize; i != e; ++i) {
      Constant *InitVal = Elts[i];
      GlobalVariable *EltGV = new llvm::GlobalVariable(
          *M, EltTy, /*IsConstant*/ isConst, linkage,
          /*InitVal*/ InitVal, GV->getName() + "." + Twine(i),
          /*InsertBefore*/nullptr,
          TLMode, AddressSpace);
      // Add debug info.
      if (m_HasDbgInfo) {
        DebugInfoFinder &Finder = m_pHLModule->GetOrCreateDebugInfoFinder();
        HLModule::CreateElementGlobalVariableDebugInfo(
            GV, Finder, EltGV, size, align, debugOffset,
            EltGV->getName().ltrim(GV->getName()));
        debugOffset += size;
      }
      vecGlobals[i] = EltGV;
    }
    for (User *user : GV->users()) {
      if (isa<Constant>(user) && user->use_empty())
        continue;
      CallInst *CI = cast<CallInst>(user);
      TranslateMatLoadStoreOnGlobal(GV, vecGlobals, CI);
      AddToDeadInsts(CI);
    }
    DeleteDeadInsts();
    GV->eraseFromParent();
  }
  else {
    // lower to array of scalar here.
    ArrayType *AT = ArrayType::get(vecTy->getVectorElementType(), vecTy->getVectorNumElements());
    Constant *InitVal = ConstantArray::get(AT, Elts);
    GlobalVariable *arrayMat = new llvm::GlobalVariable(
      *M, AT, /*IsConstant*/ false, llvm::GlobalValue::InternalLinkage,
      /*InitVal*/ InitVal, GV->getName());
    // Add debug info.
    if (m_HasDbgInfo) {
      DebugInfoFinder &Finder = m_pHLModule->GetOrCreateDebugInfoFinder();
      HLModule::UpdateGlobalVariableDebugInfo(GV, Finder,
                                                     arrayMat);
    }

    for (auto U = GV->user_begin(); U != GV->user_end();) {
      Value *user = *(U++);
      CallInst *CI = cast<CallInst>(user);
      HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
      if (group == HLOpcodeGroup::HLMatLoadStore) {
        TranslateMatLoadStoreOnGlobal(GV, arrayMat, CI);
      }
      else {
        DXASSERT(group == HLOpcodeGroup::HLSubscript, "Must be subscript operation");
        TranslateMatSubscriptOnGlobalPtr(CI, arrayMat);
      }
    }
    GV->removeDeadConstantUsers();
    GV->eraseFromParent();
  }
}

void HLMatrixLowerPass::runOnFunction(Function &Func) {
  // Skip hl function definition (like createhandle)
  if (hlsl::GetHLOpcodeGroupByName(&Func) != HLOpcodeGroup::NotHL)
    return;

  // Save the matrix instructions first since the translation process
  // will temporarily create other instructions consuming/producing matrix types.
  std::vector<Instruction *> MatInsts = getMatrixInstructions(Func);

  for (Instruction *MatInst : MatInsts) {
    Value *LoweredValue = lowerInstruction(MatInst);

    if (LoweredValue != nullptr)
      replaceAllUsesByLoweredValue(MatInst, LoweredValue);
  }

  DeleteDeadInsts();
}

// Find all instructions consuming or producing matrices,
// directly or through pointers/arrays.
std::vector<Instruction*>
HLMatrixLowerPass::getMatrixInstructions(Function &Func) {
  std::vector<Instruction*> MatInsts;
  for (BasicBlock &BasicBlock : Func) {
    for (Instruction &Inst : BasicBlock) {
      // Don't lower GEPs directly, we'll handle them
      // when we encounter a load/store
      if (isa<GetElementPtrInst>(&Inst)) continue;

      // Match matrix producers
      if (HLMatrixType::isMatrixOrPtrOrArrayPtr(Inst.getType())) {
        MatInsts.emplace_back(&Inst);
        continue;
      }

      // Match matrix consumers
      for (Value *Operand : Inst.operand_values()) {
        if (HLMatrixType::isMatrixOrPtrOrArrayPtr(Operand->getType())) {
          MatInsts.emplace_back(&Inst);
          break;
        }
      }
    }
  }
  return MatInsts;
}

// Converts a type which may or not contain a matrix type to its
// lowered form, where all matrix types have been lowered to vector types.
// Direct matrix types are lowered to their register representation,
// whereas pointer or array-indirect types are lowered to their memory representation.
Type *HLMatrixLowerPass::getLoweredType(Type *Ty, bool MemRepr) {
  if (PointerType *PtrTy = dyn_cast<PointerType>(Ty)) {
    // Pointees are always in memory representation
    Type *LoweredElemTy = getLoweredType(PtrTy->getElementType(), true);
    return LoweredElemTy == PtrTy->getElementType()
      ? Ty : PointerType::get(LoweredElemTy, PtrTy->getAddressSpace());
  }
  else if (ArrayType *ArrayTy = dyn_cast<ArrayType>(Ty)) {
    // Arrays are always in memory and so their elements are in memory representation
    Type *LoweredElemTy = getLoweredType(ArrayTy->getElementType(), true);
    return LoweredElemTy == ArrayTy->getElementType()
      ? Ty : ArrayType::get(LoweredElemTy, ArrayTy->getNumElements());
  }
  else if (HLMatrixType MatrixTy = HLMatrixType::dyn_cast(Ty)) {
    return MatrixTy.getLoweredVectorType(MemRepr);
  }
  else return Ty;
}

// Gets the matrix-lowered representation of a value, potentially adding a translation stub.
Value* HLMatrixLowerPass::getLoweredByValOperand(Value *Val, IRBuilder<> &Builder) {
  Type *Ty = Val->getType();

  // We're only lowering byval matrices.
  // Since structs and arrays are always accessed by pointer,
  // we do not need to worry about a matrix being hidden inside a more complex type.
  DXASSERT(!Ty->isPointerTy(), "Value cannot be a pointer.");
  HLMatrixType MatTy = HLMatrixType::dyn_cast(Ty);
  if (!MatTy) return Val;

  Type *LoweredTy = MatTy.getLoweredVectorTypeForReg();
  
  // Check if the value is already a vec-to-mat translation stub
  if (CallInst *Call = dyn_cast<CallInst>(Val)) {
    if (m_vecToMatStubs->contains(Call->getCalledFunction())) {
      Value *LoweredVal = Call->getArgOperand(0);
      DXASSERT(LoweredVal->getType() == LoweredTy, "Unexpected already-lowered value type.");
      return LoweredVal;
    }
  }

  // Return a mat-to-vec translation stub
  FunctionType *TranslationStubTy = FunctionType::get(LoweredTy, { Ty }, /* isVarArg */ false);
  Function *TranslationStub = m_matToVecStubs->get(TranslationStubTy);
  return Builder.CreateCall(TranslationStub, { Val });
}

// Attempts to retrieve the lowered pointer equivalent to a matrix or matrix array pointer.
// Assumes all GEPs have been stripped.
// Returns nullptr if the pointer is to memory that cannot be lowered at this time,
// for example a buffer or shader inputs/outputs, which are lowered during signature lowering.
Value *HLMatrixLowerPass::tryGetLoweredMatrixOrArrayPtrOperandNoGep(Value *MatOrArrayPtr, IRBuilder<> &Builder) {
  DXASSERT_NOMSG(MatOrArrayPtr->getType()->isPointerTy());
  DXASSERT_NOMSG(!isa<GetElementPtrInst>(MatOrArrayPtr));

  // Is this a constant buffer/resource subscript?
  if (CallInst *Call = dyn_cast<CallInst>(MatOrArrayPtr)) {
    HLOpcodeGroup OpcodeGroup = GetHLOpcodeGroupByName(Call->getCalledFunction());
    if (OpcodeGroup == HLOpcodeGroup::HLSubscript) {
      HLSubscriptOpcode SubscriptOpcode = static_cast<HLSubscriptOpcode>(GetHLOpcode(Call));
      if (SubscriptOpcode == HLSubscriptOpcode::DefaultSubscript
        || SubscriptOpcode == HLSubscriptOpcode::CBufferSubscript) {
        // Leave it to HLOperationLower
        return nullptr;
      }
    }
  }

  Type *MatOrArrayPtrTy = MatOrArrayPtr->getType();
  DXASSERT_NOMSG(HLMatrixType::isMatrixPtr(MatOrArrayPtrTy) || HLMatrixType::isMatrixArrayPtr(MatOrArrayPtrTy));

  Type *LoweredTy = getLoweredType(MatOrArrayPtrTy);
  DXASSERT(LoweredTy != MatOrArrayPtrTy, "Unexpected lowered matrix type.");

  // Is this an alloca we've already lowered?
  if (CallInst *Call = dyn_cast<CallInst>(MatOrArrayPtr)) {
    if (m_vecToMatStubs->contains(Call->getCalledFunction())) {
      Value *LoweredPtr = Call->getArgOperand(0);
      DXASSERT(LoweredPtr->getType() == LoweredTy, "Unexpected lowered matrix value type.");
      return LoweredPtr;
    }
  }

  if (AllocaInst *Alloca = dyn_cast<AllocaInst>(MatOrArrayPtr)) {
    // Return a mat-to-vec translation stub
    FunctionType *TranslationStubTy = FunctionType::get(LoweredTy, { MatOrArrayPtr->getType() }, /* isVarArg */ false);
    Function *TranslationStub = m_matToVecStubs->get(TranslationStubTy);
    return Builder.CreateCall(TranslationStub, { MatOrArrayPtr });
  }

  // TODO what about non-graphics shader input arguments?
  return nullptr;
}

// Attempts to retrieve the lowered vector pointer equivalent to a matrix pointer.
// Returns nullptr if the pointed-to matrix lives in memory that cannot be lowered at this time,
// for example a buffer or shader inputs/outputs, which are lowered during signature lowering.
Value *HLMatrixLowerPass::tryGetLoweredMatrixPtrOperand(Value *MatPtr, IRBuilder<> &Builder) {
  DXASSERT(MatPtr->getType()->isPointerTy() && HLMatrixType::isa(MatPtr->getType()->getPointerElementType()),
    "Value must be a matrix pointer.");

  // Skip all GEPs on the way to the "root" ptr (alloca or otherwise)
  SmallVector<Value*, 4> GEPIndices;
  GEPIndices.emplace_back(Builder.getInt32(0));

  Value* RootPtr = MatPtr;
  while (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(RootPtr)) {
    DXASSERT(GEP->getNumIndices() >= 1, "Unexpected GEP without indices.");
    DXASSERT(cast<ConstantInt>(GEP->idx_begin()->get())->getValue().getLimitedValue() == 0,
      "Unexpected GEP not starting with index zero.");
    for (auto IdxIt = GEP->idx_begin() + 1, IdxEnd = GEP->idx_end(); IdxIt != IdxEnd; ++IdxIt)
      GEPIndices.emplace_back(IdxIt->get());
    RootPtr = GEP->getPointerOperand();
  }

  llvm::Value *LoweredRootPtr = tryGetLoweredMatrixOrArrayPtrOperandNoGep(RootPtr, Builder);
  if (LoweredRootPtr == nullptr) return nullptr;

  // Reconstruct GEP chain around the lowered ptr, if any
  if (GEPIndices.size() > 1) {
    LoweredRootPtr = Builder.CreateGEP(LoweredRootPtr, GEPIndices);
  }

  return LoweredRootPtr;
}

// Replaces all uses of a matrix value by its lowered vector form,
// inserting translation stubs for users which still expect a matrix value.
void HLMatrixLowerPass::replaceAllUsesByLoweredValue(Instruction* MatInst, Value* VecVal) {
  if (VecVal == nullptr || VecVal == MatInst) return;

  DXASSERT(getLoweredType(MatInst->getType()) == VecVal->getType(),
    "Unexpected lowered value type.");

  Instruction *VecToMatStub = nullptr;

  while (!MatInst->use_empty()) {
    Use &ValUse = *MatInst->use_begin();

    // Handle non-matrix cases, just point to the new value.
    if (MatInst->getType() == VecVal->getType()) {
      ValUse.set(VecVal);
      continue;
    }

    // If the user is already a matrix-to-vector translation stub,
    // we can now replace it by the proper vector value.
    if (CallInst *Call = dyn_cast<CallInst>(ValUse.getUser())) {
      if (m_matToVecStubs->contains(Call->getCalledFunction())) {
        Call->replaceAllUsesWith(VecVal);
        ValUse.set(UndefValue::get(MatInst->getType()));
        AddToDeadInsts(Call);
        continue;
      }
    }

    // Otherwise, the user should point to a vector-to-matrix translation
    // stub of the new vector value.
    if (VecToMatStub == nullptr) {
      FunctionType *TranslationStubTy = FunctionType::get(
        MatInst->getType(), { VecVal->getType() }, /* isVarArg */ false);
      Function *TranslationStub = m_vecToMatStubs->get(TranslationStubTy);

      IRBuilder<> Builder(dxilutil::SkipAllocas(MatInst->getNextNode()));
      VecToMatStub = Builder.CreateCall(TranslationStub, { VecVal });
    }

    ValUse.set(VecToMatStub);
  }

  AddToDeadInsts(MatInst);
}

Value *HLMatrixLowerPass::lowerInstruction(Instruction *MatInst) {
  if (AllocaInst *Alloca = dyn_cast<AllocaInst>(MatInst)) {
    return lowerAlloca(Alloca);
  }
  
  if (CallInst *Call = dyn_cast<CallInst>(MatInst)) {
    HLOpcodeGroup OpcodeGroup = GetHLOpcodeGroupByName(Call->getCalledFunction());
    if (OpcodeGroup == HLOpcodeGroup::NotHL)
      llvm_unreachable("Not implemented");
    else
      return lowerHLOperation(Call, OpcodeGroup);
  }

  llvm_unreachable("Not implemented");
}

AllocaInst *HLMatrixLowerPass::lowerAlloca(AllocaInst *MatAlloca) {
  PointerType *LoweredAllocaTy = cast<PointerType>(getLoweredType(MatAlloca->getType()));

  IRBuilder<> Builder(MatAlloca);
  AllocaInst *VecAlloca = Builder.CreateAlloca(LoweredAllocaTy->getElementType(), nullptr, MatAlloca->getName());

  // Update debug info.
  if (DbgDeclareInst *DbgDeclare = llvm::FindAllocaDbgDeclare(MatAlloca)) {
    LLVMContext &Context = MatAlloca->getContext();
    Value *DbgDeclareVar = MetadataAsValue::get(Context, DbgDeclare->getRawVariable());
    Value *DbgDeclareExpr = MetadataAsValue::get(Context, DbgDeclare->getRawExpression());
    Value *ValueMetadata = MetadataAsValue::get(Context, ValueAsMetadata::get(VecAlloca));
    IRBuilder<> DebugBuilder(DbgDeclare);
    DebugBuilder.CreateCall(DbgDeclare->getCalledFunction(), { ValueMetadata, DbgDeclareVar, DbgDeclareExpr });
  }

  if (HLModule::HasPreciseAttributeWithMetadata(MatAlloca))
    HLModule::MarkPreciseAttributeWithMetadata(VecAlloca);

  return VecAlloca;
}

Value *HLMatrixLowerPass::lowerHLOperation(CallInst *Call, HLOpcodeGroup OpcodeGroup) {
  IRBuilder<> Builder(Call);
  switch (OpcodeGroup) {
  case HLOpcodeGroup::HLIntrinsic:
    return lowerHLIntrinsic(Call, static_cast<IntrinsicOp>(GetHLOpcode(Call)));

  case HLOpcodeGroup::HLBinOp:
    return lowerHLBinaryOperation(
      Call->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx),
      Call->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx),
      static_cast<HLBinaryOpcode>(GetHLOpcode(Call)), Builder);

  case HLOpcodeGroup::HLUnOp:
    return lowerHLUnaryOperation(
      Call->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx),
      static_cast<HLUnaryOpcode>(GetHLOpcode(Call)), Builder);

  case HLOpcodeGroup::HLMatLoadStore:
    return lowerHLLoadStore(Call, static_cast<HLMatLoadStoreOpcode>(GetHLOpcode(Call)));

  case HLOpcodeGroup::HLCast:
    return lowerHLCast(
      Call->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx), Call->getType(),
      static_cast<HLCastOpcode>(GetHLOpcode(Call)), Builder);

  case HLOpcodeGroup::HLSubscript:
    return lowerHLSubscript(Call, static_cast<HLSubscriptOpcode>(GetHLOpcode(Call)));

  case HLOpcodeGroup::HLInit:
    return lowerHLInit(Call);

  default:
    llvm_unreachable("Not implemented");
  }
}

static Value *callHLFunction(llvm::Module &Module, HLOpcodeGroup OpcodeGroup, unsigned Opcode,
  Type *RetTy, ArrayRef<Value*> Args, IRBuilder<> &Builder) {
  SmallVector<Type*, 4> ArgTys;
  ArgTys.reserve(Args.size());
  for (Value *Arg : Args)
    ArgTys.emplace_back(Arg->getType());

  FunctionType *FuncTy = FunctionType::get(RetTy, ArgTys, /* isVarArg */ false);
  Function *Func = GetOrCreateHLFunction(Module, FuncTy, OpcodeGroup, Opcode);

  return Builder.CreateCall(Func, Args);
}

Value *HLMatrixLowerPass::lowerHLIntrinsic(CallInst *Call, IntrinsicOp Opcode) {
  IRBuilder<> Builder(Call);

  // See if this is a matrix-specific intrinsic which we should expand here
  switch (Opcode) {
  case IntrinsicOp::IOP_umul:
  case IntrinsicOp::IOP_mul:
    return lowerHLMulIntrinsic(
      Call->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx),
      Call->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx),
      /* Unsigned */ Opcode == IntrinsicOp::IOP_umul, Builder);
  case IntrinsicOp::IOP_transpose:
    return lowerHLTransposeIntrinsic(Call->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx), Builder);
  case IntrinsicOp::IOP_determinant:
    return lowerHLDeterminantIntrinsic(Call->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx), Builder);
  }

  // Delegate to a lowered intrinsic call
  SmallVector<Value*, 4> LoweredArgs;
  LoweredArgs.reserve(Call->getNumArgOperands());
  for (Value *Arg : Call->arg_operands())
    LoweredArgs.emplace_back(getLoweredByValOperand(Arg, Builder));

  Type *LoweredRetTy = getLoweredType(Call->getType());
  return callHLFunction(*m_pModule, HLOpcodeGroup::HLIntrinsic, static_cast<unsigned>(Opcode), 
    LoweredRetTy, LoweredArgs, Builder);
}

Value *HLMatrixLowerPass::lowerHLMulIntrinsic(Value* Lhs, Value *Rhs,
    bool Unsigned, IRBuilder<> &Builder) {
  HLMatrixType LhsMatTy = HLMatrixType::dyn_cast(Lhs->getType());
  HLMatrixType RhsMatTy = HLMatrixType::dyn_cast(Rhs->getType());
  Value* LoweredLhs = getLoweredByValOperand(Lhs, Builder);
  Value* LoweredRhs = getLoweredByValOperand(Rhs, Builder);

  DXASSERT(LoweredLhs->getType()->getScalarType() == LoweredRhs->getType()->getScalarType(),
    "Unexpected element type mismatch in mul intrinsic.");
  DXASSERT(cast<VectorType>(LoweredLhs->getType()) && cast<VectorType>(LoweredLhs->getType()),
    "Unexpected scalar in lowered matrix mul intrinsic operands.");

  Type* ElemTy = LoweredLhs->getType()->getScalarType();

  // Figure out the dimensions of each side
  unsigned LhsNumRows, LhsNumCols, RhsNumRows, RhsNumCols;
  if (LhsMatTy && RhsMatTy) {
    LhsNumRows = LhsMatTy.getNumRows();
    LhsNumCols = LhsMatTy.getNumColumns();
    RhsNumRows = RhsMatTy.getNumRows();
    RhsNumCols = RhsMatTy.getNumColumns();
  }
  else if (LhsMatTy) {
    LhsNumRows = LhsMatTy.getNumRows();
    LhsNumCols = LhsMatTy.getNumColumns();
    RhsNumRows = LoweredRhs->getType()->getVectorNumElements();
    RhsNumCols = 1;
  }
  else if (RhsMatTy) {
    LhsNumRows = 1;
    LhsNumCols = LoweredLhs->getType()->getVectorNumElements();
    RhsNumRows = RhsMatTy.getNumRows();
    RhsNumCols = RhsMatTy.getNumColumns();
  }
  else {
    llvm_unreachable("mul intrinsic was identified as a matrix operation but neither operand is a matrix.");
  }

  DXASSERT(LhsNumCols == RhsNumRows, "Matrix mul intrinsic operands dimensions mismatch.");
  unsigned ResultNumRows = LhsNumRows;
  unsigned ResultNumCols = RhsNumCols;
  unsigned AccCount = LhsNumCols;

  // Get the multiply-and-add intrinsic function, we'll need it
  IntrinsicOp MadOpcode = Unsigned ? IntrinsicOp::IOP_umad : IntrinsicOp::IOP_mad;
  FunctionType *MadFuncTy = FunctionType::get(ElemTy, { Builder.getInt32Ty(), ElemTy, ElemTy, ElemTy }, false);
  Function *MadFunc = GetOrCreateHLFunction(*m_pModule, MadFuncTy, HLOpcodeGroup::HLIntrinsic, (unsigned)MadOpcode);
  Constant *MadOpcodeVal = Builder.getInt32((unsigned)MadOpcode);

  // Perform the multiplication!
  Value *Result = UndefValue::get(VectorType::get(ElemTy, LhsNumRows * RhsNumCols));
  for (unsigned ResultRowIdx = 0; ResultRowIdx < ResultNumRows; ++ResultRowIdx) {
    for (unsigned ResultColIdx = 0; ResultColIdx < ResultNumCols; ++ResultColIdx) {
      unsigned ResultElemIdx = ResultRowIdx * ResultNumCols + ResultColIdx;
      Value *ResultElem = Constant::getNullValue(ElemTy);

      for (unsigned AccIdx = 0; AccIdx < AccCount; ++AccIdx) {
        unsigned LhsElemIdx = ResultRowIdx * LhsNumCols + AccIdx;
        unsigned RhsElemIdx = AccIdx * RhsNumCols + ResultColIdx;
        Value* LhsElem = Builder.CreateExtractElement(LoweredLhs, static_cast<uint64_t>(LhsElemIdx));
        Value* RhsElem = Builder.CreateExtractElement(LoweredRhs, static_cast<uint64_t>(RhsElemIdx));
        ResultElem = Builder.CreateCall(MadFunc, { MadOpcodeVal, LhsElem, RhsElem, ResultElem });
      }

      Result = Builder.CreateInsertElement(Result, ResultElem, static_cast<uint64_t>(ResultElemIdx));
    }
  }

  return Result;
}

Value *HLMatrixLowerPass::lowerHLTransposeIntrinsic(Value* MatVal, IRBuilder<> &Builder) {
  HLMatrixType MatTy = HLMatrixType::cast(MatVal->getType());
  Value *LoweredVal = getLoweredByValOperand(MatVal, Builder);
  return MatTy.emitLoweredVectorRowToCol(LoweredVal, Builder);
}

Value *HLMatrixLowerPass::lowerHLDeterminantIntrinsic(Value* MatVal, IRBuilder<> &Builder) {
  HLMatrixType MatTy = HLMatrixType::cast(MatVal->getType());
  DXASSERT_NOMSG(MatTy.getNumColumns() == MatTy.getNumRows());

  Value *LoweredVal = getLoweredByValOperand(MatVal, Builder);

  // Extract all matrix elements
  SmallVector<Value*, 16> Elems;
  for (unsigned ElemIdx = 0; ElemIdx < MatTy.getNumElements(); ++ElemIdx)
    Elems.emplace_back(Builder.CreateExtractElement(LoweredVal, static_cast<uint64_t>(ElemIdx)));

  // Delegate to appropriate determinant function
  switch (MatTy.getNumColumns()) {
  case 1:
    return Elems[0];

  case 2:
    return Determinant2x2(
      Elems[0], Elems[1],
      Elems[2], Elems[3],
      Builder);

  case 3:
    return Determinant3x3(
      Elems[0], Elems[1], Elems[2],
      Elems[3], Elems[4], Elems[5],
      Elems[6], Elems[7], Elems[8],
      Builder);

  case 4:
    return Determinant4x4(
      Elems[0], Elems[1], Elems[2], Elems[3],
      Elems[4], Elems[5], Elems[6], Elems[7],
      Elems[8], Elems[9], Elems[10], Elems[11],
      Elems[12], Elems[13], Elems[14], Elems[15],
      Builder);

  default:
    llvm_unreachable("Unexpected matrix dimensions.");
  }
}

Value *HLMatrixLowerPass::lowerHLUnaryOperation(Value *MatVal, HLUnaryOpcode Opcode, IRBuilder<> &Builder) {
  Value *LoweredVal = getLoweredByValOperand(MatVal, Builder);
  VectorType *VecTy = cast<VectorType>(LoweredVal->getType());
  bool IsFloat = VecTy->getElementType()->isFloatingPointTy();
  
  switch (Opcode) {
  case HLUnaryOpcode::Plus: return LoweredVal; // No-op

  case HLUnaryOpcode::Minus:
    return IsFloat
      ? Builder.CreateFSub(Constant::getNullValue(VecTy), LoweredVal)
      : Builder.CreateSub(Constant::getNullValue(VecTy), LoweredVal);

  case HLUnaryOpcode::LNot:
    return IsFloat
      ? Builder.CreateFCmp(CmpInst::FCMP_UEQ, LoweredVal, Constant::getNullValue(VecTy))
      : Builder.CreateICmp(CmpInst::ICMP_EQ, LoweredVal, Constant::getNullValue(VecTy));

  case HLUnaryOpcode::Not:
    return Builder.CreateXor(LoweredVal, Constant::getAllOnesValue(VecTy));

  case HLUnaryOpcode::PostInc:
  case HLUnaryOpcode::PreInc:
  case HLUnaryOpcode::PostDec:
  case HLUnaryOpcode::PreDec: {
    Constant *ScalarOne = IsFloat
      ? ConstantFP::get(VecTy->getElementType(), 1)
      : ConstantInt::get(VecTy->getElementType(), 1);
    Constant *VecOne = ConstantVector::getSplat(VecTy->getNumElements(), ScalarOne);
    // BUGBUG: This implementation has incorrect semantics (GitHub #1780)
    if (Opcode == HLUnaryOpcode::PostInc || Opcode == HLUnaryOpcode::PreInc) {
      return IsFloat
        ? Builder.CreateFAdd(LoweredVal, VecOne)
        : Builder.CreateAdd(LoweredVal, VecOne);
    }
    else {
      return IsFloat
        ? Builder.CreateFSub(LoweredVal, VecOne)
        : Builder.CreateSub(LoweredVal, VecOne);
    }
  }
  default:
    llvm_unreachable("Unsupported unary matrix operator");
  }
}

Value *HLMatrixLowerPass::lowerHLBinaryOperation(Value *Lhs, Value *Rhs, HLBinaryOpcode Opcode, IRBuilder<> &Builder) {
  Value *LoweredLhs = getLoweredByValOperand(Lhs, Builder);
  Value *LoweredRhs = getLoweredByValOperand(Rhs, Builder);

  DXASSERT(LoweredLhs->getType()->isVectorTy() && LoweredRhs->getType()->isVectorTy(),
    "Expected lowered binary operation operands to be vectors");
  DXASSERT(LoweredLhs->getType() == LoweredRhs->getType(),
    "Expected lowered binary operation operands to have matching types.");

  bool IsFloat = LoweredLhs->getType()->getVectorElementType()->isFloatingPointTy();

  switch (Opcode) {
  case HLBinaryOpcode::Add:
    return IsFloat
      ? Builder.CreateFAdd(LoweredLhs, LoweredRhs)
      : Builder.CreateAdd(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::Sub:
    return IsFloat
      ? Builder.CreateFSub(LoweredLhs, LoweredRhs)
      : Builder.CreateSub(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::Mul:
    return IsFloat
      ? Builder.CreateFMul(LoweredLhs, LoweredRhs)
      : Builder.CreateMul(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::Div:
    return IsFloat
      ? Builder.CreateFDiv(LoweredLhs, LoweredRhs)
      : Builder.CreateSDiv(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::Rem:
    return IsFloat
      ? Builder.CreateFRem(LoweredLhs, LoweredRhs)
      : Builder.CreateSRem(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::And:
    return Builder.CreateAnd(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::Or:
    return Builder.CreateAnd(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::Xor:
    return Builder.CreateAnd(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::Shl:
    return Builder.CreateShl(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::Shr:
    return Builder.CreateAShr(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::LT:
    return IsFloat
      ? Builder.CreateFCmp(CmpInst::FCMP_OLT, LoweredLhs, LoweredRhs)
      : Builder.CreateICmp(CmpInst::ICMP_SLT, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::GT:
    return IsFloat
      ? Builder.CreateFCmp(CmpInst::FCMP_OGT, LoweredLhs, LoweredRhs)
      : Builder.CreateICmp(CmpInst::ICMP_SGT, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::LE:
    return IsFloat
      ? Builder.CreateFCmp(CmpInst::FCMP_OLE, LoweredLhs, LoweredRhs)
      : Builder.CreateICmp(CmpInst::ICMP_SLE, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::GE:
    return IsFloat
      ? Builder.CreateFCmp(CmpInst::FCMP_OGE, LoweredLhs, LoweredRhs)
      : Builder.CreateICmp(CmpInst::ICMP_SGE, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::EQ:
    return IsFloat
      ? Builder.CreateFCmp(CmpInst::FCMP_OEQ, LoweredLhs, LoweredRhs)
      : Builder.CreateICmp(CmpInst::ICMP_EQ, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::NE:
    return IsFloat
      ? Builder.CreateFCmp(CmpInst::FCMP_ONE, LoweredLhs, LoweredRhs)
      : Builder.CreateICmp(CmpInst::ICMP_NE, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::UDiv:
    return Builder.CreateUDiv(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::URem:
    return Builder.CreateURem(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::UShr:
    return Builder.CreateLShr(LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::ULT:
    return Builder.CreateICmp(CmpInst::ICMP_ULT, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::UGT:
    return Builder.CreateICmp(CmpInst::ICMP_UGT, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::ULE:
    return Builder.CreateICmp(CmpInst::ICMP_ULE, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::UGE:
    return Builder.CreateICmp(CmpInst::ICMP_UGE, LoweredLhs, LoweredRhs);

  case HLBinaryOpcode::LAnd:
  case HLBinaryOpcode::LOr: {
    Value *Zero = Constant::getNullValue(LoweredLhs->getType());
    Value *LhsCmp = IsFloat
      ? Builder.CreateFCmp(CmpInst::FCMP_ONE, LoweredLhs, Zero)
      : Builder.CreateICmp(CmpInst::ICMP_NE, LoweredLhs, Zero);
    Value *RhsCmp = IsFloat
      ? Builder.CreateFCmp(CmpInst::FCMP_ONE, LoweredRhs, Zero)
      : Builder.CreateICmp(CmpInst::ICMP_NE, LoweredRhs, Zero);
    return Opcode == HLBinaryOpcode::LOr
      ? Builder.CreateOr(LhsCmp, RhsCmp)
      : Builder.CreateAnd(LhsCmp, RhsCmp);
  }
  default:
    llvm_unreachable("Unsupported binary matrix operator");
  }
}

Value *HLMatrixLowerPass::lowerHLLoadStore(CallInst *Call, HLMatLoadStoreOpcode Opcode) {
  IRBuilder<> Builder(Call);
  switch (Opcode) {
  case HLMatLoadStoreOpcode::RowMatLoad:
  case HLMatLoadStoreOpcode::ColMatLoad:
    return lowerHLLoad(Call->getArgOperand(HLOperandIndex::kMatLoadPtrOpIdx),
      /* RowMajor */ Opcode == HLMatLoadStoreOpcode::RowMatLoad, Builder);

  case HLMatLoadStoreOpcode::RowMatStore:
  case HLMatLoadStoreOpcode::ColMatStore:
    return lowerHLStore(
      Call->getArgOperand(HLOperandIndex::kMatStoreValOpIdx),
      Call->getArgOperand(HLOperandIndex::kMatStoreDstPtrOpIdx),
      /* RowMajor */ Opcode == HLMatLoadStoreOpcode::RowMatStore,
      /* Return */ !Call->getType()->isVoidTy(), Builder);

  default:
    llvm_unreachable("Unsupported matrix load/store operation");
  }
}

Value *HLMatrixLowerPass::lowerHLLoad(Value *MatPtr, bool RowMajor, IRBuilder<> &Builder) {
  HLMatrixType MatTy = HLMatrixType::cast(MatPtr->getType()->getPointerElementType());

  Value *LoweredPtr = tryGetLoweredMatrixPtrOperand(MatPtr, Builder);
  if (LoweredPtr == nullptr) {
    // Can't lower this here, defer to HL signature lower
    HLMatLoadStoreOpcode Opcode = RowMajor ? HLMatLoadStoreOpcode::RowMatLoad : HLMatLoadStoreOpcode::ColMatLoad;
    return callHLFunction(
      *m_pModule, HLOpcodeGroup::HLMatLoadStore, static_cast<unsigned>(Opcode),
      MatTy.getLoweredVectorTypeForReg(), { Builder.getInt32((uint32_t)Opcode), MatPtr }, Builder);
  }

  return MatTy.emitLoweredVectorLoad(LoweredPtr, Builder);
}

Value *HLMatrixLowerPass::lowerHLStore(Value *MatVal, Value *MatPtr, bool RowMajor, bool Return, IRBuilder<> &Builder) {
  DXASSERT(MatVal->getType() == MatPtr->getType()->getPointerElementType(),
    "Matrix store value/pointer type mismatch.");

  Value *LoweredPtr = tryGetLoweredMatrixPtrOperand(MatPtr, Builder);
  Value *LoweredVal = getLoweredByValOperand(MatVal, Builder);
  if (LoweredPtr == nullptr) {
    // Can't lower the pointer here, defer to HL signature lower
    HLMatLoadStoreOpcode Opcode = RowMajor ? HLMatLoadStoreOpcode::RowMatStore : HLMatLoadStoreOpcode::ColMatStore;
    return callHLFunction(
      *m_pModule, HLOpcodeGroup::HLMatLoadStore, static_cast<unsigned>(Opcode),
      Return ? LoweredVal->getType() : Builder.getVoidTy(),
      { Builder.getInt32((uint32_t)Opcode), MatPtr, LoweredVal }, Builder);
  }

  HLMatrixType MatTy = HLMatrixType::cast(MatPtr->getType()->getPointerElementType());
  StoreInst *LoweredStore = MatTy.emitLoweredVectorStore(LoweredVal, LoweredPtr, Builder);

  // If the intrinsic returned a value, return the stored lowered value
  return Return ? LoweredVal : LoweredStore;
}

static Value *convertScalarOrVector(Value *SrcVal, Type *DstTy, HLCastOpcode HLCastOpcode, IRBuilder<> Builder) {
  DXASSERT(SrcVal->getType()->isVectorTy() == DstTy->isVectorTy(),
    "Scalar/vector type mismatch in numerical conversion.");
  Type *SrcTy = SrcVal->getType();

  // Conversions between equivalent types are no-ops,
  // even between signed/unsigned variants.
  if (SrcTy == DstTy) return SrcVal;

  // Conversions to bools are comparisons
  if (DstTy->getScalarSizeInBits() == 1) {
    // fcmp une is what regular clang uses in C++ for (bool)f;
    return cast<Instruction>(SrcTy->isIntOrIntVectorTy()
      ? Builder.CreateICmpNE(SrcVal, llvm::Constant::getNullValue(SrcTy), "tobool")
      : Builder.CreateFCmpUNE(SrcVal, llvm::Constant::getNullValue(SrcTy), "tobool"));
  }

  // Cast necessary
  bool SrcIsUnsigned = HLCastOpcode == HLCastOpcode::FromUnsignedCast ||
    HLCastOpcode == HLCastOpcode::UnsignedUnsignedCast;
  bool DstIsUnsigned = HLCastOpcode == HLCastOpcode::ToUnsignedCast ||
    HLCastOpcode == HLCastOpcode::UnsignedUnsignedCast;
  auto CastOp = static_cast<Instruction::CastOps>(HLModule::GetNumericCastOp(
    SrcTy, SrcIsUnsigned, DstTy, DstIsUnsigned));
  return cast<Instruction>(Builder.CreateCast(CastOp, SrcVal, DstTy));
}

Value *HLMatrixLowerPass::lowerHLCast(Value *Src, Type *DstTy, HLCastOpcode Opcode, IRBuilder<> &Builder) {
  DXASSERT(Opcode != HLCastOpcode::HandleToResCast, "Unexpected matrix cast opcode.");

  if (dxilutil::IsIntegerOrFloatingPointType(Src->getType())) {
    // Scalar to matrix splat
    HLMatrixType MatDstTy = HLMatrixType::cast(DstTy);

    // Apply element conversion
    Value *Result = convertScalarOrVector(Src,
      MatDstTy.getElementType(/* MemRepr */ false), Opcode, Builder);

    // Splat to a vector
    Result = Builder.CreateInsertElement(
      UndefValue::get(VectorType::get(Result->getType(), 1)),
      Result, static_cast<uint64_t>(0));
    return Builder.CreateShuffleVector(Result, Result,
      ConstantVector::getSplat(MatDstTy.getNumElements(), Builder.getInt32(0)));
  }
  else if (Src->getType()->isVectorTy()) {
    // Vector to matrix
    llvm_unreachable("Vector to matrix casts not implemented");
  }

  // Source must now be a matrix
  HLMatrixType MatSrcTy = HLMatrixType::cast(Src->getType());
  VectorType* LoweredSrcTy = MatSrcTy.getLoweredVectorTypeForReg();

  Value *LoweredSrc;
  if (isa<Argument>(Src)) {
    // Function arguments are lowered in HLSignatureLower.
    // Initial codegen first generates those cast intrinsics to tell us how to lower them into vectors.
    // Preserve them, but change the return type to vector.
    DXASSERT(Opcode == HLCastOpcode::ColMatrixToVecCast || Opcode == HLCastOpcode::RowMatrixToVecCast,
      "Unexpected cast of matrix argument.");
    LoweredSrc = callHLFunction(*m_pModule, HLOpcodeGroup::HLCast, static_cast<unsigned>(Opcode),
      LoweredSrcTy, { Builder.getInt32((uint32_t)Opcode), Src }, Builder);
  }
  else {
    LoweredSrc = getLoweredByValOperand(Src, Builder);
  }
  DXASSERT_NOMSG(LoweredSrc->getType() == LoweredSrcTy);

  Value* Result = LoweredSrc;
  Type* LoweredDstTy = DstTy;
  if (dxilutil::IsIntegerOrFloatingPointType(DstTy)) {
    // Matrix to scalar
    Result = Builder.CreateExtractElement(LoweredSrc, static_cast<uint64_t>(0));
  }
  else if (DstTy->isVectorTy()) {
    // Matrix to vector
    DXASSERT(Opcode == HLCastOpcode::ColMatrixToVecCast || Opcode == HLCastOpcode::RowMatrixToVecCast,
      "Unexpected matrix cast opcode resulting in a vector type.");
    VectorType *DstVecTy = cast<VectorType>(DstTy);
    DXASSERT(DstVecTy->getNumElements() <= LoweredSrcTy->getNumElements(),
      "Cannot cast matrix to a larger vector.");

    // We might have to truncate
    if (DstTy->getVectorNumElements() < LoweredSrcTy->getNumElements()) {
      SmallVector<int, 3> ShuffleIndices;
      for (unsigned Idx = 0; Idx < DstVecTy->getNumElements(); ++Idx)
        ShuffleIndices.emplace_back(static_cast<int>(Idx));
      Result = Builder.CreateShuffleVector(Result, Result, ShuffleIndices);
    }
  }
  else {
    // Destination must now be a matrix too
    HLMatrixType MatDstTy = HLMatrixType::cast(DstTy);

    // Apply any changes at the matrix level: orientation changes and truncation
    Value *Result = LoweredSrc;
    if (Opcode == HLCastOpcode::ColMatrixToRowMatrix)
      Result = MatSrcTy.emitLoweredVectorColToRow(Result, Builder);
    else if (Opcode == HLCastOpcode::RowMatrixToColMatrix)
      Result = MatSrcTy.emitLoweredVectorRowToCol(Result, Builder);
    else if (MatDstTy.getNumRows() != MatSrcTy.getNumRows()
      || MatDstTy.getNumColumns() != MatSrcTy.getNumColumns()) {
      // Apply truncation
      llvm_unreachable("Matrix truncation not implemented");
    }

    LoweredDstTy = MatDstTy.getLoweredVectorTypeForReg();
    DXASSERT(Result->getType()->getVectorNumElements() == LoweredDstTy->getVectorNumElements(),
      "Unexpected matrix src/dst lowered element count mismatch after truncation.");
  }

  // Apply element conversion
  return convertScalarOrVector(Result, LoweredDstTy, Opcode, Builder);
}

Value *HLMatrixLowerPass::lowerHLSubscript(CallInst *Call, HLSubscriptOpcode Opcode) {
  switch (Opcode) {
  case HLSubscriptOpcode::RowMatElement:
  case HLSubscriptOpcode::ColMatElement:
    return lowerHLMatElementSubscript(Call,
      /* RowMajor */ Opcode == HLSubscriptOpcode::RowMatElement);

  case HLSubscriptOpcode::RowMatSubscript:
  case HLSubscriptOpcode::ColMatSubscript:
    return lowerHLMatSubscript(Call,
      /* RowMajor */ Opcode == HLSubscriptOpcode::RowMatSubscript);

  default:
    llvm_unreachable("Unexpected matrix subscript opcode.");
  }
}

Value *HLMatrixLowerPass::lowerHLMatElementSubscript(CallInst *Call, bool RowMajor) {
  (void)RowMajor; // It doesn't look like we actually need this?

  Value *MatPtr = Call->getArgOperand(HLOperandIndex::kMatSubscriptMatOpIdx);
  DXASSERT_NOMSG(MatPtr->getType()->isPointerTy());

  Value *IdxVec = Call->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx);
  Constant *ConstIdxVec = cast<Constant>(IdxVec);
  DXASSERT_NOMSG(ConstIdxVec->getType()->isVectorTy());

  // Get the loaded lowered vector element indices
  SmallVector<int, 4> ElemIndices;
  ElemIndices.reserve(ConstIdxVec->getNumOperands());
  for (Value *Idx : ConstIdxVec->operand_values())
    ElemIndices.emplace_back(static_cast<int>(cast<ConstantInt>(Idx)->getLimitedValue()));

  return lowerHLMatConstantSubscript(Call, MatPtr, ElemIndices);
}

Value *HLMatrixLowerPass::lowerHLMatSubscript(CallInst *Call, bool RowMajor) {
  (void)RowMajor; // It doesn't look like we actually need this?

  Value *MatPtr = Call->getArgOperand(HLOperandIndex::kMatSubscriptMatOpIdx);
  DXASSERT_NOMSG(MatPtr->getType()->isPointerTy());

  // Gather the indices, checking if they are all constant
  SmallVector<Value*, 4> ElemIndices;
  SmallVector<int, 4> ConstElemIndices;
  bool HasDynamicIndexing = false;
  for (unsigned Idx = HLOperandIndex::kMatSubscriptSubOpIdx; Idx < Call->getNumArgOperands(); ++Idx) {
    Value *ElemIdxVal = Call->getArgOperand(Idx);
    ElemIndices.emplace_back(ElemIdxVal);

    if (!HasDynamicIndexing) {
      if (ConstantInt *ElemIdxConst = dyn_cast<ConstantInt>(ElemIdxVal))
        ConstElemIndices.emplace_back((int)ElemIdxConst->getLimitedValue());
      else
        HasDynamicIndexing = false;
    }
  }

  // Lower as constant if possible, otherwise as dynamic, which involves creating an array
  return HasDynamicIndexing
    ? lowerHLMatDynamicSubscript(Call, MatPtr, ElemIndices)
    : lowerHLMatConstantSubscript(Call, MatPtr, ConstElemIndices);
}

Value *HLMatrixLowerPass::lowerHLMatConstantSubscript(CallInst *Call, Value *MatPtr, SmallVectorImpl<int> &ElemIndices) {
  // If the source matrix is from a buffer or such, it's not this job's pass to lower it
  IRBuilder<> CallBuilder(Call);
  Value *LoweredPtr = tryGetLoweredMatrixPtrOperand(MatPtr, CallBuilder);
  if (LoweredPtr == nullptr) return nullptr;

  HLMatrixType MatTy = HLMatrixType::cast(MatPtr->getType()->getPointerElementType());

  bool AsVector = Call->getType()->getPointerElementType()->isVectorTy();
  DXASSERT(AsVector || ElemIndices.size() == 1, "Subscript should only return a scalar pointer if it has a single index.");

  // Users of the pointer resulting from the subscript should be either loads or stores.
  // Replace them by their equivalent vector element subset accesses.
  while (!Call->use_empty()) {
    llvm::Use &Use = *Call->use_begin();
    Instruction *UserInst = cast<Instruction>(Use.getUser());

    // First load the entire vector, whether we want to load a subset of it,
    // or modify a subset of it and store it back.
    // Load it every time since the value could have changed.
    IRBuilder<> UserBuilder(UserInst);
    Value* MatVec = MatTy.emitLoweredVectorLoad(LoweredPtr, UserBuilder);

    if (LoadInst *Load = dyn_cast<LoadInst>(Use.getUser())) {
      // Return the interesting subset
      Value* SubscriptVal = AsVector
        ? UserBuilder.CreateShuffleVector(MatVec, MatVec, ElemIndices)
        : UserBuilder.CreateExtractElement(MatVec, ElemIndices[0]);
      Load->replaceAllUsesWith(SubscriptVal);
    }
    else if (StoreInst *Store = dyn_cast<StoreInst>(Use.getUser())) {
      DXASSERT_NOMSG(Store->getPointerOperand() == Call);
      // Update the interesting subset
      if (AsVector) {
        // Shuffle the original and incoming vectors together,
        // start with indices 0 to N-1, preserving the original vector elements,
        // then update the indices being written to to point to the new vector elements.
        SmallVector<int, 16> ShuffleIndices;
        for (unsigned Idx = 0; Idx < MatTy.getNumElements(); ++Idx)
          ShuffleIndices.emplace_back((int)Idx);
        for (unsigned Idx = 0; Idx < ElemIndices.size(); ++Idx)
          ShuffleIndices[ElemIndices[Idx]] = MatTy.getNumElements() + (int)Idx;
        MatVec = UserBuilder.CreateShuffleVector(MatVec, Store->getValueOperand(), ShuffleIndices);
      }
      else {
        MatVec = UserBuilder.CreateInsertElement(MatVec, Store->getValueOperand(), ElemIndices[0]);
      }

      MatTy.emitLoweredVectorStore(MatVec, LoweredPtr, UserBuilder);
    }
    else
      llvm_unreachable("Unexpected matrix element subscript user.");

    // We've replaced the user, mark it dead and remove the use
    Use.set(UndefValue::get(Use->getType()));
    AddToDeadInsts(UserInst);
  }

  // We've already taken care of users of the matrix instruction
  // and the subscript doesn't exist anymore, so return null (nothing to replace).
  return nullptr;
}

Value *HLMatrixLowerPass::lowerHLMatDynamicSubscript(CallInst *Call, Value *MatPtr, SmallVectorImpl<Value*> &ElemIndices) {
  llvm_unreachable("Matrix subscripts with dynamic indices.");
}

Value *HLMatrixLowerPass::lowerHLInit(CallInst *Call) {
  DXASSERT(GetHLOpcode(Call) == 0, "Unexpected matrix init opcode.");

  // Figure out the result type
  HLMatrixType MatTy = HLMatrixType::cast(Call->getType());
  VectorType *LoweredTy = MatTy.getLoweredVectorTypeForReg();
  DXASSERT(LoweredTy->getNumElements() == Call->getNumArgOperands() - HLOperandIndex::kInitFirstArgOpIdx,
    "Invalid matrix init argument count.");

  // Build the result vector from the init args.
  // Both the args and the result vector are in row-major order, so no shuffling is necessary.
  IRBuilder<> Builder(Call);
  Value *LoweredVec = UndefValue::get(LoweredTy);
  for (unsigned VecElemIdx = 0; VecElemIdx < LoweredTy->getNumElements(); ++VecElemIdx) {
    Value *ArgVal = Call->getArgOperand(HLOperandIndex::kInitFirstArgOpIdx + VecElemIdx);
    DXASSERT(dxilutil::IsIntegerOrFloatingPointType(ArgVal->getType()),
      "Expected only scalars in matrix initialization.");
    LoweredVec = Builder.CreateInsertElement(LoweredVec, ArgVal, static_cast<uint64_t>(VecElemIdx));
  }

  return LoweredVec;
}
