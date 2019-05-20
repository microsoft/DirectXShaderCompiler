///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilTranslateRawBuffer.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/Support/Global.h"
#include "llvm/Pass.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include <vector>

using namespace llvm;
using namespace hlsl;

// Translate RawBufferLoad/RawBufferStore
// This pass is to make sure that we generate correct buffer load for DXIL
// For DXIL < 1.2, rawBufferLoad will be translated to BufferLoad instruction
// without mask.
// For DXIL >= 1.2, if min precision is enabled, currently generation pass is
// producing i16/f16 return type for min precisions. For rawBuffer, we will
// change this so that min precisions are returning its actual scalar type (i32/f32)
// and will be truncated to their corresponding types after loading / before storing.
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

class DxilTranslateRawBuffer : public ModulePass {
public:
  static char ID;
  explicit DxilTranslateRawBuffer() : ModulePass(ID) {}
  bool runOnModule(Module &M) {
    unsigned major, minor;
    DxilModule &DM = M.GetDxilModule();
    DM.GetDxilVersion(major, minor);
    OP *hlslOP = DM.GetOP();
    // Split 64bit for shader model less than 6.3.
    if (major == 1 && minor <= 2) {
      for (auto F = M.functions().begin(); F != M.functions().end();) {
        Function *func = &*(F++);
        DXIL::OpCodeClass opClass;
        if (hlslOP->GetOpCodeClass(func, opClass)) {
          if (opClass == DXIL::OpCodeClass::RawBufferLoad) {
            Type *ETy =
                hlslOP->GetOverloadType(DXIL::OpCode::RawBufferLoad, func);

            bool is64 =
                ETy->isDoubleTy() || ETy == Type::getInt64Ty(ETy->getContext());
            if (is64) {
              ReplaceRawBufferLoad64Bit(func, ETy, M);
              func->eraseFromParent();
            }
          } else if (opClass == DXIL::OpCodeClass::RawBufferStore) {
            Type *ETy =
                hlslOP->GetOverloadType(DXIL::OpCode::RawBufferStore, func);

            bool is64 =
                ETy->isDoubleTy() || ETy == Type::getInt64Ty(ETy->getContext());
            if (is64) {
              ReplaceRawBufferStore64Bit(func, ETy, M);
              func->eraseFromParent();
            }
          }
        }
      }
    }
    if (major == 1 && minor < 2) {
      for (auto F = M.functions().begin(), E = M.functions().end(); F != E;) {
        Function *func = &*(F++);
        if (func->hasName()) {
          if (func->getName().startswith("dx.op.rawBufferLoad")) {
            ReplaceRawBufferLoad(func, M);
            func->eraseFromParent();
          } else if (func->getName().startswith("dx.op.rawBufferStore")) {
            ReplaceRawBufferStore(func, M);
            func->eraseFromParent();
          }
        }
      }
    } else if (M.GetDxilModule().GetUseMinPrecision()) {
      for (auto F = M.functions().begin(), E = M.functions().end(); F != E;) {
        Function *func = &*(F++);
        if (func->hasName()) {
          if (func->getName().startswith("dx.op.rawBufferLoad")) {
            ReplaceMinPrecisionRawBufferLoad(func, M);
          } else if (func->getName().startswith("dx.op.rawBufferStore")) {
            ReplaceMinPrecisionRawBufferStore(func, M);
          }
        }
      }
    }
    return true;
  }

private:
  // Replace RawBufferLoad/Store to BufferLoad/Store for DXIL < 1.2
  void ReplaceRawBufferLoad(Function *F, Module &M);
  void ReplaceRawBufferStore(Function *F, Module &M);
  void ReplaceRawBufferLoad64Bit(Function *F, Type *EltTy, Module &M);
  void ReplaceRawBufferStore64Bit(Function *F, Type *EltTy, Module &M);
  // Replace RawBufferLoad/Store of min-precision types to have its actual storage size
  void ReplaceMinPrecisionRawBufferLoad(Function *F, Module &M);
  void ReplaceMinPrecisionRawBufferStore(Function *F, Module &M);
  void ReplaceMinPrecisionRawBufferLoadByType(Function *F, Type *FromTy,
                                              Type *ToTy, OP *Op,
                                              const DataLayout &DL);
};
} // namespace

