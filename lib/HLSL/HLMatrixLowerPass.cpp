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

#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HLSL/HLMatrixLowerPass.h"
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

bool IsMatrixType(Type *Ty) {
  if (StructType *ST = dyn_cast<StructType>(Ty)) {
    Type *EltTy = ST->getElementType(0);
    if (!ST->getName().startswith("class.matrix"))
      return false;

    bool isVecArray = EltTy->isArrayTy() &&
           EltTy->getArrayElementType()->isVectorTy();

    return isVecArray && EltTy->getArrayNumElements() <= 4;
  }
  return false;
}

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
  } else if (IsMatrixType(Ty)) {
    unsigned row, col;
    Type *EltTy = GetMatrixInfo(Ty, col, row);
    if (forMem && EltTy->isIntegerTy(1))
      EltTy = Type::getInt32Ty(Ty->getContext());
    return VectorType::get(EltTy, row * col);
  } else {
    return Ty;
  }
}

// Translate matrix type to array type.
Type *LowerMatrixTypeToOneDimArray(Type *Ty) {
  if (IsMatrixType(Ty)) {
    unsigned row, col;
    Type *EltTy = GetMatrixInfo(Ty, col, row);
    return ArrayType::get(EltTy, row * col);
  } else {
    return Ty;
  }
}


