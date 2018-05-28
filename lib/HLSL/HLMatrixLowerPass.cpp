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
#include "dxc/HLSL/DxilUtil.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilOperations.h"

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

// Translate matrix type to vector type.
Type *LowerMatrixType(Type *Ty) {
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
    return VectorType::get(EltTy, row * col);
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
Type *LowerMatrixArrayPointer(Type *Ty) {
  Ty = Ty->getPointerElementType();
  std::vector<unsigned> arraySizeList;
  while (Ty->isArrayTy()) {
    arraySizeList.push_back(Ty->getArrayNumElements());
    Ty = Ty->getArrayElementType();
  }
  Ty = LowerMatrixType(Ty);

  for (auto arraySize = arraySizeList.rbegin();
       arraySize != arraySizeList.rend(); arraySize++)
    Ty = ArrayType::get(Ty, *arraySize);
  return PointerType::get(Ty, 0);
}

Value *BuildVector(Type *EltTy, unsigned size, ArrayRef<llvm::Value *> elts,
                   IRBuilder<> &Builder) {
  Value *Vec = UndefValue::get(VectorType::get(EltTy, size));
  for (unsigned i = 0; i < size; i++)
    Vec = Builder.CreateInsertElement(Vec, elts[i], i);
  return Vec;
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
  // Replace matInst with vecInst on matUseInst.
  void TrivialMatUnOpReplace(CallInst *matInst, Instruction *vecInst,
                            CallInst *matUseInst);
  Instruction *TrivialMatBinOpToVec(CallInst *CI);
  // Replace matInst with vecInst on matUseInst.
  void TrivialMatBinOpReplace(CallInst *matInst, Instruction *vecInst,
                             CallInst *matUseInst);
  // Replace matInst with vecInst on mulInst.
  void TranslateMatMatMul(CallInst *matInst, Instruction *vecInst,
                          CallInst *mulInst, bool isSigned);
  void TranslateMatVecMul(CallInst *matInst, Instruction *vecInst,
                          CallInst *mulInst, bool isSigned);
  void TranslateVecMatMul(CallInst *matInst, Instruction *vecInst,
                          CallInst *mulInst, bool isSigned);
  void TranslateMul(CallInst *matInst, Instruction *vecInst, CallInst *mulInst,
                    bool isSigned);
  // Replace matInst with vecInst on transposeInst.
  void TranslateMatTranspose(CallInst *matInst, Instruction *vecInst,
                             CallInst *transposeInst);
  void TranslateMatDeterminant(CallInst *matInst, Instruction *vecInst,
                             CallInst *determinantInst);
  void MatIntrinsicReplace(CallInst *matInst, Instruction *vecInst,
                           CallInst *matUseInst);
  // Replace matInst with vecInst on castInst.
  void TranslateMatMatCast(CallInst *matInst, Instruction *vecInst,
                           CallInst *castInst);
  void TranslateMatToOtherCast(CallInst *matInst, Instruction *vecInst,
                               CallInst *castInst);
  void TranslateMatCast(CallInst *matInst, Instruction *vecInst,
                        CallInst *castInst);
  void TranslateMatMajorCast(CallInst *matInst, Instruction *vecInst,
                        CallInst *castInst, bool rowToCol, bool transpose);
  // Replace matInst with vecInst in matSubscript
  void TranslateMatSubscript(Value *matInst, Value *vecInst,
                             CallInst *matSubInst);
  // Replace matInst with vecInst
  void TranslateMatInit(CallInst *matInitInst);
  // Replace matInst with vecInst.
  void TranslateMatSelect(CallInst *matSelectInst);
  // Replace matInst with vecInst on matInitInst.
  void TranslateMatArrayGEP(Value *matInst, Instruction *vecInst,
                            GetElementPtrInst *matGEP);
  void TranslateMatLoadStoreOnGlobal(Value *matGlobal, ArrayRef<Value *>vecGlobals,
                             CallInst *matLdStInst);
  void TranslateMatLoadStoreOnGlobal(GlobalVariable *matGlobal, GlobalVariable *vecGlobal,
                             CallInst *matLdStInst);
  void TranslateMatSubscriptOnGlobalPtr(CallInst *matSubInst, Value *vecPtr);
  void TranslateMatLoadStoreOnGlobalPtr(CallInst *matLdStInst, Value *vecPtr);

  // Replace matInst with vecInst on matUseInst.
  void TrivialMatReplace(CallInst *matInst, Instruction *vecInst,
                        CallInst *matUseInst);
  // Lower a matrix type instruction to a vector type instruction.
  void lowerToVec(Instruction *matInst);
  // Lower users of a matrix type instruction.
  void replaceMatWithVec(Instruction *matInst, Instruction *vecInst);
  // Translate mat inst which need all operands ready.
  void finalMatTranslation(Instruction *matInst);
  // Delete dead insts in m_deadInsts.
  void DeleteDeadInsts();
  // Map from matrix inst to its vector version.
  DenseMap<Instruction *, Value *> matToVecMap;
};
}