void DxilTranslateRawBuffer::ReplaceRawBufferLoad(Function *F,
                                                                Module &M) {
  OP *op = M.GetDxilModule().GetOP();
  Type *RTy = F->getReturnType();
  if (StructType *STy = dyn_cast<StructType>(RTy)) {
    Type *ETy = STy->getElementType(0);
    Function *newFunction = op->GetOpFunc(hlsl::DXIL::OpCode::BufferLoad, ETy);
    for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
      User *user = *(U++);
      if (CallInst *CI = dyn_cast<CallInst>(user)) {
        IRBuilder<> Builder(CI);
        SmallVector<Value *, 4> args;
        args.emplace_back(op->GetI32Const((unsigned)DXIL::OpCode::BufferLoad));
        for (unsigned i = 1; i < 4; ++i) {
          args.emplace_back(CI->getArgOperand(i));
        }
        CallInst *newCall = Builder.CreateCall(newFunction, args);
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

void DxilTranslateRawBuffer::ReplaceRawBufferLoad64Bit(Function *F, Type *EltTy, Module &M) {
  OP *hlslOP = M.GetDxilModule().GetOP();
  Function *bufLd = hlslOP->GetOpFunc(DXIL::OpCode::RawBufferLoad,
                                      Type::getInt32Ty(M.getContext()));
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

void DxilTranslateRawBuffer::ReplaceRawBufferStore(Function *F,
  Module &M) {
  OP *op = M.GetDxilModule().GetOP();
  DXASSERT(F->getReturnType()->isVoidTy(), "rawBufferStore should return a void type.");
  Type *ETy = F->getFunctionType()->getParamType(4); // value
  Function *newFunction = op->GetOpFunc(hlsl::DXIL::OpCode::BufferStore, ETy);
  for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
    User *user = *(U++);
    if (CallInst *CI = dyn_cast<CallInst>(user)) {
      IRBuilder<> Builder(CI);
      SmallVector<Value *, 4> args;
      args.emplace_back(op->GetI32Const((unsigned)DXIL::OpCode::BufferStore));
      for (unsigned i = 1; i < 9; ++i) {
        args.emplace_back(CI->getArgOperand(i));
      }
      Builder.CreateCall(newFunction, args);
      CI->eraseFromParent();
    }
    else {
      DXASSERT(false, "function can only be used with call instructions.");
    }
  }
}

void DxilTranslateRawBuffer::ReplaceRawBufferStore64Bit(Function *F, Type *ETy,
                                                        Module &M) {
  OP *hlslOP = M.GetDxilModule().GetOP();
  Function *newFunction = hlslOP->GetOpFunc(hlsl::DXIL::OpCode::RawBufferStore,
                                            Type::getInt32Ty(M.getContext()));
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

void DxilTranslateRawBuffer::ReplaceMinPrecisionRawBufferLoad(Function *F,
                                                              Module &M) {
  OP *Op = M.GetDxilModule().GetOP();
  Type *RetTy = F->getReturnType();
  if (StructType *STy = dyn_cast<StructType>(RetTy)) {
    Type *EltTy = STy->getElementType(0);
    if (EltTy->isHalfTy()) {
      ReplaceMinPrecisionRawBufferLoadByType(F, Type::getHalfTy(M.getContext()),
                                             Type::getFloatTy(M.getContext()),
                                             Op, M.getDataLayout());
    } else if (EltTy == Type::getInt16Ty(M.getContext())) {
      ReplaceMinPrecisionRawBufferLoadByType(
          F, Type::getInt16Ty(M.getContext()), Type::getInt32Ty(M.getContext()),
          Op, M.getDataLayout());
    }
  } else {
    DXASSERT(false, "RawBufferLoad should return struct type.");
  }
}

void DxilTranslateRawBuffer::ReplaceMinPrecisionRawBufferStore(Function *F,
                                                              Module &M) {
  DXASSERT(F->getReturnType()->isVoidTy(), "rawBufferStore should return a void type.");
  Type *ETy = F->getFunctionType()->getParamType(4); // value
  Type *NewETy;
  if (ETy->isHalfTy()) {
    NewETy = Type::getFloatTy(M.getContext());
  }
  else if (ETy == Type::getInt16Ty(M.getContext())) {
    NewETy = Type::getInt32Ty(M.getContext());
  }
  else {
    return; // not a min precision type
  }
  Function *newFunction = M.GetDxilModule().GetOP()->GetOpFunc(
      DXIL::OpCode::RawBufferStore, NewETy);
  // for each function
  // add argument 4-7 to its upconverted values
  // replace function call
  for (auto FuncUser = F->user_begin(), FuncEnd = F->user_end(); FuncUser != FuncEnd;) {
    CallInst *CI = dyn_cast<CallInst>(*(FuncUser++));
    DXASSERT(CI, "function user must be a call instruction.");
    IRBuilder<> CIBuilder(CI);
    SmallVector<Value *, 9> Args;
    for (unsigned i = 0; i < 4; ++i) {
      Args.emplace_back(CI->getArgOperand(i));
    }
    // values to store should be converted to its higher precision types
    if (ETy->isHalfTy()) {
      for (unsigned i = 4; i < 8; ++i) {
        Value *NewV = CIBuilder.CreateFPExt(CI->getArgOperand(i),
                                            Type::getFloatTy(M.getContext()));
        Args.emplace_back(NewV);
      }
    }
    else if (ETy == Type::getInt16Ty(M.getContext())) {
      // This case only applies to typed buffer since Store operation of byte
      // address buffer for min precision is handled by implicit conversion on
      // intrinsic call. Since we are extending integer, we have to know if we
      // should sign ext or zero ext. We can do this by iterating checking the
      // size of the element at struct type and comp type at type annotation
      CallInst *handleCI = dyn_cast<CallInst>(CI->getArgOperand(1));
      DXASSERT(handleCI, "otherwise handle was not an argument to buffer store.");
      ConstantInt *resClass = dyn_cast<ConstantInt>(handleCI->getArgOperand(1));
      DXASSERT_LOCALVAR(resClass, resClass && resClass->getSExtValue() ==
                               (unsigned)DXIL::ResourceClass::UAV,
               "otherwise buffer store called on non uav kind.");
      ConstantInt *rangeID = dyn_cast<ConstantInt>(handleCI->getArgOperand(2)); // range id or idx?
      DXASSERT(rangeID, "wrong createHandle call.");
      DxilResource dxilRes = M.GetDxilModule().GetUAV(rangeID->getSExtValue());
      StructType *STy = dyn_cast<StructType>(dxilRes.GetRetType());
      DxilStructAnnotation *SAnnot = M.GetDxilModule().GetTypeSystem().GetStructAnnotation(STy);
      ConstantInt *offsetInt = dyn_cast<ConstantInt>(CI->getArgOperand(3));
      unsigned offset = offsetInt->getSExtValue();
      unsigned currentOffset = 0;
      for (DxilStructTypeIterator iter = begin(STy, SAnnot), ItEnd = end(STy, SAnnot); iter != ItEnd; ++iter) {
        std::pair<Type *, DxilFieldAnnotation*> pair = *iter;
        currentOffset += M.getDataLayout().getTypeAllocSize(pair.first);
        if (currentOffset > offset) {
          if (pair.second->GetCompType().IsUIntTy()) {
            for (unsigned i = 4; i < 8; ++i) {
              Value *NewV = CIBuilder.CreateZExt(CI->getArgOperand(i), Type::getInt32Ty(M.getContext()));
              Args.emplace_back(NewV);
            }
            break;
          }
          else if (pair.second->GetCompType().IsIntTy()) {
            for (unsigned i = 4; i < 8; ++i) {
              Value *NewV = CIBuilder.CreateSExt(CI->getArgOperand(i), Type::getInt32Ty(M.getContext()));
              Args.emplace_back(NewV);
            }
            break;
          }
          else {
            DXASSERT(false, "Invalid comp type");
          }
        }
      }
    }

    // mask
    Args.emplace_back(CI->getArgOperand(8));
    // alignment
    Args.emplace_back(M.GetDxilModule().GetOP()->GetI32Const(
        M.getDataLayout().getTypeAllocSize(NewETy)));
    CIBuilder.CreateCall(newFunction, Args);
    CI->eraseFromParent();
   }
}


void DxilTranslateRawBuffer::ReplaceMinPrecisionRawBufferLoadByType(
    Function *F, Type *FromTy, Type *ToTy, OP *Op, const DataLayout &DL) {
  Function *newFunction = Op->GetOpFunc(DXIL::OpCode::RawBufferLoad, ToTy);
  for (auto FUser = F->user_begin(), FEnd = F->user_end(); FUser != FEnd;) {
    User *UserCI = *(FUser++);
    if (CallInst *CI = dyn_cast<CallInst>(UserCI)) {
      IRBuilder<> CIBuilder(CI);
      SmallVector<Value *, 5> newFuncArgs;
      // opcode, handle, index, elementOffset, mask
      // Compiler is generating correct element offset even for min precision types
      // So no need to recalculate here
      for (unsigned i = 0; i < 5; ++i) {
        newFuncArgs.emplace_back(CI->getArgOperand(i));
      }
      // new alignment for new type
      newFuncArgs.emplace_back(Op->GetI32Const(DL.getTypeAllocSize(ToTy)));
      CallInst *newCI = CIBuilder.CreateCall(newFunction, newFuncArgs);
      for (auto CIUser = CI->user_begin(), CIEnd = CI->user_end();
           CIUser != CIEnd;) {
        User *UserEV = *(CIUser++);
        if (ExtractValueInst *EV = dyn_cast<ExtractValueInst>(UserEV)) {
          IRBuilder<> EVBuilder(EV);
          ArrayRef<unsigned> Indices = EV->getIndices();
          DXASSERT(Indices.size() == 1, "Otherwise we have wrong extract value.");
          Value *newEV = EVBuilder.CreateExtractValue(newCI, Indices);
          Value *newTruncV = nullptr;
          if (4 == Indices[0]) { // Don't truncate status
            newTruncV = newEV;
          }
          else if (FromTy->isHalfTy()) {
            newTruncV = EVBuilder.CreateFPTrunc(newEV, FromTy);
          } else if (FromTy->isIntegerTy()) {
            newTruncV = EVBuilder.CreateTrunc(newEV, FromTy);
          } else {
            DXASSERT(false, "unexpected type conversion");
          }
          EV->replaceAllUsesWith(newTruncV);
          EV->eraseFromParent();
        }
      }
      CI->eraseFromParent();
    }
  }
  F->eraseFromParent();
}

char DxilTranslateRawBuffer::ID = 0;
ModulePass *llvm::createDxilTranslateRawBuffer() {
  return new DxilTranslateRawBuffer();
}

INITIALIZE_PASS(DxilTranslateRawBuffer, "hlsl-translate-dxil-raw-buffer",
                "Translate raw buffer load", false, false)