Type *GetMatrixInfo(Type *Ty, unsigned &col, unsigned &row) {
  DXASSERT(IsMatrixType(Ty), "not matrix type");
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
  return IsMatrixType(Ty);
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

Type *LowerMatrixArrayPointerToOneDimArray(Type *Ty) {
  unsigned addrSpace = Ty->getPointerAddressSpace();
  Ty = Ty->getPointerElementType();

  unsigned arraySize = 1;
  while (Ty->isArrayTy()) {
    arraySize *= Ty->getArrayNumElements();
    Ty = Ty->getArrayElementType();
  }
  unsigned row, col;
  Type *EltTy = GetMatrixInfo(Ty, col, row);
  arraySize *= row*col;

  Ty = ArrayType::get(EltTy, arraySize);
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

class HLMatrixLowerPass : public ModulePass {

public:
  static char ID; // Pass identification, replacement for typeid
  explicit HLMatrixLowerPass() : ModulePass(ID) {}

  const char *getPassName() const override { return "HL matrix lower"; }

  bool runOnModule(Module &M) override {
    m_pModule = &M;
    m_pHLModule = &m_pModule->GetOrCreateHLModule();
    // Load up debug information, to cross-reference values and the instructions
    // used to load them.
    m_HasDbgInfo = getDebugMetadataVersionFromModule(M) != 0;

    for (Function &F : M.functions()) {

      if (F.isDeclaration())
        continue;
      runOnFunction(F);
    }
    std::vector<GlobalVariable*> staticGVs;
    for (GlobalVariable &GV : M.globals()) {
      if (dxilutil::IsStaticGlobal(&GV) ||
          dxilutil::IsSharedMemoryGlobal(&GV)) {
        staticGVs.emplace_back(&GV);
      }
    }

    for (GlobalVariable *GV : staticGVs)
      runOnGlobal(GV);

    return true;
  }

private:
  Module *m_pModule;
  HLModule *m_pHLModule;
  bool m_HasDbgInfo;
  std::vector<Instruction *> m_deadInsts;
  // For instruction like matrix array init.
  // May use more than 1 matrix alloca inst.
  // This set is here to avoid put it into deadInsts more than once.
  std::unordered_set<Instruction *> m_inDeadInstsSet;
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
  void runOnFunction(Function &F);
  void runOnGlobal(GlobalVariable *GV);
  void runOnGlobalMatrixArray(GlobalVariable *GV);
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
  // Delete dead insts in m_deadInsts.
  void DeleteDeadInsts();
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
  // Cast to bool.
  if (toTy->getScalarType()->isIntegerTy(1)) {
    Type *fromTy = src->getType();
    Constant *zero = llvm::Constant::getNullValue(src->getType());
    bool isFloat = fromTy->getScalarType()->isFloatingPointTy();
    if (isFloat)
      return cast<Instruction>(Builder.CreateFCmpONE(src, zero));
    else
      return cast<Instruction>(Builder.CreateICmpNE(src, zero));
  }

  Type *eltToTy = toTy->getScalarType();
  Type *eltFromTy = src->getType()->getScalarType();

  bool fromUnsigned = castOp == HLCastOpcode::FromUnsignedCast ||
                      castOp == HLCastOpcode::UnsignedUnsignedCast;
  bool toUnsigned = castOp == HLCastOpcode::ToUnsignedCast ||
                    castOp == HLCastOpcode::UnsignedUnsignedCast;

  Instruction::CastOps castOps = static_cast<Instruction::CastOps>(
      HLModule::FindCastOp(fromUnsigned, toUnsigned, eltFromTy, eltToTy));

  return cast<Instruction>(Builder.CreateCast(castOps, src, toTy));
}

Instruction *HLMatrixLowerPass::MatCastToVec(CallInst *CI) {
  IRBuilder<> Builder(CI);
  Value *op = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  HLCastOpcode opcode = static_cast<HLCastOpcode>(GetHLOpcode(CI));

  bool ToMat = IsMatrixType(CI->getType());
  bool FromMat = IsMatrixType(op->getType());
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
    if (IsMatrixType(GEP->getResultElementType())) {
      Value *ptr = GEP->getPointerOperand();
      if (AllocaInst *AI = dyn_cast<AllocaInst>(ptr)) {
        Type *ATy = AI->getAllocatedType();
        if (ATy->isStructTy() && !IsMatrixType(ATy)) {
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
    if (IsMatrixType(GEP->getResultElementType())) {
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
    if (IsMatrixType(Ty)) {
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

  auto GetVecConst = [&](Type *Ty, int v) -> Constant * {
    Constant *val = isFloat ? ConstantFP::get(Ty->getScalarType(), v)
                            : ConstantInt::get(Ty->getScalarType(), v);
    std::vector<Constant *> vals(Ty->getVectorNumElements(), val);
    return ConstantVector::get(vals);
  };

  Constant *one = GetVecConst(ResultTy, 1);

  Instruction *Result = nullptr;
  switch (opcode) {
  case HLUnaryOpcode::Minus: {
    Constant *zero = GetVecConst(ResultTy, 0);
    if (isFloat)
      Result = BinaryOperator::CreateFSub(zero, tmp);
    else
      Result = BinaryOperator::CreateSub(zero, tmp);
  } break;
  case HLUnaryOpcode::LNot: {
    Constant *zero = GetVecConst(ResultTy, 0);
    if (isFloat)
      Result = CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_UNE, tmp, zero);
    else
      Result = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_NE, tmp, zero);
  } break;
  case HLUnaryOpcode::Not:
    Result = BinaryOperator::CreateXor(tmp, tmp);
    break;
  case HLUnaryOpcode::PostInc:
  case HLUnaryOpcode::PreInc:
    if (isFloat)
      Result = BinaryOperator::CreateFAdd(tmp, one);
    else
      Result = BinaryOperator::CreateAdd(tmp, one);
    break;
  case HLUnaryOpcode::PostDec:
  case HLUnaryOpcode::PreDec:
    if (isFloat)
      Result = BinaryOperator::CreateFSub(tmp, one);
    else
      Result = BinaryOperator::CreateSub(tmp, one);
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
    DXASSERT_LOCALVAR(op1, IsMatrixType(op1->getType()), "must be matrix type here");
    Result = BinaryOperator::CreateShl(tmp, tmp);
  } break;
  case HLBinaryOpcode::Shr: {
    Value *op1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
    DXASSERT_LOCALVAR(op1, IsMatrixType(op1->getType()), "must be matrix type here");
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
    DXASSERT_LOCALVAR(op1, IsMatrixType(op1->getType()), "must be matrix type here");
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
    Constant *zero;
    if (isFloat)
      zero = llvm::ConstantFP::get(ResultTy->getVectorElementType(), 0);
    else
      zero = llvm::ConstantInt::get(ResultTy->getVectorElementType(), 0);

    unsigned size = ResultTy->getVectorNumElements();
    std::vector<Constant *> zeros(size, zero);
    Value *vecZero = llvm::ConstantVector::get(zeros);
    Instruction *cmpL;
    if (isFloat)
      cmpL =
          CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_OEQ, tmp, vecZero);
    else
      cmpL = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_EQ, tmp, vecZero);
    Builder.Insert(cmpL);

    Instruction *cmpR;
    if (isFloat)
      cmpR =
          CmpInst::Create(Instruction::FCmp, CmpInst::FCMP_OEQ, tmp, vecZero);
    else
      cmpR = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_EQ, tmp, vecZero);
    Builder.Insert(cmpR);
    // How to map l, r back? Need check opcode
    if (opcode == HLBinaryOpcode::LOr)
      Result = BinaryOperator::CreateAnd(cmpL, cmpR);
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
  case HLUnaryOpcode::Not:
    // Not is xor now
    vecUseInst->setOperand(0, vecVal);
    vecUseInst->setOperand(1, vecVal);
    break;
  case HLUnaryOpcode::LNot:
  case HLUnaryOpcode::PostInc:
  case HLUnaryOpcode::PreInc:
  case HLUnaryOpcode::PostDec:
  case HLUnaryOpcode::PreDec:
    vecUseInst->setOperand(0, vecVal);
    break;
  case HLUnaryOpcode::Invalid:
  case HLUnaryOpcode::Plus:
  case HLUnaryOpcode::Minus:
  case HLUnaryOpcode::NumOfUO:
    // No VecInst replacements for these.
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

  bool LMat = IsMatrixType(LVal->getType());
  bool RMat = IsMatrixType(RVal->getType());
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
    bool ToMat = IsMatrixType(castInst->getType());
    bool FromMat = IsMatrixType(matVal->getType());
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
    if (valEltTy->isVectorTy() || HLMatrixLower::IsMatrixType(valEltTy) ||
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
  } else if (HLMatrixLower::IsMatrixType(valTy)) {
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
          DXASSERT_LOCALVAR(opcode, opcode == IntrinsicOp::IOP_frexp,
                   "otherwise, unexpected opcode with matrix out parameter");
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
  // Delete the matrix version insts.
  for (Instruction *deadInst : m_deadInsts) {
    // Replace with undef and remove it.
    deadInst->replaceAllUsesWith(UndefValue::get(deadInst->getType()));
    deadInst->eraseFromParent();
  }
  m_deadInsts.clear();
  m_inDeadInstsSet.clear();
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

void HLMatrixLowerPass::runOnGlobal(GlobalVariable *GV) {
  if (HLMatrixLower::IsMatrixArrayPointer(GV->getType())) {
    runOnGlobalMatrixArray(GV);
    return;
  }

  Type *Ty = GV->getType()->getPointerElementType();
  if (!HLMatrixLower::IsMatrixType(Ty))
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

void HLMatrixLowerPass::runOnFunction(Function &F) {
  // Skip hl function definition (like createhandle)
  if (hlsl::GetHLOpcodeGroupByName(&F) != HLOpcodeGroup::NotHL)
    return;

  // Create vector version of matrix instructions first.
  // The matrix operands will be undefval for these instructions.
  for (Function::iterator BBI = F.begin(), BBE = F.end(); BBI != BBE; ++BBI) {
    BasicBlock *BB = BBI;
    for (auto II = BB->begin(); II != BB->end(); ) {
      Instruction &I = *(II++);
      if (IsMatrixType(I.getType())) {
        lowerToVec(&I);
      } else if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
        Type *Ty = AI->getAllocatedType();
        if (HLMatrixLower::IsMatrixType(Ty)) {
          lowerToVec(&I);
        } else if (HLMatrixLower::IsMatrixArrayPointer(AI->getType())) {
          lowerToVec(&I);
        }
      } else if (CallInst *CI = dyn_cast<CallInst>(&I)) {
        HLOpcodeGroup group =
            hlsl::GetHLOpcodeGroupByName(CI->getCalledFunction());
        if (group == HLOpcodeGroup::HLMatLoadStore) {
          HLMatLoadStoreOpcode opcode =
              static_cast<HLMatLoadStoreOpcode>(hlsl::GetHLOpcode(CI));
          DXASSERT_LOCALVAR(opcode,
                            opcode == HLMatLoadStoreOpcode::ColMatStore ||
                            opcode == HLMatLoadStoreOpcode::RowMatStore,
                            "Must MatStore here, load will go IsMatrixType path");
          // Lower it here to make sure it is ready before replace.
          lowerToVec(&I);
        }
      } else if (GetIfMatrixGEPOfUDTAlloca(&I) ||
                 GetIfMatrixGEPOfUDTArg(&I, *m_pHLModule)) {
        lowerToVec(&I);
      }
    }
  }

  // Update the use of matrix inst with the vector version.
  for (auto matToVecIter = matToVecMap.begin();
       matToVecIter != matToVecMap.end();) {
    auto matToVec = matToVecIter++;
    replaceMatWithVec(matToVec->first, cast<Instruction>(matToVec->second));
  }

  // Translate mat inst which require all operands ready.
  for (auto matToVecIter = matToVecMap.begin();
       matToVecIter != matToVecMap.end();) {
    auto matToVec = matToVecIter++;
    if (isa<Instruction>(matToVec->first))
      finalMatTranslation(matToVec->first);
  }

  // Remove matrix targets of vecToMatMap from matToVecMap before adding the rest to dead insts.
  for (auto &it : vecToMatMap) {
    matToVecMap.erase(it.second);
  }

  // Delete the matrix version insts.
  for (auto matToVecIter = matToVecMap.begin();
       matToVecIter != matToVecMap.end();) {
    auto matToVec = matToVecIter++;
    // Add to m_deadInsts.
    if (Instruction *matInst = dyn_cast<Instruction>(matToVec->first))
      AddToDeadInsts(matInst);
  }

  DeleteDeadInsts();
  
  matToVecMap.clear();
  vecToMatMap.clear();

  return;
}

// Matrix Bitcast lower.
// After linking Lower matrix bitcast patterns like:
//  %169 = bitcast [72 x float]* %0 to [6 x %class.matrix.float.4.3]*
//  %conv.i = fptoui float %164 to i32
//  %arrayidx.i = getelementptr inbounds [6 x %class.matrix.float.4.3], [6 x %class.matrix.float.4.3]* %169, i32 0, i32 %conv.i
//  %170 = bitcast %class.matrix.float.4.3* %arrayidx.i to <12 x float>*

namespace {

Type *TryLowerMatTy(Type *Ty) {
  Type *VecTy = nullptr;
  if (HLMatrixLower::IsMatrixArrayPointer(Ty)) {
    VecTy = HLMatrixLower::LowerMatrixArrayPointerToOneDimArray(Ty);
  } else if (isa<PointerType>(Ty) &&
             HLMatrixLower::IsMatrixType(Ty->getPointerElementType())) {
    VecTy = HLMatrixLower::LowerMatrixTypeToOneDimArray(
        Ty->getPointerElementType());
    VecTy = PointerType::get(VecTy, Ty->getPointerAddressSpace());
  }
  return VecTy;
}

class MatrixBitcastLowerPass : public FunctionPass {

public:
  static char ID; // Pass identification, replacement for typeid
  explicit MatrixBitcastLowerPass() : FunctionPass(ID) {}

  const char *getPassName() const override { return "Matrix Bitcast lower"; }
  bool runOnFunction(Function &F) override {
    bool bUpdated = false;
    std::unordered_set<BitCastInst*> matCastSet;
    for (auto blkIt = F.begin(); blkIt != F.end(); ++blkIt) {
      BasicBlock *BB = blkIt;
      for (auto iIt = BB->begin(); iIt != BB->end(); ) {
        Instruction *I = (iIt++);
        if (BitCastInst *BCI = dyn_cast<BitCastInst>(I)) {
          // Mutate mat to vec.
          Type *ToTy = BCI->getType();
          if (Type *ToVecTy = TryLowerMatTy(ToTy)) {
            matCastSet.insert(BCI);
            bUpdated = true;
          }
        }
      }
    }

    DxilModule &DM = F.getParent()->GetOrCreateDxilModule();
    // Remove bitcast which has CallInst user.
    if (DM.GetShaderModel()->IsLib()) {
      for (auto it = matCastSet.begin(); it != matCastSet.end();) {
        BitCastInst *BCI = *(it++);
        if (hasCallUser(BCI)) {
          matCastSet.erase(BCI);
        }
      }
    }

    // Lower matrix first.
    for (BitCastInst *BCI : matCastSet) {
      lowerMatrix(BCI, BCI->getOperand(0));
    }
    return bUpdated;
  }
private:
  void lowerMatrix(Instruction *M, Value *A);
  bool hasCallUser(Instruction *M);
};

}

bool MatrixBitcastLowerPass::hasCallUser(Instruction *M) {
  for (auto it = M->user_begin(); it != M->user_end();) {
    User *U = *(it++);
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(U)) {
      Type *EltTy = GEP->getType()->getPointerElementType();
      if (HLMatrixLower::IsMatrixType(EltTy)) {
        if (hasCallUser(GEP))
          return true;
      } else {
        DXASSERT(0, "invalid GEP for matrix");
      }
    } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(U)) {
      if (hasCallUser(BCI))
        return true;
    } else if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
      if (VectorType *Ty = dyn_cast<VectorType>(LI->getType())) {
      } else {
        DXASSERT(0, "invalid load for matrix");
      }
    } else if (StoreInst *ST = dyn_cast<StoreInst>(U)) {
      Value *V = ST->getValueOperand();
      if (VectorType *Ty = dyn_cast<VectorType>(V->getType())) {
      } else {
        DXASSERT(0, "invalid load for matrix");
      }
    } else if (isa<CallInst>(U)) {
      return true;
    } else {
      DXASSERT(0, "invalid use of matrix");
    }
  }
  return false;
}

namespace {
Value *CreateEltGEP(Value *A, unsigned i, Value *zeroIdx,
                    IRBuilder<> &Builder) {
  Value *GEP = nullptr;
  if (GetElementPtrInst *GEPA = dyn_cast<GetElementPtrInst>(A)) {
    // A should be gep oneDimArray, 0, index * matSize
    // Here add eltIdx to index * matSize foreach elt.
    Instruction *EltGEP = GEPA->clone();
    unsigned eltIdx = EltGEP->getNumOperands() - 1;
    Value *NewIdx =
        Builder.CreateAdd(EltGEP->getOperand(eltIdx), Builder.getInt32(i));
    EltGEP->setOperand(eltIdx, NewIdx);
    Builder.Insert(EltGEP);
    GEP = EltGEP;
  } else {
    GEP = Builder.CreateInBoundsGEP(A, {zeroIdx, Builder.getInt32(i)});
  }
  return GEP;
}
} // namespace

void MatrixBitcastLowerPass::lowerMatrix(Instruction *M, Value *A) {
  for (auto it = M->user_begin(); it != M->user_end();) {
    User *U = *(it++);
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(U)) {
      Type *EltTy = GEP->getType()->getPointerElementType();
      if (HLMatrixLower::IsMatrixType(EltTy)) {
        // Change gep matrixArray, 0, index
        // into
        //   gep oneDimArray, 0, index * matSize
        IRBuilder<> Builder(GEP);
        SmallVector<Value *, 2> idxList(GEP->idx_begin(), GEP->idx_end());
        DXASSERT(idxList.size() == 2,
                 "else not one dim matrix array index to matrix");
        unsigned col = 0;
        unsigned row = 0;
        HLMatrixLower::GetMatrixInfo(EltTy, col, row);
        Value *matSize = Builder.getInt32(col * row);
        idxList.back() = Builder.CreateMul(idxList.back(), matSize);
        Value *NewGEP = Builder.CreateGEP(A, idxList);
        lowerMatrix(GEP, NewGEP);
        DXASSERT(GEP->user_empty(), "else lower matrix fail");
        GEP->eraseFromParent();
      } else {
        DXASSERT(0, "invalid GEP for matrix");
      }
    } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(U)) {
      lowerMatrix(BCI, A);
      DXASSERT(BCI->user_empty(), "else lower matrix fail");
      BCI->eraseFromParent();
    } else if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
      if (VectorType *Ty = dyn_cast<VectorType>(LI->getType())) {
        IRBuilder<> Builder(LI);
        Value *zeroIdx = Builder.getInt32(0);
        unsigned vecSize = Ty->getNumElements();
        Value *NewVec = UndefValue::get(LI->getType());
        for (unsigned i = 0; i < vecSize; i++) {
          Value *GEP = CreateEltGEP(A, i, zeroIdx, Builder);
          Value *Elt = Builder.CreateLoad(GEP);
          NewVec = Builder.CreateInsertElement(NewVec, Elt, i);
        }
        LI->replaceAllUsesWith(NewVec);
        LI->eraseFromParent();
      } else {
        DXASSERT(0, "invalid load for matrix");
      }
    } else if (StoreInst *ST = dyn_cast<StoreInst>(U)) {
      Value *V = ST->getValueOperand();
      if (VectorType *Ty = dyn_cast<VectorType>(V->getType())) {
        IRBuilder<> Builder(LI);
        Value *zeroIdx = Builder.getInt32(0);
        unsigned vecSize = Ty->getNumElements();
        for (unsigned i = 0; i < vecSize; i++) {
          Value *GEP = CreateEltGEP(A, i, zeroIdx, Builder);
          Value *Elt = Builder.CreateExtractElement(V, i);
          Builder.CreateStore(Elt, GEP);
        }
        ST->eraseFromParent();
      } else {
        DXASSERT(0, "invalid load for matrix");
      }
    } else {
      DXASSERT(0, "invalid use of matrix");
    }
  }
}

#include "dxc/HLSL/DxilGenerationPass.h"
char MatrixBitcastLowerPass::ID = 0;
FunctionPass *llvm::createMatrixBitcastLowerPass() { return new MatrixBitcastLowerPass(); }

INITIALIZE_PASS(MatrixBitcastLowerPass, "matrixbitcastlower", "Matrix Bitcast lower", false, false)