char HLMatrixLowerPass::ID = 0;

ModulePass *llvm::createHLMatrixLowerPass() { return new HLMatrixLowerPass(); }

INITIALIZE_PASS(HLMatrixLowerPass, "hlmatrixlower", "HLSL High-Level Matrix Lower", false, false)

static Instruction *CreateTypeCast(HLCastOpcode castOp, Type *toTy, Value *src,
                                   IRBuilder<> Builder) {
  // Cast to bool.
  if (toTy->getScalarType()->isIntegerTy() &&
      toTy->getScalarType()->getIntegerBitWidth() == 1) {
    Type *fromTy = src->getType();
    bool isFloat = fromTy->getScalarType()->isFloatingPointTy();
    Constant *zero;
    if (isFloat)
      zero = llvm::ConstantFP::get(fromTy->getScalarType(), 0);
    else
      zero = llvm::ConstantInt::get(fromTy->getScalarType(), 0);

    if (toTy->getScalarType() != toTy) {
      // Create constant vector.
      unsigned size = toTy->getVectorNumElements();
      std::vector<Constant *> zeros(size, zero);
      zero = llvm::ConstantVector::get(zeros);
    }
    if (isFloat)
      return cast<Instruction>(Builder.CreateFCmpOEQ(src, zero));
    else
      return cast<Instruction>(Builder.CreateICmpEQ(src, zero));
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

Instruction *HLMatrixLowerPass::MatLdStToVec(CallInst *CI) {
  IRBuilder<> Builder(CI);
  unsigned opcode = GetHLOpcode(CI);
  HLMatLoadStoreOpcode matOpcode = static_cast<HLMatLoadStoreOpcode>(opcode);
  Instruction *result = nullptr;
  switch (matOpcode) {
  case HLMatLoadStoreOpcode::ColMatLoad:
  case HLMatLoadStoreOpcode::RowMatLoad: {
    Value *matPtr = CI->getArgOperand(HLOperandIndex::kMatLoadPtrOpIdx);
    if (isa<AllocaInst>(matPtr)) {
      Value *vecPtr = matToVecMap[cast<Instruction>(matPtr)];
      result = Builder.CreateLoad(vecPtr);
    } else
      result = MatIntrinsicToVec(CI);
  } break;
  case HLMatLoadStoreOpcode::ColMatStore:
  case HLMatLoadStoreOpcode::RowMatStore: {
    Value *matPtr = CI->getArgOperand(HLOperandIndex::kMatStoreDstPtrOpIdx);
    if (isa<AllocaInst>(matPtr)) {
      Value *vecPtr = matToVecMap[cast<Instruction>(matPtr)];
      Value *matVal = CI->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);
      Value *vecVal =
          UndefValue::get(HLMatrixLower::LowerMatrixType(matVal->getType()));
      result = Builder.CreateStore(vecVal, vecPtr);
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

void HLMatrixLowerPass::lowerToVec(Instruction *matInst) {
  Instruction *vecInst;

  if (CallInst *CI = dyn_cast<CallInst>(matInst)) {
    hlsl::HLOpcodeGroup group =
        hlsl::GetHLOpcodeGroupByName(CI->getCalledFunction());
    switch (group) {
    case HLOpcodeGroup::HLIntrinsic: {
      vecInst = MatIntrinsicToVec(CI);
    } break;
    case HLOpcodeGroup::HLSelect: {
      vecInst = MatIntrinsicToVec(CI);
    } break;
    case HLOpcodeGroup::HLBinOp: {
      vecInst = TrivialMatBinOpToVec(CI);
    } break;
    case HLOpcodeGroup::HLUnOp: {
      vecInst = TrivialMatUnOpToVec(CI);
    } break;
    case HLOpcodeGroup::HLCast: {
      vecInst = MatCastToVec(CI);
    } break;
    case HLOpcodeGroup::HLInit: {
      vecInst = MatIntrinsicToVec(CI);
    } break;
    case HLOpcodeGroup::HLMatLoadStore: {
      vecInst = MatLdStToVec(CI);
    } break;
    case HLOpcodeGroup::HLSubscript: {
      vecInst = MatSubscriptToVec(CI);
    } break;
    }
  } else if (AllocaInst *AI = dyn_cast<AllocaInst>(matInst)) {
    Type *Ty = AI->getAllocatedType();
    Type *matTy = Ty;
    
    IRBuilder<> Builder(AI);
    if (Ty->isArrayTy()) {
      Type *vecTy = HLMatrixLower::LowerMatrixArrayPointer(AI->getType());
      vecTy = vecTy->getPointerElementType();
      vecInst = Builder.CreateAlloca(vecTy, nullptr, AI->getName());
    } else {
      Type *vecTy = HLMatrixLower::LowerMatrixType(matTy);
      vecInst = Builder.CreateAlloca(vecTy, nullptr, AI->getName());
    }
    // Update debug info.
    DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(AI);
    if (DDI) {
      LLVMContext &Context = AI->getContext();
      Value *DDIVar = MetadataAsValue::get(Context, DDI->getRawVariable());
      Value *DDIExp = MetadataAsValue::get(Context, DDI->getRawExpression());
      Value *VMD = MetadataAsValue::get(Context, ValueAsMetadata::get(vecInst));
      IRBuilder<> debugBuilder(DDI);
      debugBuilder.CreateCall(DDI->getCalledFunction(), {VMD, DDIVar, DDIExp});
    }

    if (HLModule::HasPreciseAttributeWithMetadata(AI))
      HLModule::MarkPreciseAttributeWithMetadata(vecInst);

  } else {
    DXASSERT(0, "invalid inst");
  }
  matToVecMap[matInst] = vecInst;
}

// Replace matInst with vecInst on matUseInst.
void HLMatrixLowerPass::TrivialMatUnOpReplace(CallInst *matInst,
                                             Instruction *vecInst,
                                             CallInst *matUseInst) {
  HLUnaryOpcode opcode = static_cast<HLUnaryOpcode>(GetHLOpcode(matUseInst));
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[matUseInst]);
  switch (opcode) {
  case HLUnaryOpcode::Not:
    // Not is xor now
    vecUseInst->setOperand(0, vecInst);
    vecUseInst->setOperand(1, vecInst);
    break;
  case HLUnaryOpcode::LNot:
  case HLUnaryOpcode::PostInc:
  case HLUnaryOpcode::PreInc:
  case HLUnaryOpcode::PostDec:
  case HLUnaryOpcode::PreDec:
    vecUseInst->setOperand(0, vecInst);
    break;
  }
}

// Replace matInst with vecInst on matUseInst.
void HLMatrixLowerPass::TrivialMatBinOpReplace(CallInst *matInst,
                                              Instruction *vecInst,
                                              CallInst *matUseInst) {
  HLBinaryOpcode opcode = static_cast<HLBinaryOpcode>(GetHLOpcode(matUseInst));
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[matUseInst]);

  if (opcode != HLBinaryOpcode::LAnd && opcode != HLBinaryOpcode::LOr) {
    if (matUseInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx) == matInst)
      vecUseInst->setOperand(0, vecInst);
    if (matUseInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx) == matInst)
      vecUseInst->setOperand(1, vecInst);
  } else {
    if (matUseInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx) ==
        matInst) {
      Instruction *vecCmp = cast<Instruction>(vecUseInst->getOperand(0));
      vecCmp->setOperand(0, vecInst);
    }
    if (matUseInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx) ==
        matInst) {
      Instruction *vecCmp = cast<Instruction>(vecUseInst->getOperand(1));
      vecCmp->setOperand(0, vecInst);
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

void HLMatrixLowerPass::TranslateMatMatMul(CallInst *matInst,
                                           Instruction *vecInst,
                                           CallInst *mulInst, bool isSigned) {
  DXASSERT(matToVecMap.count(mulInst), "must has vec version");
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
  IRBuilder<> Builder(mulInst);

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

void HLMatrixLowerPass::TranslateMatVecMul(CallInst *matInst,
                                           Instruction *vecInst,
                                           CallInst *mulInst, bool isSigned) {
  // matInst should == mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *RVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);

  unsigned col, row;
  Type *EltTy = GetMatrixInfo(matInst->getType(), col, row);
  DXASSERT(RVal->getType()->getVectorNumElements() == col, "");

  bool isFloat = EltTy->isFloatingPointTy();

  Value *retVal = llvm::UndefValue::get(mulInst->getType());
  IRBuilder<> Builder(mulInst);

  Value *vec = RVal;
  Value *mat = vecInst; // vec version of matInst;

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

void HLMatrixLowerPass::TranslateVecMatMul(CallInst *matInst,
                                           Instruction *vecInst,
                                           CallInst *mulInst, bool isSigned) {
  Value *LVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  // matInst should == mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  Value *RVal = vecInst;

  unsigned col, row;
  Type *EltTy = GetMatrixInfo(matInst->getType(), col, row);
  DXASSERT(LVal->getType()->getVectorNumElements() == row, "");

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

void HLMatrixLowerPass::TranslateMul(CallInst *matInst, Instruction *vecInst,
                                     CallInst *mulInst, bool isSigned) {
  Value *LVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *RVal = mulInst->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);

  bool LMat = IsMatrixType(LVal->getType());
  bool RMat = IsMatrixType(RVal->getType());
  if (LMat && RMat) {
    TranslateMatMatMul(matInst, vecInst, mulInst, isSigned);
  } else if (LMat) {
    TranslateMatVecMul(matInst, vecInst, mulInst, isSigned);
  } else {
    TranslateVecMatMul(matInst, vecInst, mulInst, isSigned);
  }
}

void HLMatrixLowerPass::TranslateMatTranspose(CallInst *matInst,
                                              Instruction *vecInst,
                                              CallInst *transposeInst) {
  // Matrix value is row major, transpose is cast it to col major.
  TranslateMatMajorCast(matInst, vecInst, transposeInst,
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


void HLMatrixLowerPass::TranslateMatDeterminant(CallInst *matInst, Instruction *vecInst,
    CallInst *determinantInst) {
  unsigned row, col;
  GetMatrixInfo(matInst->getType(), col, row);
  IRBuilder<> Builder(determinantInst);
  // when row == 1, result is vecInst.
  Value *Result = vecInst;
  if (row == 2) {
    Value *m00 = Builder.CreateExtractElement(vecInst, (uint64_t)0);
    Value *m01 = Builder.CreateExtractElement(vecInst, 1);
    Value *m10 = Builder.CreateExtractElement(vecInst, 2);
    Value *m11 = Builder.CreateExtractElement(vecInst, 3);
    Result = Determinant2x2(m00, m01, m10, m11, Builder);
  }
  else if (row == 3) {
    Value *m00 = Builder.CreateExtractElement(vecInst, (uint64_t)0);
    Value *m01 = Builder.CreateExtractElement(vecInst, 1);
    Value *m02 = Builder.CreateExtractElement(vecInst, 2);
    Value *m10 = Builder.CreateExtractElement(vecInst, 3);
    Value *m11 = Builder.CreateExtractElement(vecInst, 4);
    Value *m12 = Builder.CreateExtractElement(vecInst, 5);
    Value *m20 = Builder.CreateExtractElement(vecInst, 6);
    Value *m21 = Builder.CreateExtractElement(vecInst, 7);
    Value *m22 = Builder.CreateExtractElement(vecInst, 8);
    Result = Determinant3x3(m00, m01, m02, 
                            m10, m11, m12, 
                            m20, m21, m22, Builder);
  }
  else if (row == 4) {
    Value *m00 = Builder.CreateExtractElement(vecInst, (uint64_t)0);
    Value *m01 = Builder.CreateExtractElement(vecInst, 1);
    Value *m02 = Builder.CreateExtractElement(vecInst, 2);
    Value *m03 = Builder.CreateExtractElement(vecInst, 3);

    Value *m10 = Builder.CreateExtractElement(vecInst, 4);
    Value *m11 = Builder.CreateExtractElement(vecInst, 5);
    Value *m12 = Builder.CreateExtractElement(vecInst, 6);
    Value *m13 = Builder.CreateExtractElement(vecInst, 7);

    Value *m20 = Builder.CreateExtractElement(vecInst, 8);
    Value *m21 = Builder.CreateExtractElement(vecInst, 9);
    Value *m22 = Builder.CreateExtractElement(vecInst, 10);
    Value *m23 = Builder.CreateExtractElement(vecInst, 11);

    Value *m30 = Builder.CreateExtractElement(vecInst, 12);
    Value *m31 = Builder.CreateExtractElement(vecInst, 13);
    Value *m32 = Builder.CreateExtractElement(vecInst, 14);
    Value *m33 = Builder.CreateExtractElement(vecInst, 15);

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

void HLMatrixLowerPass::TrivialMatReplace(CallInst *matInst,
                                         Instruction *vecInst,
                                         CallInst *matUseInst) {
  CallInst *vecUseInst = cast<CallInst>(matToVecMap[matUseInst]);

  for (unsigned i = 0; i < matUseInst->getNumArgOperands(); i++)
    if (matUseInst->getArgOperand(i) == matInst) {
      vecUseInst->setArgOperand(i, vecInst);
    }
}

void HLMatrixLowerPass::TranslateMatMajorCast(CallInst *matInst,
                                              Instruction *vecInst,
                                              CallInst *castInst,
                                              bool bRowToCol,
                                              bool bTranspose) {
  unsigned col, row;
  if (!bTranspose) {
    GetMatrixInfo(castInst->getType(), col, row);
    DXASSERT(castInst->getType() == matInst->getType(), "type must match");
  } else {
    unsigned castCol, castRow;
    Type *castTy = GetMatrixInfo(castInst->getType(), castCol, castRow);
    unsigned srcCol, srcRow;
    Type *srcTy = GetMatrixInfo(matInst->getType(), srcCol, srcRow);
    DXASSERT_LOCALVAR((castTy, srcTy), srcTy == castTy, "type must match");
    DXASSERT(castCol == srcRow && castRow == srcCol, "col row must match");
    col = srcCol;
    row = srcRow;
  }

  IRBuilder<> Builder(castInst);

  // shuf to change major.
  SmallVector<int, 16> castMask(col * row);
  unsigned idx = 0;
  if (bRowToCol) {
    for (unsigned c = 0; c < col; c++)
      for (unsigned r = 0; r < row; r++) {
        unsigned matIdx = HLMatrixLower::GetRowMajorIdx(r, c, col);
        castMask[idx++] = matIdx;
      }
  } else {
    for (unsigned r = 0; r < row; r++)
      for (unsigned c = 0; c < col; c++) {
        unsigned matIdx = HLMatrixLower::GetColMajorIdx(r, c, row);
        castMask[idx++] = matIdx;
      }
  }

  Instruction *vecCast = cast<Instruction>(
      Builder.CreateShuffleVector(vecInst, vecInst, castMask));

  // Replace vec cast function call with vecCast.
  DXASSERT(matToVecMap.count(castInst), "must has vec version");
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[castInst]);
  vecUseInst->replaceAllUsesWith(vecCast);
  AddToDeadInsts(vecUseInst);
  matToVecMap[castInst] = vecCast;
}

void HLMatrixLowerPass::TranslateMatMatCast(CallInst *matInst,
                                            Instruction *vecInst,
                                            CallInst *castInst) {
  unsigned toCol, toRow;
  Type *ToEltTy = GetMatrixInfo(castInst->getType(), toCol, toRow);
  unsigned fromCol, fromRow;
  Type *FromEltTy = GetMatrixInfo(matInst->getType(), fromCol, fromRow);
  unsigned fromSize = fromCol * fromRow;
  unsigned toSize = toCol * toRow;
  DXASSERT(fromSize >= toSize, "cannot extend matrix");

  IRBuilder<> Builder(castInst);
  Instruction *vecCast = nullptr;

  HLCastOpcode opcode = static_cast<HLCastOpcode>(GetHLOpcode(castInst));

  if (fromSize == toSize) {
    vecCast = CreateTypeCast(opcode, VectorType::get(ToEltTy, toSize), vecInst,
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
        Builder.CreateShuffleVector(vecInst, vecInst, castMask));

    if (ToEltTy != FromEltTy)
      vecCast = CreateTypeCast(opcode, VectorType::get(ToEltTy, toSize), shuf,
                               Builder);
    else
      vecCast = shuf;
  }
  // Replace vec cast function call with vecCast.
  DXASSERT(matToVecMap.count(castInst), "must has vec version");
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[castInst]);
  vecUseInst->replaceAllUsesWith(vecCast);
  AddToDeadInsts(vecUseInst);
  matToVecMap[castInst] = vecCast;
}

void HLMatrixLowerPass::TranslateMatToOtherCast(CallInst *matInst,
                                                Instruction *vecInst,
                                                CallInst *castInst) {
  unsigned col, row;
  Type *EltTy = GetMatrixInfo(matInst->getType(), col, row);
  unsigned fromSize = col * row;

  IRBuilder<> Builder(castInst);
  Instruction *sizeCast = nullptr;

  HLCastOpcode opcode = static_cast<HLCastOpcode>(GetHLOpcode(castInst));

  Type *ToTy = castInst->getType();
  if (ToTy->isVectorTy()) {
    unsigned toSize = ToTy->getVectorNumElements();
    if (fromSize != toSize) {
      std::vector<int> castMask(fromSize);
      for (unsigned c = 0; c < toSize; c++)
        castMask[c] = c;

      sizeCast = cast<Instruction>(
          Builder.CreateShuffleVector(vecInst, vecInst, castMask));
    } else
      sizeCast = vecInst;
  } else {
    DXASSERT(ToTy->isSingleValueType(), "must scalar here");
    sizeCast =
        cast<Instruction>(Builder.CreateExtractElement(vecInst, (uint64_t)0));
  }

  Instruction *typeCast = sizeCast;
  if (EltTy != ToTy->getScalarType()) {
    typeCast = CreateTypeCast(opcode, ToTy, typeCast, Builder);
  }
  // Replace cast function call with typeCast.
  castInst->replaceAllUsesWith(typeCast);
  AddToDeadInsts(castInst);
}

void HLMatrixLowerPass::TranslateMatCast(CallInst *matInst,
                                         Instruction *vecInst,
                                         CallInst *castInst) {
  HLCastOpcode opcode = static_cast<HLCastOpcode>(GetHLOpcode(castInst));
  if (opcode == HLCastOpcode::ColMatrixToRowMatrix ||
      opcode == HLCastOpcode::RowMatrixToColMatrix) {
    TranslateMatMajorCast(matInst, vecInst, castInst,
                          opcode == HLCastOpcode::RowMatrixToColMatrix,
                          /*bTranspose*/false);
  } else {
    bool ToMat = IsMatrixType(castInst->getType());
    bool FromMat = IsMatrixType(matInst->getType());
    if (ToMat && FromMat) {
      TranslateMatMatCast(matInst, vecInst, castInst);
    } else if (FromMat)
      TranslateMatToOtherCast(matInst, vecInst, castInst);
    else
      DXASSERT(0, "Not translate as user of matInst");
  }
}

void HLMatrixLowerPass::MatIntrinsicReplace(CallInst *matInst,
                                            Instruction *vecInst,
                                            CallInst *matUseInst) {
  IRBuilder<> Builder(matUseInst);
  IntrinsicOp opcode = static_cast<IntrinsicOp>(GetHLOpcode(matUseInst));
  switch (opcode) {
  case IntrinsicOp::IOP_umul:
    TranslateMul(matInst, vecInst, matUseInst, /*isSigned*/false);
    break;
  case IntrinsicOp::IOP_mul:
    TranslateMul(matInst, vecInst, matUseInst, /*isSigned*/true);
    break;
  case IntrinsicOp::IOP_transpose:
    TranslateMatTranspose(matInst, vecInst, matUseInst);
    break;
  case IntrinsicOp::IOP_determinant:
    TranslateMatDeterminant(matInst, vecInst, matUseInst);
    break;
  default:
    CallInst *vecUseInst = nullptr;
    if (matToVecMap.count(matUseInst))
      vecUseInst = cast<CallInst>(matToVecMap[matUseInst]);
    for (unsigned i = 0; i < matInst->getNumArgOperands(); i++)
      if (matUseInst->getArgOperand(i) == matInst) {
        if (vecUseInst)
          vecUseInst->setArgOperand(i, vecInst);
        else
          matUseInst->setArgOperand(i, vecInst);
      }
    break;
  }
}

void HLMatrixLowerPass::TranslateMatSubscript(Value *matInst, Value *vecInst,
                                              CallInst *matSubInst) {
  unsigned opcode = GetHLOpcode(matSubInst);
  HLSubscriptOpcode matOpcode = static_cast<HLSubscriptOpcode>(opcode);
  assert(matOpcode != HLSubscriptOpcode::DefaultSubscript &&
         "matrix don't use default subscript");

  Type *matType = matInst->getType()->getPointerElementType();
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
      Value *vecLd = Builder.CreateLoad(vecInst);
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
          Builder.CreateStore(vecLd, vecInst);
        } else {
          vecLd = Builder.CreateInsertElement(vecLd, val, shufMask[0]);
          Builder.CreateStore(vecLd, vecInst);
        }
      } else
        DXASSERT(0, "matrix element should only used by load/store.");
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
      Value *vecLd = Builder.CreateLoad(vecInst);
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
        Builder.CreateStore(vecLd, vecInst);
      } else if (GetElementPtrInst *GEP =
                     dyn_cast<GetElementPtrInst>(CallUser)) {
        Value *GEPOffset = HLMatrixLower::LowerGEPOnMatIndexListToIndex(GEP, idxList);
        Value *NewGEP = Builder.CreateGEP(vecInst, {zero, GEPOffset});
        GEP->replaceAllUsesWith(NewGEP);
      } else
        DXASSERT(0, "matrix subscript should only used by load/store.");
      AddToDeadInsts(CallUser);
    }
  }
  // Check vec version.
  DXASSERT(matToVecMap.count(matSubInst) == 0, "should not have vec version");
  // All the user should has been removed.
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
  if (idxList.size() == 1) {
    Value *Ptr =
        subBuilder.CreateInBoundsGEP(vecPtr, {zeroIdx, idxList[0]});
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
                            DenseMap<Instruction *, Value *> &matToVecMap,
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

  IRBuilder<> Builder(matInitInst);
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
  DXASSERT(matToVecMap.count(matInitInst), "must has vec version");
  Instruction *vecUseInst = cast<Instruction>(matToVecMap[matInitInst]);
  vecUseInst->replaceAllUsesWith(newInit);
  AddToDeadInsts(vecUseInst);
  matToVecMap[matInitInst] = newInit;
}

void HLMatrixLowerPass::TranslateMatSelect(CallInst *matSelectInst) {
  IRBuilder<> Builder(matSelectInst);
  unsigned col, row;
  Type *EltTy = GetMatrixInfo(matSelectInst->getType(), col, row);

  Type *vecTy = VectorType::get(EltTy, col * row);
  unsigned vecSize = vecTy->getVectorNumElements();

  CallInst *vecUseInst = cast<CallInst>(matToVecMap[matSelectInst]);
  Instruction *LHS = cast<Instruction>(matSelectInst->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx));
  Instruction *RHS = cast<Instruction>(matSelectInst->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx));

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
                                             Instruction *vecInst,
                                             GetElementPtrInst *matGEP) {
  SmallVector<Value *, 4> idxList(matGEP->idx_begin(), matGEP->idx_end());

  IRBuilder<> GEPBuilder(matGEP);
  Value *newGEP = GEPBuilder.CreateInBoundsGEP(vecInst, idxList);
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
          Value *newLd = Builder.CreateLoad(newGEP);
          DXASSERT(matToVecMap.count(useCall), "must has vec version");
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

          DXASSERT(matToVecMap.count(matInst), "must has vec version");
          Value *vecVal = matToVecMap[matInst];
          Builder.CreateStore(vecVal, vecPtr);
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
    } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(useInst)) {
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

void HLMatrixLowerPass::replaceMatWithVec(Instruction *matInst,
                                          Instruction *vecInst) {
  for (Value::user_iterator user = matInst->user_begin();
       user != matInst->user_end();) {
    Instruction *useInst = cast<Instruction>(*(user++));
    // Skip return here.
    if (isa<ReturnInst>(useInst))
      continue;
    // User must be function call.
    if (CallInst *useCall = dyn_cast<CallInst>(useInst)) {
      hlsl::HLOpcodeGroup group =
          hlsl::GetHLOpcodeGroupByName(useCall->getCalledFunction());
      switch (group) {
      case HLOpcodeGroup::HLIntrinsic: {
        if (CallInst *matCI = dyn_cast<CallInst>(matInst)) {
          MatIntrinsicReplace(cast<CallInst>(matInst), vecInst, useCall);
        } else {
          IntrinsicOp opcode = static_cast<IntrinsicOp>(GetHLOpcode(useCall));
          DXASSERT_LOCALVAR(opcode, opcode == IntrinsicOp::IOP_frexp,
                   "otherwise, unexpected opcode with matrix out parameter");
          // NOTE: because out param use copy out semantic, so the operand of
          // out must be temp alloca.
          DXASSERT(isa<AllocaInst>(matInst), "else invalid mat ptr for frexp");
          auto it = matToVecMap.find(useCall);
          DXASSERT(it != matToVecMap.end(),
                   "else fail to create vec version of useCall");
          CallInst *vecUseInst = cast<CallInst>(it->second);

          for (unsigned i = 0; i < vecUseInst->getNumArgOperands(); i++) {
            if (useCall->getArgOperand(i) == matInst) {
              vecUseInst->setArgOperand(i, vecInst);
            }
          }
        }
      } break;
      case HLOpcodeGroup::HLSelect: {
        MatIntrinsicReplace(cast<CallInst>(matInst), vecInst, useCall);
      } break;
      case HLOpcodeGroup::HLBinOp: {
        TrivialMatBinOpReplace(cast<CallInst>(matInst), vecInst, useCall);
      } break;
      case HLOpcodeGroup::HLUnOp: {
        TrivialMatUnOpReplace(cast<CallInst>(matInst), vecInst, useCall);
      } break;
      case HLOpcodeGroup::HLCast: {
        TranslateMatCast(cast<CallInst>(matInst), vecInst, useCall);
      } break;
      case HLOpcodeGroup::HLMatLoadStore: {
        DXASSERT(matToVecMap.count(useCall), "must has vec version");
        Value *vecUser = matToVecMap[useCall];
        if (AllocaInst *AI = dyn_cast<AllocaInst>(matInst)) {
          // Load Already translated in lowerToVec.
          // Store val operand will be set by the val use.
          // Do nothing here.
        } else if (StoreInst *stInst = dyn_cast<StoreInst>(vecUser))
          stInst->setOperand(0, vecInst);
        else
          TrivialMatReplace(cast<CallInst>(matInst), vecInst, useCall);

      } break;
      case HLOpcodeGroup::HLSubscript: {
        if (AllocaInst *AI = dyn_cast<AllocaInst>(matInst))
          TranslateMatSubscript(AI, vecInst, useCall);
        else
          TrivialMatReplace(cast<CallInst>(matInst), vecInst, useCall);

      } break;
      case HLOpcodeGroup::HLInit: {
        DXASSERT(!isa<AllocaInst>(matInst), "array of matrix init should lowered in StoreInitListToDestPtr at CGHLSLMS.cpp");
        TranslateMatInit(useCall);
      } break;
      }
    } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(useInst)) {
      // Just replace the src with vec version.
      useInst->setOperand(0, vecInst);
    } else {
      // Must be GEP on mat array alloca.
      GetElementPtrInst *GEP = cast<GetElementPtrInst>(useInst);
      AllocaInst *AI = cast<AllocaInst>(matInst);
      TranslateMatArrayGEP(AI, vecInst, GEP);
    }
  }
}

void HLMatrixLowerPass::finalMatTranslation(Instruction *matInst) {
  // Translate matInit.
  if (CallInst *CI = dyn_cast<CallInst>(matInst)) {
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
  Constant *OldInitVal = GV->getInitializer();
  Constant *InitVal =
      isa<UndefValue>(OldInitVal)
          ? UndefValue::get(VecArrayTy)
          : LowerMatrixArrayConst(OldInitVal, cast<ArrayType>(VecArrayTy));

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

  DenseMap<Instruction *, Value *> matToVecMap;
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
  // Create vector version of matrix instructions first.
  // The matrix operands will be undefval for these instructions.
  for (Function::iterator BBI = F.begin(), BBE = F.end(); BBI != BBE; ++BBI) {
    BasicBlock *BB = BBI;
    for (Instruction &I : BB->getInstList()) {
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
    finalMatTranslation(matToVec->first);
  }

  // Delete the matrix version insts.
  for (auto matToVecIter = matToVecMap.begin();
       matToVecIter != matToVecMap.end();) {
    auto matToVec = matToVecIter++;
    // Add to m_deadInsts.
    Instruction *matInst = matToVec->first;
    AddToDeadInsts(matInst);
  }

  DeleteDeadInsts();
  
  matToVecMap.clear();
}
