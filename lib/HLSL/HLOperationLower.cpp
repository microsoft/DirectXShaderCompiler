///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLOperationLower.cpp                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Lower functions to lower HL operations to DXIL operations.                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilConstants.h"
#define _USE_MATH_DEFINES
#include <cmath>
#include <functional>
#include <unordered_set>

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResourceProperties.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HLSL/DxilPoisonValues.h"
#include "dxc/HLSL/HLLowerUDT.h"
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HLSL/HLMatrixType.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/HLOperationLower.h"
#include "dxc/HLSL/HLOperationLowerExtension.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HlslIntrinsicOp.h"

#include "llvm/ADT/APSInt.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"

using namespace llvm;
using namespace hlsl;

struct HLOperationLowerHelper {
  HLModule &M;
  OP &HlslOp;
  Type *VoidTy;
  Type *F32Ty;
  Type *I32Ty;
  Type *I16Ty;
  llvm::Type *I1Ty;
  Type *I8Ty;
  DxilTypeSystem &DxilTypeSys;
  DxilFunctionProps *FunctionProps;
  DataLayout DL;
  SmallDenseMap<Type *, Type *, 4> LoweredTypes;
  HLOperationLowerHelper(HLModule &HLM);
};

HLOperationLowerHelper::HLOperationLowerHelper(HLModule &HLM)
    : M(HLM), HlslOp(*HLM.GetOP()), DxilTypeSys(HLM.GetTypeSystem()),
      DL(DataLayout(HLM.GetHLOptions().bUseMinPrecision
                                ? hlsl::DXIL::kLegacyLayoutString
                                : hlsl::DXIL::kNewLayoutString)) {
  llvm::LLVMContext &Ctx = HLM.GetCtx();
  VoidTy = Type::getVoidTy(Ctx);
  F32Ty = Type::getFloatTy(Ctx);
  I32Ty = Type::getInt32Ty(Ctx);
  I16Ty = Type::getInt16Ty(Ctx);
  I1Ty = Type::getInt1Ty(Ctx);
  I8Ty = Type::getInt8Ty(Ctx);
  Function *EntryFunc = HLM.GetEntryFunction();
  FunctionProps = nullptr;
  if (HLM.HasDxilFunctionProps(EntryFunc))
    FunctionProps = &HLM.GetDxilFunctionProps(EntryFunc);
}

struct HLObjectOperationLowerHelper {
private:
  // For object intrinsics.
  HLModule &HLM;
  struct ResAttribute {
    DXIL::ResourceClass RC;
    DXIL::ResourceKind RK;
    Type *ResourceType;
  };
  std::unordered_map<Value *, ResAttribute> HandleMetaMap;
  std::unordered_set<Instruction *> &UpdateCounterSet;
  // Map from pointer of cbuffer to pointer of resource.
  // For cbuffer like this:
  //   cbuffer A {
  //     Texture2D T;
  //   };
  // A global resource Texture2D T2 will be created for Texture2D T.
  // CBPtrToResourceMap[T] will return T2.
  std::unordered_map<Value *, Value *> CBPtrToResourceMap;

public:
  HLObjectOperationLowerHelper(HLModule &HLM,
                               std::unordered_set<Instruction *> &UpdateCounter)
      : HLM(HLM), UpdateCounterSet(UpdateCounter) {}
  DXIL::ResourceClass getRc(Value *Handle) {
    ResAttribute &Res = findCreateHandleResourceBase(Handle);
    return Res.RC;
  }
  DXIL::ResourceKind getRk(Value *Handle) {
    ResAttribute &Res = findCreateHandleResourceBase(Handle);
    return Res.RK;
  }
  Type *getResourceType(Value *Handle) {
    ResAttribute &Res = findCreateHandleResourceBase(Handle);
    return Res.ResourceType;
  }

  void markHasCounter(Value *Handle, Type *I8Ty) {
    CallInst *CIHandle = cast<CallInst>(Handle);
    DXASSERT(hlsl::GetHLOpcodeGroup(CIHandle->getCalledFunction()) ==
                 HLOpcodeGroup::HLAnnotateHandle,
             "else invalid handle");
    // Mark has counter for the input handle.
    Value *CounterHandle =
        CIHandle->getArgOperand(HLOperandIndex::kHandleOpIdx);
    // Change kind into StructurBufferWithCounter.
    Constant *Props = cast<Constant>(CIHandle->getArgOperand(
        HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx));
    DxilResourceProperties RP = resource_helper::loadPropsFromConstant(*Props);
    RP.Basic.SamplerCmpOrHasCounter = true;

    CIHandle->setArgOperand(
        HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx,
        resource_helper::getAsConstant(RP,
                                       HLM.GetOP()->GetResourcePropertiesType(),
                                       *HLM.GetShaderModel()));

    DXIL::ResourceClass RC = getRc(Handle);
    DXASSERT_LOCALVAR(RC, RC == DXIL::ResourceClass::UAV,
                      "must UAV for counter");
    std::unordered_set<Value *> ResSet;
    markHasCounterOnCreateHandle(CounterHandle, ResSet);
  }

  DxilResourceBase *findCBufferResourceFromHandle(Value *Handle) {
    if (CallInst *CI = dyn_cast<CallInst>(Handle)) {
      hlsl::HLOpcodeGroup Group =
          hlsl::GetHLOpcodeGroupByName(CI->getCalledFunction());
      if (Group == HLOpcodeGroup::HLAnnotateHandle) {
        Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
      }
    }

    Constant *Symbol = nullptr;
    if (CallInst *CI = dyn_cast<CallInst>(Handle)) {
      hlsl::HLOpcodeGroup Group =
          hlsl::GetHLOpcodeGroupByName(CI->getCalledFunction());
      if (Group == HLOpcodeGroup::HLCreateHandle) {
        Symbol = dyn_cast<Constant>(
            CI->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx));
      }
    }

    if (!Symbol)
      return nullptr;

    for (const std::unique_ptr<DxilCBuffer> &Res : HLM.GetCBuffers()) {
      if (Res->GetGlobalSymbol() == Symbol)
        return Res.get();
    }
    return nullptr;
  }

  Value *getOrCreateResourceForCbPtr(GetElementPtrInst *CbPtr,
                                     GlobalVariable *CbGV,
                                     DxilResourceProperties &RP) {
    // Change array idx to 0 to make sure all array ptr share same key.
    Value *Key = uniformCbPtr(CbPtr, CbGV);
    if (CBPtrToResourceMap.count(Key))
      return CBPtrToResourceMap[Key];
    Value *Resource = createResourceForCbPtr(CbPtr, CbGV, RP);
    CBPtrToResourceMap[Key] = Resource;
    return Resource;
  }

  Value *lowerCbResourcePtr(GetElementPtrInst *CbPtr, Value *ResPtr) {
    // Simple case.
    if (ResPtr->getType() == CbPtr->getType())
      return ResPtr;

    // Array case.
    DXASSERT_NOMSG(ResPtr->getType()->getPointerElementType()->isArrayTy());

    IRBuilder<> Builder(CbPtr);
    gep_type_iterator GEPIt = gep_type_begin(CbPtr), E = gep_type_end(CbPtr);

    Value *ArrayIdx = GEPIt.getOperand();

    // Only calc array idx and size.
    // Ignore struct type part.
    for (; GEPIt != E; ++GEPIt) {
      if (GEPIt->isArrayTy()) {
        ArrayIdx = Builder.CreateMul(
            ArrayIdx, Builder.getInt32(GEPIt->getArrayNumElements()));
        ArrayIdx = Builder.CreateAdd(ArrayIdx, GEPIt.getOperand());
      }
    }

    return Builder.CreateGEP(ResPtr, {Builder.getInt32(0), ArrayIdx});
  }

  DxilResourceProperties getResPropsFromAnnotateHandle(CallInst *Anno) {
    Constant *Props = cast<Constant>(Anno->getArgOperand(
        HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx));
    DxilResourceProperties RP = resource_helper::loadPropsFromConstant(*Props);
    return RP;
  }

private:
  ResAttribute &findCreateHandleResourceBase(Value *Handle) {
    if (HandleMetaMap.count(Handle))
      return HandleMetaMap[Handle];

    // Add invalid first to avoid dead loop.
    HandleMetaMap[Handle] = {
        DXIL::ResourceClass::Invalid, DXIL::ResourceKind::Invalid,
        StructType::get(Type::getVoidTy(HLM.GetCtx()), nullptr)};
    if (CallInst *CI = dyn_cast<CallInst>(Handle)) {
      hlsl::HLOpcodeGroup Group =
          hlsl::GetHLOpcodeGroupByName(CI->getCalledFunction());
      if (Group == HLOpcodeGroup::HLAnnotateHandle) {
        Constant *Props = cast<Constant>(CI->getArgOperand(
            HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx));
        DxilResourceProperties RP =
            resource_helper::loadPropsFromConstant(*Props);
        Type *ResTy =
            CI->getArgOperand(HLOperandIndex::kAnnotateHandleResourceTypeOpIdx)
                ->getType();

        ResAttribute Attrib = {RP.getResourceClass(), RP.getResourceKind(),
                               ResTy};

        HandleMetaMap[Handle] = Attrib;
        return HandleMetaMap[Handle];
      }
    }
    dxilutil::EmitErrorOnContext(Handle->getContext(),
                                 "cannot map resource to handle.");

    return HandleMetaMap[Handle];
  }
  CallInst *findCreateHandle(Value *Handle,
                             std::unordered_set<Value *> &ResSet) {
    // Already checked.
    if (ResSet.count(Handle))
      return nullptr;
    ResSet.insert(Handle);

    if (CallInst *CI = dyn_cast<CallInst>(Handle))
      return CI;
    if (SelectInst *Sel = dyn_cast<SelectInst>(Handle)) {
      if (CallInst *CI = findCreateHandle(Sel->getTrueValue(), ResSet))
        return CI;
      if (CallInst *CI = findCreateHandle(Sel->getFalseValue(), ResSet))
        return CI;
      return nullptr;
    }
    if (PHINode *Phi = dyn_cast<PHINode>(Handle)) {
      for (unsigned I = 0; I < Phi->getNumOperands(); I++) {
        if (CallInst *CI = findCreateHandle(Phi->getOperand(I), ResSet))
          return CI;
      }
      return nullptr;
    }

    return nullptr;
  }
  void markHasCounterOnCreateHandle(Value *Handle,
                                    std::unordered_set<Value *> &ResSet) {
    // Already checked.
    if (ResSet.count(Handle))
      return;
    ResSet.insert(Handle);

    if (CallInst *CI = dyn_cast<CallInst>(Handle)) {
      Value *Res =
          CI->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx);
      LoadInst *LdRes = dyn_cast<LoadInst>(Res);
      if (LdRes) {
        UpdateCounterSet.insert(LdRes);
        return;
      }
      if (CallInst *CallRes = dyn_cast<CallInst>(Res)) {
        hlsl::HLOpcodeGroup Group =
            hlsl::GetHLOpcodeGroup(CallRes->getCalledFunction());
        if (Group == HLOpcodeGroup::HLCast) {
          HLCastOpcode Opcode =
              static_cast<HLCastOpcode>(hlsl::GetHLOpcode(CallRes));
          if (Opcode == HLCastOpcode::HandleToResCast) {
            if (Instruction *Hdl = dyn_cast<Instruction>(
                    CallRes->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx)))
              UpdateCounterSet.insert(Hdl);
            return;
          }
        }
      }
      dxilutil::EmitErrorOnInstruction(CI, "cannot map resource to handle.");
      return;
    }
    if (SelectInst *Sel = dyn_cast<SelectInst>(Handle)) {
      markHasCounterOnCreateHandle(Sel->getTrueValue(), ResSet);
      markHasCounterOnCreateHandle(Sel->getFalseValue(), ResSet);
    }
    if (PHINode *Phi = dyn_cast<PHINode>(Handle)) {
      for (unsigned I = 0; I < Phi->getNumOperands(); I++) {
        markHasCounterOnCreateHandle(Phi->getOperand(I), ResSet);
      }
    }
  }

  Value *uniformCbPtr(GetElementPtrInst *CbPtr, GlobalVariable *CbGV) {
    gep_type_iterator GEPIt = gep_type_begin(CbPtr), E = gep_type_end(CbPtr);
    std::vector<Value *> IdxList(CbPtr->idx_begin(), CbPtr->idx_end());
    unsigned I = 0;
    IRBuilder<> Builder(HLM.GetCtx());
    Value *Zero = Builder.getInt32(0);
    for (; GEPIt != E; ++GEPIt, ++I) {
      ConstantInt *ImmIdx = dyn_cast<ConstantInt>(GEPIt.getOperand());
      if (!ImmIdx) {
        // Remove dynamic indexing to avoid crash.
        IdxList[I] = Zero;
      }
    }

    Value *Key = Builder.CreateInBoundsGEP(CbGV, IdxList);
    return Key;
  }

  Value *createResourceForCbPtr(GetElementPtrInst *CbPtr, GlobalVariable *CbGV,
                                DxilResourceProperties &RP) {
    Type *CbTy = CbPtr->getPointerOperandType();
    DXASSERT_LOCALVAR(CbTy, CbTy == CbGV->getType(),
                      "else arg not point to var");

    gep_type_iterator GEPIt = gep_type_begin(CbPtr), E = gep_type_end(CbPtr);
    unsigned I = 0;
    IRBuilder<> Builder(HLM.GetCtx());
    unsigned ArraySize = 1;
    DxilTypeSystem &TypeSys = HLM.GetTypeSystem();

    std::string Name;
    for (; GEPIt != E; ++GEPIt, ++I) {
      if (GEPIt->isArrayTy()) {
        ArraySize *= GEPIt->getArrayNumElements();
        if (!Name.empty())
          Name += ".";
        if (ConstantInt *ImmIdx = dyn_cast<ConstantInt>(GEPIt.getOperand())) {
          unsigned Idx = ImmIdx->getLimitedValue();
          Name += std::to_string(Idx);
        }
      } else if (GEPIt->isStructTy()) {
        DxilStructAnnotation *TypeAnnot =
            TypeSys.GetStructAnnotation(cast<StructType>(*GEPIt));
        DXASSERT_NOMSG(TypeAnnot);
        unsigned Idx = cast<ConstantInt>(GEPIt.getOperand())->getLimitedValue();
        DXASSERT_NOMSG(TypeAnnot->GetNumFields() > Idx);
        DxilFieldAnnotation &FieldAnnot = TypeAnnot->GetFieldAnnotation(Idx);
        if (!Name.empty())
          Name += ".";
        Name += FieldAnnot.GetFieldName();
      }
    }

    Type *Ty = CbPtr->getResultElementType();
    // Not support resource array in cbuffer.
    unsigned ResBinding =
        HLM.GetBindingForResourceInCB(CbPtr, CbGV, RP.getResourceClass());
    return createResourceGv(Ty, Name, RP, ResBinding);
  }

  Value *createResourceGv(Type *Ty, StringRef Name, DxilResourceProperties &RP,
                          unsigned ResBinding) {
    Module &M = *HLM.GetModule();
    Constant *GV = M.getOrInsertGlobal(Name, Ty);
    // Create resource and set GV as globalSym.
    DxilResourceBase *Res = HLM.AddResourceWithGlobalVariableAndProps(GV, RP);
    DXASSERT(Res, "fail to create resource for global variable in cbuffer");
    Res->SetLowerBound(ResBinding);
    return GV;
  }
};

// Helper for lowering resource extension methods.
struct HLObjectExtensionLowerHelper : public hlsl::HLResourceLookup {
  explicit HLObjectExtensionLowerHelper(HLObjectOperationLowerHelper &ObjHelper)
      : MObjHelper(ObjHelper) {}

  virtual bool GetResourceKindName(Value *HLHandle, const char **PpName) {
    DXIL::ResourceKind K = MObjHelper.getRk(HLHandle);
    bool Success = K != DXIL::ResourceKind::Invalid;
    if (Success) {
      *PpName = hlsl::GetResourceKindName(K);
    }
    return Success;
  }

private:
  HLObjectOperationLowerHelper &MObjHelper;
};

using IntrinsicLowerFuncTy = Value *(CallInst *CI, IntrinsicOp IOP,
                                     DXIL::OpCode Opcode,
                                     HLOperationLowerHelper &Helper,
                                     HLObjectOperationLowerHelper *PObjHelper,
                                     bool &Translated);

struct IntrinsicLower {
  // Intrinsic opcode.
  IntrinsicOp IntriOpcode;
  // Lower function.
  IntrinsicLowerFuncTy &LowerFunc;
  // DXIL opcode if can direct map.
  DXIL::OpCode DxilOpcode;
};

// IOP intrinsics.
namespace {

// Creates the necessary scalar calls to for a "trivial" operation where only
// call instructions to a single function type are needed.
// The overload type `Ty` determines what scalarization might be required.
// Elements of any vectors in `refArgs` are extracted  into scalars for each
// call generated while the same scalar values are used unaltered in each call.
// Utility objects `HlslOp` and `Builder` are used to generate calls to the
// given `DxilFunc` for each set of scalar arguments.
// The results are reconstructed into the given `RetTy` as needed.
Value *trivialDxilOperation(Function *DxilFunc, OP::OpCode Opcode,
                            ArrayRef<Value *> RefArgs, Type *Ty, Type *RetTy,
                            OP *HlslOp, IRBuilder<> &Builder) {
  unsigned ArgNum = RefArgs.size();
  std::vector<Value *> Args = RefArgs;

  if (Ty->isVectorTy()) {
    Value *RetVal = llvm::UndefValue::get(RetTy);
    unsigned VecSize = Ty->getVectorNumElements();
    for (unsigned I = 0; I < VecSize; I++) {
      // Update vector args, skip known opcode arg.
      for (unsigned ArgIdx = HLOperandIndex::kUnaryOpSrc0Idx; ArgIdx < ArgNum;
           ArgIdx++) {
        if (RefArgs[ArgIdx]->getType()->isVectorTy()) {
          Value *Arg = RefArgs[ArgIdx];
          Args[ArgIdx] = Builder.CreateExtractElement(Arg, I);
        }
      }
      Value *EltOP =
          Builder.CreateCall(DxilFunc, Args, HlslOp->GetOpCodeName(Opcode));
      RetVal = Builder.CreateInsertElement(RetVal, EltOP, I);
    }
    return RetVal;
  }

  // Cannot add name to void.
  if (RetTy->isVoidTy())
    return Builder.CreateCall(DxilFunc, Args);

  return Builder.CreateCall(DxilFunc, Args, HlslOp->GetOpCodeName(Opcode));
}

// Creates a native vector call to for a "trivial" operation where only a single
// call instruction is needed. The overload and return types are the same vector
// type `Ty`.
// Utility objects `HlslOp` and `Builder` are used to create a call to the given
// `DxilFunc` with `RefArgs` arguments.
Value *trivialDxilVectorOperation(Function *Func, OP::OpCode Opcode,
                                  ArrayRef<Value *> Args, Type *Ty, OP *OP,
                                  IRBuilder<> &Builder) {
  if (!Ty->isVoidTy())
    return Builder.CreateCall(Func, Args, OP->GetOpCodeName(Opcode));
  return Builder.CreateCall(Func, Args); // Cannot add name to void.
}

// Generates a DXIL operation with the overloaded type based on `Ty` and return
// type `RetTy`. When Ty is a vector, it will either generate per-element calls
// for each vector element and reconstruct the vector type from those results or
// operate on and return native vectors depending on vector size and the
// legality of the vector overload.
Value *trivialDxilOperation(OP::OpCode Opcode, ArrayRef<Value *> RefArgs,
                            Type *Ty, Type *RetTy, OP *HlslOp,
                            IRBuilder<> &Builder) {

  // If supported and the overload type is a vector with more than 1 element,
  // create a native vector operation.
  if (Ty->isVectorTy() && Ty->getVectorNumElements() > 1 &&
      HlslOp->GetModule()->GetHLModule().GetShaderModel()->IsSM69Plus() &&
      OP::IsOverloadLegal(Opcode, Ty)) {
    Function *DxilFunc = HlslOp->GetOpFunc(Opcode, Ty);
    return trivialDxilVectorOperation(DxilFunc, Opcode, RefArgs, Ty, HlslOp,
                                      Builder);
  }

  // Set overload type to the scalar type of `Ty` and generate call(s).
  Type *EltTy = Ty->getScalarType();
  Function *DxilFunc = HlslOp->GetOpFunc(Opcode, EltTy);

  return trivialDxilOperation(DxilFunc, Opcode, RefArgs, Ty, RetTy, HlslOp,
                              Builder);
}

Value *trivialDxilOperation(OP::OpCode Opcode, ArrayRef<Value *> RefArgs,
                            Type *Ty, Instruction *Inst, OP *HlslOp) {
  DXASSERT(RefArgs.size() > 0, "else opcode isn't in signature");
  DXASSERT(RefArgs[0] == nullptr,
           "else caller has already filled the value in");
  IRBuilder<> B(Inst);
  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  const_cast<llvm::Value **>(RefArgs.data())[0] =
      OpArg; // actually stack memory from caller
  return trivialDxilOperation(Opcode, RefArgs, Ty, Inst->getType(), HlslOp, B);
}

// Translate call that converts to a dxil unary operation with a different
// return type from the overload by passing the argument, explicit return type,
// and helper objects to the scalarizing unary dxil operation creation.
Value *trivialUnaryOperationRet(CallInst *CI, IntrinsicOp IOP,
                                OP::OpCode OpCode,
                                HLOperationLowerHelper &Helper,
                                HLObjectOperationLowerHelper *,
                                bool &Translated) {
  Value *Src = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Type *Ty = Src->getType();

  IRBuilder<> Builder(CI);
  hlsl::OP *OP = &Helper.HlslOp;
  Type *RetTy = CI->getType();
  Constant *OpArg = OP->GetU32Const((unsigned)OpCode);
  Value *Args[] = {OpArg, Src};

  return trivialDxilOperation(OpCode, Args, Ty, RetTy, OP, Builder);
}

Value *trivialDxilUnaryOperation(OP::OpCode OpCode, Value *Src, hlsl::OP *Op,
                                 IRBuilder<> &Builder) {
  Type *Ty = Src->getType();

  Constant *OpArg = Op->GetU32Const((unsigned)OpCode);
  Value *Args[] = {OpArg, Src};

  return trivialDxilOperation(OpCode, Args, Ty, Ty, Op, Builder);
}

Value *trivialDxilBinaryOperation(OP::OpCode Opcode, Value *Src0, Value *Src1,
                                  hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  Type *Ty = Src0->getType();

  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Value *Args[] = {OpArg, Src0, Src1};

  return trivialDxilOperation(Opcode, Args, Ty, Ty, HlslOp, Builder);
}

Value *trivialDxilTrinaryOperation(OP::OpCode Opcode, Value *Src0, Value *Src1,
                                   Value *Src2, hlsl::OP *HlslOp,
                                   IRBuilder<> &Builder) {
  Type *Ty = Src0->getType();

  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Value *Args[] = {OpArg, Src0, Src1, Src2};

  return trivialDxilOperation(Opcode, Args, Ty, Ty, HlslOp, Builder);
}

// Translate call that trivially converts to a dxil unary operation by passing
// argument, return type, and helper objects to either scalarizing or native
// vector dxil operation creation depending on version and vector size.
Value *trivialUnaryOperation(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                             HLOperationLowerHelper &Helper,
                             HLObjectOperationLowerHelper *PObjHelper,
                             bool &Translated) {
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  IRBuilder<> Builder(CI);
  hlsl::OP *HlslOp = &Helper.HlslOp;

  return trivialDxilUnaryOperation(Opcode, Src0, HlslOp, Builder);
}

// Translate call that trivially converts to a dxil binary operation by passing
// arguments, return type, and helper objects to either scalarizing or native
// vector dxil operation creation depending on version and vector size.
Value *trivialBinaryOperation(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                              HLOperationLowerHelper &Helper,
                              HLObjectOperationLowerHelper *PObjHelper,
                              bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Src1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);

  Value *BinOp =
      trivialDxilBinaryOperation(Opcode, Src0, Src1, HlslOp, Builder);
  return BinOp;
}

// Translate call that trivially converts to a dxil trinary (aka tertiary)
// operation by passing arguments, return type, and helper objects to either
// scalarizing or native vector dxil operation creation depending on version
// and vector size.
Value *trivialTrinaryOperation(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                               HLOperationLowerHelper &Helper,
                               HLObjectOperationLowerHelper *PObjHelper,
                               bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  Value *Src1 = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx);
  Value *Src2 = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx);
  IRBuilder<> Builder(CI);

  Value *TriOp =
      trivialDxilTrinaryOperation(Opcode, Src0, Src1, Src2, HlslOp, Builder);
  return TriOp;
}

Value *trivialIsSpecialFloat(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                             HLOperationLowerHelper &Helper,
                             HLObjectOperationLowerHelper *PObjHelper,
                             bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  IRBuilder<> Builder(CI);

  Type *Ty = Src->getType();
  Type *RetTy = Type::getInt1Ty(CI->getContext());
  if (Ty->isVectorTy())
    RetTy = VectorType::get(RetTy, Ty->getVectorNumElements());

  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Value *Args[] = {OpArg, Src};

  return trivialDxilOperation(Opcode, Args, Ty, RetTy, HlslOp, Builder);
}

bool isResourceGep(GetElementPtrInst *I) {
  Type *Ty = I->getType()->getPointerElementType();
  Ty = dxilutil::GetArrayEltTy(Ty);
  // Only mark on GEP which point to resource.
  return dxilutil::IsHLSLResourceType(Ty);
}

Value *translateNonUniformResourceIndex(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  Value *V = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Type *HdlTy = Helper.HlslOp.GetHandleType();
  for (User *U : CI->users()) {
    if (GetElementPtrInst *I = dyn_cast<GetElementPtrInst>(U)) {
      // Only mark on GEP which point to resource.
      if (isResourceGep(I))
        DxilMDHelper::MarkNonUniform(I);
    } else if (CastInst *CastI = dyn_cast<CastInst>(U)) {
      for (User *CastU : CastI->users()) {
        if (GetElementPtrInst *I = dyn_cast<GetElementPtrInst>(CastU)) {
          // Only mark on GEP which point to resource.
          if (isResourceGep(I))
            DxilMDHelper::MarkNonUniform(I);
        } else if (CallInst *CI = dyn_cast<CallInst>(CastU)) {
          if (CI->getType() == HdlTy)
            DxilMDHelper::MarkNonUniform(CI);
        }
      }
    } else if (CallInst *CI = dyn_cast<CallInst>(U)) {
      if (CI->getType() == HdlTy)
        DxilMDHelper::MarkNonUniform(CI);
    }
  }
  CI->replaceAllUsesWith(V);
  return nullptr;
}

Value *trivialBarrier(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                      HLOperationLowerHelper &Helper,
                      HLObjectOperationLowerHelper *PObjHelper,
                      bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  Function *DxilFunc = OP->GetOpFunc(OP::OpCode::Barrier, CI->getType());
  Constant *OpArg = OP->GetU32Const((unsigned)OP::OpCode::Barrier);

  unsigned Uglobal = static_cast<unsigned>(DXIL::BarrierMode::UAVFenceGlobal);
  unsigned G = static_cast<unsigned>(DXIL::BarrierMode::TGSMFence);
  unsigned T = static_cast<unsigned>(DXIL::BarrierMode::SyncThreadGroup);
  // unsigned ut =
  // static_cast<unsigned>(DXIL::BarrierMode::UAVFenceThreadGroup);

  unsigned BarrierMode = 0;
  switch (IOP) {
  case IntrinsicOp::IOP_AllMemoryBarrier:
    BarrierMode = Uglobal | G;
    break;
  case IntrinsicOp::IOP_AllMemoryBarrierWithGroupSync:
    BarrierMode = Uglobal | G | T;
    break;
  case IntrinsicOp::IOP_GroupMemoryBarrier:
    BarrierMode = G;
    break;
  case IntrinsicOp::IOP_GroupMemoryBarrierWithGroupSync:
    BarrierMode = G | T;
    break;
  case IntrinsicOp::IOP_DeviceMemoryBarrier:
    BarrierMode = Uglobal;
    break;
  case IntrinsicOp::IOP_DeviceMemoryBarrierWithGroupSync:
    BarrierMode = Uglobal | T;
    break;
  default:
    DXASSERT(0, "invalid opcode for barrier");
    break;
  }
  Value *Src0 = OP->GetU32Const(static_cast<unsigned>(BarrierMode));

  Value *Args[] = {OpArg, Src0};

  IRBuilder<> Builder(CI);
  Builder.CreateCall(DxilFunc, Args);
  return nullptr;
}

Value *translateD3DColorToUByte4(CallInst *CI, IntrinsicOp IOP,
                                 OP::OpCode Opcode,
                                 HLOperationLowerHelper &Helper,
                                 HLObjectOperationLowerHelper *PObjHelper,
                                 bool &Translated) {
  IRBuilder<> Builder(CI);
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Type *Ty = Val->getType();

  // Use the same scaling factor used by FXC (i.e., 255.001953)
  // Excerpt from stackoverflow discussion:
  // "Built-in rounding, necessary because of truncation. 0.001953 * 256 = 0.5"
  Constant *ToByteConst = ConstantFP::get(Ty->getScalarType(), 255.001953);

  if (Ty->isVectorTy()) {
    static constexpr int SupportedVecElemCount = 4;
    if (Ty->getVectorNumElements() != SupportedVecElemCount) {
      llvm_unreachable(
          "Unsupported input type for intrinsic D3DColorToUByte4.");
      return UndefValue::get(CI->getType());
    }

    ToByteConst = ConstantVector::getSplat(SupportedVecElemCount, ToByteConst);
    // Swizzle the input val -> val.zyxw
    SmallVector<int, 4> Mask{2, 1, 0, 3};
    Val = Builder.CreateShuffleVector(Val, Val, Mask);
  }

  Value *Byte4 = Builder.CreateFMul(ToByteConst, Val);
  return Builder.CreateCast(Instruction::CastOps::FPToSI, Byte4, CI->getType());
}

// Returns true if pow can be implemented using Fxc's mul-only code gen pattern.
// Fxc uses the below rules when choosing mul-only code gen pattern to implement
// pow function. Rule 1: Applicable only to power values in the range
// [INT32_MIN, INT32_MAX] Rule 2: The maximum number of mul ops needed shouldn't
// exceed (2n+1) or (n+1) based on whether the power
//         is a positive or a negative value. Here "n" is the number of scalar
//         elements in power.
// Rule 3: Power must be an exact value.
// +----------+---------------------+------------------+
// | BaseType | IsExponentPositive  | MaxMulOpsAllowed |
// +----------+---------------------+------------------+
// | float4x4 | True                |               33 |
// | float4x4 | False               |               17 |
// | float4x2 | True                |               17 |
// | float4x2 | False               |                9 |
// | float2x4 | True                |               17 |
// | float2x4 | False               |                9 |
// | float4   | True                |                9 |
// | float4   | False               |                5 |
// | float2   | True                |                5 |
// | float2   | False               |                3 |
// | float    | True                |                3 |
// | float    | False               |                2 |
// +----------+---------------------+------------------+

bool canUseFxcMulOnlyPatternForPow(IRBuilder<> &Builder, Value *X, Value *Pow,
                                   int32_t &PowI) {
  // Applicable only when power is a literal.
  if (!isa<ConstantDataVector>(Pow) && !isa<ConstantFP>(Pow)) {
    return false;
  }

  // Only apply this code gen on splat values.
  if (ConstantDataVector *Cdv = dyn_cast<ConstantDataVector>(Pow)) {
    if (!hlsl::dxilutil::IsSplat(Cdv)) {
      return false;
    }
  }

  // Only apply on aggregates of 16 or fewer elements,
  // representing the max 4x4 matrix size.
  Type *Ty = X->getType();
  if (Ty->isVectorTy() && Ty->getVectorNumElements() > 16)
    return false;

  APFloat PowApf = isa<ConstantDataVector>(Pow)
                       ? cast<ConstantDataVector>(Pow)->getElementAsAPFloat(0)
                       : // should be a splat value
                       cast<ConstantFP>(Pow)->getValueAPF();
  APSInt PowAps(32, false);
  bool IsExact = false;
  // Try converting float value of power to integer and also check if the float
  // value is exact.
  APFloat::opStatus Status =
      PowApf.convertToInteger(PowAps, APFloat::rmTowardZero, &IsExact);
  if (Status == APFloat::opStatus::opOK && IsExact) {
    PowI = PowAps.getExtValue();
    uint32_t PowU = abs(PowI);
    int SetBitCount = 0;
    int MaxBitSetPos = -1;
    for (int I = 0; I < 32; I++) {
      if ((PowU >> I) & 1) {
        SetBitCount++;
        MaxBitSetPos = I;
      }
    }

    DXASSERT(MaxBitSetPos <= 30, "msb should always be zero.");
    unsigned NumElem =
        isa<ConstantDataVector>(Pow) ? X->getType()->getVectorNumElements() : 1;
    int MulOpThreshold = PowI < 0 ? NumElem + 1 : 2 * NumElem + 1;
    int MulOpNeeded = MaxBitSetPos + SetBitCount - 1;
    return MulOpNeeded <= MulOpThreshold;
  }

  return false;
}

Value *translatePowUsingFxcMulOnlyPattern(IRBuilder<> &Builder, Value *X,
                                          const int32_t Y) {
  uint32_t AbsY = abs(Y);
  // If y is zero then always return 1.
  if (AbsY == 0) {
    return ConstantFP::get(X->getType(), 1);
  }

  int LastSetPos = -1;
  Value *Result = nullptr;
  Value *Mul = nullptr;
  for (int I = 0; I < 32; I++) {
    if ((AbsY >> I) & 1) {
      for (int J = I; J > LastSetPos; J--) {
        if (!Mul) {
          Mul = X;
        } else {
          Mul = Builder.CreateFMul(Mul, Mul);
        }
      }

      Result = (Result == nullptr) ? Mul : Builder.CreateFMul(Result, Mul);
      LastSetPos = I;
    }
  }

  // Compute reciprocal for negative power values.
  if (Y < 0) {
    Value *ConstOne = ConstantFP::get(X->getType(), 1);
    Result = Builder.CreateFDiv(ConstOne, Result);
  }

  return Result;
}

Value *translatePowImpl(hlsl::OP *HlslOp, IRBuilder<> &Builder, Value *X,
                        Value *Y, bool IsFxcCompatMode = false) {
  // As applicable implement pow using only mul ops as done by Fxc.
  int32_t P = 0;
  if (canUseFxcMulOnlyPatternForPow(Builder, X, Y, P)) {
    if (IsFxcCompatMode)
      return translatePowUsingFxcMulOnlyPattern(Builder, X, P);
    // Only take care 2 for it will not affect register pressure.
    if (P == 2)
      return Builder.CreateFMul(X, X);
  }

  // Default to log-mul-exp pattern if previous scenarios don't apply.
  // t = log(x);
  Value *LogX =
      trivialDxilUnaryOperation(DXIL::OpCode::Log, X, HlslOp, Builder);
  // t = y * t;
  Value *MulY = Builder.CreateFMul(LogX, Y);
  // pow = exp(t);
  return trivialDxilUnaryOperation(DXIL::OpCode::Exp, MulY, HlslOp, Builder);
}

Value *translateAddUint64(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                          HLOperationLowerHelper &Helper,
                          HLObjectOperationLowerHelper *PObjHelper,
                          bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  IRBuilder<> Builder(CI);
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Type *Ty = Val->getType();
  VectorType *VT = dyn_cast<VectorType>(Ty);
  if (!VT) {
    dxilutil::EmitErrorOnInstruction(
        CI, "AddUint64 can only be applied to uint2 and uint4 operands.");
    return UndefValue::get(Ty);
  }

  unsigned Size = VT->getNumElements();
  if (Size != 2 && Size != 4) {
    dxilutil::EmitErrorOnInstruction(
        CI, "AddUint64 can only be applied to uint2 and uint4 operands.");
    return UndefValue::get(Ty);
  }
  Value *Op0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Op1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);

  Value *RetVal = UndefValue::get(Ty);

  Function *AddC = HlslOp->GetOpFunc(DXIL::OpCode::UAddc, Helper.I32Ty);
  Value *OpArg = Builder.getInt32(static_cast<unsigned>(DXIL::OpCode::UAddc));
  for (unsigned I = 0; I < Size; I += 2) {
    Value *Low0 = Builder.CreateExtractElement(Op0, I);
    Value *Low1 = Builder.CreateExtractElement(Op1, I);
    Value *LowWithC = Builder.CreateCall(AddC, {OpArg, Low0, Low1});
    Value *Low = Builder.CreateExtractValue(LowWithC, 0);
    RetVal = Builder.CreateInsertElement(RetVal, Low, I);

    Value *Carry = Builder.CreateExtractValue(LowWithC, 1);
    // Ext i1 to i32
    Carry = Builder.CreateZExt(Carry, Helper.I32Ty);

    Value *Hi0 = Builder.CreateExtractElement(Op0, I + 1);
    Value *Hi1 = Builder.CreateExtractElement(Op1, I + 1);
    Value *Hi = Builder.CreateAdd(Hi0, Hi1);
    Hi = Builder.CreateAdd(Hi, Carry);
    RetVal = Builder.CreateInsertElement(RetVal, Hi, I + 1);
  }
  return RetVal;
}

bool isValidLoadInput(Value *V) {
  // Must be load input.
  // TODO: report this error on front-end
  if (!V || !isa<CallInst>(V)) {
    return false;
  }
  CallInst *CI = cast<CallInst>(V);
  // Must be immediate.
  ConstantInt *OpArg =
      cast<ConstantInt>(CI->getArgOperand(DXIL::OperandIndex::kOpcodeIdx));
  DXIL::OpCode Op = static_cast<DXIL::OpCode>(OpArg->getLimitedValue());
  if (Op != DXIL::OpCode::LoadInput) {
    return false;
  }
  return true;
}

// Tunnel through insert/extract element and shuffle to find original source
// of scalar value, or specified element (vecIdx) of vector value.
Value *findScalarSource(Value *Src, unsigned VecIdx = 0) {
  Type *SrcTy = Src->getType()->getScalarType();
  while (Src && !isa<UndefValue>(Src)) {
    if (Src->getType()->isVectorTy()) {
      if (InsertElementInst *IE = dyn_cast<InsertElementInst>(Src)) {
        unsigned CurIdx = (unsigned)cast<ConstantInt>(IE->getOperand(2))
                              ->getUniqueInteger()
                              .getLimitedValue();
        Src = IE->getOperand((CurIdx == VecIdx) ? 1 : 0);
      } else if (ShuffleVectorInst *SV = dyn_cast<ShuffleVectorInst>(Src)) {
        int NewIdx = SV->getMaskValue(VecIdx);
        if (NewIdx < 0)
          return UndefValue::get(SrcTy);
        VecIdx = (unsigned)NewIdx;
        Src = SV->getOperand(0);
        unsigned NumElt = Src->getType()->getVectorNumElements();
        if (NumElt <= VecIdx) {
          VecIdx -= NumElt;
          Src = SV->getOperand(1);
        }
      } else {
        return UndefValue::get(SrcTy); // Didn't find it.
      }
    } else {
      if (ExtractElementInst *EE = dyn_cast<ExtractElementInst>(Src)) {
        VecIdx = (unsigned)cast<ConstantInt>(EE->getIndexOperand())
                     ->getUniqueInteger()
                     .getLimitedValue();
        Src = EE->getVectorOperand();
      } else if (hlsl::dxilutil::IsConvergentMarker(Src)) {
        Src = hlsl::dxilutil::GetConvergentSource(Src);
      } else {
        break; // Found it.
      }
    }
  }
  return Src;
}

// Finds corresponding inputs, calls translation for each, and returns
// resulting vector or scalar.
// Uses functor that takes (inputElemID, rowIdx, colIdx), and returns
// translation for one input scalar.
Value *translateEvalHelper(
    CallInst *CI, Value *Val, IRBuilder<> &Builder,
    std::function<Value *(Value *, Value *, Value *)> FnTranslateScalarInput) {
  Type *Ty = CI->getType();
  Value *Result = UndefValue::get(Ty);
  if (Ty->isVectorTy()) {
    for (unsigned I = 0; I < Ty->getVectorNumElements(); ++I) {
      Value *InputEl = findScalarSource(Val, I);
      if (!isValidLoadInput(InputEl)) {
        dxilutil::EmitErrorOnInstruction(
            CI, "attribute evaluation can only be done "
                "on values taken directly from inputs.");
        return Result;
      }
      CallInst *LoadInput = cast<CallInst>(InputEl);
      Value *InputElemId =
          LoadInput->getArgOperand(DXIL::OperandIndex::kLoadInputIDOpIdx);
      Value *RowIdx =
          LoadInput->getArgOperand(DXIL::OperandIndex::kLoadInputRowOpIdx);
      Value *ColIdx =
          LoadInput->getArgOperand(DXIL::OperandIndex::kLoadInputColOpIdx);
      Value *Elt = FnTranslateScalarInput(InputElemId, RowIdx, ColIdx);
      Result = Builder.CreateInsertElement(Result, Elt, I);
    }
  } else {
    Value *InputEl = findScalarSource(Val);
    if (!isValidLoadInput(InputEl)) {
      dxilutil::EmitErrorOnInstruction(CI,
                                       "attribute evaluation can only be done "
                                       "on values taken directly from inputs.");
      return Result;
    }
    CallInst *LoadInput = cast<CallInst>(InputEl);
    Value *InputElemId =
        LoadInput->getArgOperand(DXIL::OperandIndex::kLoadInputIDOpIdx);
    Value *RowIdx =
        LoadInput->getArgOperand(DXIL::OperandIndex::kLoadInputRowOpIdx);
    Value *ColIdx =
        LoadInput->getArgOperand(DXIL::OperandIndex::kLoadInputColOpIdx);
    Result = FnTranslateScalarInput(InputElemId, RowIdx, ColIdx);
  }
  return Result;
}

Value *translateEvalSample(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                           HLOperationLowerHelper &Helper,
                           HLObjectOperationLowerHelper *PObjHelper,
                           bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Val = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *SampleIdx = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);
  OP::OpCode Opcode = OP::OpCode::EvalSampleIndex;
  Value *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Function *EvalFunc =
      HlslOp->GetOpFunc(Opcode, CI->getType()->getScalarType());

  return translateEvalHelper(
      CI, Val, Builder,
      [&](Value *InputElemId, Value *RowIdx, Value *ColIdx) -> Value * {
        return Builder.CreateCall(
            EvalFunc, {OpArg, InputElemId, RowIdx, ColIdx, SampleIdx});
      });
}

Value *translateEvalSnapped(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                            HLOperationLowerHelper &Helper,
                            HLObjectOperationLowerHelper *PObjHelper,
                            bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Val = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Offset = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);
  Value *OffsetX = Builder.CreateExtractElement(Offset, (uint64_t)0);
  Value *OffsetY = Builder.CreateExtractElement(Offset, 1);
  OP::OpCode Opcode = OP::OpCode::EvalSnapped;
  Value *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Function *EvalFunc =
      HlslOp->GetOpFunc(Opcode, CI->getType()->getScalarType());

  return translateEvalHelper(
      CI, Val, Builder,
      [&](Value *InputElemId, Value *RowIdx, Value *ColIdx) -> Value * {
        return Builder.CreateCall(
            EvalFunc, {OpArg, InputElemId, RowIdx, ColIdx, OffsetX, OffsetY});
      });
}

Value *translateEvalCentroid(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                             HLOperationLowerHelper &Helper,
                             HLObjectOperationLowerHelper *PObjHelper,
                             bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Val = CI->getArgOperand(DXIL::OperandIndex::kUnarySrc0OpIdx);
  IRBuilder<> Builder(CI);
  OP::OpCode Opcode = OP::OpCode::EvalCentroid;
  Value *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Function *EvalFunc =
      HlslOp->GetOpFunc(Opcode, CI->getType()->getScalarType());

  return translateEvalHelper(
      CI, Val, Builder,
      [&](Value *InputElemId, Value *RowIdx, Value *ColIdx) -> Value * {
        return Builder.CreateCall(EvalFunc,
                                  {OpArg, InputElemId, RowIdx, ColIdx});
      });
}

/*
HLSL: bool RWDispatchNodeInputRecord<recordType>::FinishedCrossGroupSharing()
DXIL: i1 @dx.op.finishedCrossGroupSharing(i32 %Opcode,
%dx.types.NodeRecordHandle %NodeInputRecordHandle)
*/
Value *translateNodeFinishedCrossGroupSharing(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;

  Function *DxilFunc = OP->GetOpFunc(Op, Type::getVoidTy(CI->getContext()));
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  DXASSERT_NOMSG(Handle->getType() == OP->GetNodeRecordHandleType());
  Value *OpArg = OP->GetU32Const((unsigned)Op);

  IRBuilder<> Builder(CI);
  return Builder.CreateCall(DxilFunc, {OpArg, Handle});
}

/*
HLSL:
    bool NodeOutput<recordType>::IsValid()
    bool EmptyNodeOutput::IsValid()
DXIL:
  i1 @dx.op.nodeOutputIsValid(i32 %Opcode, %dx.types.NodeHandle
%NodeOutputHandle)
*/
Value *translateNodeOutputIsValid(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                                  HLOperationLowerHelper &Helper,
                                  HLObjectOperationLowerHelper *PObjHelper,
                                  bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  Function *DxilFunc = OP->GetOpFunc(Op, Type::getVoidTy(CI->getContext()));
  Value *OpArg = OP->GetU32Const((unsigned)Op);

  IRBuilder<> Builder(CI);
  return Builder.CreateCall(DxilFunc, {OpArg, Handle});
}

Value *translateGetAttributeAtVertex(CallInst *CI, IntrinsicOp IOP,
                                     OP::OpCode Op,
                                     HLOperationLowerHelper &Helper,
                                     HLObjectOperationLowerHelper *PObjHelper,
                                     bool &Translated) {
  DXASSERT(Op == OP::OpCode::AttributeAtVertex, "Wrong opcode to translate");
  hlsl::OP *HlslOp = &Helper.HlslOp;
  IRBuilder<> Builder(CI);
  Value *Val = CI->getArgOperand(DXIL::OperandIndex::kBinarySrc0OpIdx);
  Value *VertexIdx = CI->getArgOperand(DXIL::OperandIndex::kBinarySrc1OpIdx);
  Value *VertexI8Idx =
      Builder.CreateTrunc(VertexIdx, Type::getInt8Ty(CI->getContext()));
  Value *OpArg = HlslOp->GetU32Const((unsigned)Op);
  Function *EvalFunc = HlslOp->GetOpFunc(Op, Val->getType()->getScalarType());

  return translateEvalHelper(
      CI, Val, Builder,
      [&](Value *InputElemId, Value *RowIdx, Value *ColIdx) -> Value * {
        return Builder.CreateCall(
            EvalFunc, {OpArg, InputElemId, RowIdx, ColIdx, VertexI8Idx});
      });
}
/*

HLSL:
void Barrier(uint MemoryTypeFlags, uint SemanticFlags)
void Barrier(Object o, uint SemanticFlags)

All UAVs and/or Node Records by types:
void @dx.op.barrierByMemoryType(i32 %Opcode,
  i32 %MemoryTypeFlags, i32 %SemanticFlags)

UAV by handle:
void @dx.op.barrierByMemoryHandle(i32 %Opcode,
  %dx.types.Handle %Object, i32 %SemanticFlags)

Node Record by handle:
void @dx.op.barrierByMemoryHandle(i32 %Opcode,
  %dx.types.NodeRecordHandle %Object, i32 %SemanticFlags)
*/

Value *translateBarrier(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                        HLOperationLowerHelper &Helper,
                        HLObjectOperationLowerHelper *PObjHelper,
                        bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  Value *HandleOrMemoryFlags =
      CI->getArgOperand(HLOperandIndex::kBarrierMemoryTypeFlagsOpIdx);
  Value *SemanticFlags =
      CI->getArgOperand(HLOperandIndex::kBarrierSemanticFlagsOpIdx);
  IRBuilder<> Builder(CI);

  if (HandleOrMemoryFlags->getType()->isIntegerTy()) {
    Op = OP::OpCode::BarrierByMemoryType;
  } else if (HandleOrMemoryFlags->getType() == OP->GetHandleType()) {
    Op = OP::OpCode::BarrierByMemoryHandle;
  } else if (HandleOrMemoryFlags->getType() == OP->GetNodeRecordHandleType()) {
    Op = OP::OpCode::BarrierByNodeRecordHandle;
  } else {
    DXASSERT(false, "Shouldn't get here");
  }

  Function *DxilFunc = OP->GetOpFunc(Op, CI->getType());
  Constant *OpArg = OP->GetU32Const((unsigned)Op);

  Value *Args[] = {OpArg, HandleOrMemoryFlags, SemanticFlags};

  Builder.CreateCall(DxilFunc, Args);
  return nullptr;
}

Value *translateGetGroupOrThreadNodeOutputRecords(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool IsPerThreadRecord, bool &Translated) {
  IRBuilder<> Builder(CI);
  hlsl::OP *OP = &Helper.HlslOp;
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  Function *DxilFunc = OP->GetOpFunc(Op, Builder.getVoidTy());
  Value *OpArg = OP->GetU32Const((unsigned)Op);
  Value *Count =
      CI->getArgOperand(HLOperandIndex::kAllocateRecordNumRecordsIdx);
  Value *PerThread = OP->GetI1Const(IsPerThreadRecord);

  Value *Args[] = {OpArg, Handle, Count, PerThread};

  return Builder.CreateCall(DxilFunc, Args);
}

/*
HLSL:
GroupNodeOutputRecords<recordType>
NodeOutput<recordType>::GetGroupNodeOutputRecords(uint numRecords); DXIL:
%dx.types.NodeRecordHandle @dx.op.allocateNodeOutputRecords(i32 %Opcode,
%dx.types.NodeHandle %NodeOutputHandle, i32 %NumRecords, i1 %PerThread)
*/
Value *
translateGetGroupNodeOutputRecords(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                                   HLOperationLowerHelper &Helper,
                                   HLObjectOperationLowerHelper *PObjHelper,
                                   bool &Translated) {
  return translateGetGroupOrThreadNodeOutputRecords(
      CI, IOP, Op, Helper, PObjHelper, /* isPerThreadRecord */ false,
      Translated);
}

/*
HLSL:
ThreadNodeOutputRecords<recordType>
NodeOutput<recordType>::GetThreadNodeOutputRecords(uint numRecords) DXIL:
%dx.types.NodeRecordHandle @dx.op.allocateNodeOutputRecords(i32 %Opcode,
%dx.types.NodeHandle %NodeOutputHandle, i32 %NumRecords, i1 %PerThread)
*/
Value *translateGetThreadNodeOutputRecords(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  return translateGetGroupOrThreadNodeOutputRecords(
      CI, IOP, Op, Helper, PObjHelper, /* isPerThreadRecord */ true,
      Translated);
}

/*
HLSL:
uint EmptyNodeInput::Count()
uint GroupNodeInputRecords<recordType>::Count()
uint RWGroupNodeInputRecords<recordType>::Count()

DXIL:
i32 @dx.op.getInputRecordCount(i32 %Opcode, %dx.types.NodeRecordHandle
%NodeInputHandle)
*/
Value *
translateNodeGetInputRecordCount(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                                 HLOperationLowerHelper &Helper,
                                 HLObjectOperationLowerHelper *PObjHelper,
                                 bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;

  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  DXASSERT_NOMSG(Handle->getType() == OP->GetNodeRecordHandleType());
  Function *DxilFunc = OP->GetOpFunc(Op, Type::getVoidTy(CI->getContext()));
  Value *OpArg = OP->GetU32Const((unsigned)Op);
  Value *Args[] = {OpArg, Handle};

  IRBuilder<> Builder(CI);
  return Builder.CreateCall(DxilFunc, Args);
}

Value *trivialNoArgOperation(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                             HLOperationLowerHelper &Helper,
                             HLObjectOperationLowerHelper *PObjHelper,
                             bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Type *Ty = Type::getVoidTy(CI->getContext());

  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Value *Args[] = {OpArg};
  IRBuilder<> Builder(CI);
  Value *DxilOp = trivialDxilOperation(Opcode, Args, Ty, Ty, HlslOp, Builder);

  return DxilOp;
}

Value *trivialNoArgWithRetOperation(CallInst *CI, IntrinsicOp IOP,
                                    OP::OpCode Opcode,
                                    HLOperationLowerHelper &Helper,
                                    HLObjectOperationLowerHelper *PObjHelper,
                                    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Type *Ty = CI->getType();

  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Value *Args[] = {OpArg};
  IRBuilder<> Builder(CI);
  Value *DxilOp = trivialDxilOperation(Opcode, Args, Ty, Ty, HlslOp, Builder);

  return DxilOp;
}

Value *translateGetRtSamplePos(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                               HLOperationLowerHelper &Helper,
                               HLObjectOperationLowerHelper *PObjHelper,
                               bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  OP::OpCode Opcode = OP::OpCode::RenderTargetGetSamplePosition;
  IRBuilder<> Builder(CI);

  Type *Ty = Type::getVoidTy(CI->getContext());
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);

  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Value *Args[] = {OpArg, Val};

  Value *SamplePos =
      trivialDxilOperation(Opcode, Args, Ty, Ty, HlslOp, Builder);

  Value *Result = UndefValue::get(CI->getType());
  Value *SamplePosX = Builder.CreateExtractValue(SamplePos, 0);
  Value *SamplePosY = Builder.CreateExtractValue(SamplePos, 1);
  Result = Builder.CreateInsertElement(Result, SamplePosX, (uint64_t)0);
  Result = Builder.CreateInsertElement(Result, SamplePosY, 1);
  return Result;
}

// val QuadReadLaneAt(val, uint);
Value *translateQuadReadLaneAt(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                               HLOperationLowerHelper &Helper,
                               HLObjectOperationLowerHelper *PObjHelper,
                               bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *RefArgs[] = {nullptr, CI->getOperand(1), CI->getOperand(2)};
  return trivialDxilOperation(DXIL::OpCode::QuadReadLaneAt, RefArgs,
                              CI->getOperand(1)->getType(), CI, HlslOp);
}

// Quad intrinsics of the form fn(val,QuadOpKind)->val
Value *translateQuadAnyAll(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                           HLOperationLowerHelper &Helper,
                           HLObjectOperationLowerHelper *PObjHelper,
                           bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  DXIL::QuadVoteOpKind OpKind;
  switch (IOP) {
  case IntrinsicOp::IOP_QuadAll:
    OpKind = DXIL::QuadVoteOpKind::All;
    break;
  case IntrinsicOp::IOP_QuadAny:
    OpKind = DXIL::QuadVoteOpKind::Any;
    break;
  default:
    llvm_unreachable(
        "QuadAny/QuadAll translation called with wrong isntruction");
  }
  Constant *OpArg = HlslOp->GetI8Const((unsigned)OpKind);
  Value *RefArgs[] = {nullptr, CI->getOperand(1), OpArg};
  return trivialDxilOperation(DXIL::OpCode::QuadVote, RefArgs,
                              CI->getOperand(1)->getType(), CI, HlslOp);
}

// Wave intrinsics of the form fn(val,QuadOpKind)->val
Value *translateQuadReadAcross(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                               HLOperationLowerHelper &Helper,
                               HLObjectOperationLowerHelper *PObjHelper,
                               bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  DXIL::QuadOpKind OpKind;
  switch (IOP) {
  case IntrinsicOp::IOP_QuadReadAcrossX:
    OpKind = DXIL::QuadOpKind::ReadAcrossX;
    break;
  case IntrinsicOp::IOP_QuadReadAcrossY:
    OpKind = DXIL::QuadOpKind::ReadAcrossY;
    break;
  default:
    DXASSERT_NOMSG(IOP == IntrinsicOp::IOP_QuadReadAcrossDiagonal);
    LLVM_FALLTHROUGH;
  case IntrinsicOp::IOP_QuadReadAcrossDiagonal:
    OpKind = DXIL::QuadOpKind::ReadAcrossDiagonal;
    break;
  }
  Constant *OpArg = HlslOp->GetI8Const((unsigned)OpKind);
  Value *RefArgs[] = {nullptr, CI->getOperand(1), OpArg};
  return trivialDxilOperation(DXIL::OpCode::QuadOp, RefArgs,
                              CI->getOperand(1)->getType(), CI, HlslOp);
}

// WaveAllEqual(val<n>)->bool<n>
Value *translateWaveAllEqual(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                             HLOperationLowerHelper &Helper,
                             HLObjectOperationLowerHelper *PObjHelper,
                             bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src = CI->getArgOperand(HLOperandIndex::kWaveAllEqualValueOpIdx);
  IRBuilder<> Builder(CI);

  Type *Ty = Src->getType();
  Type *RetTy = Type::getInt1Ty(CI->getContext());
  if (Ty->isVectorTy())
    RetTy = VectorType::get(RetTy, Ty->getVectorNumElements());

  Constant *OpArg =
      HlslOp->GetU32Const((unsigned)DXIL::OpCode::WaveActiveAllEqual);
  Value *Args[] = {OpArg, Src};

  return trivialDxilOperation(DXIL::OpCode::WaveActiveAllEqual, Args, Ty, RetTy,
                              HlslOp, Builder);
}

static Value *translateWaveMatchFixReturn(IRBuilder<> &Builder, Type *TargetTy,
                                          Value *RetVal) {
  Value *ResVec = UndefValue::get(TargetTy);
  for (unsigned I = 0; I != 4; ++I) {
    Value *Elt = Builder.CreateExtractValue(RetVal, I);
    ResVec = Builder.CreateInsertElement(ResVec, Elt, I);
  }

  return ResVec;
}

// WaveMatch(val<n>)->uint4
Value *translateWaveMatch(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opc,
                          HLOperationLowerHelper &Helper,
                          HLObjectOperationLowerHelper *ObjHelper,
                          bool &Translated) {
  hlsl::OP *Op = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  Value *Val = CI->getArgOperand(1);
  Type *ValTy = Val->getType();
  Type *EltTy = ValTy->getScalarType();
  Constant *OpcArg = Op->GetU32Const((unsigned)DXIL::OpCode::WaveMatch);

  // If we don't need to scalarize, just emit the call and exit
  const bool Scalarize =
      ValTy->isVectorTy() &&
      !Op->GetModule()->GetHLModule().GetShaderModel()->IsSM69Plus();
  if (!Scalarize) {
    Value *Fn = Op->GetOpFunc(OP::OpCode::WaveMatch, ValTy);
    Value *Args[] = {OpcArg, Val};
    Value *Ret = Builder.CreateCall(Fn, Args);
    return translateWaveMatchFixReturn(Builder, CI->getType(), Ret);
  }

  // Generate a dx.op.waveMatch call for each scalar in the input, and perform
  // a bitwise AND between each result to derive the final bitmask

  // (1) Collect the list of all scalar inputs (e.g. decompose vectors)
  SmallVector<Value *, 4> ScalarInputs;

  for (uint64_t I = 0, E = ValTy->getVectorNumElements(); I != E; ++I) {
    Value *Elt = Builder.CreateExtractElement(Val, I);
    ScalarInputs.push_back(Elt);
  }

  // (2) For each scalar, emit a call to dx.op.waveMatch. If this is not the
  // first scalar, then AND the result with the accumulator.
  Value *Fn = Op->GetOpFunc(OP::OpCode::WaveMatch, EltTy);
  Value *Args[] = {OpcArg, ScalarInputs[0]};
  Value *Res = Builder.CreateCall(Fn, Args);

  for (unsigned I = 1, E = ScalarInputs.size(); I != E; ++I) {
    Value *Args[] = {OpcArg, ScalarInputs[I]};
    Value *Call = Builder.CreateCall(Fn, Args);

    // Generate bitwise AND of the components
    for (unsigned J = 0; J != 4; ++J) {
      Value *ResVal = Builder.CreateExtractValue(Res, J);
      Value *CallVal = Builder.CreateExtractValue(Call, J);
      Value *And = Builder.CreateAnd(ResVal, CallVal);
      Res = Builder.CreateInsertValue(Res, And, J);
    }
  }

  // (3) Convert the final aggregate into a vector to make the types match
  return translateWaveMatchFixReturn(Builder, CI->getType(), Res);
}

// Wave intrinsics of the form fn(valA)->valB, where no overloading takes place
Value *translateWaveA2B(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                        HLOperationLowerHelper &Helper,
                        HLObjectOperationLowerHelper *PObjHelper,
                        bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *RefArgs[] = {nullptr, CI->getOperand(1)};
  return trivialDxilOperation(Opcode, RefArgs, Helper.VoidTy, CI, HlslOp);
}
// Wave ballot intrinsic.
Value *translateWaveBallot(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                           HLOperationLowerHelper &Helper,
                           HLObjectOperationLowerHelper *PObjHelper,
                           bool &Translated) {
  // The high-level operation is uint4 ballot(i1).
  // The DXIL operation is struct.u4 ballot(i1).
  // To avoid updating users with more than a simple replace, we translate into
  // a call into struct.u4, then reassemble the vector.
  // Scalarization and constant propagation take care of cleanup.
  IRBuilder<> B(CI);

  // Make the DXIL call itself.
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Value *RefArgs[] = {OpArg, CI->getOperand(1)};
  Function *DxilFunc =
      HlslOp->GetOpFunc(Opcode, Type::getVoidTy(CI->getContext()));
  Value *DxilVal =
      B.CreateCall(DxilFunc, RefArgs, HlslOp->GetOpCodeName(Opcode));

  // Assign from the call results into a vector.
  Type *ResTy = CI->getType();
  DXASSERT_NOMSG(ResTy->isVectorTy() && ResTy->getVectorNumElements() == 4);
  DXASSERT_NOMSG(DxilVal->getType()->isStructTy() &&
                 DxilVal->getType()->getNumContainedTypes() == 4);

  // 'x' component is the first vector element, highest bits.
  Value *ResVal = llvm::UndefValue::get(ResTy);
  for (unsigned Idx = 0; Idx < 4; ++Idx) {
    ResVal = B.CreateInsertElement(
        ResVal, B.CreateExtractValue(DxilVal, ArrayRef<unsigned>(Idx)), Idx);
  }

  return ResVal;
}

static bool waveIntrinsicNeedsSign(OP::OpCode Opcode) {
  return Opcode == OP::OpCode::WaveActiveOp ||
         Opcode == OP::OpCode::WavePrefixOp;
}

static unsigned waveIntrinsicToSignedOpKind(IntrinsicOp IOP) {
  if (IOP == IntrinsicOp::IOP_WaveActiveUMax ||
      IOP == IntrinsicOp::IOP_WaveActiveUMin ||
      IOP == IntrinsicOp::IOP_WaveActiveUSum ||
      IOP == IntrinsicOp::IOP_WaveActiveUProduct ||
      IOP == IntrinsicOp::IOP_WaveMultiPrefixUProduct ||
      IOP == IntrinsicOp::IOP_WaveMultiPrefixUSum ||
      IOP == IntrinsicOp::IOP_WavePrefixUSum ||
      IOP == IntrinsicOp::IOP_WavePrefixUProduct)
    return (unsigned)DXIL::SignedOpKind::Unsigned;
  return (unsigned)DXIL::SignedOpKind::Signed;
}

static unsigned waveIntrinsicToOpKind(IntrinsicOp IOP) {
  switch (IOP) {
  // Bit operations.
  case IntrinsicOp::IOP_WaveActiveBitOr:
    return (unsigned)DXIL::WaveBitOpKind::Or;
  case IntrinsicOp::IOP_WaveActiveBitAnd:
    return (unsigned)DXIL::WaveBitOpKind::And;
  case IntrinsicOp::IOP_WaveActiveBitXor:
    return (unsigned)DXIL::WaveBitOpKind::Xor;
  // Prefix operations.
  case IntrinsicOp::IOP_WavePrefixSum:
  case IntrinsicOp::IOP_WavePrefixUSum:
    return (unsigned)DXIL::WaveOpKind::Sum;
  case IntrinsicOp::IOP_WavePrefixProduct:
  case IntrinsicOp::IOP_WavePrefixUProduct:
    return (unsigned)DXIL::WaveOpKind::Product;
    // Numeric operations.
  case IntrinsicOp::IOP_WaveActiveMax:
  case IntrinsicOp::IOP_WaveActiveUMax:
    return (unsigned)DXIL::WaveOpKind::Max;
  case IntrinsicOp::IOP_WaveActiveMin:
  case IntrinsicOp::IOP_WaveActiveUMin:
    return (unsigned)DXIL::WaveOpKind::Min;
  case IntrinsicOp::IOP_WaveActiveSum:
  case IntrinsicOp::IOP_WaveActiveUSum:
    return (unsigned)DXIL::WaveOpKind::Sum;
  case IntrinsicOp::IOP_WaveActiveProduct:
  case IntrinsicOp::IOP_WaveActiveUProduct:
  // MultiPrefix operations
  case IntrinsicOp::IOP_WaveMultiPrefixBitAnd:
    return (unsigned)DXIL::WaveMultiPrefixOpKind::And;
  case IntrinsicOp::IOP_WaveMultiPrefixBitOr:
    return (unsigned)DXIL::WaveMultiPrefixOpKind::Or;
  case IntrinsicOp::IOP_WaveMultiPrefixBitXor:
    return (unsigned)DXIL::WaveMultiPrefixOpKind::Xor;
  case IntrinsicOp::IOP_WaveMultiPrefixProduct:
  case IntrinsicOp::IOP_WaveMultiPrefixUProduct:
    return (unsigned)DXIL::WaveMultiPrefixOpKind::Product;
  case IntrinsicOp::IOP_WaveMultiPrefixSum:
  case IntrinsicOp::IOP_WaveMultiPrefixUSum:
    return (unsigned)DXIL::WaveMultiPrefixOpKind::Sum;
  default:
    DXASSERT(IOP == IntrinsicOp::IOP_WaveActiveProduct ||
                 IOP == IntrinsicOp::IOP_WaveActiveUProduct,
             "else caller passed incorrect value");
    return (unsigned)DXIL::WaveOpKind::Product;
  }
}

// Wave intrinsics of the form fn(valA)->valA
Value *translateWaveA2A(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                        HLOperationLowerHelper &Helper,
                        HLObjectOperationLowerHelper *PObjHelper,
                        bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;

  Constant *KindValInt = HlslOp->GetI8Const(waveIntrinsicToOpKind(IOP));
  Constant *SignValInt = HlslOp->GetI8Const(waveIntrinsicToSignedOpKind(IOP));
  Value *RefArgs[] = {nullptr, CI->getOperand(1), KindValInt, SignValInt};
  unsigned RefArgCount = _countof(RefArgs);
  if (!waveIntrinsicNeedsSign(Opcode))
    RefArgCount--;
  return trivialDxilOperation(Opcode,
                              llvm::ArrayRef<Value *>(RefArgs, RefArgCount),
                              CI->getOperand(1)->getType(), CI, HlslOp);
}

// WaveMultiPrefixOP(val<n>, mask) -> val<n>
Value *translateWaveMultiPrefix(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opc,
                                HLOperationLowerHelper &Helper,
                                HLObjectOperationLowerHelper *ObjHelper,
                                bool &Translated) {
  hlsl::OP *Op = &Helper.HlslOp;

  Constant *KindValInt = Op->GetI8Const(waveIntrinsicToOpKind(IOP));
  Constant *SignValInt = Op->GetI8Const(waveIntrinsicToSignedOpKind(IOP));

  // Decompose mask into scalars
  IRBuilder<> Builder(CI);
  Value *Mask = CI->getArgOperand(2);
  Value *Mask0 = Builder.CreateExtractElement(Mask, (uint64_t)0);
  Value *Mask1 = Builder.CreateExtractElement(Mask, (uint64_t)1);
  Value *Mask2 = Builder.CreateExtractElement(Mask, (uint64_t)2);
  Value *Mask3 = Builder.CreateExtractElement(Mask, (uint64_t)3);

  Value *Args[] = {nullptr, CI->getOperand(1), Mask0,     Mask1, Mask2,
                   Mask3,   KindValInt,        SignValInt};

  return trivialDxilOperation(Opc, Args, CI->getOperand(1)->getType(), CI, Op);
}

// WaveMultiPrefixBitCount(i1, mask) -> i32
Value *translateWaveMultiPrefixBitCount(CallInst *CI, IntrinsicOp IOP,
                                        OP::OpCode Opc,
                                        HLOperationLowerHelper &Helper,
                                        HLObjectOperationLowerHelper *ObjHelper,
                                        bool &Translated) {
  hlsl::OP *Op = &Helper.HlslOp;

  // Decompose mask into scalars
  IRBuilder<> Builder(CI);
  Value *Mask = CI->getArgOperand(2);
  Value *Mask0 = Builder.CreateExtractElement(Mask, (uint64_t)0);
  Value *Mask1 = Builder.CreateExtractElement(Mask, (uint64_t)1);
  Value *Mask2 = Builder.CreateExtractElement(Mask, (uint64_t)2);
  Value *Mask3 = Builder.CreateExtractElement(Mask, (uint64_t)3);

  Value *Args[] = {nullptr, CI->getOperand(1), Mask0, Mask1, Mask2, Mask3};

  return trivialDxilOperation(Opc, Args, Helper.VoidTy, CI, Op);
}

// Wave intrinsics of the form fn()->val
Value *translateWaveToVal(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                          HLOperationLowerHelper &Helper,
                          HLObjectOperationLowerHelper *PObjHelper,
                          bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *RefArgs[] = {nullptr};
  return trivialDxilOperation(Opcode, RefArgs, Helper.VoidTy, CI, HlslOp);
}

// Wave intrinsics of the form fn(val,lane)->val
Value *translateWaveReadLaneAt(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                               HLOperationLowerHelper &Helper,
                               HLObjectOperationLowerHelper *PObjHelper,
                               bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *RefArgs[] = {nullptr, CI->getOperand(1), CI->getOperand(2)};
  return trivialDxilOperation(DXIL::OpCode::WaveReadLaneAt, RefArgs,
                              CI->getOperand(1)->getType(), CI, HlslOp);
}

// Wave intrinsics of the form fn(val)->val
Value *translateWaveReadLaneFirst(CallInst *CI, IntrinsicOp IOP,
                                  OP::OpCode Opcode,
                                  HLOperationLowerHelper &Helper,
                                  HLObjectOperationLowerHelper *PObjHelper,
                                  bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *RefArgs[] = {nullptr, CI->getOperand(1)};
  return trivialDxilOperation(DXIL::OpCode::WaveReadLaneFirst, RefArgs,
                              CI->getOperand(1)->getType(), CI, HlslOp);
}

Value *translateAbs(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Type *POverloadTy = CI->getType()->getScalarType();
  if (POverloadTy->isFloatingPointTy()) {
    Value *RefArgs[] = {nullptr, CI->getOperand(1)};
    return trivialDxilOperation(DXIL::OpCode::FAbs, RefArgs, CI->getType(), CI,
                                HlslOp);
  }

  Value *Src = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  IRBuilder<> Builder(CI);
  Value *Neg = Builder.CreateNeg(Src);
  return trivialDxilBinaryOperation(DXIL::OpCode::IMax, Src, Neg, HlslOp,
                                    Builder);
}

Value *translateUAbs(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                     HLOperationLowerHelper &Helper,
                     HLObjectOperationLowerHelper *PObjHelper,
                     bool &Translated) {
  return CI->getOperand(HLOperandIndex::kUnaryOpSrc0Idx); // No-op
}

Value *generateVectorCmpNeZero(Value *Val, IRBuilder<> Builder) {
  Type *Ty = Val->getType();
  Type *EltTy = Ty->getScalarType();

  Value *ZeroInit = ConstantAggregateZero::get(Ty);

  if (EltTy->isFloatingPointTy())
    return Builder.CreateFCmpUNE(Val, ZeroInit);

  return Builder.CreateICmpNE(Val, ZeroInit);
}

Value *generateCmpNeZero(Value *Val, IRBuilder<> Builder) {
  Type *Ty = Val->getType();
  Type *EltTy = Ty->getScalarType();

  Constant *Zero = nullptr;
  if (EltTy->isFloatingPointTy())
    Zero = ConstantFP::get(EltTy, 0);
  else
    Zero = ConstantInt::get(EltTy, 0);

  if (Ty != EltTy)
    Zero = ConstantVector::getSplat(Ty->getVectorNumElements(), Zero);

  if (EltTy->isFloatingPointTy())
    return Builder.CreateFCmpUNE(Val, Zero);

  return Builder.CreateICmpNE(Val, Zero);
}

Value *translateBitwisePredicate(CallInst *CI, IntrinsicOp IOP,
                                 hlsl::OP *HlslOP) {
  Value *Arg = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  IRBuilder<> Builder(CI);

  Type *Ty = Arg->getType();
  Type *EltTy = Ty->getScalarType();

  if (Ty == EltTy)
    return generateCmpNeZero(Arg, Builder);

  if (HlslOP->GetModule()->GetHLModule().GetShaderModel()->IsSM69Plus()) {
    DXIL::OpCode ReduceOp = DXIL::OpCode::VectorReduceAnd;
    switch (IOP) {
    case IntrinsicOp::IOP_all:
      ReduceOp = DXIL::OpCode::VectorReduceAnd;
      break;
    case IntrinsicOp::IOP_any:
      ReduceOp = DXIL::OpCode::VectorReduceOr;
      break;
    default:
      assert(false && "Unexpected reduction IOP");
      break;
    }

    // Compare each element to zero
    Value *VecCmpZero = generateVectorCmpNeZero(Arg, Builder);
    Type *VecCmpTy = VecCmpZero->getType();

    // Reduce the vector with the appropiate op
    Constant *OpArg = HlslOP->GetU32Const((unsigned)ReduceOp);
    Value *Args[] = {OpArg, VecCmpZero};
    Function *DxilFunc = HlslOP->GetOpFunc(ReduceOp, VecCmpTy);
    return trivialDxilVectorOperation(DxilFunc, ReduceOp, Args, VecCmpTy,
                                      HlslOP, Builder);
  }

  SmallVector<Value *, 4> EltIsNEZero;
  for (unsigned I = 0; I < Ty->getVectorNumElements(); I++) {
    Value *Elt = Builder.CreateExtractElement(Arg, I);
    Elt = generateCmpNeZero(Elt, Builder);
    EltIsNEZero.push_back(Elt);
  }

  // and/or the components together
  Value *Reduce = EltIsNEZero[0];
  for (unsigned I = 1; I < EltIsNEZero.size(); I++) {
    Value *Elt = EltIsNEZero[I];
    switch (IOP) {
    case IntrinsicOp::IOP_all:
      Reduce = Builder.CreateAnd(Reduce, Elt);
      break;
    case IntrinsicOp::IOP_any:
      Reduce = Builder.CreateOr(Reduce, Elt);
      break;
    default:
      assert(false && "Unexpected reduction IOP");
      break;
    }
  }

  return Reduce;
}

Value *translateAll(CallInst *CI, IntrinsicOp IOP, OP::OpCode OpCode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  return translateBitwisePredicate(CI, IOP, &Helper.HlslOp);
}

Value *translateAny(CallInst *CI, IntrinsicOp IOP, OP::OpCode OpCode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  return translateBitwisePredicate(CI, IOP, &Helper.HlslOp);
}

Value *translateBitcast(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                        HLOperationLowerHelper &Helper,
                        HLObjectOperationLowerHelper *PObjHelper,
                        bool &Translated) {
  Type *Ty = CI->getType();
  Value *Op = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  IRBuilder<> Builder(CI);
  return Builder.CreateBitCast(Op, Ty);
}

Value *translateDoubleAsUint(Value *X, Value *Lo, Value *Hi,
                             IRBuilder<> &Builder, hlsl::OP *HlslOp) {
  Type *Ty = X->getType();
  Type *OutTy = Lo->getType()->getPointerElementType();
  DXIL::OpCode Opcode = DXIL::OpCode::SplitDouble;

  Function *DxilFunc = HlslOp->GetOpFunc(Opcode, Ty->getScalarType());
  Value *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(Opcode));

  if (Ty->isVectorTy()) {
    Value *RetValLo = llvm::UndefValue::get(OutTy);
    Value *RetValHi = llvm::UndefValue::get(OutTy);
    unsigned VecSize = Ty->getVectorNumElements();

    for (unsigned I = 0; I < VecSize; I++) {
      Value *Elt = Builder.CreateExtractElement(X, I);
      Value *EltOP = Builder.CreateCall(DxilFunc, {OpArg, Elt},
                                        HlslOp->GetOpCodeName(Opcode));
      Value *EltLo = Builder.CreateExtractValue(EltOP, 0);
      RetValLo = Builder.CreateInsertElement(RetValLo, EltLo, I);
      Value *EltHi = Builder.CreateExtractValue(EltOP, 1);
      RetValHi = Builder.CreateInsertElement(RetValHi, EltHi, I);
    }
    Builder.CreateStore(RetValLo, Lo);
    Builder.CreateStore(RetValHi, Hi);
  } else {
    Value *RetVal =
        Builder.CreateCall(DxilFunc, {OpArg, X}, HlslOp->GetOpCodeName(Opcode));
    Value *RetValLo = Builder.CreateExtractValue(RetVal, 0);
    Value *RetValHi = Builder.CreateExtractValue(RetVal, 1);
    Builder.CreateStore(RetValLo, Lo);
    Builder.CreateStore(RetValHi, Hi);
  }

  return nullptr;
}

Value *translateAsUint(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                       HLOperationLowerHelper &Helper,
                       HLObjectOperationLowerHelper *PObjHelper,
                       bool &Translated) {
  if (CI->getNumArgOperands() == 2)
    return translateBitcast(CI, IOP, Opcode, Helper, PObjHelper, Translated);

  DXASSERT_NOMSG(CI->getNumArgOperands() == 4);
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *X = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  DXASSERT_NOMSG(X->getType()->getScalarType()->isDoubleTy());
  Value *Lo = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx);
  Value *Hi = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx);
  IRBuilder<> Builder(CI);
  return translateDoubleAsUint(X, Lo, Hi, Builder, HlslOp);
}

Value *translateAsDouble(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                         HLOperationLowerHelper &Helper,
                         HLObjectOperationLowerHelper *PObjHelper,
                         bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *X = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Y = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);

  Value *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(Opcode));
  IRBuilder<> Builder(CI);
  return trivialDxilOperation(Opcode, {OpArg, X, Y}, CI->getType(),
                              CI->getType(), HlslOp, Builder);
}

Value *translateAtan2(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                      HLOperationLowerHelper &Helper,
                      HLObjectOperationLowerHelper *PObjHelper,
                      bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Y = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *X = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);

  IRBuilder<> Builder(CI);
  Value *Tan = Builder.CreateFDiv(Y, X);

  Value *Atan =
      trivialDxilUnaryOperation(OP::OpCode::Atan, Tan, HlslOp, Builder);
  // Modify atan result based on https://en.wikipedia.org/wiki/Atan2.
  Type *Ty = X->getType();
  Constant *Pi = ConstantFP::get(Ty->getScalarType(), M_PI);
  Constant *HalfPi = ConstantFP::get(Ty->getScalarType(), M_PI / 2);
  Constant *NegHalfPi = ConstantFP::get(Ty->getScalarType(), -M_PI / 2);
  Constant *Zero = ConstantFP::get(Ty->getScalarType(), 0);
  if (Ty->isVectorTy()) {
    unsigned VecSize = Ty->getVectorNumElements();
    Pi = ConstantVector::getSplat(VecSize, Pi);
    HalfPi = ConstantVector::getSplat(VecSize, HalfPi);
    NegHalfPi = ConstantVector::getSplat(VecSize, NegHalfPi);
    Zero = ConstantVector::getSplat(VecSize, Zero);
  }
  Value *AtanAddPi = Builder.CreateFAdd(Atan, Pi);
  Value *AtanSubPi = Builder.CreateFSub(Atan, Pi);

  // x > 0 -> atan.
  Value *Result = Atan;
  Value *XLt0 = Builder.CreateFCmpOLT(X, Zero);
  Value *XEq0 = Builder.CreateFCmpOEQ(X, Zero);

  Value *YGe0 = Builder.CreateFCmpOGE(Y, Zero);
  Value *YLt0 = Builder.CreateFCmpOLT(Y, Zero);
  // x < 0, y >= 0 -> atan + pi.
  Value *XLt0AndyGe0 = Builder.CreateAnd(XLt0, YGe0);
  Result = Builder.CreateSelect(XLt0AndyGe0, AtanAddPi, Result);

  // x < 0, y < 0 -> atan - pi.
  Value *XLt0AndYLt0 = Builder.CreateAnd(XLt0, YLt0);
  Result = Builder.CreateSelect(XLt0AndYLt0, AtanSubPi, Result);

  // x == 0, y < 0 -> -pi/2
  Value *XEq0AndYLt0 = Builder.CreateAnd(XEq0, YLt0);
  Result = Builder.CreateSelect(XEq0AndYLt0, NegHalfPi, Result);
  // x == 0, y > 0 -> pi/2
  Value *XEq0AndYGe0 = Builder.CreateAnd(XEq0, YGe0);
  Result = Builder.CreateSelect(XEq0AndYGe0, HalfPi, Result);

  return Result;
}

Value *translateClamp(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                      HLOperationLowerHelper &Helper,
                      HLObjectOperationLowerHelper *PObjHelper,
                      bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Type *Ty = CI->getType();
  Type *EltTy = Ty->getScalarType();
  DXIL::OpCode MaxOp = DXIL::OpCode::FMax;
  DXIL::OpCode MinOp = DXIL::OpCode::FMin;
  if (IOP == IntrinsicOp::IOP_uclamp) {
    MaxOp = DXIL::OpCode::UMax;
    MinOp = DXIL::OpCode::UMin;
  } else if (EltTy->isIntegerTy()) {
    MaxOp = DXIL::OpCode::IMax;
    MinOp = DXIL::OpCode::IMin;
  }

  Value *X = CI->getArgOperand(HLOperandIndex::kClampOpXIdx);
  Value *MaxVal = CI->getArgOperand(HLOperandIndex::kClampOpMaxIdx);
  Value *MinVal = CI->getArgOperand(HLOperandIndex::kClampOpMinIdx);

  IRBuilder<> Builder(CI);
  // min(max(x, minVal), maxVal).
  Value *MaxXMinVal =
      trivialDxilBinaryOperation(MaxOp, X, MinVal, HlslOp, Builder);
  return trivialDxilBinaryOperation(MinOp, MaxXMinVal, MaxVal, HlslOp, Builder);
}

Value *translateClip(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                     HLOperationLowerHelper &Helper,
                     HLObjectOperationLowerHelper *PObjHelper,
                     bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Function *Discard =
      HlslOp->GetOpFunc(OP::OpCode::Discard, Type::getVoidTy(CI->getContext()));
  IRBuilder<> Builder(CI);
  Value *Cond = nullptr;
  Value *Arg = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  if (VectorType *VT = dyn_cast<VectorType>(Arg->getType())) {
    Value *Elt = Builder.CreateExtractElement(Arg, (uint64_t)0);
    Cond = Builder.CreateFCmpOLT(Elt, HlslOp->GetFloatConst(0));
    for (unsigned I = 1; I < VT->getNumElements(); I++) {
      Value *Elt = Builder.CreateExtractElement(Arg, I);
      Value *EltCond = Builder.CreateFCmpOLT(Elt, HlslOp->GetFloatConst(0));
      Cond = Builder.CreateOr(Cond, EltCond);
    }
  } else
    Cond = Builder.CreateFCmpOLT(Arg, HlslOp->GetFloatConst(0));

  /*If discard condition evaluates to false at compile-time, then
  don't emit the discard instruction.*/
  if (ConstantInt *ConstCond = dyn_cast<ConstantInt>(Cond))
    if (!ConstCond->getLimitedValue())
      return nullptr;

  Constant *OpArg = HlslOp->GetU32Const((unsigned)OP::OpCode::Discard);
  Builder.CreateCall(Discard, {OpArg, Cond});
  return nullptr;
}

Value *translateCross(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                      HLOperationLowerHelper &Helper,
                      HLObjectOperationLowerHelper *PObjHelper,
                      bool &Translated) {
  VectorType *VT = cast<VectorType>(CI->getType());
  DXASSERT_NOMSG(VT->getNumElements() == 3);

  Value *Op0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Op1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);

  IRBuilder<> Builder(CI);
  Value *Op0X = Builder.CreateExtractElement(Op0, (uint64_t)0);
  Value *Op0Y = Builder.CreateExtractElement(Op0, 1);
  Value *Op0Z = Builder.CreateExtractElement(Op0, 2);

  Value *Op1X = Builder.CreateExtractElement(Op1, (uint64_t)0);
  Value *Op1Y = Builder.CreateExtractElement(Op1, 1);
  Value *Op1Z = Builder.CreateExtractElement(Op1, 2);

  auto MulSub = [&](Value *X0, Value *Y0, Value *X1, Value *Y1) -> Value * {
    Value *Xy = Builder.CreateFMul(X0, Y1);
    Value *Yx = Builder.CreateFMul(Y0, X1);
    return Builder.CreateFSub(Xy, Yx);
  };

  Value *YzZy = MulSub(Op0Y, Op0Z, Op1Y, Op1Z);
  Value *ZxXz = MulSub(Op0Z, Op0X, Op1Z, Op1X);
  Value *XyYx = MulSub(Op0X, Op0Y, Op1X, Op1Y);

  Value *Cross = UndefValue::get(VT);
  Cross = Builder.CreateInsertElement(Cross, YzZy, (uint64_t)0);
  Cross = Builder.CreateInsertElement(Cross, ZxXz, 1);
  Cross = Builder.CreateInsertElement(Cross, XyYx, 2);
  return Cross;
}

Value *translateDegrees(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                        HLOperationLowerHelper &Helper,
                        HLObjectOperationLowerHelper *PObjHelper,
                        bool &Translated) {
  IRBuilder<> Builder(CI);
  Type *Ty = CI->getType();
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  // 180/pi.
  Constant *ToDegreeConst = ConstantFP::get(Ty->getScalarType(), 180 / M_PI);
  if (Ty != Ty->getScalarType()) {
    ToDegreeConst =
        ConstantVector::getSplat(Ty->getVectorNumElements(), ToDegreeConst);
  }
  return Builder.CreateFMul(ToDegreeConst, Val);
}

Value *translateDst(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Src1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  Type *Ty = Src1->getType();
  IRBuilder<> Builder(CI);
  Value *Result = UndefValue::get(Ty);
  Constant *OneConst = ConstantFP::get(Ty->getScalarType(), 1);
  // dest.x = 1;
  Result = Builder.CreateInsertElement(Result, OneConst, (uint64_t)0);
  // dest.y = src0.y * src1.y;
  Value *Src0Y = Builder.CreateExtractElement(Src0, 1);
  Value *Src1Y = Builder.CreateExtractElement(Src1, 1);
  Value *YMuly = Builder.CreateFMul(Src0Y, Src1Y);
  Result = Builder.CreateInsertElement(Result, YMuly, 1);
  // dest.z = src0.z;
  Value *Src0Z = Builder.CreateExtractElement(Src0, 2);
  Result = Builder.CreateInsertElement(Result, Src0Z, 2);
  // dest.w = src1.w;
  Value *Src1W = Builder.CreateExtractElement(Src1, 3);
  Result = Builder.CreateInsertElement(Result, Src1W, 3);
  return Result;
}

Value *translateFirstbitHi(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                           HLOperationLowerHelper &Helper,
                           HLObjectOperationLowerHelper *PObjHelper,
                           bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  IRBuilder<> Builder(CI);
  Value *Src = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);

  Type *Ty = Src->getType();
  Type *RetTy = Type::getInt32Ty(CI->getContext());
  unsigned NumElements = 0;
  if (Ty->isVectorTy()) {
    NumElements = Ty->getVectorNumElements();
    RetTy = VectorType::get(RetTy, NumElements);
  }

  Constant *OpArg = OP->GetU32Const((unsigned)Opcode);
  Value *Args[] = {OpArg, Src};

  Value *FirstbitHi =
      trivialDxilOperation(Opcode, Args, Ty, RetTy, OP, Builder);

  IntegerType *EltTy = cast<IntegerType>(Ty->getScalarType());
  Constant *Neg1 = Builder.getInt32(-1);
  Constant *BitWidth = Builder.getInt32(EltTy->getBitWidth() - 1);

  if (NumElements > 0) {
    Neg1 = ConstantVector::getSplat(NumElements, Neg1);
    BitWidth = ConstantVector::getSplat(NumElements, BitWidth);
  }

  Value *Sub = Builder.CreateSub(BitWidth, FirstbitHi);
  Value *Cond = Builder.CreateICmpEQ(Neg1, FirstbitHi);
  return Builder.CreateSelect(Cond, Neg1, Sub);
}

Value *translateFirstbitLo(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                           HLOperationLowerHelper &Helper,
                           HLObjectOperationLowerHelper *PObjHelper,
                           bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  IRBuilder<> Builder(CI);
  Value *Src = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);

  Type *Ty = Src->getType();
  Type *RetTy = Type::getInt32Ty(CI->getContext());
  if (Ty->isVectorTy())
    RetTy = VectorType::get(RetTy, Ty->getVectorNumElements());

  Constant *OpArg = OP->GetU32Const((unsigned)Opcode);
  Value *Args[] = {OpArg, Src};

  Value *FirstbitLo =
      trivialDxilOperation(Opcode, Args, Ty, RetTy, OP, Builder);

  return FirstbitLo;
}

Value *translateLit(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  Value *NDotL = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  Value *NDotH = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx);
  Value *M = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx);
  IRBuilder<> Builder(CI);

  Type *Ty = M->getType();
  Value *Result = UndefValue::get(VectorType::get(Ty, 4));
  // Result = (ambient, diffuse, specular, 1)
  // ambient = 1.
  Constant *OneConst = ConstantFP::get(Ty, 1);
  Result = Builder.CreateInsertElement(Result, OneConst, (uint64_t)0);
  // Result.w = 1.
  Result = Builder.CreateInsertElement(Result, OneConst, 3);
  // diffuse = (n_dot_l < 0) ? 0 : n_dot_l.
  Constant *ZeroConst = ConstantFP::get(Ty, 0);
  Value *NlCmp = Builder.CreateFCmpOLT(NDotL, ZeroConst);
  Value *Diffuse = Builder.CreateSelect(NlCmp, ZeroConst, NDotL);
  Result = Builder.CreateInsertElement(Result, Diffuse, 1);
  // specular = ((n_dot_l < 0) || (n_dot_h < 0)) ? 0: (n_dot_h ^ m).
  Value *NhCmp = Builder.CreateFCmpOLT(NDotH, ZeroConst);
  Value *SpecCond = Builder.CreateOr(NlCmp, NhCmp);
  bool IsFxcCompatMode =
      CI->getModule()->GetHLModule().GetHLOptions().bFXCCompatMode;
  Value *NhPowM =
      translatePowImpl(&Helper.HlslOp, Builder, NDotH, M, IsFxcCompatMode);
  Value *Spec = Builder.CreateSelect(SpecCond, ZeroConst, NhPowM);
  Result = Builder.CreateInsertElement(Result, Spec, 2);
  return Result;
}

Value *translateRadians(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                        HLOperationLowerHelper &Helper,
                        HLObjectOperationLowerHelper *PObjHelper,
                        bool &Translated) {
  IRBuilder<> Builder(CI);
  Type *Ty = CI->getType();
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  // pi/180.
  Constant *ToRadianConst = ConstantFP::get(Ty->getScalarType(), M_PI / 180);
  if (Ty != Ty->getScalarType()) {
    ToRadianConst =
        ConstantVector::getSplat(Ty->getVectorNumElements(), ToRadianConst);
  }
  return Builder.CreateFMul(ToRadianConst, Val);
}

Value *translateF16ToF32(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                         HLOperationLowerHelper &Helper,
                         HLObjectOperationLowerHelper *PObjHelper,
                         bool &Translated) {
  IRBuilder<> Builder(CI);

  Value *X = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Type *Ty = CI->getType();

  Function *F16tof32 = Helper.HlslOp.GetOpFunc(Opcode, Helper.VoidTy);
  return trivialDxilOperation(
      F16tof32, Opcode, {Builder.getInt32(static_cast<unsigned>(Opcode)), X},
      X->getType(), Ty, &Helper.HlslOp, Builder);
}

Value *translateF32ToF16(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                         HLOperationLowerHelper &Helper,
                         HLObjectOperationLowerHelper *PObjHelper,
                         bool &Translated) {
  IRBuilder<> Builder(CI);

  Value *X = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Type *Ty = CI->getType();

  Function *F32tof16 = Helper.HlslOp.GetOpFunc(Opcode, Helper.VoidTy);
  return trivialDxilOperation(
      F32tof16, Opcode, {Builder.getInt32(static_cast<unsigned>(Opcode)), X},
      X->getType(), Ty, &Helper.HlslOp, Builder);
}

Value *translateLength(CallInst *CI, Value *Val, hlsl::OP *HlslOp) {
  IRBuilder<> Builder(CI);
  if (VectorType *VT = dyn_cast<VectorType>(Val->getType())) {
    Value *Elt = Builder.CreateExtractElement(Val, (uint64_t)0);
    unsigned Size = VT->getNumElements();
    if (Size > 1) {
      Value *Sum = Builder.CreateFMul(Elt, Elt);
      for (unsigned I = 1; I < Size; I++) {
        Elt = Builder.CreateExtractElement(Val, I);
        Value *Mul = Builder.CreateFMul(Elt, Elt);
        Sum = Builder.CreateFAdd(Sum, Mul);
      }
      DXIL::OpCode Sqrt = DXIL::OpCode::Sqrt;
      Function *DxilSqrt = HlslOp->GetOpFunc(Sqrt, VT->getElementType());
      Value *OpArg = HlslOp->GetI32Const((unsigned)Sqrt);
      return Builder.CreateCall(DxilSqrt, {OpArg, Sum},
                                HlslOp->GetOpCodeName(Sqrt));
    }       Val = Elt;
   
  }
  DXIL::OpCode Fabs = DXIL::OpCode::FAbs;
  Function *DxilFAbs = HlslOp->GetOpFunc(Fabs, Val->getType());
  Value *OpArg = HlslOp->GetI32Const((unsigned)Fabs);
  return Builder.CreateCall(DxilFAbs, {OpArg, Val},
                            HlslOp->GetOpCodeName(Fabs));
}

Value *translateLength(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                       HLOperationLowerHelper &Helper,
                       HLObjectOperationLowerHelper *PObjHelper,
                       bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  return translateLength(CI, Val, HlslOp);
}

Value *translateModF(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                     HLOperationLowerHelper &Helper,
                     HLObjectOperationLowerHelper *PObjHelper,
                     bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Val = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *OutIntPtr = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);
  Value *IntP =
      trivialDxilUnaryOperation(OP::OpCode::Round_z, Val, HlslOp, Builder);
  Value *FracP = Builder.CreateFSub(Val, IntP);
  Builder.CreateStore(IntP, OutIntPtr);
  return FracP;
}

Value *translateDistance(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                         HLOperationLowerHelper &Helper,
                         HLObjectOperationLowerHelper *PObjHelper,
                         bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Src1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);
  Value *Sub = Builder.CreateFSub(Src0, Src1);
  return translateLength(CI, Sub, HlslOp);
}

Value *translateExp(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  IRBuilder<> Builder(CI);
  Type *Ty = CI->getType();
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Constant *Log2eConst = ConstantFP::get(Ty->getScalarType(), M_LOG2E);
  if (Ty != Ty->getScalarType()) {
    Log2eConst =
        ConstantVector::getSplat(Ty->getVectorNumElements(), Log2eConst);
  }
  Val = Builder.CreateFMul(Log2eConst, Val);
  Value *Exp = trivialDxilUnaryOperation(OP::OpCode::Exp, Val, HlslOp, Builder);
  return Exp;
}

Value *translateLog(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  IRBuilder<> Builder(CI);
  Type *Ty = CI->getType();
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Constant *Ln2Const = ConstantFP::get(Ty->getScalarType(), M_LN2);
  if (Ty != Ty->getScalarType()) {
    Ln2Const = ConstantVector::getSplat(Ty->getVectorNumElements(), Ln2Const);
  }
  Value *Log = trivialDxilUnaryOperation(OP::OpCode::Log, Val, HlslOp, Builder);

  return Builder.CreateFMul(Ln2Const, Log);
}

Value *translateLog10(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                      HLOperationLowerHelper &Helper,
                      HLObjectOperationLowerHelper *PObjHelper,
                      bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  IRBuilder<> Builder(CI);
  Type *Ty = CI->getType();
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Constant *Log210Const = ConstantFP::get(Ty->getScalarType(), M_LN2 / M_LN10);
  if (Ty != Ty->getScalarType()) {
    Log210Const =
        ConstantVector::getSplat(Ty->getVectorNumElements(), Log210Const);
  }
  Value *Log = trivialDxilUnaryOperation(OP::OpCode::Log, Val, HlslOp, Builder);

  return Builder.CreateFMul(Log210Const, Log);
}

Value *translateFMod(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                     HLOperationLowerHelper &Helper,
                     HLObjectOperationLowerHelper *PObjHelper,
                     bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Src1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);
  Value *Div = Builder.CreateFDiv(Src0, Src1);
  Value *NegDiv = Builder.CreateFNeg(Div);
  Value *Ge = Builder.CreateFCmpOGE(Div, NegDiv);
  Value *AbsDiv =
      trivialDxilUnaryOperation(OP::OpCode::FAbs, Div, HlslOp, Builder);
  Value *Frc =
      trivialDxilUnaryOperation(OP::OpCode::Frc, AbsDiv, HlslOp, Builder);
  Value *NegFrc = Builder.CreateFNeg(Frc);
  Value *RealFrc = Builder.CreateSelect(Ge, Frc, NegFrc);
  return Builder.CreateFMul(RealFrc, Src1);
}

Value *translateFuiBinary(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                          HLOperationLowerHelper &Helper,
                          HLObjectOperationLowerHelper *PObjHelper,
                          bool &Translated) {
  bool IsFloat = CI->getType()->getScalarType()->isFloatingPointTy();
  if (IsFloat) {
    switch (IOP) {
    case IntrinsicOp::IOP_max:
      Opcode = OP::OpCode::FMax;
      break;
    case IntrinsicOp::IOP_min:
    default:
      DXASSERT_NOMSG(IOP == IntrinsicOp::IOP_min);
      Opcode = OP::OpCode::FMin;
      break;
    }
  }
  return trivialBinaryOperation(CI, IOP, Opcode, Helper, PObjHelper,
                                Translated);
}

Value *translateFuiTrinary(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                           HLOperationLowerHelper &Helper,
                           HLObjectOperationLowerHelper *PObjHelper,
                           bool &Translated) {
  bool IsFloat = CI->getType()->getScalarType()->isFloatingPointTy();
  if (IsFloat) {
    switch (IOP) {
    case IntrinsicOp::IOP_mad:
    default:
      DXASSERT_NOMSG(IOP == IntrinsicOp::IOP_mad);
      Opcode = OP::OpCode::FMad;
      break;
    }
  }
  return trivialTrinaryOperation(CI, IOP, Opcode, Helper, PObjHelper,
                                 Translated);
}

Value *translateFrexp(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                      HLOperationLowerHelper &Helper,
                      HLObjectOperationLowerHelper *PObjHelper,
                      bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Val = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *ExpPtr = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);
  Type *I32Ty = Type::getInt32Ty(CI->getContext());
  Constant *ExponentMaskConst = ConstantInt::get(I32Ty, 0x7f800000);
  Constant *MantisaMaskConst = ConstantInt::get(I32Ty, 0x007fffff);
  Constant *ExponentShiftConst = ConstantInt::get(I32Ty, 23);
  Constant *MantisaOrConst = ConstantInt::get(I32Ty, 0x3f000000);
  Constant *ExponentBiasConst = ConstantInt::get(I32Ty, -(int)0x3f000000);
  Constant *ZeroVal = HlslOp->GetFloatConst(0);
  // int iVal = asint(val);
  Type *DstTy = I32Ty;
  Type *Ty = Val->getType();
  if (Ty->isVectorTy()) {
    unsigned VecSize = Ty->getVectorNumElements();
    DstTy = VectorType::get(I32Ty, VecSize);
    ExponentMaskConst = ConstantVector::getSplat(VecSize, ExponentMaskConst);
    MantisaMaskConst = ConstantVector::getSplat(VecSize, MantisaMaskConst);
    ExponentShiftConst = ConstantVector::getSplat(VecSize, ExponentShiftConst);
    MantisaOrConst = ConstantVector::getSplat(VecSize, MantisaOrConst);
    ExponentBiasConst = ConstantVector::getSplat(VecSize, ExponentBiasConst);
    ZeroVal = ConstantVector::getSplat(VecSize, ZeroVal);
  }

  // bool ne = val != 0;
  Value *NotZero = Builder.CreateFCmpUNE(Val, ZeroVal);
  NotZero = Builder.CreateSExt(NotZero, DstTy);

  Value *IntVal = Builder.CreateBitCast(Val, DstTy);
  // temp = intVal & exponentMask;
  Value *Temp = Builder.CreateAnd(IntVal, ExponentMaskConst);
  // temp = temp + exponentBias;
  Temp = Builder.CreateAdd(Temp, ExponentBiasConst);
  // temp = temp & ne;
  Temp = Builder.CreateAnd(Temp, NotZero);
  // temp = temp >> exponentShift;
  Temp = Builder.CreateAShr(Temp, ExponentShiftConst);
  // exp = float(temp);
  Value *Exp = Builder.CreateSIToFP(Temp, Ty);
  Builder.CreateStore(Exp, ExpPtr);
  // temp = iVal & mantisaMask;
  Temp = Builder.CreateAnd(IntVal, MantisaMaskConst);
  // temp = temp | mantisaOr;
  Temp = Builder.CreateOr(Temp, MantisaOrConst);
  // mantisa = temp & ne;
  Value *Mantisa = Builder.CreateAnd(Temp, NotZero);
  return Builder.CreateBitCast(Mantisa, Ty);
}

Value *translateLdExp(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                      HLOperationLowerHelper &Helper,
                      HLObjectOperationLowerHelper *PObjHelper,
                      bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Src1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);
  Value *Exp =
      trivialDxilUnaryOperation(OP::OpCode::Exp, Src1, HlslOp, Builder);
  return Builder.CreateFMul(Exp, Src0);
}

Value *translateFWidth(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                       HLOperationLowerHelper &Helper,
                       HLObjectOperationLowerHelper *PObjHelper,
                       bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  IRBuilder<> Builder(CI);
  Value *Ddx =
      trivialDxilUnaryOperation(OP::OpCode::DerivCoarseX, Src, HlslOp, Builder);
  Value *AbsDdx =
      trivialDxilUnaryOperation(OP::OpCode::FAbs, Ddx, HlslOp, Builder);
  Value *Ddy =
      trivialDxilUnaryOperation(OP::OpCode::DerivCoarseY, Src, HlslOp, Builder);
  Value *AbsDdy =
      trivialDxilUnaryOperation(OP::OpCode::FAbs, Ddy, HlslOp, Builder);
  return Builder.CreateFAdd(AbsDdx, AbsDdy);
}

Value *translateLerp(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                     HLOperationLowerHelper &Helper,
                     HLObjectOperationLowerHelper *PObjHelper,
                     bool &Translated) {
  // x + s(y-x)
  Value *X = CI->getArgOperand(HLOperandIndex::kLerpOpXIdx);
  Value *Y = CI->getArgOperand(HLOperandIndex::kLerpOpYIdx);
  IRBuilder<> Builder(CI);
  Value *YSubx = Builder.CreateFSub(Y, X);
  Value *S = CI->getArgOperand(HLOperandIndex::kLerpOpSIdx);
  Value *SMulSub = Builder.CreateFMul(S, YSubx);
  return Builder.CreateFAdd(X, SMulSub);
}

Value *trivialDotOperation(OP::OpCode Opcode, Value *Src0, Value *Src1,
                           hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  Type *Ty = Src0->getType()->getScalarType();
  Function *DxilFunc = HlslOp->GetOpFunc(Opcode, Ty);
  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);

  SmallVector<Value *, 9> Args;
  Args.emplace_back(OpArg);

  unsigned VecSize = Src0->getType()->getVectorNumElements();
  for (unsigned I = 0; I < VecSize; I++)
    Args.emplace_back(Builder.CreateExtractElement(Src0, I));

  for (unsigned I = 0; I < VecSize; I++)
    Args.emplace_back(Builder.CreateExtractElement(Src1, I));
  Value *DotOp = Builder.CreateCall(DxilFunc, Args);

  return DotOp;
}

// Instead of using a DXIL intrinsic, implement a dot product operation using
// multiply and add operations. Used for integer dots and long vectors.
Value *expandDot(Value *Arg0, Value *Arg1, unsigned VecSize, hlsl::OP *HlslOp,
                 IRBuilder<> &Builder,
                 DXIL::OpCode MadOpCode = DXIL::OpCode::IMad) {
  Value *Elt0 = Builder.CreateExtractElement(Arg0, (uint64_t)0);
  Value *Elt1 = Builder.CreateExtractElement(Arg1, (uint64_t)0);
  Value *Result;
  if (Elt0->getType()->isFloatingPointTy())
    Result = Builder.CreateFMul(Elt0, Elt1);
  else
    Result = Builder.CreateMul(Elt0, Elt1);
  for (unsigned Elt = 1; Elt < VecSize; ++Elt) {
    Elt0 = Builder.CreateExtractElement(Arg0, Elt);
    Elt1 = Builder.CreateExtractElement(Arg1, Elt);
    Result = trivialDxilTrinaryOperation(MadOpCode, Elt0, Elt1, Result, HlslOp,
                                         Builder);
  }

  return Result;
}

Value *translateFDot(Value *Arg0, Value *Arg1, unsigned VecSize,
                     hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  switch (VecSize) {
  case 2:
    return trivialDotOperation(OP::OpCode::Dot2, Arg0, Arg1, HlslOp, Builder);
    break;
  case 3:
    return trivialDotOperation(OP::OpCode::Dot3, Arg0, Arg1, HlslOp, Builder);
    break;
  case 4:
    return trivialDotOperation(OP::OpCode::Dot4, Arg0, Arg1, HlslOp, Builder);
    break;
  default:
    DXASSERT(VecSize == 1, "wrong vector size");
    {
      Value *VecMul = Builder.CreateFMul(Arg0, Arg1);
      return Builder.CreateExtractElement(VecMul, (uint64_t)0);
    }
    break;
  }
}

Value *translateDot(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Arg0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Type *Ty = Arg0->getType();
  Type *EltTy = Ty->getScalarType();

  // SM6.9 introduced a DXIL operation for vectorized dot product
  // The operation is only advantageous for vect size>1, vec1s will be
  // lowered to a single Mul.
  if (HlslOp->GetModule()->GetHLModule().GetShaderModel()->IsSM69Plus() &&
      EltTy->isFloatingPointTy() && Ty->getVectorNumElements() > 1) {
    Value *Arg1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
    IRBuilder<> Builder(CI);
    Constant *OpArg = HlslOp->GetU32Const((unsigned)DXIL::OpCode::FDot);
    Value *Args[] = {OpArg, Arg0, Arg1};
    Function *DxilFunc = HlslOp->GetOpFunc(DXIL::OpCode::FDot, Ty);
    return trivialDxilVectorOperation(DxilFunc, DXIL::OpCode::FDot, Args, Ty,
                                      HlslOp, Builder);
  }

  unsigned VecSize = Ty->getVectorNumElements();
  Value *Arg1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);
  if (EltTy->isFloatingPointTy() && Ty->getVectorNumElements() <= 4)
    return translateFDot(Arg0, Arg1, VecSize, HlslOp, Builder);

  DXIL::OpCode MadOpCode = DXIL::OpCode::IMad;
  if (IOP == IntrinsicOp::IOP_udot)
    MadOpCode = DXIL::OpCode::UMad;
  else if (EltTy->isFloatingPointTy())
    MadOpCode = DXIL::OpCode::FMad;
  return expandDot(Arg0, Arg1, VecSize, HlslOp, Builder, MadOpCode);
}

Value *translateNormalize(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                          HLOperationLowerHelper &Helper,
                          HLObjectOperationLowerHelper *PObjHelper,
                          bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Type *Ty = CI->getType();
  Value *Op = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  VectorType *VT = cast<VectorType>(Ty);
  unsigned VecSize = VT->getNumElements();

  IRBuilder<> Builder(CI);
  Value *Dot = translateFDot(Op, Op, VecSize, HlslOp, Builder);
  DXIL::OpCode RsqrtOp = DXIL::OpCode::Rsqrt;
  Function *DxilRsqrt = HlslOp->GetOpFunc(RsqrtOp, VT->getElementType());
  Value *Rsqrt = Builder.CreateCall(
      DxilRsqrt, {HlslOp->GetI32Const((unsigned)RsqrtOp), Dot},
      HlslOp->GetOpCodeName(RsqrtOp));
  Value *VecRsqrt = UndefValue::get(VT);
  for (unsigned I = 0; I < VT->getNumElements(); I++)
    VecRsqrt = Builder.CreateInsertElement(VecRsqrt, Rsqrt, I);

  return Builder.CreateFMul(Op, VecRsqrt);
}

Value *translateReflect(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                        HLOperationLowerHelper &Helper,
                        HLObjectOperationLowerHelper *PObjHelper,
                        bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  //  v = i - 2 * n * dot(i, n).
  IRBuilder<> Builder(CI);
  Value *I = CI->getArgOperand(HLOperandIndex::kReflectOpIIdx);
  Value *N = CI->getArgOperand(HLOperandIndex::kReflectOpNIdx);

  VectorType *VT = cast<VectorType>(I->getType());
  unsigned VecSize = VT->getNumElements();
  Value *Dot = translateFDot(I, N, VecSize, HlslOp, Builder);
  // 2 * dot (i, n).
  Dot = Builder.CreateFMul(ConstantFP::get(Dot->getType(), 2.0), Dot);
  // 2 * n * dot(i, n).
  Value *VecDot = Builder.CreateVectorSplat(VecSize, Dot);
  Value *NMulDot = Builder.CreateFMul(VecDot, N);
  // i - 2 * n * dot(i, n).
  return Builder.CreateFSub(I, NMulDot);
}

Value *translateRefract(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                        HLOperationLowerHelper &Helper,
                        HLObjectOperationLowerHelper *PObjHelper,
                        bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  //  d = dot(i, n);
  //  t = 1 - eta * eta * ( 1 - d*d);
  //  cond = t >= 1;
  //  r = eta * i - (eta * d + sqrt(t)) * n;
  //  return cond ? r : 0;
  IRBuilder<> Builder(CI);
  Value *I = CI->getArgOperand(HLOperandIndex::kRefractOpIIdx);
  Value *N = CI->getArgOperand(HLOperandIndex::kRefractOpNIdx);
  Value *Eta = CI->getArgOperand(HLOperandIndex::kRefractOpEtaIdx);

  VectorType *VT = cast<VectorType>(I->getType());
  unsigned VecSize = VT->getNumElements();
  Value *Dot = translateFDot(I, N, VecSize, HlslOp, Builder);
  // eta * eta;
  Value *Eta2 = Builder.CreateFMul(Eta, Eta);
  // d*d;
  Value *Dot2 = Builder.CreateFMul(Dot, Dot);
  Constant *One = ConstantFP::get(Eta->getType(), 1);
  Constant *Zero = ConstantFP::get(Eta->getType(), 0);
  // 1- d*d;
  Dot2 = Builder.CreateFSub(One, Dot2);
  // eta * eta * (1-d*d);
  Eta2 = Builder.CreateFMul(Dot2, Eta2);
  // t = 1 - eta * eta * ( 1 - d*d);
  Value *T = Builder.CreateFSub(One, Eta2);
  // cond = t >= 0;
  Value *Cond = Builder.CreateFCmpOGE(T, Zero);
  // eta * i;
  Value *VecEta = UndefValue::get(VT);
  for (unsigned I = 0; I < VecSize; I++)
    VecEta = Builder.CreateInsertElement(VecEta, Eta, I);
  Value *EtaMulI = Builder.CreateFMul(I, VecEta);
  // sqrt(t);
  Value *Sqrt = trivialDxilUnaryOperation(OP::OpCode::Sqrt, T, HlslOp, Builder);
  // eta * d;
  Value *EtaMulD = Builder.CreateFMul(Eta, Dot);
  // eta * d + sqrt(t);
  Value *EtaSqrt = Builder.CreateFAdd(EtaMulD, Sqrt);
  // (eta * d + sqrt(t)) * n;
  Value *VecEtaSqrt = Builder.CreateVectorSplat(VecSize, EtaSqrt);
  Value *R = Builder.CreateFMul(VecEtaSqrt, N);
  // r = eta * i - (eta * d + sqrt(t)) * n;
  R = Builder.CreateFSub(EtaMulI, R);
  Value *Refract =
      Builder.CreateSelect(Cond, R, ConstantVector::getSplat(VecSize, Zero));
  return Refract;
}

Value *translateSmoothStep(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                           HLOperationLowerHelper &Helper,
                           HLObjectOperationLowerHelper *PObjHelper,
                           bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  // s = saturate((x-min)/(max-min)).
  IRBuilder<> Builder(CI);
  Value *MinVal = CI->getArgOperand(HLOperandIndex::kSmoothStepOpMinIdx);
  Value *MaxVal = CI->getArgOperand(HLOperandIndex::kSmoothStepOpMaxIdx);
  Value *MaxSubMin = Builder.CreateFSub(MaxVal, MinVal);
  Value *X = CI->getArgOperand(HLOperandIndex::kSmoothStepOpXIdx);
  Value *XSubMin = Builder.CreateFSub(X, MinVal);
  Value *SatVal = Builder.CreateFDiv(XSubMin, MaxSubMin);

  Value *S = trivialDxilUnaryOperation(DXIL::OpCode::Saturate, SatVal, HlslOp,
                                       Builder);
  // return s * s *(3-2*s).
  Constant *C2 = ConstantFP::get(CI->getType(), 2);
  Constant *C3 = ConstantFP::get(CI->getType(), 3);

  Value *SMul2 = Builder.CreateFMul(S, C2);
  Value *Result = Builder.CreateFSub(C3, SMul2);
  Result = Builder.CreateFMul(S, Result);
  Result = Builder.CreateFMul(S, Result);
  return Result;
}

Value *translateMSad4(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                      HLOperationLowerHelper &Helper,
                      HLObjectOperationLowerHelper *PObjHelper,
                      bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Ref = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  Value *Src = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx);
  Value *Accum = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx);
  Type *Ty = CI->getType();
  IRBuilder<> Builder(CI);
  Value *VecRef = UndefValue::get(Ty);
  for (unsigned I = 0; I < 4; I++)
    VecRef = Builder.CreateInsertElement(VecRef, Ref, I);

  Value *SrcX = Builder.CreateExtractElement(Src, (uint64_t)0);
  Value *SrcY = Builder.CreateExtractElement(Src, 1);

  Value *ByteSrc = UndefValue::get(Ty);
  ByteSrc = Builder.CreateInsertElement(ByteSrc, SrcX, (uint64_t)0);

  // ushr r0.yzw, srcX, l(0, 8, 16, 24)
  // bfi r1.yzw, l(0, 8, 16, 24), l(0, 24, 16, 8), srcX, r0.yyzw
  Value *BfiOpArg =
      HlslOp->GetU32Const(static_cast<unsigned>(DXIL::OpCode::Bfi));

  Value *Imm8 = HlslOp->GetU32Const(8);
  Value *Imm16 = HlslOp->GetU32Const(16);
  Value *Imm24 = HlslOp->GetU32Const(24);

  Ty = Ref->getType();
  // Get x[31:8].
  Value *SrcXShift = Builder.CreateLShr(SrcX, Imm8);
  // y[0~7] x[31:8].
  Value *ByteSrcElt = trivialDxilOperation(
      DXIL::OpCode::Bfi, {BfiOpArg, Imm8, Imm24, SrcY, SrcXShift}, Ty, Ty,
      HlslOp, Builder);
  ByteSrc = Builder.CreateInsertElement(ByteSrc, ByteSrcElt, 1);
  // Get x[31:16].
  SrcXShift = Builder.CreateLShr(SrcXShift, Imm8);
  // y[0~15] x[31:16].
  ByteSrcElt = trivialDxilOperation(DXIL::OpCode::Bfi,
                                    {BfiOpArg, Imm16, Imm16, SrcY, SrcXShift},
                                    Ty, Ty, HlslOp, Builder);
  ByteSrc = Builder.CreateInsertElement(ByteSrc, ByteSrcElt, 2);
  // Get x[31:24].
  SrcXShift = Builder.CreateLShr(SrcXShift, Imm8);
  // y[0~23] x[31:24].
  ByteSrcElt = trivialDxilOperation(DXIL::OpCode::Bfi,
                                    {BfiOpArg, Imm24, Imm8, SrcY, SrcXShift},
                                    Ty, Ty, HlslOp, Builder);
  ByteSrc = Builder.CreateInsertElement(ByteSrc, ByteSrcElt, 3);

  // Msad on vecref and byteSrc.
  return trivialDxilTrinaryOperation(DXIL::OpCode::Msad, VecRef, ByteSrc, Accum,
                                     HlslOp, Builder);
}

Value *translateRcp(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  Type *Ty = CI->getType();
  Value *Op = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  IRBuilder<> Builder(CI);
  Constant *One = ConstantFP::get(Ty->getScalarType(), 1.0);
  if (Ty != Ty->getScalarType()) {
    One = ConstantVector::getSplat(Ty->getVectorNumElements(), One);
  }
  return Builder.CreateFDiv(One, Op);
}

Value *translateSign(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                     HLOperationLowerHelper &Helper,
                     HLObjectOperationLowerHelper *PObjHelper,
                     bool &Translated) {
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Type *Ty = Val->getType();
  bool IsInt = Ty->getScalarType()->isIntegerTy();

  IRBuilder<> Builder(CI);
  Constant *Zero = Constant::getNullValue(Ty);
  Value *ZeroLtVal = IsInt ? Builder.CreateICmpSLT(Zero, Val)
                           : Builder.CreateFCmpOLT(Zero, Val);
  Value *ValLtZero = IsInt ? Builder.CreateICmpSLT(Val, Zero)
                           : Builder.CreateFCmpOLT(Val, Zero);
  ZeroLtVal = Builder.CreateZExt(ZeroLtVal, CI->getType());
  ValLtZero = Builder.CreateZExt(ValLtZero, CI->getType());
  return Builder.CreateSub(ZeroLtVal, ValLtZero);
}

Value *translateUSign(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                      HLOperationLowerHelper &Helper,
                      HLObjectOperationLowerHelper *PObjHelper,
                      bool &Translated) {
  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Type *Ty = Val->getType();

  IRBuilder<> Builder(CI);
  Constant *Zero = Constant::getNullValue(Ty);
  Value *NonZero = Builder.CreateICmpNE(Val, Zero);
  return Builder.CreateZExt(NonZero, CI->getType());
}

Value *translateStep(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                     HLOperationLowerHelper &Helper,
                     HLObjectOperationLowerHelper *PObjHelper,
                     bool &Translated) {
  Value *Edge = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *X = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  Type *Ty = CI->getType();
  IRBuilder<> Builder(CI);

  Constant *One = ConstantFP::get(Ty->getScalarType(), 1.0);
  Constant *Zero = ConstantFP::get(Ty->getScalarType(), 0);
  Value *Cond = Builder.CreateFCmpOLT(X, Edge);

  if (Ty != Ty->getScalarType()) {
    One = ConstantVector::getSplat(Ty->getVectorNumElements(), One);
    Zero = ConstantVector::getSplat(Ty->getVectorNumElements(), Zero);
  }

  return Builder.CreateSelect(Cond, Zero, One);
}

Value *translatePow(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *X = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Y = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  bool IsFxcCompatMode =
      CI->getModule()->GetHLModule().GetHLOptions().bFXCCompatMode;
  IRBuilder<> Builder(CI);
  return translatePowImpl(HlslOp, Builder, X, Y, IsFxcCompatMode);
}

Value *translatePrintf(CallInst *CI, IntrinsicOp IOP, DXIL::OpCode Opcode,
                       HLOperationLowerHelper &Helper,
                       HLObjectOperationLowerHelper *PObjHelper,
                       bool &Translated) {
  Translated = false;
  dxilutil::EmitErrorOnInstruction(CI,
                                   "use of unsupported identifier 'printf'");
  return nullptr;
}

Value *translateFaceforward(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                            HLOperationLowerHelper &Helper,
                            HLObjectOperationLowerHelper *PObjHelper,
                            bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Type *Ty = CI->getType();

  Value *N = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  Value *I = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx);
  Value *Ng = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx);
  IRBuilder<> Builder(CI);

  unsigned VecSize = Ty->getVectorNumElements();
  // -n x sign(dot(i, ng)).
  Value *DotOp = translateFDot(I, Ng, VecSize, HlslOp, Builder);

  Constant *Zero = ConstantFP::get(Ty->getScalarType(), 0);
  Value *DotLtZero = Builder.CreateFCmpOLT(DotOp, Zero);

  Value *NegN = Builder.CreateFNeg(N);
  Value *Faceforward = Builder.CreateSelect(DotLtZero, N, NegN);
  return Faceforward;
}

Value *trivialSetMeshOutputCounts(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                                  HLOperationLowerHelper &Helper,
                                  HLObjectOperationLowerHelper *PObjHelper,
                                  bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Src1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);
  Constant *OpArg = HlslOp->GetU32Const((unsigned)Op);
  Value *Args[] = {OpArg, Src0, Src1};
  Function *DxilFunc = HlslOp->GetOpFunc(Op, Type::getVoidTy(CI->getContext()));

  Builder.CreateCall(DxilFunc, Args);
  return nullptr;
}

Value *trivialDispatchMesh(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                           HLOperationLowerHelper &Helper,
                           HLObjectOperationLowerHelper *PObjHelper,
                           bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kDispatchMeshOpThreadX);
  Value *Src1 = CI->getArgOperand(HLOperandIndex::kDispatchMeshOpThreadY);
  Value *Src2 = CI->getArgOperand(HLOperandIndex::kDispatchMeshOpThreadZ);
  Value *Src3 = CI->getArgOperand(HLOperandIndex::kDispatchMeshOpPayload);
  IRBuilder<> Builder(CI);
  Constant *OpArg = HlslOp->GetU32Const((unsigned)Op);
  Value *Args[] = {OpArg, Src0, Src1, Src2, Src3};
  Function *DxilFunc = HlslOp->GetOpFunc(Op, Src3->getType());

  Builder.CreateCall(DxilFunc, Args);
  return nullptr;
}
} // namespace

// MOP intrinsics
namespace {

Value *translateGetSamplePosition(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                                  HLOperationLowerHelper &Helper,
                                  HLObjectOperationLowerHelper *PObjHelper,
                                  bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);

  IRBuilder<> Builder(CI);
  Value *SampleIdx =
      CI->getArgOperand(HLOperandIndex::kGetSamplePositionSampleIdxOpIndex);

  OP::OpCode Opcode = OP::OpCode::Texture2DMSGetSamplePosition;
  llvm::Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Function *DxilFunc =
      HlslOp->GetOpFunc(Opcode, Type::getVoidTy(CI->getContext()));

  Value *Args[] = {OpArg, Handle, SampleIdx};
  Value *SamplePos = Builder.CreateCall(DxilFunc, Args);

  Value *Result = UndefValue::get(CI->getType());
  Value *SamplePosX = Builder.CreateExtractValue(SamplePos, 0);
  Value *SamplePosY = Builder.CreateExtractValue(SamplePos, 1);
  Result = Builder.CreateInsertElement(Result, SamplePosX, (uint64_t)0);
  Result = Builder.CreateInsertElement(Result, SamplePosY, 1);
  return Result;
}

Value *translateGetDimensions(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                              HLOperationLowerHelper &Helper,
                              HLObjectOperationLowerHelper *PObjHelper,
                              bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;

  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  DxilResource::Kind RK = PObjHelper->getRk(Handle);

  IRBuilder<> Builder(CI);
  OP::OpCode Opcode = OP::OpCode::GetDimensions;
  llvm::Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Function *DxilFunc =
      HlslOp->GetOpFunc(Opcode, Type::getVoidTy(CI->getContext()));

  Type *I32Ty = Type::getInt32Ty(CI->getContext());
  Value *MipLevel = UndefValue::get(I32Ty);
  unsigned WidthOpIdx = HLOperandIndex::kGetDimensionsMipWidthOpIndex;
  switch (RK) {
  case DxilResource::Kind::Texture1D:
  case DxilResource::Kind::Texture1DArray:
  case DxilResource::Kind::Texture2D:
  case DxilResource::Kind::Texture2DArray:
  case DxilResource::Kind::TextureCube:
  case DxilResource::Kind::TextureCubeArray:
  case DxilResource::Kind::Texture3D: {
    Value *OpMipLevel =
        CI->getArgOperand(HLOperandIndex::kGetDimensionsMipLevelOpIndex);
    // mipLevel is in parameter, should not be pointer.
    if (!OpMipLevel->getType()->isPointerTy())
      MipLevel = OpMipLevel;
    else {
      // No mip level.
      WidthOpIdx = HLOperandIndex::kGetDimensionsNoMipWidthOpIndex;
      MipLevel = ConstantInt::get(I32Ty, 0);
    }
  } break;
  default:
    WidthOpIdx = HLOperandIndex::kGetDimensionsNoMipWidthOpIndex;
    break;
  }
  Value *Args[] = {OpArg, Handle, MipLevel};
  Value *Dims = Builder.CreateCall(DxilFunc, Args);

  unsigned DimensionIdx = 0;

  Value *Width = Builder.CreateExtractValue(Dims, DimensionIdx++);
  Value *WidthPtr = CI->getArgOperand(WidthOpIdx);
  if (WidthPtr->getType()->getPointerElementType()->isFloatingPointTy())
    Width = Builder.CreateSIToFP(Width,
                                 WidthPtr->getType()->getPointerElementType());

  Builder.CreateStore(Width, WidthPtr);

  if (DXIL::IsStructuredBuffer(RK)) {
    // Set stride.
    Value *StridePtr = CI->getArgOperand(WidthOpIdx + 1);
    const DataLayout &DL = Helper.DL;
    Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
    Type *BufTy = PObjHelper->getResourceType(Handle);
    Type *BufRetTy = BufTy->getStructElementType(0);
    unsigned Stride = DL.getTypeAllocSize(BufRetTy);
    Builder.CreateStore(HlslOp->GetU32Const(Stride), StridePtr);
  } else {
    if (WidthOpIdx == HLOperandIndex::kGetDimensionsMipWidthOpIndex ||
        // Samples is in w channel too.
        RK == DXIL::ResourceKind::Texture2DMS) {
      // Has mip.
      for (unsigned ArgIdx = WidthOpIdx + 1;
           ArgIdx < CI->getNumArgOperands() - 1; ArgIdx++) {
        Value *Dim = Builder.CreateExtractValue(Dims, DimensionIdx++);
        Value *Ptr = CI->getArgOperand(ArgIdx);
        if (Ptr->getType()->getPointerElementType()->isFloatingPointTy())
          Dim = Builder.CreateSIToFP(Dim,
                                     Ptr->getType()->getPointerElementType());
        Builder.CreateStore(Dim, Ptr);
      }
      // NumOfLevel is in w channel.
      DimensionIdx = 3;
      Value *Dim = Builder.CreateExtractValue(Dims, DimensionIdx);
      Value *Ptr = CI->getArgOperand(CI->getNumArgOperands() - 1);
      if (Ptr->getType()->getPointerElementType()->isFloatingPointTy())
        Dim =
            Builder.CreateSIToFP(Dim, Ptr->getType()->getPointerElementType());
      Builder.CreateStore(Dim, Ptr);
    } else {
      for (unsigned ArgIdx = WidthOpIdx + 1; ArgIdx < CI->getNumArgOperands();
           ArgIdx++) {
        Value *Dim = Builder.CreateExtractValue(Dims, DimensionIdx++);
        Value *Ptr = CI->getArgOperand(ArgIdx);
        if (Ptr->getType()->getPointerElementType()->isFloatingPointTy())
          Dim = Builder.CreateSIToFP(Dim,
                                     Ptr->getType()->getPointerElementType());
        Builder.CreateStore(Dim, Ptr);
      }
    }
  }
  return nullptr;
}

Value *generateUpdateCounter(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                             HLOperationLowerHelper &Helper,
                             HLObjectOperationLowerHelper *PObjHelper,
                             bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);

  PObjHelper->markHasCounter(Handle, Helper.I8Ty);

  bool BInc = IOP == IntrinsicOp::MOP_IncrementCounter;
  IRBuilder<> Builder(CI);

  OP::OpCode OpCode = OP::OpCode::BufferUpdateCounter;
  Value *OpCodeArg = HlslOp->GetU32Const((unsigned)OpCode);
  Value *IncVal = HlslOp->GetI8Const(BInc ? 1 : -1);
  // Create BufferUpdateCounter call.
  Value *Args[] = {OpCodeArg, Handle, IncVal};

  Function *F =
      HlslOp->GetOpFunc(OpCode, Type::getVoidTy(Handle->getContext()));
  return Builder.CreateCall(F, Args);
}

static Value *scalarizeResRet(Type *RetTy, Value *ResRet,
                              IRBuilder<> &Builder) {
  // Extract value part.
  Value *RetVal = llvm::UndefValue::get(RetTy);
  if (RetTy->isVectorTy()) {
    for (unsigned I = 0; I < RetTy->getVectorNumElements(); I++) {
      Value *RetComp = Builder.CreateExtractValue(ResRet, I);
      RetVal = Builder.CreateInsertElement(RetVal, RetComp, I);
    }
  } else {
    RetVal = Builder.CreateExtractValue(ResRet, 0);
  }
  return RetVal;
}

void updateStatus(Value *ResRet, Value *Status, IRBuilder<> &Builder,
                  hlsl::OP *HlslOp,
                  unsigned StatusIndex = DXIL::kResRetStatusIndex) {
  if (Status && !isa<UndefValue>(Status)) {
    Value *StatusVal = Builder.CreateExtractValue(ResRet, StatusIndex);
    Value *CheckAccessOp = HlslOp->GetI32Const(
        static_cast<unsigned>(DXIL::OpCode::CheckAccessFullyMapped));
    Function *CheckAccessFn = HlslOp->GetOpFunc(
        DXIL::OpCode::CheckAccessFullyMapped, StatusVal->getType());
    // CheckAccess on status.
    Value *BStatus =
        Builder.CreateCall(CheckAccessFn, {CheckAccessOp, StatusVal});
    Value *ExtStatus =
        Builder.CreateZExt(BStatus, Type::getInt32Ty(Status->getContext()));
    Builder.CreateStore(ExtStatus, Status);
  }
}

Value *splatToVector(Value *Elt, Type *DstTy, IRBuilder<> &Builder) {
  Value *Result = UndefValue::get(DstTy);
  for (unsigned I = 0; I < DstTy->getVectorNumElements(); I++)
    Result = Builder.CreateInsertElement(Result, Elt, I);
  return Result;
}

Value *translateMul(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {

  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Arg0 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Arg1 = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  Type *Arg0Ty = Arg0->getType();
  Type *Arg1Ty = Arg1->getType();
  IRBuilder<> Builder(CI);

  if (Arg0Ty->isVectorTy()) {
    if (Arg1Ty->isVectorTy()) {
      // mul(vector, vector) == dot(vector, vector)
      unsigned VecSize = Arg0Ty->getVectorNumElements();
      if (Arg0Ty->getScalarType()->isFloatingPointTy()) {
        return translateFDot(Arg0, Arg1, VecSize, HlslOp, Builder);
      }

      DXIL::OpCode MadOpCode = DXIL::OpCode::IMad;
      if (IOP == IntrinsicOp::IOP_umul)
        MadOpCode = DXIL::OpCode::UMad;
      return expandDot(Arg0, Arg1, VecSize, HlslOp, Builder, MadOpCode);
    }       // mul(vector, scalar) == vector * scalar-splat
      Arg1 = splatToVector(Arg1, Arg0Ty, Builder);
   
  } else {
    if (Arg1Ty->isVectorTy()) {
      // mul(scalar, vector) == scalar-splat * vector
      Arg0 = splatToVector(Arg0, Arg1Ty, Builder);
    }
    // else mul(scalar, scalar) == scalar * scalar;
  }

  // create fmul/mul for the pair of vectors or scalars
  if (Arg0Ty->getScalarType()->isFloatingPointTy()) {
    return Builder.CreateFMul(Arg0, Arg1);
  }
  return Builder.CreateMul(Arg0, Arg1);
}

// Sample intrinsics.
struct SampleHelper {
  SampleHelper(CallInst *CI, OP::OpCode Op,
               HLObjectOperationLowerHelper *PObjHelper);

  OP::OpCode Opcode = OP::OpCode::NumOpCodes;
  DXIL::ResourceKind ResourceKind = DXIL::ResourceKind::Invalid;
  Value *SampledTexHandle = nullptr;
  Value *TexHandle = nullptr;
  Value *SamplerHandle = nullptr;
  static const unsigned KMaxCoordDimensions = 4;
  unsigned CoordDimensions = 0;
  Value *Coord[KMaxCoordDimensions];
  Value *CompareValue = nullptr;
  Value *Bias = nullptr;
  Value *Lod = nullptr;
  // SampleGrad only.
  static const unsigned KMaxDdxyDimensions = 3;
  Value *Ddx[KMaxDdxyDimensions];
  Value *Ddy[KMaxDdxyDimensions];
  // Optional.
  static const unsigned KMaxOffsetDimensions = 3;
  unsigned OffsetDimensions = 0;
  Value *Offset[KMaxOffsetDimensions];
  Value *Clamp = nullptr;
  Value *Status = nullptr;
  unsigned MaxHlOperandRead = 0;
  Value *readHlOperand(CallInst *CI, unsigned OpIdx) {
    if (CI->getNumArgOperands() > OpIdx) {
      MaxHlOperandRead = std::max(MaxHlOperandRead, OpIdx);
      return CI->getArgOperand(OpIdx);
    }
    return nullptr;
  }
  void translateCoord(CallInst *CI, unsigned CoordIdx) {
    Value *CoordArg = readHlOperand(CI, CoordIdx);
    DXASSERT_NOMSG(CoordArg);
    DXASSERT(CoordArg->getType()->getVectorNumElements() == CoordDimensions,
             "otherwise, HL coordinate dimensions mismatch");
    IRBuilder<> Builder(CI);
    for (unsigned I = 0; I < CoordDimensions; I++)
      Coord[I] = Builder.CreateExtractElement(CoordArg, I);
    Value *UndefF = UndefValue::get(Type::getFloatTy(CI->getContext()));
    for (unsigned I = CoordDimensions; I < KMaxCoordDimensions; I++)
      Coord[I] = UndefF;
  }
  void translateOffset(CallInst *CI, unsigned OffsetIdx) {
    IntegerType *I32Ty = Type::getInt32Ty(CI->getContext());
    if (Value *OffsetArg = readHlOperand(CI, OffsetIdx)) {
      DXASSERT(OffsetArg->getType()->getVectorNumElements() == OffsetDimensions,
               "otherwise, HL coordinate dimensions mismatch");
      IRBuilder<> Builder(CI);
      for (unsigned I = 0; I < OffsetDimensions; I++)
        Offset[I] = Builder.CreateExtractElement(OffsetArg, I);
    } else {
      // Use zeros for offsets when not specified, not undef.
      Value *Zero = ConstantInt::get(I32Ty, (uint64_t)0);
      for (unsigned I = 0; I < OffsetDimensions; I++)
        Offset[I] = Zero;
    }
    // Use undef for components that should not be used for this resource dim.
    Value *UndefI = UndefValue::get(I32Ty);
    for (unsigned I = OffsetDimensions; I < KMaxOffsetDimensions; I++)
      Offset[I] = UndefI;
  }
  void setBias(CallInst *CI, unsigned BiasIdx) {
    // Clamp bias for immediate.
    Bias = readHlOperand(CI, BiasIdx);
    DXASSERT_NOMSG(Bias);
    if (ConstantFP *FP = dyn_cast<ConstantFP>(Bias)) {
      float V = FP->getValueAPF().convertToFloat();
      if (V > DXIL::kMaxMipLodBias)
        Bias = ConstantFP::get(FP->getType(), DXIL::kMaxMipLodBias);
      if (V < DXIL::kMinMipLodBias)
        Bias = ConstantFP::get(FP->getType(), DXIL::kMinMipLodBias);
    }
  }
  void setLod(CallInst *CI, unsigned LodIdx) {
    Lod = readHlOperand(CI, LodIdx);
    DXASSERT_NOMSG(Lod);
  }
  void setCompareValue(CallInst *CI, unsigned CmpIdx) {
    CompareValue = readHlOperand(CI, CmpIdx);
    DXASSERT_NOMSG(CompareValue);
  }
  void setClamp(CallInst *CI, unsigned ClampIdx) {
    if ((Clamp = readHlOperand(CI, ClampIdx))) {
      if (Clamp->getType()->isVectorTy()) {
        IRBuilder<> Builder(CI);
        Clamp = Builder.CreateExtractElement(Clamp, (uint64_t)0);
      }
    } else
      Clamp = UndefValue::get(Type::getFloatTy(CI->getContext()));
  }
  void setStatus(CallInst *CI, unsigned StatusIdx) {
    Status = readHlOperand(CI, StatusIdx);
  }
  void setDdx(CallInst *CI, unsigned DdxIdx) {
    setDdxy(CI, Ddx, readHlOperand(CI, DdxIdx));
  }
  void setDdy(CallInst *CI, unsigned DdyIdx) {
    setDdxy(CI, Ddy, readHlOperand(CI, DdyIdx));
  }
  void setDdxy(CallInst *CI, MutableArrayRef<Value *> Ddxy, Value *DdxyArg) {
    DXASSERT_NOMSG(DdxyArg);
    IRBuilder<> Builder(CI);
    unsigned DdxySize = DdxyArg->getType()->getVectorNumElements();
    for (unsigned I = 0; I < DdxySize; I++)
      Ddxy[I] = Builder.CreateExtractElement(DdxyArg, I);
    Value *UndefF = UndefValue::get(Type::getFloatTy(CI->getContext()));
    for (unsigned I = DdxySize; I < KMaxDdxyDimensions; I++)
      Ddxy[I] = UndefF;
  }
};

SampleHelper::SampleHelper(CallInst *CI, OP::OpCode Op,
                           HLObjectOperationLowerHelper *PObjHelper)
    : Opcode(Op) {

  TexHandle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  ResourceKind = PObjHelper->getRk(TexHandle);
  if (ResourceKind == DXIL::ResourceKind::Invalid) {
    Opcode = DXIL::OpCode::NumOpCodes;
    return;
  }

  CoordDimensions = Opcode == DXIL::OpCode::CalculateLOD
                        ? DxilResource::GetNumDimensionsForCalcLOD(ResourceKind)
                        : DxilResource::GetNumCoords(ResourceKind);
  OffsetDimensions = DxilResource::GetNumOffsets(ResourceKind);

  const bool BFeedbackOp = hlsl::OP::IsDxilOpFeedback(Op);
  SampledTexHandle =
      BFeedbackOp ? CI->getArgOperand(
                        HLOperandIndex::kWriteSamplerFeedbackSampledArgIndex)
                  : nullptr;
  const unsigned KSamplerArgIndex =
      BFeedbackOp ? HLOperandIndex::kWriteSamplerFeedbackSamplerArgIndex
                  : HLOperandIndex::kSampleSamplerArgIndex;
  SamplerHandle = CI->getArgOperand(KSamplerArgIndex);

  const unsigned KCoordArgIdx =
      BFeedbackOp ? HLOperandIndex::kWriteSamplerFeedbackCoordArgIndex
                  : HLOperandIndex::kSampleCoordArgIndex;
  translateCoord(CI, KCoordArgIdx);

  // TextureCube does not support offsets, shifting each subsequent arg index
  // down by 1
  unsigned Cube = (ResourceKind == DXIL::ResourceKind::TextureCube ||
                   ResourceKind == DXIL::ResourceKind::TextureCubeArray)
                      ? 1
                      : 0;

  switch (Op) {
  case OP::OpCode::Sample:
    translateOffset(CI, Cube ? HLOperandIndex::kInvalidIdx
                             : HLOperandIndex::kSampleOffsetArgIndex);
    setClamp(CI, HLOperandIndex::kSampleClampArgIndex - Cube);
    setStatus(CI, HLOperandIndex::kSampleStatusArgIndex - Cube);
    break;
  case OP::OpCode::SampleLevel:
    setLod(CI, HLOperandIndex::kSampleLLevelArgIndex);
    translateOffset(CI, Cube ? HLOperandIndex::kInvalidIdx
                             : HLOperandIndex::kSampleLOffsetArgIndex);
    setStatus(CI, HLOperandIndex::kSampleLStatusArgIndex - Cube);
    break;
  case OP::OpCode::SampleBias:
    setBias(CI, HLOperandIndex::kSampleBBiasArgIndex);
    translateOffset(CI, Cube ? HLOperandIndex::kInvalidIdx
                             : HLOperandIndex::kSampleBOffsetArgIndex);
    setClamp(CI, HLOperandIndex::kSampleBClampArgIndex - Cube);
    setStatus(CI, HLOperandIndex::kSampleBStatusArgIndex - Cube);
    break;
  case OP::OpCode::SampleCmp:
    setCompareValue(CI, HLOperandIndex::kSampleCmpCmpValArgIndex);
    translateOffset(CI, Cube ? HLOperandIndex::kInvalidIdx
                             : HLOperandIndex::kSampleCmpOffsetArgIndex);
    setClamp(CI, HLOperandIndex::kSampleCmpClampArgIndex - Cube);
    setStatus(CI, HLOperandIndex::kSampleCmpStatusArgIndex - Cube);
    break;
  case OP::OpCode::SampleCmpBias:
    setBias(CI, HLOperandIndex::kSampleCmpBBiasArgIndex);
    setCompareValue(CI, HLOperandIndex::kSampleCmpBCmpValArgIndex);
    translateOffset(CI, Cube ? HLOperandIndex::kInvalidIdx
                             : HLOperandIndex::kSampleCmpBOffsetArgIndex);
    setClamp(CI, HLOperandIndex::kSampleCmpBClampArgIndex - Cube);
    setStatus(CI, HLOperandIndex::kSampleCmpBStatusArgIndex - Cube);
    break;
  case OP::OpCode::SampleCmpGrad:
    setDdx(CI, HLOperandIndex::kSampleCmpGDDXArgIndex);
    setDdy(CI, HLOperandIndex::kSampleCmpGDDYArgIndex);
    setCompareValue(CI, HLOperandIndex::kSampleCmpGCmpValArgIndex);
    translateOffset(CI, Cube ? HLOperandIndex::kInvalidIdx
                             : HLOperandIndex::kSampleCmpGOffsetArgIndex);
    setClamp(CI, HLOperandIndex::kSampleCmpGClampArgIndex - Cube);
    setStatus(CI, HLOperandIndex::kSampleCmpGStatusArgIndex - Cube);
    break;
  case OP::OpCode::SampleCmpLevel:
    setCompareValue(CI, HLOperandIndex::kSampleCmpCmpValArgIndex);
    translateOffset(CI, Cube ? HLOperandIndex::kInvalidIdx
                             : HLOperandIndex::kSampleCmpLOffsetArgIndex);
    setLod(CI, HLOperandIndex::kSampleCmpLLevelArgIndex);
    setStatus(CI, HLOperandIndex::kSampleCmpStatusArgIndex - Cube);
    break;
  case OP::OpCode::SampleCmpLevelZero:
    setCompareValue(CI, HLOperandIndex::kSampleCmpLZCmpValArgIndex);
    translateOffset(CI, Cube ? HLOperandIndex::kInvalidIdx
                             : HLOperandIndex::kSampleCmpLZOffsetArgIndex);
    setStatus(CI, HLOperandIndex::kSampleCmpLZStatusArgIndex - Cube);
    break;
  case OP::OpCode::SampleGrad:
    setDdx(CI, HLOperandIndex::kSampleGDDXArgIndex);
    setDdy(CI, HLOperandIndex::kSampleGDDYArgIndex);
    translateOffset(CI, Cube ? HLOperandIndex::kInvalidIdx
                             : HLOperandIndex::kSampleGOffsetArgIndex);
    setClamp(CI, HLOperandIndex::kSampleGClampArgIndex - Cube);
    setStatus(CI, HLOperandIndex::kSampleGStatusArgIndex - Cube);
    break;
  case OP::OpCode::CalculateLOD:
    // Only need coord for LOD calculation.
    break;
  case OP::OpCode::WriteSamplerFeedback:
    setClamp(CI, HLOperandIndex::kWriteSamplerFeedback_ClampArgIndex);
    break;
  case OP::OpCode::WriteSamplerFeedbackBias:
    setBias(CI, HLOperandIndex::kWriteSamplerFeedbackBias_BiasArgIndex);
    setClamp(CI, HLOperandIndex::kWriteSamplerFeedbackBias_ClampArgIndex);
    break;
  case OP::OpCode::WriteSamplerFeedbackGrad:
    setDdx(CI, HLOperandIndex::kWriteSamplerFeedbackGrad_DdxArgIndex);
    setDdy(CI, HLOperandIndex::kWriteSamplerFeedbackGrad_DdyArgIndex);
    setClamp(CI, HLOperandIndex::kWriteSamplerFeedbackGrad_ClampArgIndex);
    break;
  case OP::OpCode::WriteSamplerFeedbackLevel:
    setLod(CI, HLOperandIndex::kWriteSamplerFeedbackLevel_LodArgIndex);
    break;
  default:
    DXASSERT(0, "invalid opcode for Sample");
    break;
  }
  DXASSERT(MaxHlOperandRead == CI->getNumArgOperands() - 1,
           "otherwise, unused HL arguments for Sample op");
}

Value *translateCalculateLod(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                             HLOperationLowerHelper &Helper,
                             HLObjectOperationLowerHelper *PObjHelper,
                             bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  SampleHelper SampleHelper(CI, OP::OpCode::CalculateLOD, PObjHelper);
  if (SampleHelper.Opcode == DXIL::OpCode::NumOpCodes) {
    Translated = false;
    return nullptr;
  }

  bool BClamped = IOP == IntrinsicOp::MOP_CalculateLevelOfDetail;
  IRBuilder<> Builder(CI);
  Value *OpArg =
      HlslOp->GetU32Const(static_cast<unsigned>(OP::OpCode::CalculateLOD));
  Value *Clamped = HlslOp->GetI1Const(BClamped);

  Value *Args[] = {OpArg,
                   SampleHelper.TexHandle,
                   SampleHelper.SamplerHandle,
                   SampleHelper.Coord[0],
                   SampleHelper.Coord[1],
                   SampleHelper.Coord[2],
                   Clamped};
  Function *DxilFunc = HlslOp->GetOpFunc(OP::OpCode::CalculateLOD,
                                         Type::getFloatTy(OpArg->getContext()));
  Value *LOD = Builder.CreateCall(DxilFunc, Args);
  return LOD;
}

Value *translateCheckAccess(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                            HLOperationLowerHelper &Helper,
                            HLObjectOperationLowerHelper *PObjHelper,
                            bool &Translated) {
  // Translate CheckAccess into uint->bool, later optimization should remove it.
  // Real checkaccess is generated in UpdateStatus.
  IRBuilder<> Builder(CI);
  Value *V = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  return Builder.CreateTrunc(V, Helper.I1Ty);
}

void generateDxilSample(CallInst *CI, Function *F, ArrayRef<Value *> SampleArgs,
                        Value *Status, hlsl::OP *HlslOp) {
  IRBuilder<> Builder(CI);

  CallInst *Call = Builder.CreateCall(F, SampleArgs);

  dxilutil::MigrateDebugValue(CI, Call);

  // extract value part
  Value *RetVal = scalarizeResRet(CI->getType(), Call, Builder);

  // Replace ret val.
  CI->replaceAllUsesWith(RetVal);

  // get status
  if (Status) {
    updateStatus(Call, Status, Builder, HlslOp);
  }
}

Value *translateSample(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                       HLOperationLowerHelper &Helper,
                       HLObjectOperationLowerHelper *PObjHelper,
                       bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  SampleHelper SampleHelper(CI, Opcode, PObjHelper);

  if (SampleHelper.Opcode == DXIL::OpCode::NumOpCodes) {
    Translated = false;
    return nullptr;
  }
  Type *Ty = CI->getType();

  Function *F = HlslOp->GetOpFunc(Opcode, Ty->getScalarType());

  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);

  switch (Opcode) {
  case OP::OpCode::Sample: {
    Value *SampleArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Offset.
        SampleHelper.Offset[0], SampleHelper.Offset[1], SampleHelper.Offset[2],
        // Clamp.
        SampleHelper.Clamp};
    generateDxilSample(CI, F, SampleArgs, SampleHelper.Status, HlslOp);
  } break;
  case OP::OpCode::SampleLevel: {
    Value *SampleArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Offset.
        SampleHelper.Offset[0], SampleHelper.Offset[1], SampleHelper.Offset[2],
        // LOD.
        SampleHelper.Lod};
    generateDxilSample(CI, F, SampleArgs, SampleHelper.Status, HlslOp);
  } break;
  case OP::OpCode::SampleGrad: {
    Value *SampleArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Offset.
        SampleHelper.Offset[0], SampleHelper.Offset[1], SampleHelper.Offset[2],
        // Ddx.
        SampleHelper.Ddx[0], SampleHelper.Ddx[1], SampleHelper.Ddx[2],
        // Ddy.
        SampleHelper.Ddy[0], SampleHelper.Ddy[1], SampleHelper.Ddy[2],
        // Clamp.
        SampleHelper.Clamp};
    generateDxilSample(CI, F, SampleArgs, SampleHelper.Status, HlslOp);
  } break;
  case OP::OpCode::SampleBias: {
    Value *SampleArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Offset.
        SampleHelper.Offset[0], SampleHelper.Offset[1], SampleHelper.Offset[2],
        // Bias.
        SampleHelper.Bias,
        // Clamp.
        SampleHelper.Clamp};
    generateDxilSample(CI, F, SampleArgs, SampleHelper.Status, HlslOp);
  } break;
  case OP::OpCode::SampleCmpBias: {
    Value *SampleArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Offset.
        SampleHelper.Offset[0], SampleHelper.Offset[1], SampleHelper.Offset[2],
        // CmpVal.
        SampleHelper.CompareValue,
        // Bias.
        SampleHelper.Bias,
        // Clamp.
        SampleHelper.Clamp};
    generateDxilSample(CI, F, SampleArgs, SampleHelper.Status, HlslOp);
  } break;
  case OP::OpCode::SampleCmpGrad: {
    Value *SampleArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Offset.
        SampleHelper.Offset[0], SampleHelper.Offset[1], SampleHelper.Offset[2],
        // CmpVal.
        SampleHelper.CompareValue,
        // Ddx.
        SampleHelper.Ddx[0], SampleHelper.Ddx[1], SampleHelper.Ddx[2],
        // Ddy.
        SampleHelper.Ddy[0], SampleHelper.Ddy[1], SampleHelper.Ddy[2],
        // Clamp.
        SampleHelper.Clamp};
    generateDxilSample(CI, F, SampleArgs, SampleHelper.Status, HlslOp);
  } break;
  case OP::OpCode::SampleCmp: {
    Value *SampleArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Offset.
        SampleHelper.Offset[0], SampleHelper.Offset[1], SampleHelper.Offset[2],
        // CmpVal.
        SampleHelper.CompareValue,
        // Clamp.
        SampleHelper.Clamp};
    generateDxilSample(CI, F, SampleArgs, SampleHelper.Status, HlslOp);
  } break;
  case OP::OpCode::SampleCmpLevel: {
    Value *SampleArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Offset.
        SampleHelper.Offset[0], SampleHelper.Offset[1], SampleHelper.Offset[2],
        // CmpVal.
        SampleHelper.CompareValue,
        // LOD.
        SampleHelper.Lod};
    generateDxilSample(CI, F, SampleArgs, SampleHelper.Status, HlslOp);
  } break;
  case OP::OpCode::SampleCmpLevelZero:
  default: {
    DXASSERT(Opcode == OP::OpCode::SampleCmpLevelZero, "invalid sample opcode");
    Value *SampleArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Offset.
        SampleHelper.Offset[0], SampleHelper.Offset[1], SampleHelper.Offset[2],
        // CmpVal.
        SampleHelper.CompareValue};
    generateDxilSample(CI, F, SampleArgs, SampleHelper.Status, HlslOp);
  } break;
  }
  // CI is replaced in GenerateDxilSample.
  return nullptr;
}

// Gather intrinsics.
struct GatherHelper {
  enum class GatherChannel {
    GatherAll,
    GatherRed,
    GatherGreen,
    GatherBlue,
    GatherAlpha,
  };

  GatherHelper(CallInst *CI, OP::OpCode Op,
               HLObjectOperationLowerHelper *PObjHelper,
               GatherHelper::GatherChannel Ch);

  OP::OpCode Opcode;
  Value *TexHandle;
  Value *SamplerHandle;
  static const unsigned KMaxCoordDimensions = 4;
  Value *Coord[KMaxCoordDimensions];
  unsigned Channel;
  Value *Special; // For CompareValue, Bias, LOD.
  // Optional.
  static const unsigned KMaxOffsetDimensions = 2;
  Value *Offset[KMaxOffsetDimensions];
  // For the overload send different offset for each sample.
  // Only save 3 sampleOffsets because use offset for normal overload as first
  // sample offset.
  static const unsigned KSampleOffsetDimensions = 3;
  Value *SampleOffsets[KSampleOffsetDimensions][KMaxOffsetDimensions];
  Value *Status;

  bool HasSampleOffsets;

  unsigned MaxHlOperandRead = 0;
  Value *readHlOperand(CallInst *CI, unsigned OpIdx) {
    if (CI->getNumArgOperands() > OpIdx) {
      MaxHlOperandRead = std::max(MaxHlOperandRead, OpIdx);
      return CI->getArgOperand(OpIdx);
    }
    return nullptr;
  }
  void translateCoord(CallInst *CI, unsigned CoordIdx,
                      unsigned CoordDimensions) {
    Value *CoordArg = readHlOperand(CI, CoordIdx);
    DXASSERT_NOMSG(CoordArg);
    DXASSERT(CoordArg->getType()->getVectorNumElements() == CoordDimensions,
             "otherwise, HL coordinate dimensions mismatch");
    IRBuilder<> Builder(CI);
    for (unsigned I = 0; I < CoordDimensions; I++)
      Coord[I] = Builder.CreateExtractElement(CoordArg, I);
    Value *UndefF = UndefValue::get(Type::getFloatTy(CI->getContext()));
    for (unsigned I = CoordDimensions; I < KMaxCoordDimensions; I++)
      Coord[I] = UndefF;
  }
  void setStatus(CallInst *CI, unsigned StatusIdx) {
    Status = readHlOperand(CI, StatusIdx);
  }
  void translateOffset(CallInst *CI, unsigned OffsetIdx,
                       unsigned OffsetDimensions) {
    IntegerType *I32Ty = Type::getInt32Ty(CI->getContext());
    if (Value *OffsetArg = readHlOperand(CI, OffsetIdx)) {
      DXASSERT(OffsetArg->getType()->getVectorNumElements() == OffsetDimensions,
               "otherwise, HL coordinate dimensions mismatch");
      IRBuilder<> Builder(CI);
      for (unsigned I = 0; I < OffsetDimensions; I++)
        Offset[I] = Builder.CreateExtractElement(OffsetArg, I);
    } else {
      // Use zeros for offsets when not specified, not undef.
      Value *Zero = ConstantInt::get(I32Ty, (uint64_t)0);
      for (unsigned I = 0; I < OffsetDimensions; I++)
        Offset[I] = Zero;
    }
    // Use undef for components that should not be used for this resource dim.
    Value *UndefI = UndefValue::get(I32Ty);
    for (unsigned I = OffsetDimensions; I < KMaxOffsetDimensions; I++)
      Offset[I] = UndefI;
  }
  void translateSampleOffset(CallInst *CI, unsigned OffsetIdx,
                             unsigned OffsetDimensions) {
    Value *UndefI = UndefValue::get(Type::getInt32Ty(CI->getContext()));
    if (CI->getNumArgOperands() >= (OffsetIdx + KSampleOffsetDimensions)) {
      HasSampleOffsets = true;
      IRBuilder<> Builder(CI);
      for (unsigned Ch = 0; Ch < KSampleOffsetDimensions; Ch++) {
        Value *OffsetArg = readHlOperand(CI, OffsetIdx + Ch);
        for (unsigned I = 0; I < OffsetDimensions; I++)
          SampleOffsets[Ch][I] = Builder.CreateExtractElement(OffsetArg, I);
        for (unsigned I = OffsetDimensions; I < KMaxOffsetDimensions; I++)
          SampleOffsets[Ch][I] = UndefI;
      }
    }
  }
  // Update the offset args for gather with sample offset at sampleIdx.
  void updateOffsetInGatherArgs(MutableArrayRef<Value *> GatherArgs,
                                unsigned SampleIdx) {
    unsigned OffsetBase = DXIL::OperandIndex::kTextureGatherOffset0OpIdx;
    for (unsigned I = 0; I < KMaxOffsetDimensions; I++)
      // -1 because offset for sample 0 is in GatherHelper::offset.
      GatherArgs[OffsetBase + I] = SampleOffsets[SampleIdx - 1][I];
  }
};

GatherHelper::GatherHelper(CallInst *CI, OP::OpCode Op,
                           HLObjectOperationLowerHelper *PObjHelper,
                           GatherHelper::GatherChannel Ch)
    : Opcode(Op), Special(nullptr), HasSampleOffsets(false) {

  switch (Ch) {
  case GatherChannel::GatherAll:
    Channel = 0;
    break;
  case GatherChannel::GatherRed:
    Channel = 0;
    break;
  case GatherChannel::GatherGreen:
    Channel = 1;
    break;
  case GatherChannel::GatherBlue:
    Channel = 2;
    break;
  case GatherChannel::GatherAlpha:
    Channel = 3;
    break;
  }

  IRBuilder<> Builder(CI);
  TexHandle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  SamplerHandle = CI->getArgOperand(HLOperandIndex::kSampleSamplerArgIndex);

  DXIL::ResourceKind RK = PObjHelper->getRk(TexHandle);
  if (RK == DXIL::ResourceKind::Invalid) {
    Opcode = DXIL::OpCode::NumOpCodes;
    return;
  }
  unsigned CoordSize = DxilResource::GetNumCoords(RK);
  unsigned OffsetSize = DxilResource::GetNumOffsets(RK);
  bool Cube = RK == DXIL::ResourceKind::TextureCube ||
              RK == DXIL::ResourceKind::TextureCubeArray;

  const unsigned KCoordArgIdx = HLOperandIndex::kSampleCoordArgIndex;
  translateCoord(CI, KCoordArgIdx, CoordSize);

  switch (Op) {
  case OP::OpCode::TextureGather: {
    unsigned StatusIdx;
    if (Cube) {
      translateOffset(CI, HLOperandIndex::kInvalidIdx, OffsetSize);
      StatusIdx = HLOperandIndex::kGatherCubeStatusArgIndex;
    } else {
      translateOffset(CI, HLOperandIndex::kGatherOffsetArgIndex, OffsetSize);
      // Gather all don't have sample offset version overload.
      if (Ch != GatherChannel::GatherAll)
        translateSampleOffset(CI, HLOperandIndex::kGatherSampleOffsetArgIndex,
                              OffsetSize);
      StatusIdx = HasSampleOffsets
                      ? HLOperandIndex::kGatherStatusWithSampleOffsetArgIndex
                      : HLOperandIndex::kGatherStatusArgIndex;
    }
    setStatus(CI, StatusIdx);
  } break;
  case OP::OpCode::TextureGatherCmp: {
    Special = readHlOperand(CI, HLOperandIndex::kGatherCmpCmpValArgIndex);
    unsigned StatusIdx;
    if (Cube) {
      translateOffset(CI, HLOperandIndex::kInvalidIdx, OffsetSize);
      StatusIdx = HLOperandIndex::kGatherCmpCubeStatusArgIndex;
    } else {
      translateOffset(CI, HLOperandIndex::kGatherCmpOffsetArgIndex, OffsetSize);
      // Gather all don't have sample offset version overload.
      if (Ch != GatherChannel::GatherAll)
        translateSampleOffset(
            CI, HLOperandIndex::kGatherCmpSampleOffsetArgIndex, OffsetSize);
      StatusIdx = HasSampleOffsets
                      ? HLOperandIndex::kGatherCmpStatusWithSampleOffsetArgIndex
                      : HLOperandIndex::kGatherCmpStatusArgIndex;
    }
    setStatus(CI, StatusIdx);
  } break;
  case OP::OpCode::TextureGatherRaw: {
    unsigned StatusIdx;
    translateOffset(CI, HLOperandIndex::kGatherOffsetArgIndex, OffsetSize);
    // Gather all don't have sample offset version overload.
    DXASSERT(Ch == GatherChannel::GatherAll,
             "Raw gather must use all channels");
    DXASSERT(!Cube, "Raw gather can't be used with cube textures");
    DXASSERT(!HasSampleOffsets,
             "Raw gather doesn't support individual offsets");
    StatusIdx = HLOperandIndex::kGatherStatusArgIndex;
    setStatus(CI, StatusIdx);
  } break;
  default:
    DXASSERT(0, "invalid opcode for Gather");
    break;
  }
  DXASSERT(MaxHlOperandRead == CI->getNumArgOperands() - 1,
           "otherwise, unused HL arguments for Sample op");
}

void generateDxilGather(CallInst *CI, Function *F,
                        MutableArrayRef<Value *> GatherArgs,
                        GatherHelper &Helper, hlsl::OP *HlslOp) {
  IRBuilder<> Builder(CI);

  CallInst *Call = Builder.CreateCall(F, GatherArgs);

  dxilutil::MigrateDebugValue(CI, Call);

  Value *RetVal;
  if (!Helper.HasSampleOffsets) {
    // extract value part
    RetVal = scalarizeResRet(CI->getType(), Call, Builder);
  } else {
    RetVal = UndefValue::get(CI->getType());
    Value *Elt = Builder.CreateExtractValue(Call, (uint64_t)0);
    RetVal = Builder.CreateInsertElement(RetVal, Elt, (uint64_t)0);

    Helper.updateOffsetInGatherArgs(GatherArgs, /*sampleIdx*/ 1);
    CallInst *CallY = Builder.CreateCall(F, GatherArgs);
    Elt = Builder.CreateExtractValue(CallY, (uint64_t)1);
    RetVal = Builder.CreateInsertElement(RetVal, Elt, 1);

    Helper.updateOffsetInGatherArgs(GatherArgs, /*sampleIdx*/ 2);
    CallInst *CallZ = Builder.CreateCall(F, GatherArgs);
    Elt = Builder.CreateExtractValue(CallZ, (uint64_t)2);
    RetVal = Builder.CreateInsertElement(RetVal, Elt, 2);

    Helper.updateOffsetInGatherArgs(GatherArgs, /*sampleIdx*/ 3);
    CallInst *CallW = Builder.CreateCall(F, GatherArgs);
    Elt = Builder.CreateExtractValue(CallW, (uint64_t)3);
    RetVal = Builder.CreateInsertElement(RetVal, Elt, 3);

    // TODO: UpdateStatus for each gather call.
  }

  // Replace ret val.
  CI->replaceAllUsesWith(RetVal);

  // Get status
  if (Helper.Status) {
    updateStatus(Call, Helper.Status, Builder, HlslOp);
  }
}

Value *translateGather(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                       HLOperationLowerHelper &Helper,
                       HLObjectOperationLowerHelper *PObjHelper,
                       bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  GatherHelper::GatherChannel Ch = GatherHelper::GatherChannel::GatherAll;
  switch (IOP) {
  case IntrinsicOp::MOP_Gather:
  case IntrinsicOp::MOP_GatherCmp:
  case IntrinsicOp::MOP_GatherRaw:
    Ch = GatherHelper::GatherChannel::GatherAll;
    break;
  case IntrinsicOp::MOP_GatherRed:
  case IntrinsicOp::MOP_GatherCmpRed:
    Ch = GatherHelper::GatherChannel::GatherRed;
    break;
  case IntrinsicOp::MOP_GatherGreen:
  case IntrinsicOp::MOP_GatherCmpGreen:
    Ch = GatherHelper::GatherChannel::GatherGreen;
    break;
  case IntrinsicOp::MOP_GatherBlue:
  case IntrinsicOp::MOP_GatherCmpBlue:
    Ch = GatherHelper::GatherChannel::GatherBlue;
    break;
  case IntrinsicOp::MOP_GatherAlpha:
  case IntrinsicOp::MOP_GatherCmpAlpha:
    Ch = GatherHelper::GatherChannel::GatherAlpha;
    break;
  default:
    DXASSERT(0, "invalid gather intrinsic");
    break;
  }

  GatherHelper GatherHelper(CI, Opcode, PObjHelper, Ch);

  if (GatherHelper.Opcode == DXIL::OpCode::NumOpCodes) {
    Translated = false;
    return nullptr;
  }
  Type *Ty = CI->getType();

  Function *F = HlslOp->GetOpFunc(GatherHelper.Opcode, Ty->getScalarType());

  Constant *OpArg = HlslOp->GetU32Const((unsigned)GatherHelper.Opcode);
  Value *ChannelArg = HlslOp->GetU32Const(GatherHelper.Channel);

  switch (Opcode) {
  case OP::OpCode::TextureGather: {
    Value *GatherArgs[] = {OpArg, GatherHelper.TexHandle,
                           GatherHelper.SamplerHandle,
                           // Coord.
                           GatherHelper.Coord[0], GatherHelper.Coord[1],
                           GatherHelper.Coord[2], GatherHelper.Coord[3],
                           // Offset.
                           GatherHelper.Offset[0], GatherHelper.Offset[1],
                           // Channel.
                           ChannelArg};
    generateDxilGather(CI, F, GatherArgs, GatherHelper, HlslOp);
  } break;
  case OP::OpCode::TextureGatherCmp: {
    Value *GatherArgs[] = {OpArg, GatherHelper.TexHandle,
                           GatherHelper.SamplerHandle,
                           // Coord.
                           GatherHelper.Coord[0], GatherHelper.Coord[1],
                           GatherHelper.Coord[2], GatherHelper.Coord[3],
                           // Offset.
                           GatherHelper.Offset[0], GatherHelper.Offset[1],
                           // Channel.
                           ChannelArg,
                           // CmpVal.
                           GatherHelper.Special};
    generateDxilGather(CI, F, GatherArgs, GatherHelper, HlslOp);
  } break;
  case OP::OpCode::TextureGatherRaw: {
    Value *GatherArgs[] = {OpArg, GatherHelper.TexHandle,
                           GatherHelper.SamplerHandle,
                           // Coord.
                           GatherHelper.Coord[0], GatherHelper.Coord[1],
                           GatherHelper.Coord[2], GatherHelper.Coord[3],
                           // Offset.
                           GatherHelper.Offset[0], GatherHelper.Offset[1]};
    generateDxilGather(CI, F, GatherArgs, GatherHelper, HlslOp);
    break;
  }
  default:
    DXASSERT(0, "invalid opcode for Gather");
    break;
  }
  // CI is replaced in GenerateDxilGather.
  return nullptr;
}

static Value *
translateWriteSamplerFeedback(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                              HLOperationLowerHelper &Helper,
                              HLObjectOperationLowerHelper *PObjHelper,
                              bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  SampleHelper SampleHelper(CI, Opcode, PObjHelper);

  if (SampleHelper.Opcode == DXIL::OpCode::NumOpCodes) {
    Translated = false;
    return nullptr;
  }
  Type *Ty = CI->getType();

  Function *F = HlslOp->GetOpFunc(Opcode, Ty->getScalarType());

  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);

  IRBuilder<> Builder(CI);

  switch (Opcode) {
  case OP::OpCode::WriteSamplerFeedback: {
    Value *SamplerFeedbackArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SampledTexHandle,
        SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Clamp.
        SampleHelper.Clamp};
    return Builder.CreateCall(F, SamplerFeedbackArgs);
  } break;
  case OP::OpCode::WriteSamplerFeedbackBias: {
    Value *SamplerFeedbackArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SampledTexHandle,
        SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Bias.
        SampleHelper.Bias,
        // Clamp.
        SampleHelper.Clamp};
    return Builder.CreateCall(F, SamplerFeedbackArgs);
  } break;
  case OP::OpCode::WriteSamplerFeedbackGrad: {
    Value *SamplerFeedbackArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SampledTexHandle,
        SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // Ddx.
        SampleHelper.Ddx[0], SampleHelper.Ddx[1], SampleHelper.Ddx[2],
        // Ddy.
        SampleHelper.Ddy[0], SampleHelper.Ddy[1], SampleHelper.Ddy[2],
        // Clamp.
        SampleHelper.Clamp};
    return Builder.CreateCall(F, SamplerFeedbackArgs);
  } break;
  case OP::OpCode::WriteSamplerFeedbackLevel: {
    Value *SamplerFeedbackArgs[] = {
        OpArg, SampleHelper.TexHandle, SampleHelper.SampledTexHandle,
        SampleHelper.SamplerHandle,
        // Coord.
        SampleHelper.Coord[0], SampleHelper.Coord[1], SampleHelper.Coord[2],
        SampleHelper.Coord[3],
        // LOD.
        SampleHelper.Lod};
    return Builder.CreateCall(F, SamplerFeedbackArgs);
  } break;
  default:
    DXASSERT(false, "otherwise, unknown SamplerFeedback Op");
    break;
  }
  return nullptr;
}

// Load/Store intrinsics.
OP::OpCode loadOpFromResKind(DxilResource::Kind RK) {
  switch (RK) {
  case DxilResource::Kind::RawBuffer:
  case DxilResource::Kind::StructuredBuffer:
    return OP::OpCode::RawBufferLoad;
  case DxilResource::Kind::TypedBuffer:
    return OP::OpCode::BufferLoad;
  case DxilResource::Kind::Invalid:
    DXASSERT(0, "invalid resource kind");
    break;
  default:
    return OP::OpCode::TextureLoad;
  }
  return OP::OpCode::TextureLoad;
}

struct ResLoadHelper {
  // Default constructor uses CI load intrinsic call
  //  to get the retval and various location indicators.
  ResLoadHelper(CallInst *CI, DxilResource::Kind RK, DxilResourceBase::Class RC,
                Value *H, IntrinsicOp IOP, LoadInst *TyBufSubLoad = nullptr);
  // Alternative constructor explicitly sets the index.
  // Used for some subscript operators that feed the generic HL call inst
  // into a load op and by the matrixload call instruction.
  ResLoadHelper(Instruction *Inst, DxilResource::Kind RK, Value *H, Value *Idx,
                Value *Offset, Value *Status = nullptr, Value *Mip = nullptr)
      : IntrinsicOpCode(IntrinsicOp::Num_Intrinsics), Handle(H), RetVal(Inst),
        Addr(Idx), Offset(Offset), Status(Status), MipLevel(Mip) {
    Opcode = loadOpFromResKind(RK);
    Type *Ty = Inst->getType();
    if (Opcode == OP::OpCode::RawBufferLoad && Ty->isVectorTy() &&
        Ty->getVectorNumElements() > 1 &&
        Inst->getModule()->GetHLModule().GetShaderModel()->IsSM69Plus())
      Opcode = OP::OpCode::RawBufferVectorLoad;
  }
  OP::OpCode Opcode;
  IntrinsicOp IntrinsicOpCode;
  unsigned DxilMajor;
  unsigned DxilMinor;
  Value *Handle;
  Value *RetVal;
  Value *Addr;
  Value *Offset;
  Value *Status;
  Value *MipLevel;
};

// Uses CI arguments to determine the index, offset, and mipLevel also depending
// on the RK/RC resource kind and class, which determine the opcode.
// Handle and IOP are set explicitly.
// For typed buffer loads, the call instruction feeds into a load
// represented by TyBufSubLoad which determines the instruction to replace.
// Otherwise, CI is replaced.
ResLoadHelper::ResLoadHelper(CallInst *CI, DxilResource::Kind RK,
                             DxilResourceBase::Class RC, Value *Hdl,
                             IntrinsicOp IOP, LoadInst *TyBufSubLoad)
    : IntrinsicOpCode(IOP), Handle(Hdl), Offset(nullptr), Status(nullptr) {
  Opcode = loadOpFromResKind(RK);
  bool BForSubscript = false;
  if (TyBufSubLoad) {
    BForSubscript = true;
    RetVal = TyBufSubLoad;
  } else
    RetVal = CI;
  const unsigned KAddrIdx = HLOperandIndex::kBufLoadAddrOpIdx;
  Addr = CI->getArgOperand(KAddrIdx);
  unsigned Argc = CI->getNumArgOperands();
  Type *I32Ty = Type::getInt32Ty(CI->getContext());
  unsigned StatusIdx = HLOperandIndex::kBufLoadStatusOpIdx;
  unsigned OffsetIdx = HLOperandIndex::kInvalidIdx;

  if (Opcode == OP::OpCode::TextureLoad) {
    bool IsMS = (RK == DxilResource::Kind::Texture2DMS ||
                 RK == DxilResource::Kind::Texture2DMSArray);
    // Set mip and status index.
    Offset = UndefValue::get(I32Ty);
    if (IsMS) {
      // Retrieve appropriate MS parameters.
      StatusIdx = HLOperandIndex::kTex2DMSLoadStatusOpIdx;
      // MS textures keep the sample param (mipLevel) regardless of writability.
      if (BForSubscript)
        MipLevel = ConstantInt::get(I32Ty, 0);
      else
        MipLevel =
            CI->getArgOperand(HLOperandIndex::kTex2DMSLoadSampleIdxOpIdx);
    } else if (RC == DxilResourceBase::Class::UAV) {
      // DXIL requires that non-MS UAV accesses set miplevel to undef.
      MipLevel = UndefValue::get(I32Ty);
      StatusIdx = HLOperandIndex::kRWTexLoadStatusOpIdx;
    } else {
      // Non-MS SRV case.
      StatusIdx = HLOperandIndex::kTexLoadStatusOpIdx;
      if (BForSubscript)
        // Having no miplevel param, single subscripted SRVs default to 0.
        MipLevel = ConstantInt::get(I32Ty, 0);
      else
        // Mip is stored at the last channel of the coordinate vector.
        MipLevel = IRBuilder<>(CI).CreateExtractElement(
            Addr, DxilResource::GetNumCoords(RK));
    }
    if (RC == DxilResourceBase::Class::SRV)
      OffsetIdx = IsMS ? HLOperandIndex::kTex2DMSLoadOffsetOpIdx
                       : HLOperandIndex::kTexLoadOffsetOpIdx;
  } else if (Opcode == OP::OpCode::RawBufferLoad) {
    // If native vectors are available and this load had a vector
    // with more than one elements, convert the RawBufferLod to the
    // native vector variant RawBufferVectorLoad.
    Type *Ty = CI->getType();
    if (Ty->isVectorTy() && Ty->getVectorNumElements() > 1 &&
        CI->getModule()->GetHLModule().GetShaderModel()->IsSM69Plus())
      Opcode = OP::OpCode::RawBufferVectorLoad;
  }

  // Set offset.
  if (DXIL::IsStructuredBuffer(RK))
    // Structured buffers receive no exterior offset in this constructor,
    // but may need to increment it later.
    Offset = ConstantInt::get(I32Ty, 0U);
  else if (Argc > OffsetIdx)
    // Textures may set the offset from an explicit argument.
    Offset = CI->getArgOperand(OffsetIdx);
  else
    // All other cases use undef.
    Offset = UndefValue::get(I32Ty);

  // Retrieve status value if provided.
  if (Argc > StatusIdx)
    Status = CI->getArgOperand(StatusIdx);
}

void translateStructBufSubscript(CallInst *CI, Value *Handle, Value *Status,
                                 hlsl::OP *OP, HLResource::Kind RK,
                                 const DataLayout &DL);

static Constant *getRawBufferMaskForETy(Type *Ty, unsigned NumComponents,
                                        hlsl::OP *OP) {
  unsigned Mask = 0;

  switch (NumComponents) {
  case 0:
    break;
  case 1:
    Mask = DXIL::kCompMask_X;
    break;
  case 2:
    Mask = DXIL::kCompMask_X | DXIL::kCompMask_Y;
    break;
  case 3:
    Mask = DXIL::kCompMask_X | DXIL::kCompMask_Y | DXIL::kCompMask_Z;
    break;
  case 4:
    Mask = DXIL::kCompMask_All;
    break;
  default:
    DXASSERT(false, "Cannot load more than 2 components for 64bit types.");
  }
  return OP->GetI8Const(Mask);
}

Value *generateRawBufLd(Value *Handle, Value *BufIdx, Value *Offset,
                        Value *Status, Type *EltTy,
                        MutableArrayRef<Value *> ResultElts, hlsl::OP *OP,
                        IRBuilder<> &Builder, unsigned NumComponents,
                        Constant *Alignment);

// Sets up arguments for buffer load call.
static SmallVector<Value *, 10> getBufLoadArgs(ResLoadHelper Helper,
                                               HLResource::Kind RK,
                                               IRBuilder<> Builder,
                                               unsigned LdSize) {
  OP::OpCode Opcode = Helper.Opcode;
  llvm::Constant *OpArg = Builder.getInt32((uint32_t)Opcode);

  unsigned Alignment = RK == DxilResource::Kind::RawBuffer ? 4U : 8U;
  Alignment = std::min(Alignment, LdSize);
  Constant *AlignmentVal = Builder.getInt32(Alignment);

  // Assemble args specific to the type bab/struct/typed:
  // - Typed needs to handle the possibility of vector coords
  // - Raws need to calculate alignment and mask values.
  SmallVector<Value *, 10> Args;
  Args.emplace_back(OpArg);         // opcode @0.
  Args.emplace_back(Helper.Handle); // Resource handle @1

  // Set offsets appropriate for the load operation.
  bool IsVectorAddr = Helper.Addr->getType()->isVectorTy();
  if (Opcode == OP::OpCode::TextureLoad) {
    llvm::Value *UndefI = llvm::UndefValue::get(Builder.getInt32Ty());

    // Set mip level or sample for MS texutures @2.
    Args.emplace_back(Helper.MipLevel);
    // Set texture coords according to resource kind @3-5
    // Coords unused by the resource kind are undefs.
    unsigned CoordSize = DxilResource::GetNumCoords(RK);
    for (unsigned I = 0; I < 3; I++)
      if (I < CoordSize)
        Args.emplace_back(IsVectorAddr
                              ? Builder.CreateExtractElement(Helper.Addr, I)
                              : Helper.Addr);
      else
        Args.emplace_back(UndefI);

    // Set texture offsets according to resource kind @7-9
    // Coords unused by the resource kind are undefs.
    unsigned OffsetSize = DxilResource::GetNumOffsets(RK);
    if (!Helper.Offset || isa<llvm::UndefValue>(Helper.Offset))
      OffsetSize = 0;
    for (unsigned I = 0; I < 3; I++)
      if (I < OffsetSize)
        Args.emplace_back(Builder.CreateExtractElement(Helper.Offset, I));
      else
        Args.emplace_back(UndefI);
  } else {
    // If not TextureLoad, it could be a typed or raw buffer load.
    // They have mostly similar arguments.
    DXASSERT(Opcode == OP::OpCode::RawBufferLoad ||
                 Opcode == OP::OpCode::RawBufferVectorLoad ||
                 Opcode == OP::OpCode::BufferLoad,
             "Wrong opcode in get load args");
    Args.emplace_back(
        IsVectorAddr ? Builder.CreateExtractElement(Helper.Addr, (uint64_t)0)
                     : Helper.Addr);
    Args.emplace_back(Helper.Offset);
    if (Opcode == OP::OpCode::RawBufferLoad) {
      // Unlike typed buffer load, raw buffer load has mask and alignment.
      Args.emplace_back(nullptr);      // Mask will be added later %4.
      Args.emplace_back(AlignmentVal); // alignment @5.
    } else if (Opcode == OP::OpCode::RawBufferVectorLoad) {
      // RawBufferVectorLoad takes just alignment, no mask.
      Args.emplace_back(AlignmentVal); // alignment @4
    }
  }
  return Args;
}

// Emits as many calls as needed to load the full vector
// Performs any needed extractions and conversions of the results.
Value *translateBufLoad(ResLoadHelper &Helper, HLResource::Kind RK,
                        IRBuilder<> &Builder, hlsl::OP *OP,
                        const DataLayout &DL) {
  OP::OpCode Opcode = Helper.Opcode;
  Type *Ty = Helper.RetVal->getType();

  unsigned NumComponents = 1;
  if (Ty->isVectorTy())
    NumComponents = Ty->getVectorNumElements();

  const bool IsTyped = DXIL::IsTyped(RK);
  Type *EltTy = Ty->getScalarType();
  const bool Is64 = (EltTy->isIntegerTy(64) || EltTy->isDoubleTy());
  const bool IsBool = EltTy->isIntegerTy(1);
  // Values will be loaded in memory representations.
  if (IsBool || (Is64 && IsTyped))
    EltTy = Builder.getInt32Ty();

  // Calculate load size with the scalar memory element type.
  unsigned LdSize = DL.getTypeAllocSize(EltTy);

  // Adjust number of components as needed.
  if (Is64 && IsTyped) {
    // 64-bit types are stored as int32 pairs in typed buffers.
    DXASSERT(NumComponents <= 2, "Typed buffers only allow 4 dwords.");
    NumComponents *= 2;
  } else if (Opcode == OP::OpCode::RawBufferVectorLoad) {
    // Native vector loads only have a single vector element in ResRet.
    EltTy = VectorType::get(EltTy, NumComponents);
    NumComponents = 1;
  }

  SmallVector<Value *, 10> Args = getBufLoadArgs(Helper, RK, Builder, LdSize);

  // Keep track of the first load for debug info migration.
  Value *FirstLd = nullptr;

  unsigned OffsetIdx = 0;
  if (RK == DxilResource::Kind::RawBuffer)
    // Raw buffers can't use offset param. Add to coord index.
    OffsetIdx = DXIL::OperandIndex::kRawBufferLoadIndexOpIdx;
  else if (RK == DxilResource::Kind::StructuredBuffer)
    OffsetIdx = DXIL::OperandIndex::kRawBufferLoadElementOffsetOpIdx;

  // Create call(s) to function object and collect results in Elts.
  // Typed buffer loads are limited to one load of up to 4 32-bit values.
  // Raw buffer loads might need multiple loads in chunks of 4.
  SmallVector<Value *, 4> Elts(NumComponents);
  for (unsigned I = 0; I < NumComponents;) {
    // Load 4 elements or however many less than 4 are left to load.
    unsigned ChunkSize = std::min(NumComponents - I, 4U);

    // Assign mask for raw buffer loads.
    if (Opcode == OP::OpCode::RawBufferLoad) {
      Args[DXIL::OperandIndex::kRawBufferLoadMaskOpIdx] =
          getRawBufferMaskForETy(EltTy, ChunkSize, OP);
      // If we've loaded a chunk already, update offset to next chunk.
      if (FirstLd != nullptr)
        Args[OffsetIdx] =
            Builder.CreateAdd(Args[OffsetIdx], OP->GetU32Const(4 * LdSize));
    }

    Function *F = OP->GetOpFunc(Opcode, EltTy);
    Value *Ld = Builder.CreateCall(F, Args, OP::GetOpCodeName(Opcode));
    unsigned StatusIndex;

    // Extract elements from returned ResRet.
    // Native vector loads just have one vector element in the ResRet.
    // Others have up to four scalars that need to be individually extracted.
    if (Opcode == OP::OpCode::RawBufferVectorLoad) {
      Elts[I++] = Builder.CreateExtractValue(Ld, 0);
      StatusIndex = DXIL::kVecResRetStatusIndex;
    } else {
      for (unsigned J = 0; J < ChunkSize; J++, I++)
        Elts[I] = Builder.CreateExtractValue(Ld, J);
      StatusIndex = DXIL::kResRetStatusIndex;
    }

    // Update status.
    updateStatus(Ld, Helper.Status, Builder, OP, StatusIndex);

    if (!FirstLd)
      FirstLd = Ld;
  }
  DXASSERT(FirstLd, "No loads created by TranslateBufLoad");

  // Convert loaded 32-bit integers to intended 64-bit type representation.
  if (IsTyped) {
    Type *RegEltTy = Ty->getScalarType();
    if (RegEltTy->isDoubleTy()) {
      Function *MakeDouble = OP->GetOpFunc(DXIL::OpCode::MakeDouble, RegEltTy);
      Value *MakeDoubleOpArg =
          Builder.getInt32((unsigned)DXIL::OpCode::MakeDouble);
      NumComponents /= 2; // Convert back to number of doubles.
      for (unsigned I = 0; I < NumComponents; I++) {
        Value *Lo = Elts[2 * I];
        Value *Hi = Elts[2 * I + 1];
        Elts[I] = Builder.CreateCall(MakeDouble, {MakeDoubleOpArg, Lo, Hi});
      }
      EltTy = RegEltTy;
    } else if (RegEltTy->isIntegerTy(64)) {
      NumComponents /= 2; // Convert back to number of int64s.
      for (unsigned I = 0; I < NumComponents; I++) {
        Value *Lo = Elts[2 * I];
        Value *Hi = Elts[2 * I + 1];
        Lo = Builder.CreateZExt(Lo, RegEltTy);
        Hi = Builder.CreateZExt(Hi, RegEltTy);
        Hi = Builder.CreateShl(Hi, 32);
        Elts[I] = Builder.CreateOr(Lo, Hi);
      }
      EltTy = RegEltTy;
    }
  }

  // Package elements into a vector as needed.
  Value *RetValNew = nullptr;
  // Scalar or native vector loads need not construct vectors from elements.
  if (!Ty->isVectorTy() || Opcode == OP::OpCode::RawBufferVectorLoad) {
    RetValNew = Elts[0];
  } else {
    RetValNew = UndefValue::get(VectorType::get(EltTy, NumComponents));
    for (unsigned I = 0; I < NumComponents; I++)
      RetValNew = Builder.CreateInsertElement(RetValNew, Elts[I], I);
  }

  // Convert loaded int32 bool results to i1 register representation.
  if (IsBool)
    RetValNew = Builder.CreateICmpNE(
        RetValNew, Constant::getNullValue(RetValNew->getType()));

  Helper.RetVal->replaceAllUsesWith(RetValNew);
  Helper.RetVal = RetValNew;

  return FirstLd;
}

Value *translateResourceLoad(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                             HLOperationLowerHelper &Helper,
                             HLObjectOperationLowerHelper *PObjHelper,
                             bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  DataLayout &DL = Helper.DL;
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);

  IRBuilder<> Builder(CI);

  DXIL::ResourceClass RC = PObjHelper->getRc(Handle);
  DXIL::ResourceKind RK = PObjHelper->getRk(Handle);

  ResLoadHelper LdHelper(CI, RK, RC, Handle, IOP);
  Type *Ty = CI->getType();
  Value *Ld = nullptr;
  if (Ty->isPointerTy()) {
    DXASSERT(!DxilResource::IsAnyTexture(RK),
             "Textures should not be treated as structured buffers.");
    translateStructBufSubscript(cast<CallInst>(LdHelper.RetVal), Handle,
                                LdHelper.Status, HlslOp, RK, DL);
  } else {
    Ld = translateBufLoad(LdHelper, RK, Builder, HlslOp, DL);
    dxilutil::MigrateDebugValue(CI, Ld);
  }
  // CI is replaced by above translation calls..
  return nullptr;
}

// Split { v0, v1 } to { v0.lo, v0.hi, v1.lo, v1.hi }
void split64bitValForStore(Type *EltTy, ArrayRef<Value *> Vals, unsigned Size,
                           MutableArrayRef<Value *> Vals32, hlsl::OP *HlslOp,
                           IRBuilder<> &Builder) {
  Type *I32Ty = Builder.getInt32Ty();
  Type *DoubleTy = Builder.getDoubleTy();
  Value *UndefI32 = UndefValue::get(I32Ty);

  if (EltTy == DoubleTy) {
    Function *DToU = HlslOp->GetOpFunc(DXIL::OpCode::SplitDouble, DoubleTy);
    Value *DToUOpArg = Builder.getInt32((unsigned)DXIL::OpCode::SplitDouble);
    for (unsigned I = 0; I < Size; I++) {
      if (isa<UndefValue>(Vals[I])) {
        Vals32[2 * I] = UndefI32;
        Vals32[2 * I + 1] = UndefI32;
      } else {
        Value *RetVal = Builder.CreateCall(DToU, {DToUOpArg, Vals[I]});
        Value *Lo = Builder.CreateExtractValue(RetVal, 0);
        Value *Hi = Builder.CreateExtractValue(RetVal, 1);
        Vals32[2 * I] = Lo;
        Vals32[2 * I + 1] = Hi;
      }
    }
  } else {
    for (unsigned I = 0; I < Size; I++) {
      if (isa<UndefValue>(Vals[I])) {
        Vals32[2 * I] = UndefI32;
        Vals32[2 * I + 1] = UndefI32;
      } else {
        Value *Lo = Builder.CreateTrunc(Vals[I], I32Ty);
        Value *Hi = Builder.CreateLShr(Vals[I], 32);
        Hi = Builder.CreateTrunc(Hi, I32Ty);
        Vals32[2 * I] = Lo;
        Vals32[2 * I + 1] = Hi;
      }
    }
  }
}

void translateStore(DxilResource::Kind RK, Value *Handle, Value *Val,
                    Value *Idx, Value *Offset, IRBuilder<> &Builder,
                    hlsl::OP *OP, Value *SampIdx = nullptr) {
  Type *Ty = Val->getType();
  OP::OpCode Opcode = OP::OpCode::NumOpCodes;
  bool IsTyped = true;
  switch (RK) {
  case DxilResource::Kind::RawBuffer:
  case DxilResource::Kind::StructuredBuffer:
    IsTyped = false;
    Opcode = OP::OpCode::RawBufferStore;
    // Where shader model and type allows, use vector store intrinsic.
    if (OP->GetModule()->GetHLModule().GetShaderModel()->IsSM69Plus() &&
        Ty->isVectorTy() && Ty->getVectorNumElements() > 1)
      Opcode = OP::OpCode::RawBufferVectorStore;
    break;
  case DxilResource::Kind::TypedBuffer:
    Opcode = OP::OpCode::BufferStore;
    break;
  case DxilResource::Kind::Invalid:
    DXASSERT(0, "invalid resource kind");
    break;
  case DxilResource::Kind::Texture2DMS:
  case DxilResource::Kind::Texture2DMSArray:
    Opcode = OP::OpCode::TextureStoreSample;
    break;
  default:
    Opcode = OP::OpCode::TextureStore;
    break;
  }

  Type *I32Ty = Builder.getInt32Ty();
  Type *I64Ty = Builder.getInt64Ty();
  Type *DoubleTy = Builder.getDoubleTy();
  Type *EltTy = Ty->getScalarType();
  if (EltTy->isIntegerTy(1)) {
    // Since we're going to memory, convert bools to their memory
    // representation.
    EltTy = I32Ty;
    if (Ty->isVectorTy())
      Ty = VectorType::get(EltTy, Ty->getVectorNumElements());
    else
      Ty = EltTy;
    Val = Builder.CreateZExt(Val, Ty);
  }

  // If RawBuffer store of 64-bit value, don't set alignment to 8,
  // since buffer alignment isn't known to be anything over 4.
  unsigned AlignValue = OP->GetAllocSizeForType(EltTy);
  if (RK == HLResource::Kind::RawBuffer && AlignValue > 4)
    AlignValue = 4;
  Constant *Alignment = OP->GetI32Const(AlignValue);
  bool Is64 = EltTy == I64Ty || EltTy == DoubleTy;
  if (Is64 && IsTyped) {
    EltTy = I32Ty;
  }

  llvm::Constant *OpArg = OP->GetU32Const((unsigned)Opcode);

  llvm::Value *UndefI =
      llvm::UndefValue::get(llvm::Type::getInt32Ty(Ty->getContext()));

  llvm::Value *UndefVal = llvm::UndefValue::get(Ty->getScalarType());

  SmallVector<Value *, 13> StoreArgs;
  StoreArgs.emplace_back(OpArg);  // opcode
  StoreArgs.emplace_back(Handle); // resource handle

  unsigned OffsetIdx = 0;
  if (Opcode == OP::OpCode::RawBufferStore ||
      Opcode == OP::OpCode::RawBufferVectorStore ||
      Opcode == OP::OpCode::BufferStore) {
    // Append Coord0 (Index) value.
    if (Idx->getType()->isVectorTy()) {
      Value *ScalarIdx = Builder.CreateExtractElement(Idx, (uint64_t)0);
      StoreArgs.emplace_back(ScalarIdx); // Coord0 (Index).
    } else {
      StoreArgs.emplace_back(Idx); // Coord0 (Index).
    }

    // Store OffsetIdx representing the argument that may need to be incremented
    // later to load additional chunks of data.
    // Only structured buffers can use the offset parameter.
    // Others must increment the index.
    if (RK == DxilResource::Kind::StructuredBuffer)
      OffsetIdx = StoreArgs.size();
    else
      OffsetIdx = StoreArgs.size() - 1;

    // Coord1 (Offset).
    StoreArgs.emplace_back(Offset);
  } else {
    // texture store
    unsigned CoordSize = DxilResource::GetNumCoords(RK);

    // Set x first.
    if (Idx->getType()->isVectorTy())
      StoreArgs.emplace_back(Builder.CreateExtractElement(Idx, (uint64_t)0));
    else
      StoreArgs.emplace_back(Idx);

    for (unsigned I = 1; I < 3; I++) {
      if (I < CoordSize)
        StoreArgs.emplace_back(Builder.CreateExtractElement(Idx, I));
      else
        StoreArgs.emplace_back(UndefI);
    }
    // TODO: support mip for texture ST
  }

  // RawBufferVectorStore only takes a single value and alignment arguments.
  if (Opcode == DXIL::OpCode::RawBufferVectorStore) {
    StoreArgs.emplace_back(Val);
    StoreArgs.emplace_back(Alignment);
    Function *F = OP->GetOpFunc(DXIL::OpCode::RawBufferVectorStore, Ty);
    Builder.CreateCall(F, StoreArgs);
    return;
  }
  Function *F = OP->GetOpFunc(Opcode, EltTy);

  constexpr unsigned MaxStoreElemCount = 4;
  const unsigned CompCount = Ty->isVectorTy() ? Ty->getVectorNumElements() : 1;
  const unsigned StoreInstCount =
      (CompCount / MaxStoreElemCount) + (CompCount % MaxStoreElemCount != 0);
  SmallVector<decltype(StoreArgs), 4> StoreArgsList;

  // Max number of element to store should be 16 (for a 4x4 matrix)
  DXASSERT_NOMSG(StoreInstCount >= 1 && StoreInstCount <= 4);

  // If number of elements to store exceeds the maximum number of elements
  // that can be stored in a single store call,  make sure to generate enough
  // store calls to store all elements
  for (unsigned J = 0; J < StoreInstCount; J++) {
    decltype(StoreArgs) NewStoreArgs;
    for (Value *StoreArg : StoreArgs)
      NewStoreArgs.emplace_back(StoreArg);
    StoreArgsList.emplace_back(NewStoreArgs);
  }

  for (unsigned J = 0; J < StoreArgsList.size(); J++) {
    // For second and subsequent store calls, increment the resource-appropriate
    // index or offset parameter.
    if (J > 0) {
      unsigned EltSize = OP->GetAllocSizeForType(EltTy);
      unsigned NewCoord = EltSize * MaxStoreElemCount * J;
      Value *NewCoordVal = ConstantInt::get(Builder.getInt32Ty(), NewCoord);
      NewCoordVal = Builder.CreateAdd(StoreArgsList[0][OffsetIdx], NewCoordVal);
      StoreArgsList[J][OffsetIdx] = NewCoordVal;
    }

    // Set value parameters.
    uint8_t Mask = 0;
    if (Ty->isVectorTy()) {
      unsigned VecSize =
          std::min((J + 1) * MaxStoreElemCount, Ty->getVectorNumElements()) -
          (J * MaxStoreElemCount);
      Value *EmptyVal = UndefVal;
      if (IsTyped) {
        Mask = DXIL::kCompMask_All;
        EmptyVal = Builder.CreateExtractElement(Val, (uint64_t)0);
      }

      for (unsigned I = 0; I < MaxStoreElemCount; I++) {
        if (I < VecSize) {
          StoreArgsList[J].emplace_back(
              Builder.CreateExtractElement(Val, (J * MaxStoreElemCount) + I));
          Mask |= (1 << I);
        } else {
          StoreArgsList[J].emplace_back(EmptyVal);
        }
      }

    } else {
      if (IsTyped) {
        Mask = DXIL::kCompMask_All;
        StoreArgsList[J].emplace_back(Val);
        StoreArgsList[J].emplace_back(Val);
        StoreArgsList[J].emplace_back(Val);
        StoreArgsList[J].emplace_back(Val);
      } else {
        StoreArgsList[J].emplace_back(Val);
        StoreArgsList[J].emplace_back(UndefVal);
        StoreArgsList[J].emplace_back(UndefVal);
        StoreArgsList[J].emplace_back(UndefVal);
        Mask = DXIL::kCompMask_X;
      }
    }

    if (Is64 && IsTyped) {
      unsigned Size = 1;
      if (Ty->isVectorTy()) {
        Size =
            std::min((J + 1) * MaxStoreElemCount, Ty->getVectorNumElements()) -
            (J * MaxStoreElemCount);
      }
      DXASSERT(Size <= 2, "raw/typed buffer only allow 4 dwords");
      unsigned Val0OpIdx = Opcode == DXIL::OpCode::TextureStore ||
                                   Opcode == DXIL::OpCode::TextureStoreSample
                               ? DXIL::OperandIndex::kTextureStoreVal0OpIdx
                               : DXIL::OperandIndex::kBufferStoreVal0OpIdx;
      Value *V0 = StoreArgsList[J][Val0OpIdx];
      Value *V1 = StoreArgsList[J][Val0OpIdx + 1];

      Value *Vals32[4];
      EltTy = Ty->getScalarType();
      split64bitValForStore(EltTy, {V0, V1}, Size, Vals32, OP, Builder);
      // Fill the uninit vals.
      if (Size == 1) {
        Vals32[2] = Vals32[0];
        Vals32[3] = Vals32[1];
      }
      // Change valOp to 32 version.
      for (unsigned I = 0; I < 4; I++) {
        StoreArgsList[J][Val0OpIdx + I] = Vals32[I];
      }
      // change mask for double
      if (Opcode == DXIL::OpCode::RawBufferStore) {
        Mask = Size == 1 ? DXIL::kCompMask_X | DXIL::kCompMask_Y
                         : DXIL::kCompMask_All;
      }
    }

    StoreArgsList[J].emplace_back(OP->GetU8Const(Mask)); // mask
    if (Opcode == DXIL::OpCode::RawBufferStore)
      StoreArgsList[J].emplace_back(Alignment); // alignment only for raw buffer
    else if (Opcode == DXIL::OpCode::TextureStoreSample) {
      StoreArgsList[J].emplace_back(
          SampIdx ? SampIdx
                  : Builder.getInt32(0)); // sample idx only for MS textures
    }
    Builder.CreateCall(F, StoreArgsList[J]);
  }
}

Value *translateResourceStore(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                              HLOperationLowerHelper &Helper,
                              HLObjectOperationLowerHelper *PObjHelper,
                              bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);

  IRBuilder<> Builder(CI);
  DXIL::ResourceKind RK = PObjHelper->getRk(Handle);

  Value *Val = CI->getArgOperand(HLOperandIndex::kStoreValOpIdx);
  Value *Offset = CI->getArgOperand(HLOperandIndex::kStoreOffsetOpIdx);
  Value *UndefI = UndefValue::get(Builder.getInt32Ty());
  translateStore(RK, Handle, Val, Offset, UndefI, Builder, HlslOp);

  return nullptr;
}
} // namespace

// Atomic intrinsics.
namespace {
// Atomic intrinsics.
struct AtomicHelper {
  AtomicHelper(CallInst *CI, OP::OpCode Op, Value *H, Type *OpType = nullptr);
  AtomicHelper(CallInst *CI, OP::OpCode Op, Value *H, Value *BufIdx,
               Value *BaseOffset, Type *OpType = nullptr);
  OP::OpCode Opcode;
  Value *Handle;
  Value *Addr;
  Value *Offset; // Offset for structrued buffer.
  Value *Val;
  Value *OriginalValue;
  Value *CompareValue;
  Type *OperationType;
};

// For MOP version of Interlocked*.
AtomicHelper::AtomicHelper(CallInst *CI, OP::OpCode Op, Value *H, Type *OpType)
    : Opcode(Op), Handle(H), Offset(nullptr), OriginalValue(nullptr),
      OperationType(OpType) {
  Addr = CI->getArgOperand(HLOperandIndex::kObjectInterlockedDestOpIndex);
  if (Op == OP::OpCode::AtomicCompareExchange) {
    CompareValue = CI->getArgOperand(
        HLOperandIndex::kObjectInterlockedCmpCompareValueOpIndex);
    Val =
        CI->getArgOperand(HLOperandIndex::kObjectInterlockedCmpValueOpIndex);
    if (CI->getNumArgOperands() ==
        (HLOperandIndex::kObjectInterlockedCmpOriginalValueOpIndex + 1))
      OriginalValue = CI->getArgOperand(
          HLOperandIndex::kObjectInterlockedCmpOriginalValueOpIndex);
  } else {
    Val = CI->getArgOperand(HLOperandIndex::kObjectInterlockedValueOpIndex);
    if (CI->getNumArgOperands() ==
        (HLOperandIndex::kObjectInterlockedOriginalValueOpIndex + 1))
      OriginalValue = CI->getArgOperand(
          HLOperandIndex::kObjectInterlockedOriginalValueOpIndex);
  }
  if (nullptr == OperationType)
    OperationType = Val->getType();
}
// For IOP version of Interlocked*.
AtomicHelper::AtomicHelper(CallInst *CI, OP::OpCode Op, Value *H, Value *BufIdx,
                           Value *BaseOffset, Type *OpType)
    : Opcode(Op), Handle(H), Addr(BufIdx), Offset(BaseOffset),
      OriginalValue(nullptr), OperationType(OpType) {
  if (Op == OP::OpCode::AtomicCompareExchange) {
    CompareValue =
        CI->getArgOperand(HLOperandIndex::kInterlockedCmpCompareValueOpIndex);
    Val = CI->getArgOperand(HLOperandIndex::kInterlockedCmpValueOpIndex);
    if (CI->getNumArgOperands() ==
        (HLOperandIndex::kInterlockedCmpOriginalValueOpIndex + 1))
      OriginalValue = CI->getArgOperand(
          HLOperandIndex::kInterlockedCmpOriginalValueOpIndex);
  } else {
    Val = CI->getArgOperand(HLOperandIndex::kInterlockedValueOpIndex);
    if (CI->getNumArgOperands() ==
        (HLOperandIndex::kInterlockedOriginalValueOpIndex + 1))
      OriginalValue =
          CI->getArgOperand(HLOperandIndex::kInterlockedOriginalValueOpIndex);
  }
  if (nullptr == OperationType)
    OperationType = Val->getType();
}

void translateAtomicBinaryOperation(AtomicHelper &Helper,
                                    DXIL::AtomicBinOpCode AtomicOp,
                                    IRBuilder<> &Builder, hlsl::OP *HlslOp) {
  Value *Handle = Helper.Handle;
  Value *Addr = Helper.Addr;
  Value *Val = Helper.Val;
  Type *Ty = Helper.OperationType;
  Type *ValTy = Val->getType();

  Value *UndefI = UndefValue::get(Type::getInt32Ty(Ty->getContext()));

  Function *DxilAtomic = HlslOp->GetOpFunc(Helper.Opcode, Ty->getScalarType());
  Value *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(Helper.Opcode));
  Value *AtomicOpArg = HlslOp->GetU32Const(static_cast<unsigned>(AtomicOp));

  if (Ty != ValTy)
    Val = Builder.CreateBitCast(Val, Ty);

  Value *Args[] = {OpArg,  Handle, AtomicOpArg,
                   UndefI, UndefI, UndefI, // coordinates
                   Val};

  // Setup coordinates.
  if (Addr->getType()->isVectorTy()) {
    unsigned VectorNumElements = Addr->getType()->getVectorNumElements();
    DXASSERT(VectorNumElements <= 3, "up to 3 elements for atomic binary op");
    assert(VectorNumElements <= 3);
    for (unsigned I = 0; I < VectorNumElements; I++) {
      Value *Elt = Builder.CreateExtractElement(Addr, I);
      Args[DXIL::OperandIndex::kAtomicBinOpCoord0OpIdx + I] = Elt;
    }
  } else
    Args[DXIL::OperandIndex::kAtomicBinOpCoord0OpIdx] = Addr;

  // Set offset for structured buffer.
  if (Helper.Offset)
    Args[DXIL::OperandIndex::kAtomicBinOpCoord1OpIdx] = Helper.Offset;

  Value *OrigVal =
      Builder.CreateCall(DxilAtomic, Args, HlslOp->GetAtomicOpName(AtomicOp));
  if (Helper.OriginalValue) {
    if (Ty != ValTy)
      OrigVal = Builder.CreateBitCast(OrigVal, ValTy);
    Builder.CreateStore(OrigVal, Helper.OriginalValue);
  }
}

Value *translateMopAtomicBinaryOperation(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;

  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  IRBuilder<> Builder(CI);

  switch (IOP) {
  case IntrinsicOp::MOP_InterlockedAdd:
  case IntrinsicOp::MOP_InterlockedAdd64: {
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle);
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::Add, Builder,
                                   HlslOp);
  } break;
  case IntrinsicOp::MOP_InterlockedAnd:
  case IntrinsicOp::MOP_InterlockedAnd64: {
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle);
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::And, Builder,
                                   HlslOp);
  } break;
  case IntrinsicOp::MOP_InterlockedExchange:
  case IntrinsicOp::MOP_InterlockedExchange64: {
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle);
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::Exchange,
                                   Builder, HlslOp);
  } break;
  case IntrinsicOp::MOP_InterlockedExchangeFloat: {
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle,
                        Type::getInt32Ty(CI->getContext()));
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::Exchange,
                                   Builder, HlslOp);
  } break;
  case IntrinsicOp::MOP_InterlockedMax:
  case IntrinsicOp::MOP_InterlockedMax64: {
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle);
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::IMax, Builder,
                                   HlslOp);
  } break;
  case IntrinsicOp::MOP_InterlockedMin:
  case IntrinsicOp::MOP_InterlockedMin64: {
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle);
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::IMin, Builder,
                                   HlslOp);
  } break;
  case IntrinsicOp::MOP_InterlockedUMax: {
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle);
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::UMax, Builder,
                                   HlslOp);
  } break;
  case IntrinsicOp::MOP_InterlockedUMin: {
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle);
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::UMin, Builder,
                                   HlslOp);
  } break;
  case IntrinsicOp::MOP_InterlockedOr:
  case IntrinsicOp::MOP_InterlockedOr64: {
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle);
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::Or, Builder,
                                   HlslOp);
  } break;
  case IntrinsicOp::MOP_InterlockedXor:
  case IntrinsicOp::MOP_InterlockedXor64:
  default: {
    DXASSERT(IOP == IntrinsicOp::MOP_InterlockedXor ||
                 IOP == IntrinsicOp::MOP_InterlockedXor64,
             "invalid MOP atomic intrinsic");
    AtomicHelper Helper(CI, DXIL::OpCode::AtomicBinOp, Handle);
    translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::Xor, Builder,
                                   HlslOp);
  } break;
  }

  return nullptr;
}
void translateAtomicCmpXChg(AtomicHelper &Helper, IRBuilder<> &Builder,
                            hlsl::OP *HlslOp) {
  Value *Handle = Helper.Handle;
  Value *Addr = Helper.Addr;
  Value *Val = Helper.Val;
  Value *CmpVal = Helper.CompareValue;

  Type *Ty = Helper.OperationType;
  Type *ValTy = Val->getType();

  Value *UndefI = UndefValue::get(Type::getInt32Ty(Ty->getContext()));

  Function *DxilAtomic = HlslOp->GetOpFunc(Helper.Opcode, Ty->getScalarType());
  Value *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(Helper.Opcode));

  if (Ty != ValTy) {
    Val = Builder.CreateBitCast(Val, Ty);
    if (CmpVal)
      CmpVal = Builder.CreateBitCast(CmpVal, Ty);
  }

  Value *Args[] = {OpArg,  Handle, UndefI, UndefI, UndefI, // coordinates
                   CmpVal, Val};

  // Setup coordinates.
  if (Addr->getType()->isVectorTy()) {
    unsigned VectorNumElements = Addr->getType()->getVectorNumElements();
    DXASSERT(VectorNumElements <= 3, "up to 3 elements in atomic op");
    assert(VectorNumElements <= 3);
    for (unsigned I = 0; I < VectorNumElements; I++) {
      Value *Elt = Builder.CreateExtractElement(Addr, I);
      Args[DXIL::OperandIndex::kAtomicCmpExchangeCoord0OpIdx + I] = Elt;
    }
  } else
    Args[DXIL::OperandIndex::kAtomicCmpExchangeCoord0OpIdx] = Addr;

  // Set offset for structured buffer.
  if (Helper.Offset)
    Args[DXIL::OperandIndex::kAtomicCmpExchangeCoord1OpIdx] = Helper.Offset;

  Value *OrigVal = Builder.CreateCall(DxilAtomic, Args);
  if (Helper.OriginalValue) {
    if (Ty != ValTy)
      OrigVal = Builder.CreateBitCast(OrigVal, ValTy);
    Builder.CreateStore(OrigVal, Helper.OriginalValue);
  }
}

Value *translateMopAtomicCmpXChg(CallInst *CI, IntrinsicOp IOP,
                                 OP::OpCode Opcode,
                                 HLOperationLowerHelper &Helper,
                                 HLObjectOperationLowerHelper *PObjHelper,
                                 bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;

  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  IRBuilder<> Builder(CI);
  Type *OpType = nullptr;
  if (IOP == IntrinsicOp::MOP_InterlockedCompareStoreFloatBitwise ||
      IOP == IntrinsicOp::MOP_InterlockedCompareExchangeFloatBitwise)
    OpType = Type::getInt32Ty(CI->getContext());
  AtomicHelper AtomicHelper(CI, OP::OpCode::AtomicCompareExchange, Handle,
                            OpType);
  translateAtomicCmpXChg(AtomicHelper, Builder, HlslOp);
  return nullptr;
}

void translateSharedMemOrNodeAtomicBinOp(CallInst *CI, IntrinsicOp IOP,
                                         Value *Addr) {
  AtomicRMWInst::BinOp Op;
  IRBuilder<> Builder(CI);
  Value *Val = CI->getArgOperand(HLOperandIndex::kInterlockedValueOpIndex);
  PointerType *PtrType = dyn_cast<PointerType>(
      CI->getArgOperand(HLOperandIndex::kInterlockedDestOpIndex)->getType());
  bool NeedCast = PtrType && PtrType->getElementType()->isFloatTy();
  switch (IOP) {
  case IntrinsicOp::IOP_InterlockedAdd:
    Op = AtomicRMWInst::BinOp::Add;
    break;
  case IntrinsicOp::IOP_InterlockedAnd:
    Op = AtomicRMWInst::BinOp::And;
    break;
  case IntrinsicOp::IOP_InterlockedExchange:
    if (NeedCast) {
      Val = Builder.CreateBitCast(Val, Type::getInt32Ty(CI->getContext()));
      Addr = Builder.CreateBitCast(
          Addr, Type::getInt32PtrTy(CI->getContext(),
                                    Addr->getType()->getPointerAddressSpace()));
    }
    Op = AtomicRMWInst::BinOp::Xchg;
    break;
  case IntrinsicOp::IOP_InterlockedMax:
    Op = AtomicRMWInst::BinOp::Max;
    break;
  case IntrinsicOp::IOP_InterlockedUMax:
    Op = AtomicRMWInst::BinOp::UMax;
    break;
  case IntrinsicOp::IOP_InterlockedMin:
    Op = AtomicRMWInst::BinOp::Min;
    break;
  case IntrinsicOp::IOP_InterlockedUMin:
    Op = AtomicRMWInst::BinOp::UMin;
    break;
  case IntrinsicOp::IOP_InterlockedOr:
    Op = AtomicRMWInst::BinOp::Or;
    break;
  case IntrinsicOp::IOP_InterlockedXor:
  default:
    DXASSERT(IOP == IntrinsicOp::IOP_InterlockedXor, "Invalid Intrinsic");
    Op = AtomicRMWInst::BinOp::Xor;
    break;
  }

  Value *Result = Builder.CreateAtomicRMW(
      Op, Addr, Val, AtomicOrdering::SequentiallyConsistent);
  if (CI->getNumArgOperands() >
      HLOperandIndex::kInterlockedOriginalValueOpIndex) {
    if (NeedCast)
      Result =
          Builder.CreateBitCast(Result, Type::getFloatTy(CI->getContext()));
    Builder.CreateStore(
        Result,
        CI->getArgOperand(HLOperandIndex::kInterlockedOriginalValueOpIndex));
  }
}

static Value *skipAddrSpaceCast(Value *Ptr) {
  if (AddrSpaceCastInst *CastInst = dyn_cast<AddrSpaceCastInst>(Ptr))
    return CastInst->getOperand(0);
  if (ConstantExpr *ConstExpr = dyn_cast<ConstantExpr>(Ptr)) {
    if (ConstExpr->getOpcode() == Instruction::AddrSpaceCast) {
      return ConstExpr->getOperand(0);
    }
  }
  return Ptr;
}

Value *
translateNodeIncrementOutputCount(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                                  HLOperationLowerHelper &Helper,
                                  HLObjectOperationLowerHelper *PObjHelper,
                                  bool IsPerThread, bool &Translated) {

  hlsl::OP *OP = &Helper.HlslOp;
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  Value *Count =
      CI->getArgOperand(HLOperandIndex::kIncrementOutputCountCountIdx);
  Function *DxilFunc = OP->GetOpFunc(Op, CI->getType());
  Value *OpArg = OP->GetU32Const((unsigned)Op);
  Value *PerThread = OP->GetI1Const(IsPerThread);

  Value *Args[] = {OpArg, Handle, Count, PerThread};

  IRBuilder<> Builder(CI);
  Builder.CreateCall(DxilFunc, Args);
  return nullptr;
}

/*
HLSL:
void EmptyNodeOutput::GroupIncrementOutputCount(uint count)
DXIL:
void @dx.op.groupIncrementOutputCount(i32 %Opcode, %dx.types.NodeHandle
%NodeOutput, i32 count)
*/
Value *translateNodeGroupIncrementOutputCount(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  return translateNodeIncrementOutputCount(CI, IOP, Op, Helper, PObjHelper,
                                           /*isPerThread*/ false, Translated);
}

/*
HLSL:
void EmptyNodeOutput::ThreadIncrementOutputCount(uint count)
DXIL:
void @dx.op.threadIncrementOutputCount(i32 %Opcode, %dx.types.NodeHandle
%NodeOutput, i32 count)
*/
Value *translateNodeThreadIncrementOutputCount(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  return translateNodeIncrementOutputCount(CI, IOP, Op, Helper, PObjHelper,
                                           /*isPerThread*/ true, Translated);
}

// For known non-groupshared, verify that the destination param is valid
void validateAtomicDestination(CallInst *CI,
                               HLObjectOperationLowerHelper *PObjHelper) {
  Value *Dest = CI->getArgOperand(HLOperandIndex::kInterlockedDestOpIndex);
  // If we encounter a gep, we may provide a more specific error message
  bool HasGep = isa<GetElementPtrInst>(Dest);

  // Confirm that dest is a properly-used UAV

  // Drill through subscripts and geps, anything else indicates a misuse
  while (true) {
    if (GetElementPtrInst *Gep = dyn_cast<GetElementPtrInst>(Dest)) {
      Dest = Gep->getPointerOperand();
      continue;
    }
    if (CallInst *Handle = dyn_cast<CallInst>(Dest)) {
      hlsl::HLOpcodeGroup Group =
          hlsl::GetHLOpcodeGroup(Handle->getCalledFunction());
      if (Group != HLOpcodeGroup::HLSubscript)
        break;
      Dest = Handle->getArgOperand(HLOperandIndex::kSubscriptObjectOpIdx);
      continue;
    }
    break;
  }

  if (PObjHelper->getRc(Dest) == DXIL::ResourceClass::UAV) {
    DXIL::ResourceKind RK = PObjHelper->getRk(Dest);
    if (DXIL::IsStructuredBuffer(RK))
      return; // no errors
    if (DXIL::IsTyped(RK)) {
      if (HasGep)
        dxilutil::EmitErrorOnInstruction(
            CI, "Typed resources used in atomic operations must have a scalar "
                "element type.");
      return; // error emitted or else no errors
    }
  }

  dxilutil::EmitErrorOnInstruction(
      CI, "Atomic operation targets must be groupshared, Node Record or UAV.");
}

Value *translateIopAtomicBinaryOperation(
    CallInst *CI, IntrinsicOp IOP, DXIL::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  Value *Addr = CI->getArgOperand(HLOperandIndex::kInterlockedDestOpIndex);
  Addr = skipAddrSpaceCast(Addr);

  unsigned AddressSpace = Addr->getType()->getPointerAddressSpace();
  if (AddressSpace == DXIL::kTGSMAddrSpace ||
      AddressSpace == DXIL::kNodeRecordAddrSpace)
    translateSharedMemOrNodeAtomicBinOp(CI, IOP, Addr);
  else {
    // If not groupshared or node record, we either have an error case or will
    // translate the atomic op in the process of translating users of the
    // subscript operator Mark not translated and validate dest param
    Translated = false;
    validateAtomicDestination(CI, PObjHelper);
  }

  return nullptr;
}

void translateSharedMemOrNodeAtomicCmpXChg(CallInst *CI, Value *Addr) {
  Value *Val = CI->getArgOperand(HLOperandIndex::kInterlockedCmpValueOpIndex);
  Value *CmpVal =
      CI->getArgOperand(HLOperandIndex::kInterlockedCmpCompareValueOpIndex);
  IRBuilder<> Builder(CI);

  PointerType *PtrType = dyn_cast<PointerType>(
      CI->getArgOperand(HLOperandIndex::kInterlockedDestOpIndex)->getType());
  bool NeedCast = false;
  if (PtrType && PtrType->getElementType()->isFloatTy()) {
    NeedCast = true;
    Val = Builder.CreateBitCast(Val, Type::getInt32Ty(CI->getContext()));
    CmpVal = Builder.CreateBitCast(CmpVal, Type::getInt32Ty(CI->getContext()));
    unsigned AddrSpace = cast<PointerType>(Addr->getType())->getAddressSpace();
    Addr = Builder.CreateBitCast(
        Addr, Type::getInt32PtrTy(CI->getContext(), AddrSpace));
  }

  Value *Result = Builder.CreateAtomicCmpXchg(
      Addr, CmpVal, Val, AtomicOrdering::SequentiallyConsistent,
      AtomicOrdering::SequentiallyConsistent);

  if (CI->getNumArgOperands() >
      HLOperandIndex::kInterlockedCmpOriginalValueOpIndex) {
    Value *OriginVal = Builder.CreateExtractValue(Result, 0);
    if (NeedCast)
      OriginVal =
          Builder.CreateBitCast(OriginVal, Type::getFloatTy(CI->getContext()));
    Builder.CreateStore(
        OriginVal,
        CI->getArgOperand(HLOperandIndex::kInterlockedCmpOriginalValueOpIndex));
  }
}

Value *translateIopAtomicCmpXChg(CallInst *CI, IntrinsicOp IOP,
                                 DXIL::OpCode Opcode,
                                 HLOperationLowerHelper &Helper,
                                 HLObjectOperationLowerHelper *PObjHelper,
                                 bool &Translated) {
  Value *Addr = CI->getArgOperand(HLOperandIndex::kInterlockedDestOpIndex);
  Addr = skipAddrSpaceCast(Addr);

  unsigned AddressSpace = Addr->getType()->getPointerAddressSpace();
  if (AddressSpace == DXIL::kTGSMAddrSpace ||
      AddressSpace == DXIL::kNodeRecordAddrSpace)
    translateSharedMemOrNodeAtomicCmpXChg(CI, Addr);
  else {
    // If not groupshared, we either have an error case or will translate
    // the atomic op in the process of translating users of the subscript
    // operator Mark not translated and validate dest param
    Translated = false;
    validateAtomicDestination(CI, PObjHelper);
  }

  return nullptr;
}
} // namespace

// Process Tess Factor.
namespace {

// Clamp to [0.0f..1.0f], NaN->0.0f.
Value *cleanupTessFactorScale(Value *Input, hlsl::OP *HlslOp,
                              IRBuilder<> &Builder) {
  float FMin = 0;
  float FMax = 1;
  Type *F32Ty = Input->getType()->getScalarType();
  Value *MinFactor = ConstantFP::get(F32Ty, FMin);
  Value *MaxFactor = ConstantFP::get(F32Ty, FMax);
  Type *Ty = Input->getType();
  if (Ty->isVectorTy())
    MinFactor = splatToVector(MinFactor, Input->getType(), Builder);
  Value *Temp = trivialDxilBinaryOperation(DXIL::OpCode::FMax, Input, MinFactor,
                                           HlslOp, Builder);
  if (Ty->isVectorTy())
    MaxFactor = splatToVector(MaxFactor, Input->getType(), Builder);
  return trivialDxilBinaryOperation(DXIL::OpCode::FMin, Temp, MaxFactor, HlslOp,
                                    Builder);
}

// Clamp to [1.0f..Inf], NaN->1.0f.
Value *cleanupTessFactor(Value *Input, hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  float FMin = 1.0;
  Type *F32Ty = Input->getType()->getScalarType();
  Value *MinFactor = ConstantFP::get(F32Ty, FMin);
  MinFactor = splatToVector(MinFactor, Input->getType(), Builder);
  return trivialDxilBinaryOperation(DXIL::OpCode::FMax, Input, MinFactor,
                                    HlslOp, Builder);
}

// Do partitioning-specific clamping.
Value *clampTessFactor(Value *Input,
                       DXIL::TessellatorPartitioning PartitionMode,
                       hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  const unsigned KTessellatorMaxEvenTessellationFactor = 64;
  const unsigned KTessellatorMaxOddTessellationFactor = 63;

  const unsigned KTessellatorMinEvenTessellationFactor = 2;
  const unsigned KTessellatorMinOddTessellationFactor = 1;

  const unsigned KTessellatorMaxTessellationFactor = 64;

  float FMin;
  float FMax;
  switch (PartitionMode) {
  case DXIL::TessellatorPartitioning::Integer:
    FMin = KTessellatorMinOddTessellationFactor;
    FMax = KTessellatorMaxTessellationFactor;
    break;
  case DXIL::TessellatorPartitioning::Pow2:
    FMin = KTessellatorMinOddTessellationFactor;
    FMax = KTessellatorMaxEvenTessellationFactor;
    break;
  case DXIL::TessellatorPartitioning::FractionalOdd:
    FMin = KTessellatorMinOddTessellationFactor;
    FMax = KTessellatorMaxOddTessellationFactor;
    break;
  case DXIL::TessellatorPartitioning::FractionalEven:
  default:
    DXASSERT(PartitionMode == DXIL::TessellatorPartitioning::FractionalEven,
             "invalid partition mode");
    FMin = KTessellatorMinEvenTessellationFactor;
    FMax = KTessellatorMaxEvenTessellationFactor;
    break;
  }
  Type *F32Ty = Input->getType()->getScalarType();
  Value *MinFactor = ConstantFP::get(F32Ty, FMin);
  Value *MaxFactor = ConstantFP::get(F32Ty, FMax);
  Type *Ty = Input->getType();
  if (Ty->isVectorTy())
    MinFactor = splatToVector(MinFactor, Input->getType(), Builder);
  Value *Temp = trivialDxilBinaryOperation(DXIL::OpCode::FMax, Input, MinFactor,
                                           HlslOp, Builder);
  if (Ty->isVectorTy())
    MaxFactor = splatToVector(MaxFactor, Input->getType(), Builder);
  return trivialDxilBinaryOperation(DXIL::OpCode::FMin, Temp, MaxFactor, HlslOp,
                                    Builder);
}

// round up for integer/pow2 partitioning
// note that this code assumes the inputs should be in the range [1, inf),
// which should be enforced by the clamp above.
Value *roundUpTessFactor(Value *Input,
                         DXIL::TessellatorPartitioning PartitionMode,
                         hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  switch (PartitionMode) {
  case DXIL::TessellatorPartitioning::Integer:
    return trivialDxilUnaryOperation(DXIL::OpCode::Round_pi, Input, HlslOp,
                                     Builder);
  case DXIL::TessellatorPartitioning::Pow2: {
    const unsigned KExponentMask = 0x7f800000;
    const unsigned KExponentLsb = 0x00800000;
    const unsigned KMantissaMask = 0x007fffff;
    Type *Ty = Input->getType();
    // (val = (asuint(val) & mantissamask) ?
    //      (asuint(val) & exponentmask) + exponentbump :
    //      asuint(val) & exponentmask;
    Type *UintTy = Type::getInt32Ty(Ty->getContext());
    if (Ty->isVectorTy())
      UintTy = VectorType::get(UintTy, Ty->getVectorNumElements());
    Value *UintVal =
        Builder.CreateCast(Instruction::CastOps::FPToUI, Input, UintTy);

    Value *MantMask = ConstantInt::get(UintTy->getScalarType(), KMantissaMask);
    MantMask = splatToVector(MantMask, UintTy, Builder);
    Value *ManVal = Builder.CreateAnd(UintVal, MantMask);

    Value *ExpMask = ConstantInt::get(UintTy->getScalarType(), KExponentMask);
    ExpMask = splatToVector(ExpMask, UintTy, Builder);
    Value *ExpVal = Builder.CreateAnd(UintVal, ExpMask);

    Value *ExpLsb = ConstantInt::get(UintTy->getScalarType(), KExponentLsb);
    ExpLsb = splatToVector(ExpLsb, UintTy, Builder);
    Value *NewExpVal = Builder.CreateAdd(ExpVal, ExpLsb);

    Value *ManValNotZero =
        Builder.CreateICmpEQ(ManVal, ConstantAggregateZero::get(UintTy));
    Value *Factors = Builder.CreateSelect(ManValNotZero, NewExpVal, ExpVal);
    return Builder.CreateUIToFP(Factors, Ty);
  } break;
  case DXIL::TessellatorPartitioning::FractionalEven:
  case DXIL::TessellatorPartitioning::FractionalOdd:
    return Input;
  default:
    DXASSERT(0, "invalid partition mode");
    return nullptr;
  }
}

Value *translateProcessIsolineTessFactors(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  // Get partition mode
  DXASSERT_NOMSG(Helper.FunctionProps);
  DXASSERT(Helper.FunctionProps->shaderKind == ShaderModel::Kind::Hull,
           "must be hull shader");
  DXIL::TessellatorPartitioning Partition =
      Helper.FunctionProps->ShaderProps.HS.partition;

  IRBuilder<> Builder(CI);

  Value *RawDetailFactor =
      CI->getArgOperand(HLOperandIndex::kProcessTessFactorRawDetailFactor);
  RawDetailFactor = Builder.CreateExtractElement(RawDetailFactor, (uint64_t)0);

  Value *RawDensityFactor =
      CI->getArgOperand(HLOperandIndex::kProcessTessFactorRawDensityFactor);
  RawDensityFactor =
      Builder.CreateExtractElement(RawDensityFactor, (uint64_t)0);

  Value *Init = UndefValue::get(VectorType::get(Helper.F32Ty, 2));
  Init = Builder.CreateInsertElement(Init, RawDetailFactor, (uint64_t)0);
  Init = Builder.CreateInsertElement(Init, RawDetailFactor, (uint64_t)1);

  Value *Clamped = clampTessFactor(Init, Partition, HlslOp, Builder);
  Value *Rounded = roundUpTessFactor(Clamped, Partition, HlslOp, Builder);

  Value *RoundedDetailFactor =
      CI->getArgOperand(HLOperandIndex::kProcessTessFactorRoundedDetailFactor);
  Value *Temp = UndefValue::get(VectorType::get(Helper.F32Ty, 1));
  Value *RoundedX = Builder.CreateExtractElement(Rounded, (uint64_t)0);
  Temp = Builder.CreateInsertElement(Temp, RoundedX, (uint64_t)0);
  Builder.CreateStore(Temp, RoundedDetailFactor);

  Value *RoundedDensityFactor =
      CI->getArgOperand(HLOperandIndex::kProcessTessFactorRoundedDensityFactor);
  Value *RoundedY = Builder.CreateExtractElement(Rounded, 1);
  Temp = Builder.CreateInsertElement(Temp, RoundedY, (uint64_t)0);
  Builder.CreateStore(Temp, RoundedDensityFactor);
  return nullptr;
}

// 3 inputs, 1 result
Value *applyTriTessFactorOp(Value *Input, DXIL::OpCode Opcode, hlsl::OP *HlslOp,
                            IRBuilder<> &Builder) {
  Value *Input0 = Builder.CreateExtractElement(Input, (uint64_t)0);
  Value *Input1 = Builder.CreateExtractElement(Input, 1);
  Value *Input2 = Builder.CreateExtractElement(Input, 2);

  if (Opcode == DXIL::OpCode::FMax || Opcode == DXIL::OpCode::FMin) {
    Value *Temp =
        trivialDxilBinaryOperation(Opcode, Input0, Input1, HlslOp, Builder);
    Value *Combined =
        trivialDxilBinaryOperation(Opcode, Temp, Input2, HlslOp, Builder);
    return Combined;
  }

  // Avg.
  Value *Temp = Builder.CreateFAdd(Input0, Input1);
  Value *Combined = Builder.CreateFAdd(Temp, Input2);
  Value *Rcp = ConstantFP::get(Input0->getType(), 1.0 / 3.0);
  Combined = Builder.CreateFMul(Combined, Rcp);
  return Combined;
}

// 4 inputs, 1 result
Value *applyQuadTessFactorOp(Value *Input, DXIL::OpCode Opcode,
                             hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  Value *Input0 = Builder.CreateExtractElement(Input, (uint64_t)0);
  Value *Input1 = Builder.CreateExtractElement(Input, 1);
  Value *Input2 = Builder.CreateExtractElement(Input, 2);
  Value *Input3 = Builder.CreateExtractElement(Input, 3);

  if (Opcode == DXIL::OpCode::FMax || Opcode == DXIL::OpCode::FMin) {
    Value *Temp0 =
        trivialDxilBinaryOperation(Opcode, Input0, Input1, HlslOp, Builder);
    Value *Temp1 =
        trivialDxilBinaryOperation(Opcode, Input2, Input3, HlslOp, Builder);
    Value *Combined =
        trivialDxilBinaryOperation(Opcode, Temp0, Temp1, HlslOp, Builder);
    return Combined;
  }

  // Avg.
  Value *Temp0 = Builder.CreateFAdd(Input0, Input1);
  Value *Temp1 = Builder.CreateFAdd(Input2, Input3);
  Value *Combined = Builder.CreateFAdd(Temp0, Temp1);
  Value *Rcp = ConstantFP::get(Input0->getType(), 0.25);
  Combined = Builder.CreateFMul(Combined, Rcp);
  return Combined;
}

// 4 inputs, 2 result
Value *apply2DQuadTessFactorOp(Value *Input, DXIL::OpCode Opcode,
                               hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  Value *Input0 = Builder.CreateExtractElement(Input, (uint64_t)0);
  Value *Input1 = Builder.CreateExtractElement(Input, 1);
  Value *Input2 = Builder.CreateExtractElement(Input, 2);
  Value *Input3 = Builder.CreateExtractElement(Input, 3);

  if (Opcode == DXIL::OpCode::FMax || Opcode == DXIL::OpCode::FMin) {
    Value *Temp0 =
        trivialDxilBinaryOperation(Opcode, Input0, Input1, HlslOp, Builder);
    Value *Temp1 =
        trivialDxilBinaryOperation(Opcode, Input2, Input3, HlslOp, Builder);
    Value *Combined = UndefValue::get(VectorType::get(Input0->getType(), 2));
    Combined = Builder.CreateInsertElement(Combined, Temp0, (uint64_t)0);
    Combined = Builder.CreateInsertElement(Combined, Temp1, 1);
    return Combined;
  }

  // Avg.
  Value *Temp0 = Builder.CreateFAdd(Input0, Input1);
  Value *Temp1 = Builder.CreateFAdd(Input2, Input3);
  Value *Combined = UndefValue::get(VectorType::get(Input0->getType(), 2));
  Combined = Builder.CreateInsertElement(Combined, Temp0, (uint64_t)0);
  Combined = Builder.CreateInsertElement(Combined, Temp1, 1);
  Constant *Rcp = ConstantFP::get(Input0->getType(), 0.5);
  Rcp = ConstantVector::getSplat(2, Rcp);
  Combined = Builder.CreateFMul(Combined, Rcp);
  return Combined;
}

Value *resolveSmallValue(Value **PClampedResult, Value *Rounded,
                         Value *AverageUnscaled, float CutoffVal,
                         DXIL::TessellatorPartitioning PartitionMode,
                         hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  Value *ClampedResult = *PClampedResult;
  Value *ClampedVal = ClampedResult;
  Value *RoundedVal = Rounded;
  // Do partitioning-specific clamping.
  Value *ClampedAvg =
      clampTessFactor(AverageUnscaled, PartitionMode, HlslOp, Builder);
  Constant *CutoffVals =
      ConstantFP::get(Type::getFloatTy(Rounded->getContext()), CutoffVal);
  if (ClampedAvg->getType()->isVectorTy())
    CutoffVals = ConstantVector::getSplat(
        ClampedAvg->getType()->getVectorNumElements(), CutoffVals);
  // Limit the value.
  ClampedAvg = trivialDxilBinaryOperation(DXIL::OpCode::FMin, ClampedAvg,
                                          CutoffVals, HlslOp, Builder);
  // Round up for integer/pow2 partitioning.
  Value *RoundedAvg =
      roundUpTessFactor(ClampedAvg, PartitionMode, HlslOp, Builder);

  if (Rounded->getType() != CutoffVals->getType())
    CutoffVals = ConstantVector::getSplat(
        Rounded->getType()->getVectorNumElements(), CutoffVals);
  // If the scaled value is less than three, then take the unscaled average.
  Value *Lt = Builder.CreateFCmpOLT(Rounded, CutoffVals);
  if (ClampedAvg->getType() != ClampedVal->getType())
    ClampedAvg = splatToVector(ClampedAvg, ClampedVal->getType(), Builder);
  *PClampedResult = Builder.CreateSelect(Lt, ClampedAvg, ClampedVal);

  if (RoundedAvg->getType() != RoundedVal->getType())
    RoundedAvg = splatToVector(RoundedAvg, RoundedVal->getType(), Builder);
  Value *Result = Builder.CreateSelect(Lt, RoundedAvg, RoundedVal);
  return Result;
}

void resolveQuadAxes(Value **PFinalResult, Value **PClampedResult,
                     float CutoffVal,
                     DXIL::TessellatorPartitioning PartitionMode,
                     hlsl::OP *HlslOp, IRBuilder<> &Builder) {
  Value *FinalResult = *PFinalResult;
  Value *ClampedResult = *PClampedResult;

  Value *ClampR = ClampedResult;
  Value *FinalR = FinalResult;
  Type *F32Ty = Type::getFloatTy(FinalR->getContext());
  Constant *CutoffVals = ConstantFP::get(F32Ty, CutoffVal);

  Value *MinValsX = CutoffVals;
  Value *MinValsY =
      roundUpTessFactor(CutoffVals, PartitionMode, HlslOp, Builder);

  Value *ClampRx = Builder.CreateExtractElement(ClampR, (uint64_t)0);
  Value *ClampRy = Builder.CreateExtractElement(ClampR, 1);
  Value *MaxValsX = trivialDxilBinaryOperation(DXIL::OpCode::FMax, ClampRx,
                                               ClampRy, HlslOp, Builder);

  Value *FinalRx = Builder.CreateExtractElement(FinalR, (uint64_t)0);
  Value *FinalRy = Builder.CreateExtractElement(FinalR, 1);
  Value *MaxValsY = trivialDxilBinaryOperation(DXIL::OpCode::FMax, FinalRx,
                                               FinalRy, HlslOp, Builder);

  // Don't go over our threshold ("final" one is rounded).
  Value *OptionX = trivialDxilBinaryOperation(DXIL::OpCode::FMin, MaxValsX,
                                              MinValsX, HlslOp, Builder);
  Value *OptionY = trivialDxilBinaryOperation(DXIL::OpCode::FMin, MaxValsY,
                                              MinValsY, HlslOp, Builder);

  Value *ClampL = splatToVector(OptionX, ClampR->getType(), Builder);
  Value *FinalL = splatToVector(OptionY, FinalR->getType(), Builder);

  CutoffVals = ConstantVector::getSplat(2, CutoffVals);
  Value *Lt = Builder.CreateFCmpOLT(ClampedResult, CutoffVals);
  *PClampedResult = Builder.CreateSelect(Lt, ClampL, ClampR);
  *PFinalResult = Builder.CreateSelect(Lt, FinalL, FinalR);
}

Value *translateProcessTessFactors(CallInst *CI, IntrinsicOp IOP,
                                   OP::OpCode Opcode,
                                   HLOperationLowerHelper &Helper,
                                   HLObjectOperationLowerHelper *PObjHelper,
                                   bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  // Get partition mode
  DXASSERT_NOMSG(Helper.FunctionProps);
  DXASSERT(Helper.FunctionProps->shaderKind == ShaderModel::Kind::Hull,
           "must be hull shader");
  DXIL::TessellatorPartitioning Partition =
      Helper.FunctionProps->ShaderProps.HS.partition;

  IRBuilder<> Builder(CI);

  DXIL::OpCode TessFactorOp = DXIL::OpCode::NumOpCodes;
  switch (IOP) {
  case IntrinsicOp::IOP_Process2DQuadTessFactorsMax:
  case IntrinsicOp::IOP_ProcessQuadTessFactorsMax:
  case IntrinsicOp::IOP_ProcessTriTessFactorsMax:
    TessFactorOp = DXIL::OpCode::FMax;
    break;
  case IntrinsicOp::IOP_Process2DQuadTessFactorsMin:
  case IntrinsicOp::IOP_ProcessQuadTessFactorsMin:
  case IntrinsicOp::IOP_ProcessTriTessFactorsMin:
    TessFactorOp = DXIL::OpCode::FMin;
    break;
  default:
    // Default is Avg.
    break;
  }

  Value *RawEdgeFactor =
      CI->getArgOperand(HLOperandIndex::kProcessTessFactorRawEdgeFactor);

  Value *InsideScale =
      CI->getArgOperand(HLOperandIndex::kProcessTessFactorInsideScale);
  // Clamp to [0.0f..1.0f], NaN->0.0f.
  Value *Scales = cleanupTessFactorScale(InsideScale, HlslOp, Builder);
  // Do partitioning-specific clamping.
  Value *Clamped = clampTessFactor(RawEdgeFactor, Partition, HlslOp, Builder);
  // Round up for integer/pow2 partitioning.
  Value *Rounded = roundUpTessFactor(Clamped, Partition, HlslOp, Builder);
  // Store the output.
  Value *RoundedEdgeFactor =
      CI->getArgOperand(HLOperandIndex::kProcessTessFactorRoundedEdgeFactor);
  Builder.CreateStore(Rounded, RoundedEdgeFactor);

  // Clamp to [1.0f..Inf], NaN->1.0f.
  bool IsQuad = false;
  Value *Clean = cleanupTessFactor(RawEdgeFactor, HlslOp, Builder);
  Value *Factors = nullptr;
  switch (IOP) {
  case IntrinsicOp::IOP_Process2DQuadTessFactorsAvg:
  case IntrinsicOp::IOP_Process2DQuadTessFactorsMax:
  case IntrinsicOp::IOP_Process2DQuadTessFactorsMin:
    Factors = apply2DQuadTessFactorOp(Clean, TessFactorOp, HlslOp, Builder);
    break;
  case IntrinsicOp::IOP_ProcessQuadTessFactorsAvg:
  case IntrinsicOp::IOP_ProcessQuadTessFactorsMax:
  case IntrinsicOp::IOP_ProcessQuadTessFactorsMin:
    Factors = applyQuadTessFactorOp(Clean, TessFactorOp, HlslOp, Builder);
    IsQuad = true;
    break;
  case IntrinsicOp::IOP_ProcessTriTessFactorsAvg:
  case IntrinsicOp::IOP_ProcessTriTessFactorsMax:
  case IntrinsicOp::IOP_ProcessTriTessFactorsMin:
    Factors = applyTriTessFactorOp(Clean, TessFactorOp, HlslOp, Builder);
    break;
  default:
    DXASSERT(0, "invalid opcode for ProcessTessFactor");
    break;
  }

  Value *ScaledI = nullptr;
  if (Scales->getType() == Factors->getType())
    ScaledI = Builder.CreateFMul(Factors, Scales);
  else {
    Value *VecFactors = splatToVector(Factors, Scales->getType(), Builder);
    ScaledI = Builder.CreateFMul(VecFactors, Scales);
  }

  // Do partitioning-specific clamping.
  Value *ClampedI = clampTessFactor(ScaledI, Partition, HlslOp, Builder);

  // Round up for integer/pow2 partitioning.
  Value *RoundedI = roundUpTessFactor(ClampedI, Partition, HlslOp, Builder);

  Value *FinalI = RoundedI;

  if (Partition == DXIL::TessellatorPartitioning::FractionalOdd) {
    // If not max, set to AVG.
    if (TessFactorOp != DXIL::OpCode::FMax)
      TessFactorOp = DXIL::OpCode::NumOpCodes;

    bool B2D = false;
    Value *AvgFactorsI = nullptr;
    switch (IOP) {
    case IntrinsicOp::IOP_Process2DQuadTessFactorsAvg:
    case IntrinsicOp::IOP_Process2DQuadTessFactorsMax:
    case IntrinsicOp::IOP_Process2DQuadTessFactorsMin:
      AvgFactorsI =
          apply2DQuadTessFactorOp(Clean, TessFactorOp, HlslOp, Builder);
      B2D = true;
      break;
    case IntrinsicOp::IOP_ProcessQuadTessFactorsAvg:
    case IntrinsicOp::IOP_ProcessQuadTessFactorsMax:
    case IntrinsicOp::IOP_ProcessQuadTessFactorsMin:
      AvgFactorsI = applyQuadTessFactorOp(Clean, TessFactorOp, HlslOp, Builder);
      break;
    case IntrinsicOp::IOP_ProcessTriTessFactorsAvg:
    case IntrinsicOp::IOP_ProcessTriTessFactorsMax:
    case IntrinsicOp::IOP_ProcessTriTessFactorsMin:
      AvgFactorsI = applyTriTessFactorOp(Clean, TessFactorOp, HlslOp, Builder);
      break;
    default:
      DXASSERT(0, "invalid opcode for ProcessTessFactor");
      break;
    }

    FinalI = resolveSmallValue(/*inout*/ &ClampedI, RoundedI, AvgFactorsI,
                               /*cufoff*/ 3.0, Partition, HlslOp, Builder);

    if (B2D)
      resolveQuadAxes(/*inout*/ &FinalI, /*inout*/ &ClampedI, /*cutoff*/ 3.0,
                      Partition, HlslOp, Builder);
  }

  Value *UnroundedInsideFactor = CI->getArgOperand(
      HLOperandIndex::kProcessTessFactorUnRoundedInsideFactor);
  Type *OutFactorTy = UnroundedInsideFactor->getType()->getPointerElementType();
  if (OutFactorTy != ClampedI->getType()) {
    DXASSERT(IsQuad, "quad only write one channel of out factor");
    (void)IsQuad;
    ClampedI = Builder.CreateExtractElement(ClampedI, (uint64_t)0);
    // Splat clampedI to float2.
    ClampedI = splatToVector(ClampedI, OutFactorTy, Builder);
  }
  Builder.CreateStore(ClampedI, UnroundedInsideFactor);

  Value *RoundedInsideFactor =
      CI->getArgOperand(HLOperandIndex::kProcessTessFactorRoundedInsideFactor);
  if (OutFactorTy != FinalI->getType()) {
    DXASSERT(IsQuad, "quad only write one channel of out factor");
    FinalI = Builder.CreateExtractElement(FinalI, (uint64_t)0);
    // Splat finalI to float2.
    FinalI = splatToVector(FinalI, OutFactorTy, Builder);
  }
  Builder.CreateStore(FinalI, RoundedInsideFactor);
  return nullptr;
}

} // namespace

// Ray Tracing.
namespace {
Value *translateReportIntersection(CallInst *CI, IntrinsicOp IOP,
                                   OP::OpCode Opcode,
                                   HLOperationLowerHelper &Helper,
                                   HLObjectOperationLowerHelper *PObjHelper,
                                   bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *THit = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  Value *HitKind = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx);
  Value *Attr = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx);
  Value *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(Opcode));

  Type *Ty = Attr->getType();
  Function *F = HlslOp->GetOpFunc(Opcode, Ty);

  IRBuilder<> Builder(CI);
  return Builder.CreateCall(F, {OpArg, THit, HitKind, Attr});
}

Value *translateCallShader(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                           HLOperationLowerHelper &Helper,
                           HLObjectOperationLowerHelper *PObjHelper,
                           bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *ShaderIndex = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Parameter = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  Value *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(Opcode));

  Type *Ty = Parameter->getType();
  Function *F = HlslOp->GetOpFunc(Opcode, Ty);

  IRBuilder<> Builder(CI);
  return Builder.CreateCall(F, {OpArg, ShaderIndex, Parameter});
}

static void transferRayDescArgs(Value **Args, hlsl::OP *OP,
                                IRBuilder<> &Builder, CallInst *CI,
                                unsigned &Index, unsigned &HLIndex) {
  // Extract elements from flattened ray desc arguments in HL op.
  // float3 Origin;
  Value *Origin = CI->getArgOperand(HLIndex++);
  Args[Index++] = Builder.CreateExtractElement(Origin, (uint64_t)0);
  Args[Index++] = Builder.CreateExtractElement(Origin, 1);
  Args[Index++] = Builder.CreateExtractElement(Origin, 2);
  // float  TMin;
  Args[Index++] = CI->getArgOperand(HLIndex++);
  // float3 Direction;
  Value *Direction = CI->getArgOperand(HLIndex++);
  Args[Index++] = Builder.CreateExtractElement(Direction, (uint64_t)0);
  Args[Index++] = Builder.CreateExtractElement(Direction, 1);
  Args[Index++] = Builder.CreateExtractElement(Direction, 2);
  // float  TMax;
  Args[Index++] = CI->getArgOperand(HLIndex++);
}

Value *translateTraceRay(CallInst *CI, IntrinsicOp IOP, OP::OpCode OpCode,
                         HLOperationLowerHelper &Helper,
                         HLObjectOperationLowerHelper *PObjHelper,
                         bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;

  Value *Args[DXIL::OperandIndex::kTraceRayNumOp];
  Args[0] = OP->GetU32Const(static_cast<unsigned>(OpCode));
  unsigned Index = 1, HLIndex = 1;
  while (HLIndex < HLOperandIndex::kTraceRayRayDescOpIdx)
    Args[Index++] = CI->getArgOperand(HLIndex++);

  IRBuilder<> Builder(CI);
  transferRayDescArgs(Args, OP, Builder, CI, Index, HLIndex);
  DXASSERT_NOMSG(HLIndex == CI->getNumArgOperands() - 1);
  DXASSERT_NOMSG(Index == DXIL::OperandIndex::kTraceRayPayloadOpIdx);

  Value *Payload = CI->getArgOperand(HLIndex++);
  Args[Index++] = Payload;

  DXASSERT_NOMSG(HLIndex == CI->getNumArgOperands());
  DXASSERT_NOMSG(Index == DXIL::OperandIndex::kTraceRayNumOp);

  Type *Ty = Payload->getType();
  Function *F = OP->GetOpFunc(OpCode, Ty);

  return Builder.CreateCall(F, Args);
}

// RayQuery methods

Value *translateAllocateRayQuery(CallInst *CI, IntrinsicOp IOP,
                                 OP::OpCode Opcode,
                                 HLOperationLowerHelper &Helper,
                                 HLObjectOperationLowerHelper *PObjHelper,
                                 bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  // upgrade to allocateRayQuery2 if there is a non-zero 2nd template arg
  DXASSERT(CI->getNumArgOperands() == 3,
           "hlopcode for allocaterayquery always expects 3 arguments");

  llvm::Value *Arg =
      CI->getArgOperand(HLOperandIndex::kAllocateRayQueryRayQueryFlagsIdx);
  llvm::ConstantInt *ConstVal = llvm::dyn_cast<llvm::ConstantInt>(Arg);
  DXASSERT(ConstVal,
           "2nd argument to allocaterayquery must always be a constant value");
  if (ConstVal->getValue().getZExtValue() != 0) {
    Value *RefArgs[3] = {
        nullptr, CI->getOperand(HLOperandIndex::kAllocateRayQueryRayFlagsIdx),
        CI->getOperand(HLOperandIndex::kAllocateRayQueryRayQueryFlagsIdx)};
    Opcode = OP::OpCode::AllocateRayQuery2;
    return trivialDxilOperation(Opcode, RefArgs, Helper.VoidTy, CI, HlslOp);
  }
  Value *RefArgs[2] = {
      nullptr, CI->getOperand(HLOperandIndex::kAllocateRayQueryRayFlagsIdx)};
  return trivialDxilOperation(Opcode, RefArgs, Helper.VoidTy, CI, HlslOp);
}

Value *translateTraceRayInline(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                               HLOperationLowerHelper &Helper,
                               HLObjectOperationLowerHelper *PObjHelper,
                               bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(Opcode));

  Value *Args[DXIL::OperandIndex::kTraceRayInlineNumOp];
  Args[0] = OpArg;
  unsigned Index = 1, HLIndex = 1;
  while (HLIndex < HLOperandIndex::kTraceRayInlineRayDescOpIdx)
    Args[Index++] = CI->getArgOperand(HLIndex++);

  IRBuilder<> Builder(CI);
  DXASSERT_NOMSG(HLIndex == HLOperandIndex::kTraceRayInlineRayDescOpIdx);
  DXASSERT_NOMSG(Index == DXIL::OperandIndex::kTraceRayInlineRayDescOpIdx);
  transferRayDescArgs(Args, HlslOp, Builder, CI, Index, HLIndex);
  DXASSERT_NOMSG(HLIndex == CI->getNumArgOperands());
  DXASSERT_NOMSG(Index == DXIL::OperandIndex::kTraceRayInlineNumOp);

  Function *F = HlslOp->GetOpFunc(Opcode, Builder.getVoidTy());

  return Builder.CreateCall(F, Args);
}

Value *translateCommitProceduralPrimitiveHit(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *THit = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  Value *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(Opcode));
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);

  Value *Args[] = {OpArg, Handle, THit};

  IRBuilder<> Builder(CI);
  Function *F = HlslOp->GetOpFunc(Opcode, Builder.getVoidTy());

  return Builder.CreateCall(F, Args);
}

Value *translateGenericRayQueryMethod(CallInst *CI, IntrinsicOp IOP,
                                      OP::OpCode Opcode,
                                      HLOperationLowerHelper &Helper,
                                      HLObjectOperationLowerHelper *PObjHelper,
                                      bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;

  Value *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(Opcode));
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);

  IRBuilder<> Builder(CI);
  Function *F = HlslOp->GetOpFunc(Opcode, CI->getType());

  return Builder.CreateCall(F, {OpArg, Handle});
}

Value *translateRayQueryMatrix3x4Operation(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  VectorType *Ty = cast<VectorType>(CI->getType());
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  uint32_t RVals[] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2};
  Constant *Rows = ConstantDataVector::get(CI->getContext(), RVals);
  uint8_t CVals[] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
  Constant *Cols = ConstantDataVector::get(CI->getContext(), CVals);
  Value *RetVal = trivialDxilOperation(Opcode, {nullptr, Handle, Rows, Cols},
                                       Ty, CI, HlslOp);
  return RetVal;
}

Value *translateRayQueryTransposedMatrix3x4Operation(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  VectorType *Ty = cast<VectorType>(CI->getType());
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  uint32_t RVals[] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2};
  Constant *Rows = ConstantDataVector::get(CI->getContext(), RVals);
  uint8_t CVals[] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3};
  Constant *Cols = ConstantDataVector::get(CI->getContext(), CVals);
  Value *RetVal = trivialDxilOperation(Opcode, {nullptr, Handle, Rows, Cols},
                                       Ty, CI, HlslOp);
  return RetVal;
}

Value *translateRayQueryFloat2Getter(CallInst *CI, IntrinsicOp IOP,
                                     OP::OpCode Opcode,
                                     HLOperationLowerHelper &Helper,
                                     HLObjectOperationLowerHelper *PObjHelper,
                                     bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  VectorType *Ty = cast<VectorType>(CI->getType());
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  uint8_t ElementVals[] = {0, 1};
  Constant *Element = ConstantDataVector::get(CI->getContext(), ElementVals);
  Value *RetVal =
      trivialDxilOperation(Opcode, {nullptr, Handle, Element}, Ty, CI, HlslOp);
  return RetVal;
}

Value *translateRayQueryFloat3Getter(CallInst *CI, IntrinsicOp IOP,
                                     OP::OpCode Opcode,
                                     HLOperationLowerHelper &Helper,
                                     HLObjectOperationLowerHelper *PObjHelper,
                                     bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  VectorType *Ty = cast<VectorType>(CI->getType());
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  uint8_t ElementVals[] = {0, 1, 2};
  Constant *Element = ConstantDataVector::get(CI->getContext(), ElementVals);
  Value *RetVal =
      trivialDxilOperation(Opcode, {nullptr, Handle, Element}, Ty, CI, HlslOp);
  return RetVal;
}

Value *translateNoArgVectorOperation(CallInst *CI, IntrinsicOp IOP,
                                     OP::OpCode Opcode,
                                     HLOperationLowerHelper &Helper,
                                     HLObjectOperationLowerHelper *PObjHelper,
                                     bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  VectorType *Ty = cast<VectorType>(CI->getType());
  uint8_t Vals[] = {0, 1, 2, 3};
  Constant *Src = ConstantDataVector::get(CI->getContext(), Vals);
  Value *RetVal = trivialDxilOperation(Opcode, {nullptr, Src}, Ty, CI, HlslOp);
  return RetVal;
}

template <typename ColElemTy>
static void getMatrixIndices(Constant *&Rows, Constant *&Cols, bool Is3x4,
                             LLVMContext &Ctx) {
  if (Is3x4) {
    uint32_t RVals[] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2};
    Rows = ConstantDataVector::get(Ctx, RVals);
    ColElemTy CVals[] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
    Cols = ConstantDataVector::get(Ctx, CVals);
    return;
  }
  uint32_t RVals[] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2};
  Rows = ConstantDataVector::get(Ctx, RVals);
  ColElemTy CVals[] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3};
  Cols = ConstantDataVector::get(Ctx, CVals);
}

Value *translateNoArgMatrix3x4Operation(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  VectorType *Ty = cast<VectorType>(CI->getType());
  Constant *Rows, *Cols;
  getMatrixIndices<uint8_t>(Rows, Cols, true, CI->getContext());
  return trivialDxilOperation(Opcode, {nullptr, Rows, Cols}, Ty, CI, HlslOp);
}

Value *translateNoArgTransposedMatrix3x4Operation(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  VectorType *Ty = cast<VectorType>(CI->getType());
  Constant *Rows, *Cols;
  getMatrixIndices<uint8_t>(Rows, Cols, false, CI->getContext());
  return trivialDxilOperation(Opcode, {nullptr, Rows, Cols}, Ty, CI, HlslOp);
}

/*
HLSL:
void ThreadNodeOutputRecords<recordType>::OutputComplete();
void GroupNodeOutputRecords<recordType>::OutputComplete();
DXIL:
void @dx.op.outputComplete(i32 %Opcode, %dx.types.NodeRecordHandle
%RecordHandle)
*/
Value *translateNodeOutputComplete(CallInst *CI, IntrinsicOp IOP, OP::OpCode Op,
                                   HLOperationLowerHelper &Helper,
                                   HLObjectOperationLowerHelper *PObjHelper,
                                   bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;

  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  DXASSERT_NOMSG(Handle->getType() == OP->GetNodeRecordHandleType());
  Function *DxilFunc = OP->GetOpFunc(Op, CI->getType());
  Value *OpArg = OP->GetU32Const((unsigned)Op);

  IRBuilder<> Builder(CI);
  return Builder.CreateCall(DxilFunc, {OpArg, Handle});
}

Value *translateNoArgNoReturnPreserveOutput(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  Instruction *PResult = cast<Instruction>(
      trivialNoArgOperation(CI, IOP, Opcode, Helper, PObjHelper, Translated));
  // HL intrinsic must have had a return injected just after the call.
  // SROA_Parameter_HLSL will copy from alloca to output just before each
  // return. Now move call after the copy and just before the return.
  if (isa<ReturnInst>(PResult->getNextNode()))
    return PResult;
  ReturnInst *RetI = cast<ReturnInst>(PResult->getParent()->getTerminator());
  PResult->removeFromParent();
  PResult->insertBefore(RetI);
  return PResult;
}

// Special half dot2 with accumulate to float
Value *translateDot2Add(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                        HLOperationLowerHelper &Helper,
                        HLObjectOperationLowerHelper *PObjHelper,
                        bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  const unsigned VecSize = 2;
  DXASSERT(Src0->getType()->isVectorTy() &&
               VecSize == Src0->getType()->getVectorNumElements() &&
               Src0->getType()->getScalarType()->isHalfTy(),
           "otherwise, unexpected input dimension or component type");

  Value *Src1 = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx);
  DXASSERT(Src0->getType() == Src1->getType(),
           "otherwise, mismatched argument types");
  Value *AccArg = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx);
  Type *AccTy = AccArg->getType();
  DXASSERT(!AccTy->isVectorTy() && AccTy->isFloatTy(),
           "otherwise, unexpected accumulator type");
  IRBuilder<> Builder(CI);

  Function *DxilFunc = HlslOp->GetOpFunc(Opcode, AccTy);
  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);

  SmallVector<Value *, 6> Args;
  Args.emplace_back(OpArg);
  Args.emplace_back(AccArg);
  for (unsigned I = 0; I < VecSize; I++)
    Args.emplace_back(Builder.CreateExtractElement(Src0, I));
  for (unsigned I = 0; I < VecSize; I++)
    Args.emplace_back(Builder.CreateExtractElement(Src1, I));
  return Builder.CreateCall(DxilFunc, Args);
}

Value *translateDot4AddPacked(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                              HLOperationLowerHelper &Helper,
                              HLObjectOperationLowerHelper *PObjHelper,
                              bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;
  Value *Src0 = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  DXASSERT(
      !Src0->getType()->isVectorTy() && Src0->getType()->isIntegerTy(32),
      "otherwise, unexpected vector support in high level intrinsic template");
  Value *Src1 = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx);
  DXASSERT(Src0->getType() == Src1->getType(),
           "otherwise, mismatched argument types");
  Value *AccArg = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx);
  Type *AccTy = AccArg->getType();
  DXASSERT(
      !AccTy->isVectorTy() && AccTy->isIntegerTy(32),
      "otherwise, unexpected vector support in high level intrinsic template");
  IRBuilder<> Builder(CI);

  Function *DxilFunc = HlslOp->GetOpFunc(Opcode, AccTy);
  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  return Builder.CreateCall(DxilFunc, {OpArg, AccArg, Src0, Src1});
}

Value *translatePack(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                     HLOperationLowerHelper &Helper,
                     HLObjectOperationLowerHelper *PObjHelper,
                     bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;

  Value *Val = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  Type *ValTy = Val->getType();
  Type *EltTy = ValTy->getScalarType();

  DXASSERT(ValTy->isVectorTy() && ValTy->getVectorNumElements() == 4 &&
               EltTy->isIntegerTy() &&
               (EltTy->getIntegerBitWidth() == 32 ||
                EltTy->getIntegerBitWidth() == 16),
           "otherwise, unexpected input dimension or component type");

  DXIL::PackMode PackMode = DXIL::PackMode::Trunc;
  switch (IOP) {
  case hlsl::IntrinsicOp::IOP_pack_clamp_s8:
    PackMode = DXIL::PackMode::SClamp;
    break;
  case hlsl::IntrinsicOp::IOP_pack_clamp_u8:
    PackMode = DXIL::PackMode::UClamp;
    break;
  case hlsl::IntrinsicOp::IOP_pack_s8:
  case hlsl::IntrinsicOp::IOP_pack_u8:
    PackMode = DXIL::PackMode::Trunc;
    break;
  default:
    DXASSERT(false, "unexpected opcode");
    break;
  }

  IRBuilder<> Builder(CI);
  Function *DxilFunc = HlslOp->GetOpFunc(Opcode, EltTy);
  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Constant *PackModeArg = HlslOp->GetU8Const((unsigned)PackMode);

  Value *Elt0 = Builder.CreateExtractElement(Val, (uint64_t)0);
  Value *Elt1 = Builder.CreateExtractElement(Val, (uint64_t)1);
  Value *Elt2 = Builder.CreateExtractElement(Val, (uint64_t)2);
  Value *Elt3 = Builder.CreateExtractElement(Val, (uint64_t)3);
  return Builder.CreateCall(DxilFunc,
                            {OpArg, PackModeArg, Elt0, Elt1, Elt2, Elt3});
}

Value *translateUnpack(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                       HLOperationLowerHelper &Helper,
                       HLObjectOperationLowerHelper *PObjHelper,
                       bool &Translated) {
  hlsl::OP *HlslOp = &Helper.HlslOp;

  Value *PackedVal = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
  DXASSERT(
      !PackedVal->getType()->isVectorTy() &&
          PackedVal->getType()->isIntegerTy(32),
      "otherwise, unexpected vector support in high level intrinsic template");

  Type *OverloadType = nullptr;
  DXIL::UnpackMode UnpackMode = DXIL::UnpackMode::Unsigned;
  switch (IOP) {
  case hlsl::IntrinsicOp::IOP_unpack_s8s32:
    UnpackMode = DXIL::UnpackMode::Signed;
    OverloadType = Helper.I32Ty;
    break;
  case hlsl::IntrinsicOp::IOP_unpack_u8u32:
    UnpackMode = DXIL::UnpackMode::Unsigned;
    OverloadType = Helper.I32Ty;
    break;
  case hlsl::IntrinsicOp::IOP_unpack_s8s16:
    UnpackMode = DXIL::UnpackMode::Signed;
    OverloadType = Helper.I16Ty;
    break;
  case hlsl::IntrinsicOp::IOP_unpack_u8u16:
    UnpackMode = DXIL::UnpackMode::Unsigned;
    OverloadType = Helper.I16Ty;
    break;
  default:
    DXASSERT(false, "unexpected opcode");
    break;
  }

  IRBuilder<> Builder(CI);
  Function *DxilFunc = HlslOp->GetOpFunc(Opcode, OverloadType);
  Constant *OpArg = HlslOp->GetU32Const((unsigned)Opcode);
  Constant *UnpackModeArg = HlslOp->GetU8Const((unsigned)UnpackMode);
  Value *Res = Builder.CreateCall(DxilFunc, {OpArg, UnpackModeArg, PackedVal});

  // Convert the final aggregate into a vector to make the types match
  const unsigned VecSize = 4;
  Value *ResVec = UndefValue::get(CI->getType());
  for (unsigned I = 0; I < VecSize; ++I) {
    Value *Elt = Builder.CreateExtractValue(Res, I);
    ResVec = Builder.CreateInsertElement(ResVec, Elt, I);
  }
  return ResVec;
}

} // namespace

// Shader Execution Reordering.
namespace {
Value *translateHitObjectMakeNop(CallInst *CI, IntrinsicOp IOP,
                                 OP::OpCode Opcode,
                                 HLOperationLowerHelper &Helper,
                                 HLObjectOperationLowerHelper *ObjHelper,
                                 bool &Translated) {
  hlsl::OP *HlslOP = &Helper.HlslOp;
  IRBuilder<> Builder(CI);
  Value *HitObjectPtr = CI->getArgOperand(1);
  Value *HitObject = trivialDxilOperation(
      Opcode, {nullptr}, Type::getVoidTy(CI->getContext()), CI, HlslOP);
  Builder.CreateStore(HitObject, HitObjectPtr);
  DXASSERT(
      CI->use_empty(),
      "Default ctor return type is a Clang artifact. Value must not be used");
  return nullptr;
}

Value *translateHitObjectMakeMiss(CallInst *CI, IntrinsicOp IOP,
                                  OP::OpCode Opcode,
                                  HLOperationLowerHelper &Helper,
                                  HLObjectOperationLowerHelper *ObjHelper,
                                  bool &Translated) {
  DXASSERT_NOMSG(CI->getNumArgOperands() ==
                 HLOperandIndex::kHitObjectMakeMiss_NumOp);
  hlsl::OP *OP = &Helper.HlslOp;
  IRBuilder<> Builder(CI);
  Value *Args[DXIL::OperandIndex::kHitObjectMakeMiss_NumOp];
  Args[0] = nullptr; // Filled in by TrivialDxilOperation

  unsigned DestIdx = 1, SrcIdx = 1;
  Value *HitObjectPtr = CI->getArgOperand(SrcIdx++);
  Args[DestIdx++] = CI->getArgOperand(SrcIdx++); // RayFlags
  Args[DestIdx++] = CI->getArgOperand(SrcIdx++); // MissShaderIdx

  DXASSERT_NOMSG(SrcIdx == HLOperandIndex::kHitObjectMakeMiss_RayDescOpIdx);
  DXASSERT_NOMSG(DestIdx ==
                 DXIL::OperandIndex::kHitObjectMakeMiss_RayDescOpIdx);
  transferRayDescArgs(Args, OP, Builder, CI, DestIdx, SrcIdx);
  DXASSERT_NOMSG(SrcIdx == CI->getNumArgOperands());
  DXASSERT_NOMSG(DestIdx == DXIL::OperandIndex::kHitObjectMakeMiss_NumOp);

  Value *OutHitObject =
      trivialDxilOperation(Opcode, Args, Helper.VoidTy, CI, OP);
  Builder.CreateStore(OutHitObject, HitObjectPtr);
  return nullptr;
}

Value *translateMaybeReorderThread(CallInst *CI, IntrinsicOp IOP,
                                   OP::OpCode OpCode,
                                   HLOperationLowerHelper &Helper,
                                   HLObjectOperationLowerHelper *PObjHelper,
                                   bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;

  // clang-format off
  // Match MaybeReorderThread overload variants:
  // void MaybeReorderThread(<Op>,
  //                    HitObject Hit);
  // void MaybeReorderThread(<Op>,
  //                    uint CoherenceHint,
  //                    uint NumCoherenceHintBitsFromLSB );
  // void MaybeReorderThread(<Op>,
  //                    HitObject Hit,
  //                    uint CoherenceHint,
  //                    uint NumCoherenceHintBitsFromLSB);
  // clang-format on
  const unsigned NumHLArgs = CI->getNumArgOperands();
  DXASSERT_NOMSG(NumHLArgs >= 2);

  // Use a NOP HitObject for MaybeReorderThread without HitObject.
  Value *HitObject = nullptr;
  unsigned HLIndex = 1;
  if (3 == NumHLArgs) {
    HitObject = trivialDxilOperation(DXIL::OpCode::HitObject_MakeNop, {nullptr},
                                     Type::getVoidTy(CI->getContext()), CI, OP);
  } else {
    Value *FirstParam = CI->getArgOperand(HLIndex);
    DXASSERT_NOMSG(isa<PointerType>(FirstParam->getType()));
    IRBuilder<> Builder(CI);
    HitObject = Builder.CreateLoad(FirstParam);
    HLIndex++;
  }

  // If there are trailing parameters, these have to be the two coherence bit
  // parameters
  Value *CoherenceHint = nullptr;
  Value *NumCoherenceHintBits = nullptr;
  if (2 != NumHLArgs) {
    DXASSERT_NOMSG(HLIndex + 2 == NumHLArgs);
    CoherenceHint = CI->getArgOperand(HLIndex++);
    NumCoherenceHintBits = CI->getArgOperand(HLIndex++);
    DXASSERT_NOMSG(Helper.I32Ty == CoherenceHint->getType());
    DXASSERT_NOMSG(Helper.I32Ty == NumCoherenceHintBits->getType());
  } else {
    CoherenceHint = UndefValue::get(Helper.I32Ty);
    NumCoherenceHintBits = OP->GetU32Const(0);
  }

  trivialDxilOperation(
      OpCode, {nullptr, HitObject, CoherenceHint, NumCoherenceHintBits},
      Type::getVoidTy(CI->getContext()), CI, OP);
  return nullptr;
}

Value *translateHitObjectFromRayQuery(CallInst *CI, IntrinsicOp IOP,
                                      OP::OpCode OpCode,
                                      HLOperationLowerHelper &Helper,
                                      HLObjectOperationLowerHelper *PObjHelper,
                                      bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  unsigned SrcIdx = 1;
  Value *HitObjectPtr = CI->getArgOperand(SrcIdx++);
  Value *RayQuery = CI->getArgOperand(SrcIdx++);

  if (CI->getNumArgOperands() ==
      HLOperandIndex::kHitObjectFromRayQuery_WithAttrs_NumOp) {
    Value *HitKind = CI->getArgOperand(SrcIdx++);
    Value *AttribSrc = CI->getArgOperand(SrcIdx++);
    DXASSERT_NOMSG(SrcIdx == CI->getNumArgOperands());
    OpCode = DXIL::OpCode::HitObject_FromRayQueryWithAttrs;
    Type *AttrTy = AttribSrc->getType();
    Value *OutHitObject = trivialDxilOperation(
        OpCode, {nullptr, RayQuery, HitKind, AttribSrc}, AttrTy, CI, OP);
    Builder.CreateStore(OutHitObject, HitObjectPtr);
    return nullptr;
  }

  DXASSERT_NOMSG(SrcIdx == CI->getNumArgOperands());
  OpCode = DXIL::OpCode::HitObject_FromRayQuery;
  Value *OutHitObject =
      trivialDxilOperation(OpCode, {nullptr, RayQuery}, Helper.VoidTy, CI, OP);
  Builder.CreateStore(OutHitObject, HitObjectPtr);
  return nullptr;
}

Value *translateHitObjectTraceRay(CallInst *CI, IntrinsicOp IOP,
                                  OP::OpCode OpCode,
                                  HLOperationLowerHelper &Helper,
                                  HLObjectOperationLowerHelper *PObjHelper,
                                  bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  DXASSERT_NOMSG(CI->getNumArgOperands() ==
                 HLOperandIndex::kHitObjectTraceRay_NumOp);
  Value *Args[DXIL::OperandIndex::kHitObjectTraceRay_NumOp];
  Value *OpArg = OP->GetU32Const(static_cast<unsigned>(OpCode));
  Args[0] = OpArg;

  unsigned DestIdx = 1, SrcIdx = 1;
  Value *HitObjectPtr = CI->getArgOperand(SrcIdx++);
  Args[DestIdx++] = CI->getArgOperand(SrcIdx++);
  for (; SrcIdx < HLOperandIndex::kHitObjectTraceRay_RayDescOpIdx;
       ++SrcIdx, ++DestIdx) {
    Args[DestIdx] = CI->getArgOperand(SrcIdx);
  }

  DXASSERT_NOMSG(SrcIdx == HLOperandIndex::kHitObjectTraceRay_RayDescOpIdx);
  DXASSERT_NOMSG(DestIdx ==
                 DXIL::OperandIndex::kHitObjectTraceRay_RayDescOpIdx);
  transferRayDescArgs(Args, OP, Builder, CI, DestIdx, SrcIdx);
  DXASSERT_NOMSG(SrcIdx == CI->getNumArgOperands() - 1);
  DXASSERT_NOMSG(DestIdx ==
                 DXIL::OperandIndex::kHitObjectTraceRay_PayloadOpIdx);

  Value *Payload = CI->getArgOperand(SrcIdx++);
  Args[DestIdx++] = Payload;

  DXASSERT_NOMSG(SrcIdx == CI->getNumArgOperands());
  DXASSERT_NOMSG(DestIdx == DXIL::OperandIndex::kHitObjectTraceRay_NumOp);

  Function *F = OP->GetOpFunc(OpCode, Payload->getType());

  Value *OutHitObject = Builder.CreateCall(F, Args);
  Builder.CreateStore(OutHitObject, HitObjectPtr);
  return nullptr;
}

Value *translateHitObjectInvoke(CallInst *CI, IntrinsicOp IOP,
                                OP::OpCode OpCode,
                                HLOperationLowerHelper &Helper,
                                HLObjectOperationLowerHelper *PObjHelper,
                                bool &Translated) {
  unsigned SrcIdx = 1;
  Value *HitObjectPtr = CI->getArgOperand(SrcIdx++);
  Value *Payload = CI->getArgOperand(SrcIdx++);
  DXASSERT_NOMSG(SrcIdx == CI->getNumArgOperands());

  IRBuilder<> Builder(CI);
  Value *HitObject = Builder.CreateLoad(HitObjectPtr);
  trivialDxilOperation(OpCode, {nullptr, HitObject, Payload},
                       Payload->getType(), CI, &Helper.HlslOp);
  return nullptr;
}

Value *translateHitObjectGetAttributes(CallInst *CI, IntrinsicOp IOP,
                                       OP::OpCode OpCode,
                                       HLOperationLowerHelper &Helper,
                                       HLObjectOperationLowerHelper *PObjHelper,
                                       bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  Value *HitObjectPtr = CI->getArgOperand(1);
  Value *HitObject = Builder.CreateLoad(HitObjectPtr);
  Value *AttrOutPtr =
      CI->getArgOperand(HLOperandIndex::kHitObjectGetAttributes_AttributeOpIdx);
  trivialDxilOperation(OpCode, {nullptr, HitObject, AttrOutPtr},
                       AttrOutPtr->getType(), CI, OP);
  return nullptr;
}

Value *translateHitObjectScalarGetter(CallInst *CI, IntrinsicOp IOP,
                                      OP::OpCode OpCode,
                                      HLOperationLowerHelper &Helper,
                                      HLObjectOperationLowerHelper *PObjHelper,
                                      bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  Value *HitObjectPtr = CI->getArgOperand(1);
  IRBuilder<> Builder(CI);
  Value *HitObject = Builder.CreateLoad(HitObjectPtr);
  return trivialDxilOperation(OpCode, {nullptr, HitObject}, CI->getType(), CI,
                              OP);
}

Value *translateHitObjectVectorGetter(CallInst *CI, IntrinsicOp IOP,
                                      OP::OpCode OpCode,
                                      HLOperationLowerHelper &Helper,
                                      HLObjectOperationLowerHelper *PObjHelper,
                                      bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  Value *HitObjectPtr = CI->getArgOperand(1);
  IRBuilder<> Builder(CI);
  Value *HitObject = Builder.CreateLoad(HitObjectPtr);
  VectorType *Ty = cast<VectorType>(CI->getType());
  uint32_t Vals[] = {0, 1, 2, 3};
  Constant *Src = ConstantDataVector::get(CI->getContext(), Vals);
  return trivialDxilOperation(OpCode, {nullptr, HitObject, Src}, Ty, CI, OP);
}

static bool isHitObject3x4Getter(IntrinsicOp IOP) {
  switch (IOP) {
  default:
    return false;
  case IntrinsicOp::MOP_DxHitObject_GetObjectToWorld3x4:
  case IntrinsicOp::MOP_DxHitObject_GetWorldToObject3x4:
    return true;
  }
}

Value *translateHitObjectMatrixGetter(CallInst *CI, IntrinsicOp IOP,
                                      OP::OpCode OpCode,
                                      HLOperationLowerHelper &Helper,
                                      HLObjectOperationLowerHelper *PObjHelper,
                                      bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  Value *HitObjectPtr = CI->getArgOperand(1);
  IRBuilder<> Builder(CI);
  Value *HitObject = Builder.CreateLoad(HitObjectPtr);

  // Create 3x4 matrix indices
  bool Is3x4 = isHitObject3x4Getter(IOP);
  Constant *Rows, *Cols;
  getMatrixIndices<uint32_t>(Rows, Cols, Is3x4, CI->getContext());

  VectorType *Ty = cast<VectorType>(CI->getType());
  return trivialDxilOperation(OpCode, {nullptr, HitObject, Rows, Cols}, Ty, CI,
                              OP);
}

Value *translateHitObjectLoadLocalRootTableConstant(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode OpCode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  Value *HitObjectPtr = CI->getArgOperand(1);
  Value *Offset = CI->getArgOperand(2);

  Value *HitObject = Builder.CreateLoad(HitObjectPtr);
  return trivialDxilOperation(OpCode, {nullptr, HitObject, Offset},
                              Helper.VoidTy, CI, OP);
}

Value *translateHitObjectSetShaderTableIndex(
    CallInst *CI, IntrinsicOp IOP, OP::OpCode OpCode,
    HLOperationLowerHelper &Helper, HLObjectOperationLowerHelper *PObjHelper,
    bool &Translated) {
  hlsl::OP *OP = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  Value *HitObjectPtr = CI->getArgOperand(1);
  Value *ShaderTableIndex = CI->getArgOperand(2);

  Value *InHitObject = Builder.CreateLoad(HitObjectPtr);
  Value *OutHitObject = trivialDxilOperation(
      OpCode, {nullptr, InHitObject, ShaderTableIndex}, Helper.VoidTy, CI, OP);
  Builder.CreateStore(OutHitObject, HitObjectPtr);
  return nullptr;
}

} // namespace

// Resource Handle.
namespace {
Value *translateGetHandleFromHeap(CallInst *CI, IntrinsicOp IOP,
                                  DXIL::OpCode Opcode,
                                  HLOperationLowerHelper &Helper,
                                  HLObjectOperationLowerHelper *PObjHelper,
                                  bool &Translated) {
  hlsl::OP &HlslOp = Helper.HlslOp;
  Function *DxilFunc = HlslOp.GetOpFunc(Opcode, Helper.VoidTy);
  IRBuilder<> Builder(CI);
  Value *OpArg = ConstantInt::get(Helper.I32Ty, (unsigned)Opcode);
  return Builder.CreateCall(
      DxilFunc, {OpArg, CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx),
                 CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx),
                 // TODO: update nonUniformIndex later.
                 Builder.getInt1(false)});
}
} // namespace

// Translate and/or/select intrinsics
namespace {
Value *translateAnd(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                    HLOperationLowerHelper &Helper,
                    HLObjectOperationLowerHelper *PObjHelper,
                    bool &Translated) {
  Value *X = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Y = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);

  return Builder.CreateAnd(X, Y);
}
Value *translateOr(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                   HLOperationLowerHelper &Helper,
                   HLObjectOperationLowerHelper *PObjHelper, bool &Translated) {
  Value *X = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc0Idx);
  Value *Y = CI->getArgOperand(HLOperandIndex::kBinaryOpSrc1Idx);
  IRBuilder<> Builder(CI);

  return Builder.CreateOr(X, Y);
}
Value *translateSelect(CallInst *CI, IntrinsicOp IOP, OP::OpCode Opcode,
                       HLOperationLowerHelper &Helper,
                       HLObjectOperationLowerHelper *PObjHelper,
                       bool &Translated) {
  Value *Cond = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc0Idx);
  Value *T = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc1Idx);
  Value *F = CI->getArgOperand(HLOperandIndex::kTrinaryOpSrc2Idx);
  IRBuilder<> Builder(CI);

  return Builder.CreateSelect(Cond, T, F);
}

Value *translateMatVecMul(CallInst *CI, IntrinsicOp IOP, OP::OpCode OpCode,
                          HLOperationLowerHelper &Helper,
                          HLObjectOperationLowerHelper *ObjHelper,
                          bool &Translated) {

  hlsl::OP *HlslOp = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  Constant *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(OpCode));

  // Input parameters
  Value *InputVector =
      CI->getArgOperand(HLOperandIndex::kMatVecMulInputVectorIdx);
  Value *InputIsUnsigned =
      CI->getArgOperand(HLOperandIndex::kMatVecMulIsInputUnsignedIdx);
  Value *InputInterpretation =
      CI->getArgOperand(HLOperandIndex::kMatVecMulInputInterpretationIdx);

  // Matrix parameters
  Value *MatrixBuffer =
      CI->getArgOperand(HLOperandIndex::kMatVecMulMatrixBufferIdx);
  Value *MatrixOffset =
      CI->getArgOperand(HLOperandIndex::kMatVecMulMatrixOffsetIdx);
  Value *MatrixInterpretation =
      CI->getArgOperand(HLOperandIndex::kMatVecMulMatrixInterpretationIdx);
  Value *MatrixM = CI->getArgOperand(HLOperandIndex::kMatVecMulMatrixMIdx);
  Value *MatrixK = CI->getArgOperand(HLOperandIndex::kMatVecMulMatrixKIdx);
  Value *MatrixLayout =
      CI->getArgOperand(HLOperandIndex::kMatVecMulMatrixLayoutIdx);
  Value *MatrixTranspose =
      CI->getArgOperand(HLOperandIndex::kMatVecMulMatrixTransposeIdx);
  Value *MatrixStride =
      CI->getArgOperand(HLOperandIndex::kMatVecMulMatrixStrideIdx);

  // Output parameters
  Value *OutputIsUnsigned =
      CI->getArgOperand(HLOperandIndex::kMatVecMulIsOutputUnsignedIdx);

  // Get the DXIL function for the operation
  Function *DxilFunc = HlslOp->GetOpFunc(
      OpCode, {CI->getArgOperand(HLOperandIndex::kMatVecMulOutputVectorIdx)
                   ->getType()
                   ->getPointerElementType(),
               InputVector->getType()});

  // Create a call to the DXIL function
  Value *NewCI = Builder.CreateCall(
      DxilFunc,
      {OpArg, InputVector, InputIsUnsigned, InputInterpretation, MatrixBuffer,
       MatrixOffset, MatrixInterpretation, MatrixM, MatrixK, MatrixLayout,
       MatrixTranspose, MatrixStride, OutputIsUnsigned});

  // Get the output parameter and store the result
  Value *OutParam =
      CI->getArgOperand(HLOperandIndex::kMatVecMulOutputVectorIdx);

  Builder.CreateStore(NewCI, OutParam);

  return nullptr;
}

Value *translateMatVecMulAdd(CallInst *CI, IntrinsicOp IOP, OP::OpCode OpCode,
                             HLOperationLowerHelper &Helper,
                             HLObjectOperationLowerHelper *ObjHelper,
                             bool &Translated) {

  hlsl::OP *HlslOp = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  Constant *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(OpCode));

  // Input vector parameters
  Value *InputVector =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddInputVectorIdx);
  Value *InputIsUnsigned =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddIsInputUnsignedIdx);
  Value *InputInterpretation =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddInputInterpretationIdx);

  // Matrix parameters
  Value *MatrixBuffer =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddMatrixBufferIdx);
  Value *MatrixOffset =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddMatrixOffsetIdx);
  Value *MatrixInterpretation =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddMatrixInterpretationIdx);
  Value *MatrixM = CI->getArgOperand(HLOperandIndex::kMatVecMulAddMatrixMIdx);
  Value *MatrixK = CI->getArgOperand(HLOperandIndex::kMatVecMulAddMatrixKIdx);
  Value *MatrixLayout =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddMatrixLayoutIdx);
  Value *MatrixTranspose =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddMatrixTransposeIdx);
  Value *MatrixStride =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddMatrixStrideIdx);

  // Bias parameters
  Value *BiasBuffer =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddBiasBufferIdx);
  Value *BiasOffset =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddBiasOffsetIdx);
  Value *BiasInterpretation =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddBiasInterpretationIdx);

  // Output parameters
  Value *OutputIsUnsigned =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddIsOutputUnsignedIdx);

  // Get the DXIL function for the operation
  Function *DxilFunc = HlslOp->GetOpFunc(
      OpCode, {CI->getArgOperand(HLOperandIndex::kMatVecMulAddOutputVectorIdx)
                   ->getType()
                   ->getPointerElementType(),
               InputVector->getType()});

  // Create a call to the DXIL function
  Value *NewCI = Builder.CreateCall(
      DxilFunc, {OpArg, InputVector, InputIsUnsigned, InputInterpretation,
                 MatrixBuffer, MatrixOffset, MatrixInterpretation, MatrixM,
                 MatrixK, MatrixLayout, MatrixTranspose, MatrixStride,
                 BiasBuffer, BiasOffset, BiasInterpretation, OutputIsUnsigned});

  // Store the result in the output parameter
  Value *OutParam =
      CI->getArgOperand(HLOperandIndex::kMatVecMulAddOutputVectorIdx);
  Builder.CreateStore(NewCI, OutParam);

  return nullptr;
}

Value *translateOuterProductAccumulate(CallInst *CI, IntrinsicOp IOP,
                                       OP::OpCode OpCode,
                                       HLOperationLowerHelper &Helper,
                                       HLObjectOperationLowerHelper *ObjHelper,
                                       bool &Translated) {

  hlsl::OP *HlslOp = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  Constant *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(OpCode));

  // Input vector parameters
  Value *InputVector1 =
      CI->getArgOperand(HLOperandIndex::kOuterProdAccInputVec1Idx);
  Value *InputVector2 =
      CI->getArgOperand(HLOperandIndex::kOuterProdAccInputVec2Idx);

  // Matrix parameters
  Value *MatrixBuffer =
      CI->getArgOperand(HLOperandIndex::kOuterProdAccMatrixIdx);
  Value *MatrixOffset =
      CI->getArgOperand(HLOperandIndex::kOuterProdAccMatrixOffsetIdx);
  Value *MatrixInterpretation =
      CI->getArgOperand(HLOperandIndex::kOuterProdAccMatrixInterpretationIdx);
  Value *MatrixLayout =
      CI->getArgOperand(HLOperandIndex::kOuterProdAccMatrixLayoutIdx);
  Value *MatrixStride =
      CI->getArgOperand(HLOperandIndex::kOuterProdAccMatrixStrideIdx);

  // Get the DXIL function for the operation
  Function *DxilFunc = HlslOp->GetOpFunc(
      OpCode, {InputVector1->getType(), InputVector2->getType()});

  return Builder.CreateCall(
      DxilFunc, {OpArg, InputVector1, InputVector2, MatrixBuffer, MatrixOffset,
                 MatrixInterpretation, MatrixLayout, MatrixStride});
}

Value *translateVectorAccumulate(CallInst *CI, IntrinsicOp IOP,
                                 OP::OpCode OpCode,
                                 HLOperationLowerHelper &Helper,
                                 HLObjectOperationLowerHelper *ObjHelper,
                                 bool &Translated) {

  hlsl::OP *HlslOp = &Helper.HlslOp;
  IRBuilder<> Builder(CI);

  Constant *OpArg = HlslOp->GetU32Const(static_cast<unsigned>(OpCode));

  // Input vector parameter
  Value *InputVector = CI->getArgOperand(HLOperandIndex::kVectorAccInputVecIdx);

  // Matrix parameters
  Value *MatrixBuffer = CI->getArgOperand(HLOperandIndex::kVectorAccMatrixIdx);
  Value *MatrixOffset =
      CI->getArgOperand(HLOperandIndex::kVectorAccMatrixOffsetIdx);

  // Get the DXIL function for the operation
  Function *DxilFunc = HlslOp->GetOpFunc(OpCode, InputVector->getType());

  return Builder.CreateCall(DxilFunc,
                            {OpArg, InputVector, MatrixBuffer, MatrixOffset});
}

} // namespace

// Lower table.
namespace {

Value *emptyLower(CallInst *CI, IntrinsicOp IOP, DXIL::OpCode Opcode,
                  HLOperationLowerHelper &Helper,
                  HLObjectOperationLowerHelper *PObjHelper, bool &Translated) {
  Translated = false;
  dxilutil::EmitErrorOnInstruction(CI, "Unsupported intrinsic.");
  return nullptr;
}

// SPIRV change starts
Value *unsupportedVulkanIntrinsic(CallInst *CI, IntrinsicOp IOP,
                                  DXIL::OpCode Opcode,
                                  HLOperationLowerHelper &Helper,
                                  HLObjectOperationLowerHelper *PObjHelper,
                                  bool &Translated) {
  Translated = false;
  dxilutil::EmitErrorOnInstruction(CI, "Unsupported Vulkan intrinsic.");
  return nullptr;
}
// SPIRV change ends

Value *streamOutputLower(CallInst *CI, IntrinsicOp IOP, DXIL::OpCode Opcode,
                         HLOperationLowerHelper &Helper,
                         HLObjectOperationLowerHelper *PObjHelper,
                         bool &Translated) {
  // Translated in DxilGenerationPass::GenerateStreamOutputOperation.
  // Do nothing here.
  // Mark not translated.
  Translated = false;
  return nullptr;
}

// This table has to match IntrinsicOp orders
IntrinsicLower GLowerTable[] = {
    {IntrinsicOp::IOP_AcceptHitAndEndSearch,
     translateNoArgNoReturnPreserveOutput, DXIL::OpCode::AcceptHitAndEndSearch},
    {IntrinsicOp::IOP_AddUint64, translateAddUint64, DXIL::OpCode::UAddc},
    {IntrinsicOp::IOP_AllMemoryBarrier, trivialBarrier, DXIL::OpCode::Barrier},
    {IntrinsicOp::IOP_AllMemoryBarrierWithGroupSync, trivialBarrier,
     DXIL::OpCode::Barrier},
    {IntrinsicOp::IOP_AllocateRayQuery, translateAllocateRayQuery,
     DXIL::OpCode::AllocateRayQuery},
    {IntrinsicOp::IOP_Barrier, translateBarrier, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_CallShader, translateCallShader,
     DXIL::OpCode::CallShader},
    {IntrinsicOp::IOP_CheckAccessFullyMapped, translateCheckAccess,
     DXIL::OpCode::CheckAccessFullyMapped},
    {IntrinsicOp::IOP_CreateResourceFromHeap, translateGetHandleFromHeap,
     DXIL::OpCode::CreateHandleFromHeap},
    {IntrinsicOp::IOP_D3DCOLORtoUBYTE4, translateD3DColorToUByte4,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_DeviceMemoryBarrier, trivialBarrier,
     DXIL::OpCode::Barrier},
    {IntrinsicOp::IOP_DeviceMemoryBarrierWithGroupSync, trivialBarrier,
     DXIL::OpCode::Barrier},
    {IntrinsicOp::IOP_DispatchMesh, trivialDispatchMesh,
     DXIL::OpCode::DispatchMesh},
    {IntrinsicOp::IOP_DispatchRaysDimensions, translateNoArgVectorOperation,
     DXIL::OpCode::DispatchRaysDimensions},
    {IntrinsicOp::IOP_DispatchRaysIndex, translateNoArgVectorOperation,
     DXIL::OpCode::DispatchRaysIndex},
    {IntrinsicOp::IOP_EvaluateAttributeAtSample, translateEvalSample,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_EvaluateAttributeCentroid, translateEvalCentroid,
     DXIL::OpCode::EvalCentroid},
    {IntrinsicOp::IOP_EvaluateAttributeSnapped, translateEvalSnapped,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_GeometryIndex, trivialNoArgWithRetOperation,
     DXIL::OpCode::GeometryIndex},
    {IntrinsicOp::IOP_GetAttributeAtVertex, translateGetAttributeAtVertex,
     DXIL::OpCode::AttributeAtVertex},
    {IntrinsicOp::IOP_GetRemainingRecursionLevels, trivialNoArgOperation,
     DXIL::OpCode::GetRemainingRecursionLevels},
    {IntrinsicOp::IOP_GetRenderTargetSampleCount, trivialNoArgOperation,
     DXIL::OpCode::RenderTargetGetSampleCount},
    {IntrinsicOp::IOP_GetRenderTargetSamplePosition, translateGetRtSamplePos,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_GroupMemoryBarrier, trivialBarrier,
     DXIL::OpCode::Barrier},
    {IntrinsicOp::IOP_GroupMemoryBarrierWithGroupSync, trivialBarrier,
     DXIL::OpCode::Barrier},
    {IntrinsicOp::IOP_HitKind, trivialNoArgWithRetOperation,
     DXIL::OpCode::HitKind},
    {IntrinsicOp::IOP_IgnoreHit, translateNoArgNoReturnPreserveOutput,
     DXIL::OpCode::IgnoreHit},
    {IntrinsicOp::IOP_InstanceID, trivialNoArgWithRetOperation,
     DXIL::OpCode::InstanceID},
    {IntrinsicOp::IOP_InstanceIndex, trivialNoArgWithRetOperation,
     DXIL::OpCode::InstanceIndex},
    {IntrinsicOp::IOP_InterlockedAdd, translateIopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedAnd, translateIopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedCompareExchange, translateIopAtomicCmpXChg,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedCompareExchangeFloatBitwise,
     translateIopAtomicCmpXChg, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedCompareStore, translateIopAtomicCmpXChg,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedCompareStoreFloatBitwise,
     translateIopAtomicCmpXChg, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedExchange, translateIopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedMax, translateIopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedMin, translateIopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedOr, translateIopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedXor, translateIopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_IsHelperLane, trivialNoArgWithRetOperation,
     DXIL::OpCode::IsHelperLane},
    {IntrinsicOp::IOP_NonUniformResourceIndex, translateNonUniformResourceIndex,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ObjectRayDirection, translateNoArgVectorOperation,
     DXIL::OpCode::ObjectRayDirection},
    {IntrinsicOp::IOP_ObjectRayOrigin, translateNoArgVectorOperation,
     DXIL::OpCode::ObjectRayOrigin},
    {IntrinsicOp::IOP_ObjectToWorld, translateNoArgMatrix3x4Operation,
     DXIL::OpCode::ObjectToWorld},
    {IntrinsicOp::IOP_ObjectToWorld3x4, translateNoArgMatrix3x4Operation,
     DXIL::OpCode::ObjectToWorld},
    {IntrinsicOp::IOP_ObjectToWorld4x3,
     translateNoArgTransposedMatrix3x4Operation, DXIL::OpCode::ObjectToWorld},
    {IntrinsicOp::IOP_PrimitiveIndex, trivialNoArgWithRetOperation,
     DXIL::OpCode::PrimitiveIndex},
    {IntrinsicOp::IOP_Process2DQuadTessFactorsAvg, translateProcessTessFactors,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_Process2DQuadTessFactorsMax, translateProcessTessFactors,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_Process2DQuadTessFactorsMin, translateProcessTessFactors,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ProcessIsolineTessFactors,
     translateProcessIsolineTessFactors, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ProcessQuadTessFactorsAvg, translateProcessTessFactors,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ProcessQuadTessFactorsMax, translateProcessTessFactors,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ProcessQuadTessFactorsMin, translateProcessTessFactors,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ProcessTriTessFactorsAvg, translateProcessTessFactors,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ProcessTriTessFactorsMax, translateProcessTessFactors,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ProcessTriTessFactorsMin, translateProcessTessFactors,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_QuadAll, translateQuadAnyAll, DXIL::OpCode::QuadVote},
    {IntrinsicOp::IOP_QuadAny, translateQuadAnyAll, DXIL::OpCode::QuadVote},
    {IntrinsicOp::IOP_QuadReadAcrossDiagonal, translateQuadReadAcross,
     DXIL::OpCode::QuadOp},
    {IntrinsicOp::IOP_QuadReadAcrossX, translateQuadReadAcross,
     DXIL::OpCode::QuadOp},
    {IntrinsicOp::IOP_QuadReadAcrossY, translateQuadReadAcross,
     DXIL::OpCode::QuadOp},
    {IntrinsicOp::IOP_QuadReadLaneAt, translateQuadReadLaneAt,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_RayFlags, trivialNoArgWithRetOperation,
     DXIL::OpCode::RayFlags},
    {IntrinsicOp::IOP_RayTCurrent, trivialNoArgWithRetOperation,
     DXIL::OpCode::RayTCurrent},
    {IntrinsicOp::IOP_RayTMin, trivialNoArgWithRetOperation,
     DXIL::OpCode::RayTMin},
    {IntrinsicOp::IOP_ReportHit, translateReportIntersection,
     DXIL::OpCode::ReportHit},
    {IntrinsicOp::IOP_SetMeshOutputCounts, trivialSetMeshOutputCounts,
     DXIL::OpCode::SetMeshOutputCounts},
    {IntrinsicOp::IOP_TraceRay, translateTraceRay, DXIL::OpCode::TraceRay},
    {IntrinsicOp::IOP_WaveActiveAllEqual, translateWaveAllEqual,
     DXIL::OpCode::WaveActiveAllEqual},
    {IntrinsicOp::IOP_WaveActiveAllTrue, translateWaveA2B,
     DXIL::OpCode::WaveAllTrue},
    {IntrinsicOp::IOP_WaveActiveAnyTrue, translateWaveA2B,
     DXIL::OpCode::WaveAnyTrue},
    {IntrinsicOp::IOP_WaveActiveBallot, translateWaveBallot,
     DXIL::OpCode::WaveActiveBallot},
    {IntrinsicOp::IOP_WaveActiveBitAnd, translateWaveA2A,
     DXIL::OpCode::WaveActiveBit},
    {IntrinsicOp::IOP_WaveActiveBitOr, translateWaveA2A,
     DXIL::OpCode::WaveActiveBit},
    {IntrinsicOp::IOP_WaveActiveBitXor, translateWaveA2A,
     DXIL::OpCode::WaveActiveBit},
    {IntrinsicOp::IOP_WaveActiveCountBits, translateWaveA2B,
     DXIL::OpCode::WaveAllBitCount},
    {IntrinsicOp::IOP_WaveActiveMax, translateWaveA2A,
     DXIL::OpCode::WaveActiveOp},
    {IntrinsicOp::IOP_WaveActiveMin, translateWaveA2A,
     DXIL::OpCode::WaveActiveOp},
    {IntrinsicOp::IOP_WaveActiveProduct, translateWaveA2A,
     DXIL::OpCode::WaveActiveOp},
    {IntrinsicOp::IOP_WaveActiveSum, translateWaveA2A,
     DXIL::OpCode::WaveActiveOp},
    {IntrinsicOp::IOP_WaveGetLaneCount, translateWaveToVal,
     DXIL::OpCode::WaveGetLaneCount},
    {IntrinsicOp::IOP_WaveGetLaneIndex, translateWaveToVal,
     DXIL::OpCode::WaveGetLaneIndex},
    {IntrinsicOp::IOP_WaveIsFirstLane, translateWaveToVal,
     DXIL::OpCode::WaveIsFirstLane},
    {IntrinsicOp::IOP_WaveMatch, translateWaveMatch, DXIL::OpCode::WaveMatch},
    {IntrinsicOp::IOP_WaveMultiPrefixBitAnd, translateWaveMultiPrefix,
     DXIL::OpCode::WaveMultiPrefixOp},
    {IntrinsicOp::IOP_WaveMultiPrefixBitOr, translateWaveMultiPrefix,
     DXIL::OpCode::WaveMultiPrefixOp},
    {IntrinsicOp::IOP_WaveMultiPrefixBitXor, translateWaveMultiPrefix,
     DXIL::OpCode::WaveMultiPrefixOp},
    {IntrinsicOp::IOP_WaveMultiPrefixCountBits,
     translateWaveMultiPrefixBitCount, DXIL::OpCode::WaveMultiPrefixBitCount},
    {IntrinsicOp::IOP_WaveMultiPrefixProduct, translateWaveMultiPrefix,
     DXIL::OpCode::WaveMultiPrefixOp},
    {IntrinsicOp::IOP_WaveMultiPrefixSum, translateWaveMultiPrefix,
     DXIL::OpCode::WaveMultiPrefixOp},
    {IntrinsicOp::IOP_WavePrefixCountBits, translateWaveA2B,
     DXIL::OpCode::WavePrefixBitCount},
    {IntrinsicOp::IOP_WavePrefixProduct, translateWaveA2A,
     DXIL::OpCode::WavePrefixOp},
    {IntrinsicOp::IOP_WavePrefixSum, translateWaveA2A,
     DXIL::OpCode::WavePrefixOp},
    {IntrinsicOp::IOP_WaveReadLaneAt, translateWaveReadLaneAt,
     DXIL::OpCode::WaveReadLaneAt},
    {IntrinsicOp::IOP_WaveReadLaneFirst, translateWaveReadLaneFirst,
     DXIL::OpCode::WaveReadLaneFirst},
    {IntrinsicOp::IOP_WorldRayDirection, translateNoArgVectorOperation,
     DXIL::OpCode::WorldRayDirection},
    {IntrinsicOp::IOP_WorldRayOrigin, translateNoArgVectorOperation,
     DXIL::OpCode::WorldRayOrigin},
    {IntrinsicOp::IOP_WorldToObject, translateNoArgMatrix3x4Operation,
     DXIL::OpCode::WorldToObject},
    {IntrinsicOp::IOP_WorldToObject3x4, translateNoArgMatrix3x4Operation,
     DXIL::OpCode::WorldToObject},
    {IntrinsicOp::IOP_WorldToObject4x3,
     translateNoArgTransposedMatrix3x4Operation, DXIL::OpCode::WorldToObject},
    {IntrinsicOp::IOP_abort, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_abs, translateAbs, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_acos, trivialUnaryOperation, DXIL::OpCode::Acos},
    {IntrinsicOp::IOP_all, translateAll, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_and, translateAnd, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_any, translateAny, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_asdouble, translateAsDouble, DXIL::OpCode::MakeDouble},
    {IntrinsicOp::IOP_asfloat, translateBitcast, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_asfloat16, translateBitcast, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_asin, trivialUnaryOperation, DXIL::OpCode::Asin},
    {IntrinsicOp::IOP_asint, translateBitcast, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_asint16, translateBitcast, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_asuint, translateAsUint, DXIL::OpCode::SplitDouble},
    {IntrinsicOp::IOP_asuint16, translateAsUint, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_atan, trivialUnaryOperation, DXIL::OpCode::Atan},
    {IntrinsicOp::IOP_atan2, translateAtan2, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ceil, trivialUnaryOperation, DXIL::OpCode::Round_pi},
    {IntrinsicOp::IOP_clamp, translateClamp, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_clip, translateClip, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_cos, trivialUnaryOperation, DXIL::OpCode::Cos},
    {IntrinsicOp::IOP_cosh, trivialUnaryOperation, DXIL::OpCode::Hcos},
    {IntrinsicOp::IOP_countbits, trivialUnaryOperationRet,
     DXIL::OpCode::Countbits},
    {IntrinsicOp::IOP_cross, translateCross, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ddx, trivialUnaryOperation, DXIL::OpCode::DerivCoarseX},
    {IntrinsicOp::IOP_ddx_coarse, trivialUnaryOperation,
     DXIL::OpCode::DerivCoarseX},
    {IntrinsicOp::IOP_ddx_fine, trivialUnaryOperation,
     DXIL::OpCode::DerivFineX},
    {IntrinsicOp::IOP_ddy, trivialUnaryOperation, DXIL::OpCode::DerivCoarseY},
    {IntrinsicOp::IOP_ddy_coarse, trivialUnaryOperation,
     DXIL::OpCode::DerivCoarseY},
    {IntrinsicOp::IOP_ddy_fine, trivialUnaryOperation,
     DXIL::OpCode::DerivFineY},
    {IntrinsicOp::IOP_degrees, translateDegrees, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_determinant, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_distance, translateDistance, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_dot, translateDot, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_dot2add, translateDot2Add, DXIL::OpCode::Dot2AddHalf},
    {IntrinsicOp::IOP_dot4add_i8packed, translateDot4AddPacked,
     DXIL::OpCode::Dot4AddI8Packed},
    {IntrinsicOp::IOP_dot4add_u8packed, translateDot4AddPacked,
     DXIL::OpCode::Dot4AddU8Packed},
    {IntrinsicOp::IOP_dst, translateDst, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_exp, translateExp, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_exp2, trivialUnaryOperation, DXIL::OpCode::Exp},
    {IntrinsicOp::IOP_f16tof32, translateF16ToF32,
     DXIL::OpCode::LegacyF16ToF32},
    {IntrinsicOp::IOP_f32tof16, translateF32ToF16,
     DXIL::OpCode::LegacyF32ToF16},
    {IntrinsicOp::IOP_faceforward, translateFaceforward,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_firstbithigh, translateFirstbitHi,
     DXIL::OpCode::FirstbitSHi},
    {IntrinsicOp::IOP_firstbitlow, translateFirstbitLo,
     DXIL::OpCode::FirstbitLo},
    {IntrinsicOp::IOP_floor, trivialUnaryOperation, DXIL::OpCode::Round_ni},
    {IntrinsicOp::IOP_fma, trivialTrinaryOperation, DXIL::OpCode::Fma},
    {IntrinsicOp::IOP_fmod, translateFMod, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_frac, trivialUnaryOperation, DXIL::OpCode::Frc},
    {IntrinsicOp::IOP_frexp, translateFrexp, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_fwidth, translateFWidth, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_isfinite, trivialIsSpecialFloat, DXIL::OpCode::IsFinite},
    {IntrinsicOp::IOP_isinf, trivialIsSpecialFloat, DXIL::OpCode::IsInf},
    {IntrinsicOp::IOP_isnan, trivialIsSpecialFloat, DXIL::OpCode::IsNaN},
    {IntrinsicOp::IOP_ldexp, translateLdExp, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_length, translateLength, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_lerp, translateLerp, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_lit, translateLit, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_log, translateLog, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_log10, translateLog10, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_log2, trivialUnaryOperation, DXIL::OpCode::Log},
    {IntrinsicOp::IOP_mad, translateFuiTrinary, DXIL::OpCode::IMad},
    {IntrinsicOp::IOP_max, translateFuiBinary, DXIL::OpCode::IMax},
    {IntrinsicOp::IOP_min, translateFuiBinary, DXIL::OpCode::IMin},
    {IntrinsicOp::IOP_modf, translateModF, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_msad4, translateMSad4, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_mul, translateMul, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_normalize, translateNormalize, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_or, translateOr, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_pack_clamp_s8, translatePack, DXIL::OpCode::Pack4x8},
    {IntrinsicOp::IOP_pack_clamp_u8, translatePack, DXIL::OpCode::Pack4x8},
    {IntrinsicOp::IOP_pack_s8, translatePack, DXIL::OpCode::Pack4x8},
    {IntrinsicOp::IOP_pack_u8, translatePack, DXIL::OpCode::Pack4x8},
    {IntrinsicOp::IOP_pow, translatePow, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_printf, translatePrintf, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_radians, translateRadians, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_rcp, translateRcp, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_reflect, translateReflect, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_refract, translateRefract, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_reversebits, trivialUnaryOperation, DXIL::OpCode::Bfrev},
    {IntrinsicOp::IOP_round, trivialUnaryOperation, DXIL::OpCode::Round_ne},
    {IntrinsicOp::IOP_rsqrt, trivialUnaryOperation, DXIL::OpCode::Rsqrt},
    {IntrinsicOp::IOP_saturate, trivialUnaryOperation, DXIL::OpCode::Saturate},
    {IntrinsicOp::IOP_select, translateSelect, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_sign, translateSign, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_sin, trivialUnaryOperation, DXIL::OpCode::Sin},
    {IntrinsicOp::IOP_sincos, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_sinh, trivialUnaryOperation, DXIL::OpCode::Hsin},
    {IntrinsicOp::IOP_smoothstep, translateSmoothStep,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_source_mark, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_sqrt, trivialUnaryOperation, DXIL::OpCode::Sqrt},
    {IntrinsicOp::IOP_step, translateStep, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tan, trivialUnaryOperation, DXIL::OpCode::Tan},
    {IntrinsicOp::IOP_tanh, trivialUnaryOperation, DXIL::OpCode::Htan},
    {IntrinsicOp::IOP_tex1D, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex1Dbias, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex1Dgrad, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex1Dlod, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex1Dproj, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex2D, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex2Dbias, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex2Dgrad, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex2Dlod, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex2Dproj, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex3D, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex3Dbias, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex3Dgrad, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex3Dlod, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_tex3Dproj, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_texCUBE, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_texCUBEbias, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_texCUBEgrad, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_texCUBElod, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_texCUBEproj, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_transpose, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_trunc, trivialUnaryOperation, DXIL::OpCode::Round_z},
    {IntrinsicOp::IOP_unpack_s8s16, translateUnpack, DXIL::OpCode::Unpack4x8},
    {IntrinsicOp::IOP_unpack_s8s32, translateUnpack, DXIL::OpCode::Unpack4x8},
    {IntrinsicOp::IOP_unpack_u8u16, translateUnpack, DXIL::OpCode::Unpack4x8},
    {IntrinsicOp::IOP_unpack_u8u32, translateUnpack, DXIL::OpCode::Unpack4x8},
    {IntrinsicOp::IOP_VkRawBufferLoad, unsupportedVulkanIntrinsic,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_VkRawBufferStore, unsupportedVulkanIntrinsic,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_VkReadClock, unsupportedVulkanIntrinsic,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_Vkext_execution_mode, unsupportedVulkanIntrinsic,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_Vkext_execution_mode_id, unsupportedVulkanIntrinsic,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Append, streamOutputLower, DXIL::OpCode::EmitStream},
    {IntrinsicOp::MOP_RestartStrip, streamOutputLower, DXIL::OpCode::CutStream},
    {IntrinsicOp::MOP_CalculateLevelOfDetail, translateCalculateLod,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_CalculateLevelOfDetailUnclamped, translateCalculateLod,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_GetDimensions, translateGetDimensions,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Load, translateResourceLoad, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Sample, translateSample, DXIL::OpCode::Sample},
    {IntrinsicOp::MOP_SampleBias, translateSample, DXIL::OpCode::SampleBias},
    {IntrinsicOp::MOP_SampleCmp, translateSample, DXIL::OpCode::SampleCmp},
    {IntrinsicOp::MOP_SampleCmpBias, translateSample,
     DXIL::OpCode::SampleCmpBias},
    {IntrinsicOp::MOP_SampleCmpGrad, translateSample,
     DXIL::OpCode::SampleCmpGrad},
    {IntrinsicOp::MOP_SampleCmpLevel, translateSample,
     DXIL::OpCode::SampleCmpLevel},
    {IntrinsicOp::MOP_SampleCmpLevelZero, translateSample,
     DXIL::OpCode::SampleCmpLevelZero},
    {IntrinsicOp::MOP_SampleGrad, translateSample, DXIL::OpCode::SampleGrad},
    {IntrinsicOp::MOP_SampleLevel, translateSample, DXIL::OpCode::SampleLevel},
    {IntrinsicOp::MOP_Gather, translateGather, DXIL::OpCode::TextureGather},
    {IntrinsicOp::MOP_GatherAlpha, translateGather,
     DXIL::OpCode::TextureGather},
    {IntrinsicOp::MOP_GatherBlue, translateGather, DXIL::OpCode::TextureGather},
    {IntrinsicOp::MOP_GatherCmp, translateGather,
     DXIL::OpCode::TextureGatherCmp},
    {IntrinsicOp::MOP_GatherCmpAlpha, translateGather,
     DXIL::OpCode::TextureGatherCmp},
    {IntrinsicOp::MOP_GatherCmpBlue, translateGather,
     DXIL::OpCode::TextureGatherCmp},
    {IntrinsicOp::MOP_GatherCmpGreen, translateGather,
     DXIL::OpCode::TextureGatherCmp},
    {IntrinsicOp::MOP_GatherCmpRed, translateGather,
     DXIL::OpCode::TextureGatherCmp},
    {IntrinsicOp::MOP_GatherGreen, translateGather,
     DXIL::OpCode::TextureGather},
    {IntrinsicOp::MOP_GatherRaw, translateGather,
     DXIL::OpCode::TextureGatherRaw},
    {IntrinsicOp::MOP_GatherRed, translateGather, DXIL::OpCode::TextureGather},
    {IntrinsicOp::MOP_GetSamplePosition, translateGetSamplePosition,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Load2, translateResourceLoad, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Load3, translateResourceLoad, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Load4, translateResourceLoad, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedAdd, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedAdd64, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedAnd, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedAnd64, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedCompareExchange, translateMopAtomicCmpXChg,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedCompareExchange64, translateMopAtomicCmpXChg,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedCompareExchangeFloatBitwise,
     translateMopAtomicCmpXChg, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedCompareStore, translateMopAtomicCmpXChg,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedCompareStore64, translateMopAtomicCmpXChg,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedCompareStoreFloatBitwise,
     translateMopAtomicCmpXChg, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedExchange, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedExchange64, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedExchangeFloat,
     translateMopAtomicBinaryOperation, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedMax, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedMax64, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedMin, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedMin64, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedOr, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedOr64, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedXor, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedXor64, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Store, translateResourceStore, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Store2, translateResourceStore, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Store3, translateResourceStore, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Store4, translateResourceStore, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_DecrementCounter, generateUpdateCounter,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_IncrementCounter, generateUpdateCounter,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_Consume, emptyLower, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_WriteSamplerFeedback, translateWriteSamplerFeedback,
     DXIL::OpCode::WriteSamplerFeedback},
    {IntrinsicOp::MOP_WriteSamplerFeedbackBias, translateWriteSamplerFeedback,
     DXIL::OpCode::WriteSamplerFeedbackBias},
    {IntrinsicOp::MOP_WriteSamplerFeedbackGrad, translateWriteSamplerFeedback,
     DXIL::OpCode::WriteSamplerFeedbackGrad},
    {IntrinsicOp::MOP_WriteSamplerFeedbackLevel, translateWriteSamplerFeedback,
     DXIL::OpCode::WriteSamplerFeedbackLevel},

    {IntrinsicOp::MOP_Abort, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_Abort},
    {IntrinsicOp::MOP_CandidateGeometryIndex, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CandidateGeometryIndex},
    {IntrinsicOp::MOP_CandidateInstanceContributionToHitGroupIndex,
     translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CandidateInstanceContributionToHitGroupIndex},
    {IntrinsicOp::MOP_CandidateInstanceID, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CandidateInstanceID},
    {IntrinsicOp::MOP_CandidateInstanceIndex, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CandidateInstanceIndex},
    {IntrinsicOp::MOP_CandidateObjectRayDirection,
     translateRayQueryFloat3Getter,
     DXIL::OpCode::RayQuery_CandidateObjectRayDirection},
    {IntrinsicOp::MOP_CandidateObjectRayOrigin, translateRayQueryFloat3Getter,
     DXIL::OpCode::RayQuery_CandidateObjectRayOrigin},
    {IntrinsicOp::MOP_CandidateObjectToWorld3x4,
     translateRayQueryMatrix3x4Operation,
     DXIL::OpCode::RayQuery_CandidateObjectToWorld3x4},
    {IntrinsicOp::MOP_CandidateObjectToWorld4x3,
     translateRayQueryTransposedMatrix3x4Operation,
     DXIL::OpCode::RayQuery_CandidateObjectToWorld3x4},
    {IntrinsicOp::MOP_CandidatePrimitiveIndex, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CandidatePrimitiveIndex},
    {IntrinsicOp::MOP_CandidateProceduralPrimitiveNonOpaque,
     translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CandidateProceduralPrimitiveNonOpaque},
    {IntrinsicOp::MOP_CandidateTriangleBarycentrics,
     translateRayQueryFloat2Getter,
     DXIL::OpCode::RayQuery_CandidateTriangleBarycentrics},
    {IntrinsicOp::MOP_CandidateTriangleFrontFace,
     translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CandidateTriangleFrontFace},
    {IntrinsicOp::MOP_CandidateTriangleRayT, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CandidateTriangleRayT},
    {IntrinsicOp::MOP_CandidateType, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CandidateType},
    {IntrinsicOp::MOP_CandidateWorldToObject3x4,
     translateRayQueryMatrix3x4Operation,
     DXIL::OpCode::RayQuery_CandidateWorldToObject3x4},
    {IntrinsicOp::MOP_CandidateWorldToObject4x3,
     translateRayQueryTransposedMatrix3x4Operation,
     DXIL::OpCode::RayQuery_CandidateWorldToObject3x4},
    {IntrinsicOp::MOP_CommitNonOpaqueTriangleHit,
     translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CommitNonOpaqueTriangleHit},
    {IntrinsicOp::MOP_CommitProceduralPrimitiveHit,
     translateCommitProceduralPrimitiveHit,
     DXIL::OpCode::RayQuery_CommitProceduralPrimitiveHit},
    {IntrinsicOp::MOP_CommittedGeometryIndex, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CommittedGeometryIndex},
    {IntrinsicOp::MOP_CommittedInstanceContributionToHitGroupIndex,
     translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CommittedInstanceContributionToHitGroupIndex},
    {IntrinsicOp::MOP_CommittedInstanceID, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CommittedInstanceID},
    {IntrinsicOp::MOP_CommittedInstanceIndex, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CommittedInstanceIndex},
    {IntrinsicOp::MOP_CommittedObjectRayDirection,
     translateRayQueryFloat3Getter,
     DXIL::OpCode::RayQuery_CommittedObjectRayDirection},
    {IntrinsicOp::MOP_CommittedObjectRayOrigin, translateRayQueryFloat3Getter,
     DXIL::OpCode::RayQuery_CommittedObjectRayOrigin},
    {IntrinsicOp::MOP_CommittedObjectToWorld3x4,
     translateRayQueryMatrix3x4Operation,
     DXIL::OpCode::RayQuery_CommittedObjectToWorld3x4},
    {IntrinsicOp::MOP_CommittedObjectToWorld4x3,
     translateRayQueryTransposedMatrix3x4Operation,
     DXIL::OpCode::RayQuery_CommittedObjectToWorld3x4},
    {IntrinsicOp::MOP_CommittedPrimitiveIndex, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CommittedPrimitiveIndex},
    {IntrinsicOp::MOP_CommittedRayT, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CommittedRayT},
    {IntrinsicOp::MOP_CommittedStatus, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CommittedStatus},
    {IntrinsicOp::MOP_CommittedTriangleBarycentrics,
     translateRayQueryFloat2Getter,
     DXIL::OpCode::RayQuery_CommittedTriangleBarycentrics},
    {IntrinsicOp::MOP_CommittedTriangleFrontFace,
     translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_CommittedTriangleFrontFace},
    {IntrinsicOp::MOP_CommittedWorldToObject3x4,
     translateRayQueryMatrix3x4Operation,
     DXIL::OpCode::RayQuery_CommittedWorldToObject3x4},
    {IntrinsicOp::MOP_CommittedWorldToObject4x3,
     translateRayQueryTransposedMatrix3x4Operation,
     DXIL::OpCode::RayQuery_CommittedWorldToObject3x4},
    {IntrinsicOp::MOP_Proceed, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_Proceed},
    {IntrinsicOp::MOP_RayFlags, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_RayFlags},
    {IntrinsicOp::MOP_RayTMin, translateGenericRayQueryMethod,
     DXIL::OpCode::RayQuery_RayTMin},
    {IntrinsicOp::MOP_TraceRayInline, translateTraceRayInline,
     DXIL::OpCode::RayQuery_TraceRayInline},
    {IntrinsicOp::MOP_WorldRayDirection, translateRayQueryFloat3Getter,
     DXIL::OpCode::RayQuery_WorldRayDirection},
    {IntrinsicOp::MOP_WorldRayOrigin, translateRayQueryFloat3Getter,
     DXIL::OpCode::RayQuery_WorldRayOrigin},
    {IntrinsicOp::MOP_Count, translateNodeGetInputRecordCount,
     DXIL::OpCode::GetInputRecordCount},
    {IntrinsicOp::MOP_FinishedCrossGroupSharing,
     translateNodeFinishedCrossGroupSharing,
     DXIL::OpCode::FinishedCrossGroupSharing},
    {IntrinsicOp::MOP_GetGroupNodeOutputRecords,
     translateGetGroupNodeOutputRecords,
     DXIL::OpCode::AllocateNodeOutputRecords},
    {IntrinsicOp::MOP_GetThreadNodeOutputRecords,
     translateGetThreadNodeOutputRecords,
     DXIL::OpCode::AllocateNodeOutputRecords},
    {IntrinsicOp::MOP_IsValid, translateNodeOutputIsValid,
     DXIL::OpCode::NodeOutputIsValid},
    {IntrinsicOp::MOP_GroupIncrementOutputCount,
     translateNodeGroupIncrementOutputCount,
     DXIL::OpCode::IncrementOutputCount},
    {IntrinsicOp::MOP_ThreadIncrementOutputCount,
     translateNodeThreadIncrementOutputCount,
     DXIL::OpCode::IncrementOutputCount},
    {IntrinsicOp::MOP_OutputComplete, translateNodeOutputComplete,
     DXIL::OpCode::OutputComplete},

    // SPIRV change starts
    {IntrinsicOp::MOP_SubpassLoad, unsupportedVulkanIntrinsic,
     DXIL::OpCode::NumOpCodes},
    // SPIRV change ends

    // Manually added part.
    {IntrinsicOp::IOP_InterlockedUMax, translateIopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_InterlockedUMin, translateIopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_WaveActiveUMax, translateWaveA2A,
     DXIL::OpCode::WaveActiveOp},
    {IntrinsicOp::IOP_WaveActiveUMin, translateWaveA2A,
     DXIL::OpCode::WaveActiveOp},
    {IntrinsicOp::IOP_WaveActiveUProduct, translateWaveA2A,
     DXIL::OpCode::WaveActiveOp},
    {IntrinsicOp::IOP_WaveActiveUSum, translateWaveA2A,
     DXIL::OpCode::WaveActiveOp},
    {IntrinsicOp::IOP_WaveMultiPrefixUProduct, translateWaveMultiPrefix,
     DXIL::OpCode::WaveMultiPrefixOp},
    {IntrinsicOp::IOP_WaveMultiPrefixUSum, translateWaveMultiPrefix,
     DXIL::OpCode::WaveMultiPrefixOp},
    {IntrinsicOp::IOP_WavePrefixUProduct, translateWaveA2A,
     DXIL::OpCode::WavePrefixOp},
    {IntrinsicOp::IOP_WavePrefixUSum, translateWaveA2A,
     DXIL::OpCode::WavePrefixOp},
    {IntrinsicOp::IOP_uabs, translateUAbs, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_uclamp, translateClamp, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_udot, translateDot, DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_ufirstbithigh, translateFirstbitHi,
     DXIL::OpCode::FirstbitHi},
    {IntrinsicOp::IOP_umad, translateFuiTrinary, DXIL::OpCode::UMad},
    {IntrinsicOp::IOP_umax, translateFuiBinary, DXIL::OpCode::UMax},
    {IntrinsicOp::IOP_umin, translateFuiBinary, DXIL::OpCode::UMin},
    {IntrinsicOp::IOP_umul, translateMul, DXIL::OpCode::UMul},
    {IntrinsicOp::IOP_usign, translateUSign, DXIL::OpCode::UMax},
    {IntrinsicOp::MOP_InterlockedUMax, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_InterlockedUMin, translateMopAtomicBinaryOperation,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_DxHitObject_MakeNop, translateHitObjectMakeNop,
     DXIL::OpCode::HitObject_MakeNop},
    {IntrinsicOp::IOP_DxMaybeReorderThread, translateMaybeReorderThread,
     DXIL::OpCode::MaybeReorderThread},
    {IntrinsicOp::IOP_Vkstatic_pointer_cast, unsupportedVulkanIntrinsic,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::IOP_Vkreinterpret_pointer_cast, unsupportedVulkanIntrinsic,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_GetBufferContents, unsupportedVulkanIntrinsic,
     DXIL::OpCode::NumOpCodes},
    {IntrinsicOp::MOP_DxHitObject_FromRayQuery, translateHitObjectFromRayQuery,
     DXIL::OpCode::HitObject_FromRayQuery},
    {IntrinsicOp::MOP_DxHitObject_GetAttributes,
     translateHitObjectGetAttributes, DXIL::OpCode::HitObject_Attributes},
    {IntrinsicOp::MOP_DxHitObject_GetGeometryIndex,
     translateHitObjectScalarGetter, DXIL::OpCode::HitObject_GeometryIndex},
    {IntrinsicOp::MOP_DxHitObject_GetHitKind, translateHitObjectScalarGetter,
     DXIL::OpCode::HitObject_HitKind},
    {IntrinsicOp::MOP_DxHitObject_GetInstanceID, translateHitObjectScalarGetter,
     DXIL::OpCode::HitObject_InstanceID},
    {IntrinsicOp::MOP_DxHitObject_GetInstanceIndex,
     translateHitObjectScalarGetter, DXIL::OpCode::HitObject_InstanceIndex},
    {IntrinsicOp::MOP_DxHitObject_GetObjectRayDirection,
     translateHitObjectVectorGetter,
     DXIL::OpCode::HitObject_ObjectRayDirection},
    {IntrinsicOp::MOP_DxHitObject_GetObjectRayOrigin,
     translateHitObjectVectorGetter, DXIL::OpCode::HitObject_ObjectRayOrigin},
    {IntrinsicOp::MOP_DxHitObject_GetObjectToWorld3x4,
     translateHitObjectMatrixGetter, DXIL::OpCode::HitObject_ObjectToWorld3x4},
    {IntrinsicOp::MOP_DxHitObject_GetObjectToWorld4x3,
     translateHitObjectMatrixGetter, DXIL::OpCode::HitObject_ObjectToWorld3x4},
    {IntrinsicOp::MOP_DxHitObject_GetPrimitiveIndex,
     translateHitObjectScalarGetter, DXIL::OpCode::HitObject_PrimitiveIndex},
    {IntrinsicOp::MOP_DxHitObject_GetRayFlags, translateHitObjectScalarGetter,
     DXIL::OpCode::HitObject_RayFlags},
    {IntrinsicOp::MOP_DxHitObject_GetRayTCurrent,
     translateHitObjectScalarGetter, DXIL::OpCode::HitObject_RayTCurrent},
    {IntrinsicOp::MOP_DxHitObject_GetRayTMin, translateHitObjectScalarGetter,
     DXIL::OpCode::HitObject_RayTMin},
    {IntrinsicOp::MOP_DxHitObject_GetShaderTableIndex,
     translateHitObjectScalarGetter, DXIL::OpCode::HitObject_ShaderTableIndex},
    {IntrinsicOp::MOP_DxHitObject_GetWorldRayDirection,
     translateHitObjectVectorGetter, DXIL::OpCode::HitObject_WorldRayDirection},
    {IntrinsicOp::MOP_DxHitObject_GetWorldRayOrigin,
     translateHitObjectVectorGetter, DXIL::OpCode::HitObject_WorldRayOrigin},
    {IntrinsicOp::MOP_DxHitObject_GetWorldToObject3x4,
     translateHitObjectMatrixGetter, DXIL::OpCode::HitObject_WorldToObject3x4},
    {IntrinsicOp::MOP_DxHitObject_GetWorldToObject4x3,
     translateHitObjectMatrixGetter, DXIL::OpCode::HitObject_WorldToObject3x4},
    {IntrinsicOp::MOP_DxHitObject_Invoke, translateHitObjectInvoke,
     DXIL::OpCode::HitObject_Invoke},
    {IntrinsicOp::MOP_DxHitObject_IsHit, translateHitObjectScalarGetter,
     DXIL::OpCode::HitObject_IsHit},
    {IntrinsicOp::MOP_DxHitObject_IsMiss, translateHitObjectScalarGetter,
     DXIL::OpCode::HitObject_IsMiss},
    {IntrinsicOp::MOP_DxHitObject_IsNop, translateHitObjectScalarGetter,
     DXIL::OpCode::HitObject_IsNop},
    {IntrinsicOp::MOP_DxHitObject_LoadLocalRootTableConstant,
     translateHitObjectLoadLocalRootTableConstant,
     DXIL::OpCode::HitObject_LoadLocalRootTableConstant},
    {IntrinsicOp::MOP_DxHitObject_MakeMiss, translateHitObjectMakeMiss,
     DXIL::OpCode::HitObject_MakeMiss},
    {IntrinsicOp::MOP_DxHitObject_SetShaderTableIndex,
     translateHitObjectSetShaderTableIndex,
     DXIL::OpCode::HitObject_SetShaderTableIndex},
    {IntrinsicOp::MOP_DxHitObject_TraceRay, translateHitObjectTraceRay,
     DXIL::OpCode::HitObject_TraceRay},

    {IntrinsicOp::IOP___builtin_MatVecMul, translateMatVecMul,
     DXIL::OpCode::MatVecMul},
    {IntrinsicOp::IOP___builtin_MatVecMulAdd, translateMatVecMulAdd,
     DXIL::OpCode::MatVecMulAdd},
    {IntrinsicOp::IOP___builtin_OuterProductAccumulate,
     translateOuterProductAccumulate, DXIL::OpCode::OuterProductAccumulate},
    {IntrinsicOp::IOP___builtin_VectorAccumulate, translateVectorAccumulate,
     DXIL::OpCode::VectorAccumulate},
    {IntrinsicOp::IOP_isnormal, trivialIsSpecialFloat, DXIL::OpCode::IsNormal},
};
} // namespace
static_assert(
    sizeof(GLowerTable) / sizeof(GLowerTable[0]) ==
        static_cast<size_t>(IntrinsicOp::Num_Intrinsics),
    "Intrinsic lowering table must be updated to account for new intrinsics.");

static void translateBuiltinIntrinsic(CallInst *CI,
                                      HLOperationLowerHelper &Helper,
                                      HLObjectOperationLowerHelper *PObjHelper,
                                      bool &Translated) {
  unsigned Opcode = hlsl::GetHLOpcode(CI);
  const IntrinsicLower &Lower = GLowerTable[Opcode];
  Value *Result = Lower.LowerFunc(CI, Lower.IntriOpcode, Lower.DxilOpcode,
                                  Helper, PObjHelper, Translated);
  if (Result)
    CI->replaceAllUsesWith(Result);
}

// SharedMem.
namespace {

bool isSharedMemPtr(Value *Ptr) {
  return Ptr->getType()->getPointerAddressSpace() == DXIL::kTGSMAddrSpace;
}

bool isLocalVariablePtr(Value *Ptr) {
  while (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
    Ptr = GEP->getPointerOperand();
  }
  bool IsAlloca = isa<AllocaInst>(Ptr);
  if (IsAlloca)
    return true;

  GlobalVariable *GV = dyn_cast<GlobalVariable>(Ptr);
  if (!GV)
    return false;

  return GV->getLinkage() == GlobalValue::LinkageTypes::InternalLinkage;
}

} // namespace

// Constant buffer.
namespace {
unsigned getEltTypeByteSizeForConstBuf(Type *EltType, const DataLayout &DL) {
  DXASSERT(EltType->isIntegerTy() || EltType->isFloatingPointTy(),
           "not an element type");
  // TODO: Use real size after change constant buffer into linear layout.
  if (DL.getTypeSizeInBits(EltType) <= 32) {
    // Constant buffer is 4 bytes align.
    return 4;
  }

  return 8;
}

Value *generateCbLoad(Value *Handle, Value *Offset, Type *EltTy, OP *HlslOp,
                      IRBuilder<> &Builder) {
  Constant *OpArg = HlslOp->GetU32Const((unsigned)OP::OpCode::CBufferLoad);

  DXASSERT(!EltTy->isIntegerTy(1),
           "Bools should not be loaded as their register representation.");

  // Align to 8 bytes for now.
  Constant *Align = HlslOp->GetU32Const(8);
  Function *CBLoad = HlslOp->GetOpFunc(OP::OpCode::CBufferLoad, EltTy);
  return Builder.CreateCall(CBLoad, {OpArg, Handle, Offset, Align});
}

Value *translateConstBufMatLd(Type *MatType, Value *Handle, Value *Offset,
                              bool ColMajor, OP *OP, const DataLayout &DL,
                              IRBuilder<> &Builder) {
  HLMatrixType MatTy = HLMatrixType::cast(MatType);
  Type *EltTy = MatTy.getElementTypeForMem();
  unsigned MatSize = MatTy.getNumElements();
  std::vector<Value *> Elts(MatSize);
  Value *EltByteSize = ConstantInt::get(
      Offset->getType(), getEltTypeByteSizeForConstBuf(EltTy, DL));

  // TODO: use real size after change constant buffer into linear layout.
  Value *BaseOffset = Offset;
  for (unsigned I = 0; I < MatSize; I++) {
    Elts[I] = generateCbLoad(Handle, BaseOffset, EltTy, OP, Builder);
    BaseOffset = Builder.CreateAdd(BaseOffset, EltByteSize);
  }

  Value *Vec = HLMatrixLower::BuildVector(EltTy, Elts, Builder);
  Vec = MatTy.emitLoweredMemToReg(Vec, Builder);
  return Vec;
}

void translateCbGep(GetElementPtrInst *GEP, Value *Handle, Value *BaseOffset,
                    hlsl::OP *HlslOp, IRBuilder<> &Builder,
                    DxilFieldAnnotation *PrevFieldAnnotation,
                    const DataLayout &DL, DxilTypeSystem &DxilTypeSys,
                    HLObjectOperationLowerHelper *PObjHelper);

Value *generateVecEltFromGep(Value *LdData, GetElementPtrInst *GEP,
                             IRBuilder<> &Builder, bool BInsertLdNextToGep) {
  DXASSERT(GEP->getNumIndices() == 2, "must have 2 level");
  Value *BaseIdx = (GEP->idx_begin())->get();
  Value *ZeroIdx = Builder.getInt32(0);
  DXASSERT_LOCALVAR(baseIdx && zeroIdx, BaseIdx == ZeroIdx,
                    "base index must be 0");
  Value *Idx = (GEP->idx_begin() + 1)->get();
  if (isa<ConstantInt>(Idx)) {
    return Builder.CreateExtractElement(LdData, Idx);
  }

  // Dynamic indexing.
  // Copy vec to array.
  Type *Ty = LdData->getType();
  Type *EltTy = Ty->getVectorElementType();
  unsigned VecSize = Ty->getVectorNumElements();
  ArrayType *AT = ArrayType::get(EltTy, VecSize);
  IRBuilder<> AllocaBuilder(
      GEP->getParent()->getParent()->getEntryBlock().getFirstInsertionPt());
  Value *TempArray = AllocaBuilder.CreateAlloca(AT);
  Value *Zero = Builder.getInt32(0);
  for (unsigned int I = 0; I < VecSize; I++) {
    Value *Elt = Builder.CreateExtractElement(LdData, Builder.getInt32(I));
    Value *Ptr =
        Builder.CreateInBoundsGEP(TempArray, {Zero, Builder.getInt32(I)});
    Builder.CreateStore(Elt, Ptr);
  }
  // Load from temp array.
  if (BInsertLdNextToGep) {
    // Insert the new GEP just before the old and to-be-deleted GEP
    Builder.SetInsertPoint(GEP);
  }
  Value *EltGEP = Builder.CreateInBoundsGEP(TempArray, {Zero, Idx});
  return Builder.CreateLoad(EltGEP);
}

void translateResourceInCb(LoadInst *LI,
                           HLObjectOperationLowerHelper *PObjHelper,
                           GlobalVariable *CbGV) {
  if (LI->user_empty()) {
    LI->eraseFromParent();
    return;
  }

  GetElementPtrInst *Ptr = cast<GetElementPtrInst>(LI->getPointerOperand());
  CallInst *CI = cast<CallInst>(LI->user_back());
  CallInst *Anno = cast<CallInst>(CI->user_back());
  DxilResourceProperties RP = PObjHelper->getResPropsFromAnnotateHandle(Anno);
  Value *ResPtr = PObjHelper->getOrCreateResourceForCbPtr(Ptr, CbGV, RP);

  // Lower Ptr to GV base Ptr.
  Value *GvPtr = PObjHelper->lowerCbResourcePtr(Ptr, ResPtr);
  IRBuilder<> Builder(LI);
  Value *GvLd = Builder.CreateLoad(GvPtr);
  LI->replaceAllUsesWith(GvLd);
  LI->eraseFromParent();
}

void translateCbAddressUser(Instruction *User, Value *Handle, Value *BaseOffset,
                            hlsl::OP *HlslOp,
                            DxilFieldAnnotation *PrevFieldAnnotation,
                            DxilTypeSystem &DxilTypeSys, const DataLayout &DL,
                            HLObjectOperationLowerHelper *PObjHelper) {
  IRBuilder<> Builder(User);
  if (CallInst *CI = dyn_cast<CallInst>(User)) {
    HLOpcodeGroup Group = GetHLOpcodeGroupByName(CI->getCalledFunction());
    unsigned Opcode = GetHLOpcode(CI);
    if (Group == HLOpcodeGroup::HLMatLoadStore) {
      HLMatLoadStoreOpcode MatOp = static_cast<HLMatLoadStoreOpcode>(Opcode);
      bool ColMajor = MatOp == HLMatLoadStoreOpcode::ColMatLoad;
      DXASSERT(MatOp == HLMatLoadStoreOpcode::ColMatLoad ||
                   MatOp == HLMatLoadStoreOpcode::RowMatLoad,
               "No store on cbuffer");
      Type *MatType = CI->getArgOperand(HLOperandIndex::kMatLoadPtrOpIdx)
                          ->getType()
                          ->getPointerElementType();
      Value *NewLd = translateConstBufMatLd(MatType, Handle, BaseOffset,
                                            ColMajor, HlslOp, DL, Builder);
      CI->replaceAllUsesWith(NewLd);
      CI->eraseFromParent();
    } else if (Group == HLOpcodeGroup::HLSubscript) {
      HLSubscriptOpcode SubOp = static_cast<HLSubscriptOpcode>(Opcode);
      Value *BasePtr = CI->getArgOperand(HLOperandIndex::kMatSubscriptMatOpIdx);
      HLMatrixType MatTy =
          HLMatrixType::cast(BasePtr->getType()->getPointerElementType());
      Type *EltTy = MatTy.getElementTypeForReg();

      Value *EltByteSize = ConstantInt::get(
          BaseOffset->getType(), getEltTypeByteSizeForConstBuf(EltTy, DL));

      Value *Idx = CI->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx);

      Type *ResultType = CI->getType()->getPointerElementType();
      unsigned ResultSize = 1;
      if (ResultType->isVectorTy())
        ResultSize = ResultType->getVectorNumElements();
      DXASSERT(ResultSize <= 16, "up to 4x4 elements in vector or matrix");
      assert(ResultSize <= 16);
      Value *IdxList[16];

      switch (SubOp) {
      case HLSubscriptOpcode::ColMatSubscript:
      case HLSubscriptOpcode::RowMatSubscript: {
        for (unsigned I = 0; I < ResultSize; I++) {
          Value *Idx =
              CI->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx + I);
          Value *Offset = Builder.CreateMul(Idx, EltByteSize);
          IdxList[I] = Builder.CreateAdd(BaseOffset, Offset);
        }

      } break;
      case HLSubscriptOpcode::RowMatElement:
      case HLSubscriptOpcode::ColMatElement: {
        Constant *EltIdxs = cast<Constant>(Idx);
        for (unsigned I = 0; I < ResultSize; I++) {
          Value *Offset =
              Builder.CreateMul(EltIdxs->getAggregateElement(I), EltByteSize);
          IdxList[I] = Builder.CreateAdd(BaseOffset, Offset);
        }
      } break;
      default:
        DXASSERT(0, "invalid operation on const buffer");
        break;
      }

      Value *LdData = UndefValue::get(ResultType);
      if (ResultType->isVectorTy()) {
        for (unsigned I = 0; I < ResultSize; I++) {
          Value *EltData =
              generateCbLoad(Handle, IdxList[I], EltTy, HlslOp, Builder);
          LdData = Builder.CreateInsertElement(LdData, EltData, I);
        }
      } else {
        LdData = generateCbLoad(Handle, IdxList[0], EltTy, HlslOp, Builder);
      }

      for (auto U = CI->user_begin(); U != CI->user_end();) {
        Value *SubsUser = *(U++);
        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(SubsUser)) {
          Value *SubData = generateVecEltFromGep(LdData, GEP, Builder,
                                                 /*bInsertLdNextToGEP*/ true);

          for (auto GepU = GEP->user_begin(); GepU != GEP->user_end();) {
            Value *GepUser = *(GepU++);
            // Must be load here;
            LoadInst *LdUser = cast<LoadInst>(GepUser);
            LdUser->replaceAllUsesWith(SubData);
            LdUser->eraseFromParent();
          }
          GEP->eraseFromParent();
        } else {
          // Must be load here.
          LoadInst *LdUser = cast<LoadInst>(SubsUser);
          LdUser->replaceAllUsesWith(LdData);
          LdUser->eraseFromParent();
        }
      }

      CI->eraseFromParent();
    } else {
      DXASSERT(0, "not implemented yet");
    }
  } else if (LoadInst *LdInst = dyn_cast<LoadInst>(User)) {
    Type *Ty = LdInst->getType();
    Type *EltTy = Ty->getScalarType();
    // Resource inside cbuffer is lowered after GenerateDxilOperations.
    if (dxilutil::IsHLSLObjectType(Ty)) {
      CallInst *CI = cast<CallInst>(Handle);
      // CI should be annotate handle.
      // Need createHandle here.
      if (GetHLOpcodeGroup(CI->getCalledFunction()) ==
          HLOpcodeGroup::HLAnnotateHandle)
        CI = cast<CallInst>(CI->getArgOperand(HLOperandIndex::kHandleOpIdx));
      GlobalVariable *CbGV = cast<GlobalVariable>(
          CI->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx));
      translateResourceInCb(LdInst, PObjHelper, CbGV);
      return;
    }
    DXASSERT(!Ty->isAggregateType(), "should be flat in previous pass");

    unsigned EltByteSize = getEltTypeByteSizeForConstBuf(EltTy, DL);

    Value *NewLd = generateCbLoad(Handle, BaseOffset, EltTy, HlslOp, Builder);
    if (Ty->isVectorTy()) {
      Value *Result = UndefValue::get(Ty);
      Result = Builder.CreateInsertElement(Result, NewLd, (uint64_t)0);
      // Update offset by 4 bytes.
      Value *Offset =
          Builder.CreateAdd(BaseOffset, HlslOp->GetU32Const(EltByteSize));
      for (unsigned I = 1; I < Ty->getVectorNumElements(); I++) {
        Value *Elt = generateCbLoad(Handle, Offset, EltTy, HlslOp, Builder);
        Result = Builder.CreateInsertElement(Result, Elt, I);
        // Update offset by 4 bytes.
        Offset = Builder.CreateAdd(Offset, HlslOp->GetU32Const(EltByteSize));
      }
      NewLd = Result;
    }

    LdInst->replaceAllUsesWith(NewLd);
    LdInst->eraseFromParent();
  } else {
    // Must be GEP here
    GetElementPtrInst *GEP = cast<GetElementPtrInst>(User);
    translateCbGep(GEP, Handle, BaseOffset, HlslOp, Builder,
                   PrevFieldAnnotation, DL, DxilTypeSys, PObjHelper);
    GEP->eraseFromParent();
  }
}

void translateCbGep(GetElementPtrInst *GEP, Value *Handle, Value *BaseOffset,
                    hlsl::OP *HlslOp, IRBuilder<> &Builder,
                    DxilFieldAnnotation *PrevFieldAnnotation,
                    const DataLayout &DL, DxilTypeSystem &DxilTypeSys,
                    HLObjectOperationLowerHelper *PObjHelper) {
  SmallVector<Value *, 8> Indices(GEP->idx_begin(), GEP->idx_end());

  Value *Offset = BaseOffset;
  // update offset
  DxilFieldAnnotation *FieldAnnotation = PrevFieldAnnotation;

  gep_type_iterator GEPIt = gep_type_begin(GEP), E = gep_type_end(GEP);

  for (; GEPIt != E; GEPIt++) {
    Value *Idx = GEPIt.getOperand();
    unsigned ImmIdx = 0;
    bool BImmIdx = false;
    if (Constant *ConstIdx = dyn_cast<Constant>(Idx)) {
      ImmIdx = ConstIdx->getUniqueInteger().getLimitedValue();
      BImmIdx = true;
    }

    if (GEPIt->isPointerTy()) {
      Type *EltTy = GEPIt->getPointerElementType();
      unsigned Size = 0;
      if (StructType *ST = dyn_cast<StructType>(EltTy)) {
        DxilStructAnnotation *Annotation = DxilTypeSys.GetStructAnnotation(ST);
        Size = Annotation->GetCBufferSize();
      } else {
        DXASSERT(FieldAnnotation, "must be a field");
        if (ArrayType *AT = dyn_cast<ArrayType>(EltTy)) {
          unsigned EltSize = dxilutil::GetLegacyCBufferFieldElementSize(
              *FieldAnnotation, EltTy, DxilTypeSys);

          // Decide the nested array size.
          unsigned NestedArraySize = 1;

          Type *EltTy = AT->getArrayElementType();
          // support multi level of array
          while (EltTy->isArrayTy()) {
            ArrayType *EltAT = cast<ArrayType>(EltTy);
            NestedArraySize *= EltAT->getNumElements();
            EltTy = EltAT->getElementType();
          }
          // Align to 4 * 4 bytes.
          unsigned AlignedSize = (EltSize + 15) & 0xfffffff0;
          Size = NestedArraySize * AlignedSize;
        } else {
          Size = DL.getTypeAllocSize(EltTy);
        }
      }
      // Align to 4 * 4 bytes.
      Size = (Size + 15) & 0xfffffff0;
      if (BImmIdx) {
        unsigned TempOffset = Size * ImmIdx;
        Offset = Builder.CreateAdd(Offset, HlslOp->GetU32Const(TempOffset));
      } else {
        Value *TempOffset = Builder.CreateMul(Idx, HlslOp->GetU32Const(Size));
        Offset = Builder.CreateAdd(Offset, TempOffset);
      }
    } else if (GEPIt->isStructTy()) {
      StructType *ST = cast<StructType>(*GEPIt);
      DxilStructAnnotation *Annotation = DxilTypeSys.GetStructAnnotation(ST);
      FieldAnnotation = &Annotation->GetFieldAnnotation(ImmIdx);
      unsigned StructOffset = FieldAnnotation->GetCBufferOffset();
      Offset = Builder.CreateAdd(Offset, HlslOp->GetU32Const(StructOffset));
    } else if (GEPIt->isArrayTy()) {
      DXASSERT(FieldAnnotation != nullptr, "must a field");
      unsigned EltSize = dxilutil::GetLegacyCBufferFieldElementSize(
          *FieldAnnotation, *GEPIt, DxilTypeSys);
      // Decide the nested array size.
      unsigned NestedArraySize = 1;

      Type *EltTy = GEPIt->getArrayElementType();
      // support multi level of array
      while (EltTy->isArrayTy()) {
        ArrayType *EltAT = cast<ArrayType>(EltTy);
        NestedArraySize *= EltAT->getNumElements();
        EltTy = EltAT->getElementType();
      }
      // Align to 4 * 4 bytes.
      unsigned AlignedSize = (EltSize + 15) & 0xfffffff0;
      unsigned Size = NestedArraySize * AlignedSize;
      if (BImmIdx) {
        unsigned TempOffset = Size * ImmIdx;
        Offset = Builder.CreateAdd(Offset, HlslOp->GetU32Const(TempOffset));
      } else {
        Value *TempOffset = Builder.CreateMul(Idx, HlslOp->GetU32Const(Size));
        Offset = Builder.CreateAdd(Offset, TempOffset);
      }
    } else if (GEPIt->isVectorTy()) {
      unsigned Size = DL.getTypeAllocSize(GEPIt->getVectorElementType());
      if (BImmIdx) {
        unsigned TempOffset = Size * ImmIdx;
        Offset = Builder.CreateAdd(Offset, HlslOp->GetU32Const(TempOffset));
      } else {
        Value *TempOffset = Builder.CreateMul(Idx, HlslOp->GetU32Const(Size));
        Offset = Builder.CreateAdd(Offset, TempOffset);
      }
    } else {
      gep_type_iterator Temp = GEPIt;
      Temp++;
      DXASSERT(Temp == E, "scalar type must be the last");
    }
  }

  for (auto U = GEP->user_begin(); U != GEP->user_end();) {
    Instruction *User = cast<Instruction>(*(U++));

    translateCbAddressUser(User, Handle, Offset, HlslOp, FieldAnnotation,
                           DxilTypeSys, DL, PObjHelper);
  }
}

Value *generateCbLoadLegacy(Value *Handle, Value *LegacyIdx,
                            unsigned ChannelOffset, Type *EltTy, OP *HlslOp,
                            IRBuilder<> &Builder) {
  Constant *OpArg =
      HlslOp->GetU32Const((unsigned)OP::OpCode::CBufferLoadLegacy);

  DXASSERT(!EltTy->isIntegerTy(1),
           "Bools should not be loaded as their register representation.");

  Type *DoubleTy = Type::getDoubleTy(EltTy->getContext());
  Type *HalfTy = Type::getHalfTy(EltTy->getContext());
  Type *I64Ty = Type::getInt64Ty(EltTy->getContext());
  Type *I16Ty = Type::getInt16Ty(EltTy->getContext());

  bool Is64 = (EltTy == DoubleTy) | (EltTy == I64Ty);
  bool Is16 = (EltTy == HalfTy || EltTy == I16Ty) && !HlslOp->UseMinPrecision();
  DXASSERT_LOCALVAR(is16, (Is16 && ChannelOffset < 8) || ChannelOffset < 4,
                    "legacy cbuffer don't across 16 bytes register.");
  if (Is64) {
    Function *CBLoad = HlslOp->GetOpFunc(OP::OpCode::CBufferLoadLegacy, EltTy);
    Value *LoadLegacy = Builder.CreateCall(CBLoad, {OpArg, Handle, LegacyIdx});
    DXASSERT((ChannelOffset & 1) == 0,
             "channel offset must be even for double");
    unsigned EltIdx = ChannelOffset >> 1;
    Value *Result = Builder.CreateExtractValue(LoadLegacy, EltIdx);
    return Result;
  }

  Function *CBLoad = HlslOp->GetOpFunc(OP::OpCode::CBufferLoadLegacy, EltTy);
  Value *LoadLegacy = Builder.CreateCall(CBLoad, {OpArg, Handle, LegacyIdx});
  return Builder.CreateExtractValue(LoadLegacy, ChannelOffset);
}

Value *generateCbLoadLegacy(Value *Handle, Value *LegacyIdx,
                            unsigned ChannelOffset, Type *EltTy,
                            unsigned VecSize, OP *HlslOp,
                            IRBuilder<> &Builder) {
  Constant *OpArg =
      HlslOp->GetU32Const((unsigned)OP::OpCode::CBufferLoadLegacy);

  DXASSERT(!EltTy->isIntegerTy(1),
           "Bools should not be loaded as their register representation.");

  Type *DoubleTy = Type::getDoubleTy(EltTy->getContext());
  Type *I64Ty = Type::getInt64Ty(EltTy->getContext());
  Type *HalfTy = Type::getHalfTy(EltTy->getContext());
  Type *ShortTy = Type::getInt16Ty(EltTy->getContext());

  bool Is64 = (EltTy == DoubleTy) | (EltTy == I64Ty);
  bool Is16 =
      (EltTy == ShortTy || EltTy == HalfTy) && !HlslOp->UseMinPrecision();
  DXASSERT((Is16 && ChannelOffset + VecSize <= 8) ||
               (ChannelOffset + VecSize) <= 4,
           "legacy cbuffer don't across 16 bytes register.");
  if (Is16) {
    Function *CBLoad = HlslOp->GetOpFunc(OP::OpCode::CBufferLoadLegacy, EltTy);
    Value *LoadLegacy = Builder.CreateCall(CBLoad, {OpArg, Handle, LegacyIdx});
    Value *Result = UndefValue::get(VectorType::get(EltTy, VecSize));
    for (unsigned I = 0; I < VecSize; ++I) {
      Value *NewElt = Builder.CreateExtractValue(LoadLegacy, ChannelOffset + I);
      Result = Builder.CreateInsertElement(Result, NewElt, I);
    }
    return Result;
  }

  if (Is64) {
    Function *CBLoad = HlslOp->GetOpFunc(OP::OpCode::CBufferLoadLegacy, EltTy);
    Value *LoadLegacy = Builder.CreateCall(CBLoad, {OpArg, Handle, LegacyIdx});
    Value *Result = UndefValue::get(VectorType::get(EltTy, VecSize));
    unsigned SmallVecSize = 2;
    if (VecSize < SmallVecSize)
      SmallVecSize = VecSize;
    for (unsigned I = 0; I < SmallVecSize; ++I) {
      Value *NewElt = Builder.CreateExtractValue(LoadLegacy, ChannelOffset + I);
      Result = Builder.CreateInsertElement(Result, NewElt, I);
    }
    if (VecSize > 2) {
      // Got to next cb register.
      LegacyIdx = Builder.CreateAdd(LegacyIdx, HlslOp->GetU32Const(1));
      Value *LoadLegacy =
          Builder.CreateCall(CBLoad, {OpArg, Handle, LegacyIdx});
      for (unsigned I = 2; I < VecSize; ++I) {
        Value *NewElt = Builder.CreateExtractValue(LoadLegacy, I - 2);
        Result = Builder.CreateInsertElement(Result, NewElt, I);
      }
    }
    return Result;
  }

  Function *CBLoad = HlslOp->GetOpFunc(OP::OpCode::CBufferLoadLegacy, EltTy);
  Value *LoadLegacy = Builder.CreateCall(CBLoad, {OpArg, Handle, LegacyIdx});
  Value *Result = UndefValue::get(VectorType::get(EltTy, VecSize));
  for (unsigned I = 0; I < VecSize; ++I) {
    Value *NewElt = Builder.CreateExtractValue(LoadLegacy, ChannelOffset + I);
    Result = Builder.CreateInsertElement(Result, NewElt, I);
  }
  return Result;
}

Value *translateConstBufMatLdLegacy(HLMatrixType MatTy, Value *Handle,
                                    Value *LegacyIdx, bool ColMajor, OP *OP,
                                    bool MemElemRepr, const DataLayout &DL,
                                    IRBuilder<> &Builder) {
  Type *EltTy = MatTy.getElementTypeForMem();

  unsigned MatSize = MatTy.getNumElements();
  std::vector<Value *> Elts(MatSize);
  unsigned EltByteSize = getEltTypeByteSizeForConstBuf(EltTy, DL);
  if (ColMajor) {
    unsigned ColByteSize = 4 * EltByteSize;
    unsigned ColRegSize = (ColByteSize + 15) >> 4;
    for (unsigned C = 0; C < MatTy.getNumColumns(); C++) {
      Value *Col = generateCbLoadLegacy(Handle, LegacyIdx, /*channelOffset*/ 0,
                                        EltTy, MatTy.getNumRows(), OP, Builder);

      for (unsigned R = 0; R < MatTy.getNumRows(); R++) {
        unsigned MatIdx = MatTy.getColumnMajorIndex(R, C);
        Elts[MatIdx] = Builder.CreateExtractElement(Col, R);
      }
      // Update offset for a column.
      LegacyIdx = Builder.CreateAdd(LegacyIdx, OP->GetU32Const(ColRegSize));
    }
  } else {
    unsigned RowByteSize = 4 * EltByteSize;
    unsigned RowRegSize = (RowByteSize + 15) >> 4;
    for (unsigned R = 0; R < MatTy.getNumRows(); R++) {
      Value *Row =
          generateCbLoadLegacy(Handle, LegacyIdx, /*channelOffset*/ 0, EltTy,
                               MatTy.getNumColumns(), OP, Builder);
      for (unsigned C = 0; C < MatTy.getNumColumns(); C++) {
        unsigned MatIdx = MatTy.getRowMajorIndex(R, C);
        Elts[MatIdx] = Builder.CreateExtractElement(Row, C);
      }
      // Update offset for a row.
      LegacyIdx = Builder.CreateAdd(LegacyIdx, OP->GetU32Const(RowRegSize));
    }
  }

  Value *Vec = HLMatrixLower::BuildVector(EltTy, Elts, Builder);
  if (!MemElemRepr)
    Vec = MatTy.emitLoweredMemToReg(Vec, Builder);
  return Vec;
}

void translateCbGepLegacy(GetElementPtrInst *GEP, Value *Handle,
                          Value *LegacyIdx, unsigned ChannelOffset,
                          hlsl::OP *HlslOp, IRBuilder<> &Builder,
                          DxilFieldAnnotation *PrevFieldAnnotation,
                          const DataLayout &DL, DxilTypeSystem &DxilTypeSys,
                          HLObjectOperationLowerHelper *PObjHelper);

void translateCbAddressUserLegacy(Instruction *User, Value *Handle,
                                  Value *LegacyIdx, unsigned ChannelOffset,
                                  hlsl::OP *HlslOp,
                                  DxilFieldAnnotation *PrevFieldAnnotation,
                                  DxilTypeSystem &DxilTypeSys,
                                  const DataLayout &DL,
                                  HLObjectOperationLowerHelper *PObjHelper) {
  IRBuilder<> Builder(User);
  if (CallInst *CI = dyn_cast<CallInst>(User)) {
    HLOpcodeGroup Group = GetHLOpcodeGroupByName(CI->getCalledFunction());
    if (Group == HLOpcodeGroup::HLMatLoadStore) {
      unsigned Opcode = GetHLOpcode(CI);
      HLMatLoadStoreOpcode MatOp = static_cast<HLMatLoadStoreOpcode>(Opcode);
      bool ColMajor = MatOp == HLMatLoadStoreOpcode::ColMatLoad;
      DXASSERT(MatOp == HLMatLoadStoreOpcode::ColMatLoad ||
                   MatOp == HLMatLoadStoreOpcode::RowMatLoad,
               "No store on cbuffer");
      HLMatrixType MatTy =
          HLMatrixType::cast(CI->getArgOperand(HLOperandIndex::kMatLoadPtrOpIdx)
                                 ->getType()
                                 ->getPointerElementType());
      // This will replace a call, so we should use the register representation
      // of elements
      Value *NewLd = translateConstBufMatLdLegacy(
          MatTy, Handle, LegacyIdx, ColMajor, HlslOp, /*memElemRepr*/ false, DL,
          Builder);
      CI->replaceAllUsesWith(NewLd);
      dxilutil::TryScatterDebugValueToVectorElements(NewLd);
      CI->eraseFromParent();
    } else if (Group == HLOpcodeGroup::HLSubscript) {
      unsigned Opcode = GetHLOpcode(CI);
      HLSubscriptOpcode SubOp = static_cast<HLSubscriptOpcode>(Opcode);
      Value *BasePtr = CI->getArgOperand(HLOperandIndex::kMatSubscriptMatOpIdx);
      HLMatrixType MatTy =
          HLMatrixType::cast(BasePtr->getType()->getPointerElementType());
      Type *EltTy = MatTy.getElementTypeForReg();

      Value *Idx = CI->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx);

      Type *ResultType = CI->getType()->getPointerElementType();
      unsigned ResultSize = 1;
      if (ResultType->isVectorTy())
        ResultSize = ResultType->getVectorNumElements();
      DXASSERT(ResultSize <= 16, "up to 4x4 elements in vector or matrix");
      assert(ResultSize <= 16);
      Value *IdxList[16];
      bool ColMajor = SubOp == HLSubscriptOpcode::ColMatSubscript ||
                      SubOp == HLSubscriptOpcode::ColMatElement;
      bool DynamicIndexing = !isa<ConstantInt>(Idx) &&
                             !isa<ConstantAggregateZero>(Idx) &&
                             !isa<ConstantDataSequential>(Idx);

      Value *LdData = UndefValue::get(ResultType);
      if (!DynamicIndexing) {
        // This will replace a load or GEP, so we should use the memory
        // representation of elements
        Value *MatLd = translateConstBufMatLdLegacy(
            MatTy, Handle, LegacyIdx, ColMajor, HlslOp, /*memElemRepr*/ true,
            DL, Builder);
        // The matLd is keep original layout, just use the idx calc in
        // EmitHLSLMatrixElement and EmitHLSLMatrixSubscript.
        switch (SubOp) {
        case HLSubscriptOpcode::RowMatSubscript:
        case HLSubscriptOpcode::ColMatSubscript: {
          for (unsigned I = 0; I < ResultSize; I++) {
            IdxList[I] =
                CI->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx + I);
          }
        } break;
        case HLSubscriptOpcode::RowMatElement:
        case HLSubscriptOpcode::ColMatElement: {
          Constant *EltIdxs = cast<Constant>(Idx);
          for (unsigned I = 0; I < ResultSize; I++) {
            IdxList[I] = EltIdxs->getAggregateElement(I);
          }
        } break;
        default:
          DXASSERT(0, "invalid operation on const buffer");
          break;
        }

        if (ResultType->isVectorTy()) {
          for (unsigned I = 0; I < ResultSize; I++) {
            Value *EltData = Builder.CreateExtractElement(MatLd, IdxList[I]);
            LdData = Builder.CreateInsertElement(LdData, EltData, I);
          }
        } else {
          Value *EltData = Builder.CreateExtractElement(MatLd, IdxList[0]);
          LdData = EltData;
        }
      } else {
        // Must be matSub here.
        Value *Idx = CI->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx);

        if (ColMajor) {
          // idx is c * row + r.
          // For first col, c is 0, so idx is r.
          Value *One = Builder.getInt32(1);
          // row.x = c[0].[idx]
          // row.y = c[1].[idx]
          // row.z = c[2].[idx]
          // row.w = c[3].[idx]
          Value *Elts[4];
          ArrayType *AT = ArrayType::get(EltTy, MatTy.getNumColumns());

          IRBuilder<> AllocaBuilder(User->getParent()
                                        ->getParent()
                                        ->getEntryBlock()
                                        .getFirstInsertionPt());

          Value *TempArray = AllocaBuilder.CreateAlloca(AT);
          Value *Zero = AllocaBuilder.getInt32(0);
          Value *CbufIdx = LegacyIdx;
          for (unsigned int C = 0; C < MatTy.getNumColumns(); C++) {
            Value *ColVal = generateCbLoadLegacy(
                Handle, CbufIdx, /*channelOffset*/ 0, EltTy, MatTy.getNumRows(),
                HlslOp, Builder);
            // Convert ColVal to array for indexing.
            for (unsigned int R = 0; R < MatTy.getNumRows(); R++) {
              Value *Elt =
                  Builder.CreateExtractElement(ColVal, Builder.getInt32(R));
              Value *Ptr = Builder.CreateInBoundsGEP(
                  TempArray, {Zero, Builder.getInt32(R)});
              Builder.CreateStore(Elt, Ptr);
            }

            Value *Ptr = Builder.CreateInBoundsGEP(TempArray, {Zero, Idx});
            Elts[C] = Builder.CreateLoad(Ptr);
            // Update cbufIdx.
            CbufIdx = Builder.CreateAdd(CbufIdx, One);
          }
          if (ResultType->isVectorTy()) {
            for (unsigned int C = 0; C < MatTy.getNumColumns(); C++) {
              LdData = Builder.CreateInsertElement(LdData, Elts[C], C);
            }
          } else {
            LdData = Elts[0];
          }
        } else {
          // idx is r * col + c;
          // r = idx / col;
          Value *CCol = ConstantInt::get(Idx->getType(), MatTy.getNumColumns());
          Idx = Builder.CreateUDiv(Idx, CCol);
          Idx = Builder.CreateAdd(Idx, LegacyIdx);
          // Just return a row; 'col' is the number of columns in the row.
          LdData = generateCbLoadLegacy(Handle, Idx, /*channelOffset*/ 0, EltTy,
                                        MatTy.getNumColumns(), HlslOp, Builder);
        }
        if (!ResultType->isVectorTy()) {
          LdData = Builder.CreateExtractElement(LdData, Builder.getInt32(0));
        }
      }

      for (auto U = CI->user_begin(); U != CI->user_end();) {
        Value *SubsUser = *(U++);
        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(SubsUser)) {
          Value *SubData = generateVecEltFromGep(LdData, GEP, Builder,
                                                 /*bInsertLdNextToGEP*/ true);
          for (auto GepU = GEP->user_begin(); GepU != GEP->user_end();) {
            Value *GepUser = *(GepU++);
            // Must be load here;
            LoadInst *LdUser = cast<LoadInst>(GepUser);
            LdUser->replaceAllUsesWith(SubData);
            LdUser->eraseFromParent();
          }
          GEP->eraseFromParent();
        } else {
          // Must be load here.
          LoadInst *LdUser = cast<LoadInst>(SubsUser);
          LdUser->replaceAllUsesWith(LdData);
          LdUser->eraseFromParent();
        }
      }

      CI->eraseFromParent();
    } else if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(User)) {
      if (II->getIntrinsicID() == Intrinsic::lifetime_start ||
          II->getIntrinsicID() == Intrinsic::lifetime_end) {
        DXASSERT(II->use_empty(), "lifetime intrinsic can't have uses");
        II->eraseFromParent();
      } else {
        DXASSERT(0, "not implemented yet");
      }
    } else {
      DXASSERT(0, "not implemented yet");
    }
  } else if (LoadInst *LdInst = dyn_cast<LoadInst>(User)) {
    Type *Ty = LdInst->getType();
    Type *EltTy = Ty->getScalarType();
    // Resource inside cbuffer is lowered after GenerateDxilOperations.
    if (dxilutil::IsHLSLObjectType(Ty)) {
      CallInst *CI = cast<CallInst>(Handle);
      // CI should be annotate handle.
      // Need createHandle here.
      if (GetHLOpcodeGroup(CI->getCalledFunction()) ==
          HLOpcodeGroup::HLAnnotateHandle)
        CI = cast<CallInst>(CI->getArgOperand(HLOperandIndex::kHandleOpIdx));

      GlobalVariable *CbGV = cast<GlobalVariable>(
          CI->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx));
      translateResourceInCb(LdInst, PObjHelper, CbGV);
      return;
    }
    DXASSERT(!Ty->isAggregateType(), "should be flat in previous pass");

    Value *NewLd = nullptr;

    if (Ty->isVectorTy())
      NewLd = generateCbLoadLegacy(Handle, LegacyIdx, ChannelOffset, EltTy,
                                   Ty->getVectorNumElements(), HlslOp, Builder);
    else
      NewLd = generateCbLoadLegacy(Handle, LegacyIdx, ChannelOffset, EltTy,
                                   HlslOp, Builder);

    LdInst->replaceAllUsesWith(NewLd);
    dxilutil::TryScatterDebugValueToVectorElements(NewLd);
    LdInst->eraseFromParent();
  } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(User)) {
    for (auto It = BCI->user_begin(); It != BCI->user_end();) {
      Instruction *I = cast<Instruction>(*It++);
      translateCbAddressUserLegacy(I, Handle, LegacyIdx, ChannelOffset, HlslOp,
                                   PrevFieldAnnotation, DxilTypeSys, DL,
                                   PObjHelper);
    }
    BCI->eraseFromParent();
  } else {
    // Must be GEP here
    GetElementPtrInst *GEP = cast<GetElementPtrInst>(User);
    translateCbGepLegacy(GEP, Handle, LegacyIdx, ChannelOffset, HlslOp, Builder,
                         PrevFieldAnnotation, DL, DxilTypeSys, PObjHelper);
    GEP->eraseFromParent();
  }
}

void translateCbGepLegacy(GetElementPtrInst *GEP, Value *Handle,
                          Value *LegacyIndex, unsigned Channel,
                          hlsl::OP *HlslOp, IRBuilder<> &Builder,
                          DxilFieldAnnotation *PrevFieldAnnotation,
                          const DataLayout &DL, DxilTypeSystem &DxilTypeSys,
                          HLObjectOperationLowerHelper *PObjHelper) {
  SmallVector<Value *, 8> Indices(GEP->idx_begin(), GEP->idx_end());

  // update offset
  DxilFieldAnnotation *FieldAnnotation = PrevFieldAnnotation;

  gep_type_iterator GEPIt = gep_type_begin(GEP), E = gep_type_end(GEP);

  for (; GEPIt != E; GEPIt++) {
    Value *Idx = GEPIt.getOperand();
    unsigned ImmIdx = 0;
    bool BImmIdx = false;
    if (Constant *ConstIdx = dyn_cast<Constant>(Idx)) {
      ImmIdx = ConstIdx->getUniqueInteger().getLimitedValue();
      BImmIdx = true;
    }

    if (GEPIt->isPointerTy()) {
      Type *EltTy = GEPIt->getPointerElementType();
      unsigned Size = 0;
      if (StructType *ST = dyn_cast<StructType>(EltTy)) {
        DxilStructAnnotation *Annotation = DxilTypeSys.GetStructAnnotation(ST);
        Size = Annotation->GetCBufferSize();
      } else {
        DXASSERT(FieldAnnotation, "must be a field");
        if (ArrayType *AT = dyn_cast<ArrayType>(EltTy)) {
          unsigned EltSize = dxilutil::GetLegacyCBufferFieldElementSize(
              *FieldAnnotation, EltTy, DxilTypeSys);

          // Decide the nested array size.
          unsigned NestedArraySize = 1;

          Type *EltTy = AT->getArrayElementType();
          // support multi level of array
          while (EltTy->isArrayTy()) {
            ArrayType *EltAT = cast<ArrayType>(EltTy);
            NestedArraySize *= EltAT->getNumElements();
            EltTy = EltAT->getElementType();
          }
          // Align to 4 * 4 bytes.
          unsigned AlignedSize = (EltSize + 15) & 0xfffffff0;
          Size = NestedArraySize * AlignedSize;
        } else {
          Size = DL.getTypeAllocSize(EltTy);
        }
      }
      // Skip 0 idx.
      if (BImmIdx && ImmIdx == 0)
        continue;
      // Align to 4 * 4 bytes.
      Size = (Size + 15) & 0xfffffff0;

      // Take this as array idxing.
      if (BImmIdx) {
        unsigned TempOffset = Size * ImmIdx;
        unsigned IdxInc = TempOffset >> 4;
        LegacyIndex =
            Builder.CreateAdd(LegacyIndex, HlslOp->GetU32Const(IdxInc));
      } else {
        Value *IdxInc = Builder.CreateMul(Idx, HlslOp->GetU32Const(Size >> 4));
        LegacyIndex = Builder.CreateAdd(LegacyIndex, IdxInc);
      }

      // Array always start from x channel.
      Channel = 0;
    } else if (GEPIt->isStructTy()) {
      StructType *ST = cast<StructType>(*GEPIt);
      DxilStructAnnotation *Annotation = DxilTypeSys.GetStructAnnotation(ST);
      FieldAnnotation = &Annotation->GetFieldAnnotation(ImmIdx);

      unsigned IdxInc = 0;
      unsigned StructOffset = 0;
      if (FieldAnnotation->GetCompType().Is16Bit() &&
          !HlslOp->UseMinPrecision()) {
        StructOffset = FieldAnnotation->GetCBufferOffset() >> 1;
        Channel += StructOffset;
        IdxInc = Channel >> 3;
        Channel = Channel & 0x7;
      } else {
        StructOffset = FieldAnnotation->GetCBufferOffset() >> 2;
        Channel += StructOffset;
        IdxInc = Channel >> 2;
        Channel = Channel & 0x3;
      }
      if (IdxInc)
        LegacyIndex =
            Builder.CreateAdd(LegacyIndex, HlslOp->GetU32Const(IdxInc));
    } else if (GEPIt->isArrayTy()) {
      DXASSERT(FieldAnnotation != nullptr, "must a field");
      unsigned EltSize = dxilutil::GetLegacyCBufferFieldElementSize(
          *FieldAnnotation, *GEPIt, DxilTypeSys);
      // Decide the nested array size.
      unsigned NestedArraySize = 1;

      Type *EltTy = GEPIt->getArrayElementType();
      // support multi level of array
      while (EltTy->isArrayTy()) {
        ArrayType *EltAT = cast<ArrayType>(EltTy);
        NestedArraySize *= EltAT->getNumElements();
        EltTy = EltAT->getElementType();
      }
      // Align to 4 * 4 bytes.
      unsigned AlignedSize = (EltSize + 15) & 0xfffffff0;
      unsigned Size = NestedArraySize * AlignedSize;
      if (BImmIdx) {
        unsigned TempOffset = Size * ImmIdx;
        unsigned IdxInc = TempOffset >> 4;
        LegacyIndex =
            Builder.CreateAdd(LegacyIndex, HlslOp->GetU32Const(IdxInc));
      } else {
        Value *IdxInc = Builder.CreateMul(Idx, HlslOp->GetU32Const(Size >> 4));
        LegacyIndex = Builder.CreateAdd(LegacyIndex, IdxInc);
      }

      // Array always start from x channel.
      Channel = 0;
    } else if (GEPIt->isVectorTy()) {
      // Indexing on vector.
      if (BImmIdx) {
        if (ImmIdx < GEPIt->getVectorNumElements()) {
          const unsigned VectorElmSize =
              DL.getTypeAllocSize(GEPIt->getVectorElementType());
          const bool BIs16bitType = VectorElmSize == 2;
          const unsigned TempOffset = VectorElmSize * ImmIdx;
          const unsigned NumChannelsPerRow = BIs16bitType ? 8 : 4;
          const unsigned ChannelInc =
              BIs16bitType ? TempOffset >> 1 : TempOffset >> 2;

          DXASSERT((Channel + ChannelInc) < NumChannelsPerRow,
                   "vector should not cross cb register");
          Channel += ChannelInc;
          if (Channel == NumChannelsPerRow) {
            // Get to another row.
            // Update index and channel.
            Channel = 0;
            LegacyIndex = Builder.CreateAdd(LegacyIndex, Builder.getInt32(1));
          }
        } else {
          StringRef ResName = "(unknown)";
          if (DxilResourceBase *Res =
                  PObjHelper->findCBufferResourceFromHandle(Handle)) {
            ResName = Res->GetGlobalName();
          }
          LegacyIndex = hlsl::CreatePoisonValue(
              LegacyIndex->getType(),
              Twine("Out of bounds index (") + Twine(ImmIdx) +
                  Twine(") in CBuffer '") + Twine(ResName) + ("'"),
              GEP->getDebugLoc(), GEP);
          Channel = 0;
        }
      } else {
        Type *EltTy = GEPIt->getVectorElementType();
        unsigned VecSize = GEPIt->getVectorNumElements();

        // Load the whole register.
        Value *NewLd =
            generateCbLoadLegacy(Handle, LegacyIndex,
                                 /*channelOffset*/ Channel, EltTy,
                                 /*vecSize*/ VecSize, HlslOp, Builder);
        // Copy to array.
        IRBuilder<> AllocaBuilder(GEP->getParent()
                                      ->getParent()
                                      ->getEntryBlock()
                                      .getFirstInsertionPt());
        Value *TempArray =
            AllocaBuilder.CreateAlloca(ArrayType::get(EltTy, VecSize));
        Value *ZeroIdx = HlslOp->GetU32Const(0);
        for (unsigned I = 0; I < VecSize; I++) {
          Value *Elt = Builder.CreateExtractElement(NewLd, I);
          Value *EltGEP = Builder.CreateInBoundsGEP(
              TempArray, {ZeroIdx, HlslOp->GetU32Const(I)});
          Builder.CreateStore(Elt, EltGEP);
        }
        // Make sure this is the end of GEP.
        gep_type_iterator Temp = GEPIt;
        Temp++;
        DXASSERT(Temp == E, "scalar type must be the last");

        // Replace the GEP with array GEP.
        Value *ArrayGEP = Builder.CreateInBoundsGEP(TempArray, {ZeroIdx, Idx});
        GEP->replaceAllUsesWith(ArrayGEP);
        return;
      }
    } else {
      gep_type_iterator Temp = GEPIt;
      Temp++;
      DXASSERT(Temp == E, "scalar type must be the last");
    }
  }

  for (auto U = GEP->user_begin(); U != GEP->user_end();) {
    Instruction *User = cast<Instruction>(*(U++));

    translateCbAddressUserLegacy(User, Handle, LegacyIndex, Channel, HlslOp,
                                 FieldAnnotation, DxilTypeSys, DL, PObjHelper);
  }
}

void translateCbOperationsLegacy(Value *Handle, Value *Ptr, OP *HlslOp,
                                 DxilTypeSystem &DxilTypeSys,
                                 const DataLayout &DL,
                                 HLObjectOperationLowerHelper *PObjHelper) {
  auto User = Ptr->user_begin();
  auto UserE = Ptr->user_end();
  Value *ZeroIdx = HlslOp->GetU32Const(0);
  for (; User != UserE;) {
    // Must be Instruction.
    Instruction *I = cast<Instruction>(*(User++));
    translateCbAddressUserLegacy(
        I, Handle, ZeroIdx, /*channelOffset*/ 0, HlslOp,
        /*prevFieldAnnotation*/ nullptr, DxilTypeSys, DL, PObjHelper);
  }
}

} // namespace

// Structured buffer.
namespace {

Value *generateRawBufLd(Value *Handle, Value *BufIdx, Value *Offset,
                        Value *Status, Type *EltTy,
                        MutableArrayRef<Value *> ResultElts, hlsl::OP *OP,
                        IRBuilder<> &Builder, unsigned NumComponents,
                        Constant *Alignment) {
  OP::OpCode Opcode = OP::OpCode::RawBufferLoad;

  DXASSERT(ResultElts.size() <= 4,
           "buffer load cannot load more than 4 values");

  if (BufIdx == nullptr) {
    // This is actually a byte address buffer load with a struct template type.
    // The call takes only one coordinates for the offset.
    BufIdx = Offset;
    Offset = UndefValue::get(Offset->getType());
  }

  Function *DxilF = OP->GetOpFunc(Opcode, EltTy);
  Constant *Mask = getRawBufferMaskForETy(EltTy, NumComponents, OP);
  Value *Args[] = {OP->GetU32Const((unsigned)Opcode),
                   Handle,
                   BufIdx,
                   Offset,
                   Mask,
                   Alignment};
  Value *Ld = Builder.CreateCall(DxilF, Args, OP::GetOpCodeName(Opcode));

  for (unsigned I = 0; I < ResultElts.size(); I++) {
    ResultElts[I] = Builder.CreateExtractValue(Ld, I);
  }

  // status
  updateStatus(Ld, Status, Builder, OP);
  return Ld;
}

void generateStructBufSt(Value *Handle, Value *BufIdx, Value *Offset,
                         Type *EltTy, hlsl::OP *OP, IRBuilder<> &Builder,
                         ArrayRef<Value *> Vals, uint8_t Mask,
                         Constant *Alignment) {
  OP::OpCode Opcode = OP::OpCode::RawBufferStore;
  DXASSERT(Vals.size() == 4, "buffer store need 4 values");

  Value *Args[] = {OP->GetU32Const((unsigned)Opcode),
                   Handle,
                   BufIdx,
                   Offset,
                   Vals[0],
                   Vals[1],
                   Vals[2],
                   Vals[3],
                   OP->GetU8Const(Mask),
                   Alignment};
  Function *DxilF = OP->GetOpFunc(Opcode, EltTy);
  Builder.CreateCall(DxilF, Args);
}

Value *translateStructBufMatLd(CallInst *CI, IRBuilder<> &Builder,
                               Value *Handle, HLResource::Kind RK, hlsl::OP *OP,
                               Value *Status, Value *BufIdx, Value *BaseOffset,
                               const DataLayout &DL) {

  ResLoadHelper Helper(CI, RK, Handle, BufIdx, BaseOffset, Status);
#ifndef NDEBUG
  Value *Ptr = CI->getArgOperand(HLOperandIndex::kMatLoadPtrOpIdx);
  Type *MatType = Ptr->getType()->getPointerElementType();
  HLMatrixType MatTy = HLMatrixType::cast(MatType);
  DXASSERT(MatTy.getLoweredVectorType(false /*MemRepr*/) ==
               Helper.RetVal->getType(),
           "helper type should match vectorized matrix");
#endif
  return translateBufLoad(Helper, RK, Builder, OP, DL);
}

void translateStructBufMatSt(Type *MatType, IRBuilder<> &Builder, Value *Handle,
                             hlsl::OP *OP, Value *BufIdx, Value *BaseOffset,
                             Value *Val, const DataLayout &DL) {
  [[maybe_unused]] HLMatrixType MatTy = HLMatrixType::cast(MatType);
  DXASSERT(MatTy.getLoweredVectorType(false /*MemRepr*/) == Val->getType(),
           "helper type should match vectorized matrix");
  translateStore(DxilResource::Kind::StructuredBuffer, Handle, Val, BufIdx,
                 BaseOffset, Builder, OP);
}

void translateStructBufMatLdSt(CallInst *CI, Value *Handle, HLResource::Kind RK,
                               hlsl::OP *OP, Value *Status, Value *BufIdx,
                               Value *BaseOffset, const DataLayout &DL) {
  IRBuilder<> Builder(CI);
  HLOpcodeGroup Group = hlsl::GetHLOpcodeGroupByName(CI->getCalledFunction());
  unsigned Opcode = GetHLOpcode(CI);
  DXASSERT_LOCALVAR(group, Group == HLOpcodeGroup::HLMatLoadStore,
                    "only translate matrix loadStore here.");
  HLMatLoadStoreOpcode MatOp = static_cast<HLMatLoadStoreOpcode>(Opcode);
  // Due to the current way the initial codegen generates matrix
  // orientation casts, the in-register vector matrix has already been
  // reordered based on the destination's row or column-major packing
  // orientation.
  switch (MatOp) {
  case HLMatLoadStoreOpcode::RowMatLoad:
  case HLMatLoadStoreOpcode::ColMatLoad:
    translateStructBufMatLd(CI, Builder, Handle, RK, OP, Status, BufIdx,
                            BaseOffset, DL);
    break;
  case HLMatLoadStoreOpcode::RowMatStore:
  case HLMatLoadStoreOpcode::ColMatStore: {
    Value *Ptr = CI->getArgOperand(HLOperandIndex::kMatStoreDstPtrOpIdx);
    Value *Val = CI->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);
    translateStructBufMatSt(Ptr->getType()->getPointerElementType(), Builder,
                            Handle, OP, BufIdx, BaseOffset, Val, DL);
  } break;
  }

  CI->eraseFromParent();
}

void translateStructBufSubscriptUser(Instruction *User, Value *Handle,
                                     HLResource::Kind ResKind, Value *BufIdx,
                                     Value *BaseOffset, Value *Status,
                                     hlsl::OP *OP, const DataLayout &DL);

// For case like mat[i][j].
// IdxList is [i][0], [i][1], [i][2],[i][3].
// Idx is j.
// return [i][j] not mat[i][j] because resource ptr and temp ptr need different
// code gen.
static Value *lowerGepOnMatIndexListToIndex(llvm::GetElementPtrInst *GEP,
                                            ArrayRef<Value *> IdxList) {
  IRBuilder<> Builder(GEP);
  Value *Zero = Builder.getInt32(0);
  DXASSERT(GEP->getNumIndices() == 2, "must have 2 level");
  Value *BaseIdx = (GEP->idx_begin())->get();
  DXASSERT_LOCALVAR(baseIdx, BaseIdx == Zero, "base index must be 0");
  Value *Idx = (GEP->idx_begin() + 1)->get();

  if (ConstantInt *ImmIdx = dyn_cast<ConstantInt>(Idx)) {
    return IdxList[ImmIdx->getSExtValue()];
  }

  IRBuilder<> AllocaBuilder(
      GEP->getParent()->getParent()->getEntryBlock().getFirstInsertionPt());
  unsigned Size = IdxList.size();
  // Store idxList to temp array.
  ArrayType *AT = ArrayType::get(IdxList[0]->getType(), Size);
  Value *TempArray = AllocaBuilder.CreateAlloca(AT);

  for (unsigned I = 0; I < Size; I++) {
    Value *EltPtr = Builder.CreateGEP(TempArray, {Zero, Builder.getInt32(I)});
    Builder.CreateStore(IdxList[I], EltPtr);
  }
  // Load the idx.
  Value *GEPOffset = Builder.CreateGEP(TempArray, {Zero, Idx});
  return Builder.CreateLoad(GEPOffset);
}

// subscript operator for matrix of struct element.
void translateStructBufMatSubscript(CallInst *CI, Value *Handle,
                                    HLResource::Kind ResKind, Value *BufIdx,
                                    Value *BaseOffset, Value *Status,
                                    hlsl::OP *HlslOp, const DataLayout &DL) {
  unsigned Opcode = GetHLOpcode(CI);
  IRBuilder<> SubBuilder(CI);
  HLSubscriptOpcode SubOp = static_cast<HLSubscriptOpcode>(Opcode);
  Value *BasePtr = CI->getArgOperand(HLOperandIndex::kMatSubscriptMatOpIdx);
  HLMatrixType MatTy =
      HLMatrixType::cast(BasePtr->getType()->getPointerElementType());
  Type *EltTy = MatTy.getElementTypeForReg();
  Constant *Alignment = HlslOp->GetI32Const(DL.getTypeAllocSize(EltTy));

  Value *EltByteSize = ConstantInt::get(
      BaseOffset->getType(), getEltTypeByteSizeForConstBuf(EltTy, DL));

  Value *Idx = CI->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx);

  Type *ResultType = CI->getType()->getPointerElementType();
  unsigned ResultSize = 1;
  if (ResultType->isVectorTy())
    ResultSize = ResultType->getVectorNumElements();
  DXASSERT(ResultSize <= 16, "up to 4x4 elements in vector or matrix");
  assert(ResultSize <= 16);
  std::vector<Value *> IdxList(ResultSize);

  switch (SubOp) {
  case HLSubscriptOpcode::ColMatSubscript:
  case HLSubscriptOpcode::RowMatSubscript: {
    for (unsigned I = 0; I < ResultSize; I++) {
      Value *Offset =
          CI->getArgOperand(HLOperandIndex::kMatSubscriptSubOpIdx + I);
      Offset = SubBuilder.CreateMul(Offset, EltByteSize);
      IdxList[I] = SubBuilder.CreateAdd(BaseOffset, Offset);
    }
  } break;
  case HLSubscriptOpcode::RowMatElement:
  case HLSubscriptOpcode::ColMatElement: {
    Constant *EltIdxs = cast<Constant>(Idx);
    for (unsigned I = 0; I < ResultSize; I++) {
      Value *Offset =
          SubBuilder.CreateMul(EltIdxs->getAggregateElement(I), EltByteSize);
      IdxList[I] = SubBuilder.CreateAdd(BaseOffset, Offset);
    }
  } break;
  default:
    DXASSERT(0, "invalid operation on const buffer");
    break;
  }

  Value *UndefElt = UndefValue::get(EltTy);

  for (auto U = CI->user_begin(); U != CI->user_end();) {
    Value *SubsUser = *(U++);
    if (ResultSize == 1) {
      translateStructBufSubscriptUser(cast<Instruction>(SubsUser), Handle,
                                      ResKind, BufIdx, IdxList[0], Status,
                                      HlslOp, DL);
      continue;
    }
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(SubsUser)) {
      Value *GEPOffset = lowerGepOnMatIndexListToIndex(GEP, IdxList);

      for (auto GepU = GEP->user_begin(); GepU != GEP->user_end();) {
        Instruction *GepUserInst = cast<Instruction>(*(GepU++));
        translateStructBufSubscriptUser(GepUserInst, Handle, ResKind, BufIdx,
                                        GEPOffset, Status, HlslOp, DL);
      }

      GEP->eraseFromParent();
    } else if (StoreInst *StUser = dyn_cast<StoreInst>(SubsUser)) {
      // Store elements of matrix in a struct. Needs to be done one scalar at a
      // time even for vectors in the case that matrix orientation spreads the
      // indexed scalars throughout the matrix vector.
      IRBuilder<> StBuilder(StUser);
      Value *Val = StUser->getValueOperand();
      if (Val->getType()->isVectorTy()) {
        for (unsigned I = 0; I < ResultSize; I++) {
          Value *EltVal = StBuilder.CreateExtractElement(Val, I);
          uint8_t Mask = DXIL::kCompMask_X;
          generateStructBufSt(Handle, BufIdx, IdxList[I], EltTy, HlslOp,
                              StBuilder, {EltVal, UndefElt, UndefElt, UndefElt},
                              Mask, Alignment);
        }
      } else {
        uint8_t Mask = DXIL::kCompMask_X;
        generateStructBufSt(Handle, BufIdx, IdxList[0], EltTy, HlslOp,
                            StBuilder, {Val, UndefElt, UndefElt, UndefElt},
                            Mask, Alignment);
      }

      StUser->eraseFromParent();
    } else {
      // Must be load here.
      LoadInst *LdUser = cast<LoadInst>(SubsUser);
      IRBuilder<> LdBuilder(LdUser);
      Value *LdData = UndefValue::get(ResultType);
      // Load elements of matrix in a struct. Needs to be done one scalar at a
      // time even for vectors in the case that matrix orientation spreads the
      // indexed scalars throughout the matrix vector.
      if (ResultType->isVectorTy()) {
        for (unsigned I = 0; I < ResultSize; I++) {
          Value *ResultElt;
          // TODO: This can be inefficient for row major matrix load
          generateRawBufLd(Handle, BufIdx, IdxList[I],
                           /*status*/ nullptr, EltTy, ResultElt, HlslOp,
                           LdBuilder, 1, Alignment);
          LdData = LdBuilder.CreateInsertElement(LdData, ResultElt, I);
        }
      } else {
        generateRawBufLd(Handle, BufIdx, IdxList[0], /*status*/ nullptr, EltTy,
                         LdData, HlslOp, LdBuilder, 4, Alignment);
      }
      LdUser->replaceAllUsesWith(LdData);
      LdUser->eraseFromParent();
    }
  }

  CI->eraseFromParent();
}

void translateStructBufSubscriptUser(Instruction *User, Value *Handle,
                                     HLResource::Kind ResKind, Value *BufIdx,
                                     Value *BaseOffset, Value *Status,
                                     hlsl::OP *OP, const DataLayout &DL) {
  IRBuilder<> Builder(User);
  if (CallInst *UserCall = dyn_cast<CallInst>(User)) {
    HLOpcodeGroup Group = // user call?
        hlsl::GetHLOpcodeGroupByName(UserCall->getCalledFunction());
    unsigned Opcode = GetHLOpcode(UserCall);
    // For case element type of structure buffer is not structure type.
    if (BaseOffset == nullptr)
      BaseOffset = OP->GetU32Const(0);
    if (Group == HLOpcodeGroup::HLIntrinsic) {
      IntrinsicOp IOP = static_cast<IntrinsicOp>(Opcode);
      switch (IOP) {
      case IntrinsicOp::MOP_Load: {
        if (UserCall->getType()->isPointerTy()) {
          // Struct will return pointers which like []

        } else {
          // Use builtin types on structuredBuffer.
        }
        DXASSERT(0, "not implement yet");
      } break;
      case IntrinsicOp::IOP_InterlockedAdd: {
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicBinOp, Handle, BufIdx,
                            BaseOffset);
        translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::Add,
                                       Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedAnd: {
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicBinOp, Handle, BufIdx,
                            BaseOffset);
        translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::And,
                                       Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedExchange: {
        Type *OpType = nullptr;
        PointerType *PtrType = dyn_cast<PointerType>(
            UserCall->getArgOperand(HLOperandIndex::kInterlockedDestOpIndex)
                ->getType());
        if (PtrType && PtrType->getElementType()->isFloatTy())
          OpType = Type::getInt32Ty(UserCall->getContext());
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicBinOp, Handle, BufIdx,
                            BaseOffset, OpType);
        translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::Exchange,
                                       Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedMax: {
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicBinOp, Handle, BufIdx,
                            BaseOffset);
        translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::IMax,
                                       Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedMin: {
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicBinOp, Handle, BufIdx,
                            BaseOffset);
        translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::IMin,
                                       Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedUMax: {
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicBinOp, Handle, BufIdx,
                            BaseOffset);
        translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::UMax,
                                       Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedUMin: {
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicBinOp, Handle, BufIdx,
                            BaseOffset);
        translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::UMin,
                                       Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedOr: {
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicBinOp, Handle, BufIdx,
                            BaseOffset);
        translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::Or,
                                       Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedXor: {
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicBinOp, Handle, BufIdx,
                            BaseOffset);
        translateAtomicBinaryOperation(Helper, DXIL::AtomicBinOpCode::Xor,
                                       Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedCompareStore:
      case IntrinsicOp::IOP_InterlockedCompareExchange: {
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicCompareExchange,
                            Handle, BufIdx, BaseOffset);
        translateAtomicCmpXChg(Helper, Builder, OP);
      } break;
      case IntrinsicOp::IOP_InterlockedCompareStoreFloatBitwise:
      case IntrinsicOp::IOP_InterlockedCompareExchangeFloatBitwise: {
        Type *I32Ty = Type::getInt32Ty(UserCall->getContext());
        AtomicHelper Helper(UserCall, DXIL::OpCode::AtomicCompareExchange,
                            Handle, BufIdx, BaseOffset, I32Ty);
        translateAtomicCmpXChg(Helper, Builder, OP);
      } break;
      default:
        DXASSERT(0, "invalid opcode");
        break;
      }
      UserCall->eraseFromParent();
    } else if (Group == HLOpcodeGroup::HLMatLoadStore)
      // Load/Store matrix within a struct
      translateStructBufMatLdSt(UserCall, Handle, ResKind, OP, Status, BufIdx,
                                BaseOffset, DL);
    else if (Group == HLOpcodeGroup::HLSubscript) {
      // Subscript of matrix within a struct
      translateStructBufMatSubscript(UserCall, Handle, ResKind, BufIdx,
                                     BaseOffset, Status, OP, DL);
    }
  } else if (LoadInst *LdInst = dyn_cast<LoadInst>(User)) {
    // Load of scalar/vector within a struct or structured raw load.
    ResLoadHelper Helper(LdInst, ResKind, Handle, BufIdx, BaseOffset, Status);
    translateBufLoad(Helper, ResKind, Builder, OP, DL);

    LdInst->eraseFromParent();
  } else if (StoreInst *StInst = dyn_cast<StoreInst>(User)) {
    // Store of scalar/vector within a struct or structured raw store.
    Value *Val = StInst->getValueOperand();
    translateStore(DxilResource::Kind::StructuredBuffer, Handle, Val, BufIdx,
                   BaseOffset, Builder, OP);
    StInst->eraseFromParent();
  } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(User)) {
    // Recurse users
    for (auto U = BCI->user_begin(); U != BCI->user_end();) {
      Value *BCIUser = *(U++);
      translateStructBufSubscriptUser(cast<Instruction>(BCIUser), Handle,
                                      ResKind, BufIdx, BaseOffset, Status, OP,
                                      DL);
    }
    BCI->eraseFromParent();
  } else if (PHINode *Phi = dyn_cast<PHINode>(User)) {
    if (Phi->getNumIncomingValues() != 1) {
      dxilutil::EmitErrorOnInstruction(
          Phi, "Phi not supported for buffer subscript");
      return;
    }
    // Since the phi only has a single value we can safely process its
    // users to translate the subscript. These single-value phis are
    // inserted by the lcssa pass.
    for (auto U = Phi->user_begin(); U != Phi->user_end();) {
      Value *PhiUser = *(U++);
      translateStructBufSubscriptUser(cast<Instruction>(PhiUser), Handle,
                                      ResKind, BufIdx, BaseOffset, Status, OP,
                                      DL);
    }
    Phi->eraseFromParent();
  } else {
    // should only used by GEP
    GetElementPtrInst *GEP = cast<GetElementPtrInst>(User);
    Type *Ty = GEP->getType()->getPointerElementType();

    Value *Offset = dxilutil::GEPIdxToOffset(GEP, Builder, OP, DL);
    DXASSERT_LOCALVAR(Ty,
                      Offset->getType() == Type::getInt32Ty(Ty->getContext()),
                      "else bitness is wrong");
    // No offset into element for Raw buffers; byte offset is in bufIdx.
    if (DXIL::IsRawBuffer(ResKind))
      BufIdx = Builder.CreateAdd(Offset, BufIdx);
    else
      BaseOffset = Builder.CreateAdd(Offset, BaseOffset);

    for (auto U = GEP->user_begin(); U != GEP->user_end();) {
      Value *GEPUser = *(U++);

      translateStructBufSubscriptUser(cast<Instruction>(GEPUser), Handle,
                                      ResKind, BufIdx, BaseOffset, Status, OP,
                                      DL);
    }
    // delete the inst
    GEP->eraseFromParent();
  }
}

void translateStructBufSubscript(CallInst *CI, Value *Handle, Value *Status,
                                 hlsl::OP *OP, HLResource::Kind ResKind,
                                 const DataLayout &DL) {
  Value *SubscriptIndex =
      CI->getArgOperand(HLOperandIndex::kSubscriptIndexOpIdx);
  Value *BufIdx = nullptr;
  Value *Offset = nullptr;
  BufIdx = SubscriptIndex;
  if (ResKind == HLResource::Kind::RawBuffer)
    Offset = UndefValue::get(Type::getInt32Ty(CI->getContext()));
  else
    // StructuredBuffer, TypedBuffer, etc.
    Offset = OP->GetU32Const(0);

  for (auto U = CI->user_begin(); U != CI->user_end();) {
    Value *User = *(U++);

    translateStructBufSubscriptUser(cast<Instruction>(User), Handle, ResKind,
                                    BufIdx, Offset, Status, OP, DL);
  }
}
} // namespace

// HLSubscript.
namespace {

Value *translateTypedBufSubscript(CallInst *CI, DXIL::ResourceKind RK,
                                  DXIL::ResourceClass RC, Value *Handle,
                                  LoadInst *LdInst, IRBuilder<> &Builder,
                                  hlsl::OP *HlslOp, const DataLayout &DL) {
  // The arguments to the call instruction are used to determine the access,
  // the return value and type come from the load instruction.
  ResLoadHelper LdHelper(CI, RK, RC, Handle, IntrinsicOp::MOP_Load, LdInst);
  translateBufLoad(LdHelper, RK, Builder, HlslOp, DL);
  // delete the ld
  LdInst->eraseFromParent();
  return LdHelper.RetVal;
}

Value *updateVectorElt(Value *VecVal, Value *EltVal, Value *EltIdx,
                       unsigned VectorSize, Instruction *InsertPt) {
  IRBuilder<> Builder(InsertPt);
  if (ConstantInt *CEltIdx = dyn_cast<ConstantInt>(EltIdx)) {
    VecVal =
        Builder.CreateInsertElement(VecVal, EltVal, CEltIdx->getLimitedValue());
  } else {
    BasicBlock *BB = InsertPt->getParent();
    BasicBlock *EndBB = BB->splitBasicBlock(InsertPt);

    TerminatorInst *TI = BB->getTerminator();
    IRBuilder<> SwitchBuilder(TI);
    LLVMContext &Ctx = InsertPt->getContext();

    SwitchInst *Switch = SwitchBuilder.CreateSwitch(EltIdx, EndBB, VectorSize);
    TI->eraseFromParent();

    Function *F = EndBB->getParent();
    IRBuilder<> EndSwitchBuilder(EndBB->begin());
    Type *Ty = VecVal->getType();
    PHINode *VecPhi = EndSwitchBuilder.CreatePHI(Ty, VectorSize + 1);

    for (unsigned I = 0; I < VectorSize; I++) {
      BasicBlock *CaseBB = BasicBlock::Create(Ctx, "case", F, EndBB);
      Switch->addCase(SwitchBuilder.getInt32(I), CaseBB);
      IRBuilder<> CaseBuilder(CaseBB);

      Value *CaseVal = CaseBuilder.CreateInsertElement(VecVal, EltVal, I);
      VecPhi->addIncoming(CaseVal, CaseBB);
      CaseBuilder.CreateBr(EndBB);
    }
    VecPhi->addIncoming(VecVal, BB);
    VecVal = VecPhi;
  }
  return VecVal;
}

void translateTypedBufferSubscript(CallInst *CI, HLOperationLowerHelper &Helper,
                                   HLObjectOperationLowerHelper *PObjHelper,
                                   bool &Translated) {
  Value *Ptr = CI->getArgOperand(HLOperandIndex::kSubscriptObjectOpIdx);

  hlsl::OP *HlslOp = &Helper.HlslOp;
  // Resource ptr.
  Value *Handle = Ptr;
  DXIL::ResourceClass RC = PObjHelper->getRc(Handle);
  DXIL::ResourceKind RK = PObjHelper->getRk(Handle);

  Type *Ty = CI->getType()->getPointerElementType();

  for (auto It = CI->user_begin(); It != CI->user_end();) {
    User *U = *(It++);
    Instruction *I = cast<Instruction>(U);
    IRBuilder<> Builder(I);
    Value *UndefI = UndefValue::get(Builder.getInt32Ty());
    if (LoadInst *LdInst = dyn_cast<LoadInst>(U)) {
      translateTypedBufSubscript(CI, RK, RC, Handle, LdInst, Builder, HlslOp,
                                 Helper.DL);
    } else if (StoreInst *StInst = dyn_cast<StoreInst>(U)) {
      Value *Val = StInst->getValueOperand();
      translateStore(RK, Handle, Val,
                     CI->getArgOperand(HLOperandIndex::kSubscriptIndexOpIdx),
                     UndefI, Builder, HlslOp);
      // delete the st
      StInst->eraseFromParent();
    } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(U)) {
      // Must be vector type here.
      unsigned VectorSize = Ty->getVectorNumElements();
      DXASSERT_NOMSG(GEP->getNumIndices() == 2);
      Use *GEPIdx = GEP->idx_begin();
      GEPIdx++;
      Value *EltIdx = *GEPIdx;
      for (auto GEPIt = GEP->user_begin(); GEPIt != GEP->user_end();) {
        User *GEPUser = *(GEPIt++);
        if (StoreInst *SI = dyn_cast<StoreInst>(GEPUser)) {
          IRBuilder<> StBuilder(SI);
          // Generate Ld.
          LoadInst *TmpLd = StBuilder.CreateLoad(CI);

          Value *LdVal = translateTypedBufSubscript(
              CI, RK, RC, Handle, TmpLd, StBuilder, HlslOp, Helper.DL);
          // Update vector.
          LdVal = updateVectorElt(LdVal, SI->getValueOperand(), EltIdx,
                                  VectorSize, SI);
          // Generate St.
          // Reset insert point, UpdateVectorElt may move SI to different block.
          StBuilder.SetInsertPoint(SI);
          translateStore(
              RK, Handle, LdVal,
              CI->getArgOperand(HLOperandIndex::kSubscriptIndexOpIdx), UndefI,
              StBuilder, HlslOp);
          SI->eraseFromParent();
          continue;
        }
        if (LoadInst *LI = dyn_cast<LoadInst>(GEPUser)) {
          IRBuilder<> LdBuilder(LI);

          // Generate tmp vector load with vector type & translate it
          LoadInst *TmpLd = LdBuilder.CreateLoad(CI);

          Value *LdVal = translateTypedBufSubscript(
              CI, RK, RC, Handle, TmpLd, LdBuilder, HlslOp, Helper.DL);

          // get the single element
          LdVal = generateVecEltFromGep(LdVal, GEP, LdBuilder,
                                        /*bInsertLdNextToGEP*/ false);

          LI->replaceAllUsesWith(LdVal);
          LI->eraseFromParent();
          continue;
        }
        // Invalid operations.
        Translated = false;
        dxilutil::EmitErrorOnInstruction(GEP,
                                         "Invalid operation on typed buffer.");
        return;
      }
      GEP->eraseFromParent();
    } else {
      CallInst *UserCall = cast<CallInst>(U);
      HLOpcodeGroup Group =
          hlsl::GetHLOpcodeGroupByName(UserCall->getCalledFunction());
      unsigned Opcode = hlsl::GetHLOpcode(UserCall);
      if (Group == HLOpcodeGroup::HLIntrinsic) {
        IntrinsicOp IOP = static_cast<IntrinsicOp>(Opcode);
        if (RC == DXIL::ResourceClass::SRV) {
          // Invalid operations.
          Translated = false;
          dxilutil::EmitErrorOnInstruction(UserCall,
                                           "Invalid operation on SRV.");
          return;
        }
        switch (IOP) {
        case IntrinsicOp::IOP_InterlockedAdd: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedAdd);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicBinOp, Handle,
                                  Helper.Addr, /*offset*/ nullptr);
          translateAtomicBinaryOperation(AtomHelper, DXIL::AtomicBinOpCode::Add,
                                         Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedAnd: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedAnd);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicBinOp, Handle,
                                  Helper.Addr, /*offset*/ nullptr);
          translateAtomicBinaryOperation(AtomHelper, DXIL::AtomicBinOpCode::And,
                                         Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedExchange: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedExchange);
          Type *OpType = nullptr;
          PointerType *PtrType = dyn_cast<PointerType>(
              UserCall->getArgOperand(HLOperandIndex::kInterlockedDestOpIndex)
                  ->getType());
          if (PtrType && PtrType->getElementType()->isFloatTy())
            OpType = Type::getInt32Ty(UserCall->getContext());
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicBinOp, Handle,
                                  Helper.Addr, /*offset*/ nullptr, OpType);
          translateAtomicBinaryOperation(
              AtomHelper, DXIL::AtomicBinOpCode::Exchange, Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedMax: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedMax);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicBinOp, Handle,
                                  Helper.Addr, /*offset*/ nullptr);
          translateAtomicBinaryOperation(
              AtomHelper, DXIL::AtomicBinOpCode::IMax, Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedMin: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedMin);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicBinOp, Handle,
                                  Helper.Addr, /*offset*/ nullptr);
          translateAtomicBinaryOperation(
              AtomHelper, DXIL::AtomicBinOpCode::IMin, Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedUMax: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedUMax);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicBinOp, Handle,
                                  Helper.Addr, /*offset*/ nullptr);
          translateAtomicBinaryOperation(
              AtomHelper, DXIL::AtomicBinOpCode::UMax, Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedUMin: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedUMin);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicBinOp, Handle,
                                  Helper.Addr, /*offset*/ nullptr);
          translateAtomicBinaryOperation(
              AtomHelper, DXIL::AtomicBinOpCode::UMin, Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedOr: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedOr);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicBinOp, Handle,
                                  Helper.Addr, /*offset*/ nullptr);
          translateAtomicBinaryOperation(AtomHelper, DXIL::AtomicBinOpCode::Or,
                                         Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedXor: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedXor);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicBinOp, Handle,
                                  Helper.Addr, /*offset*/ nullptr);
          translateAtomicBinaryOperation(AtomHelper, DXIL::AtomicBinOpCode::Xor,
                                         Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedCompareStore:
        case IntrinsicOp::IOP_InterlockedCompareExchange: {
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedCompareExchange);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicCompareExchange,
                                  Handle, Helper.Addr, /*offset*/ nullptr);
          translateAtomicCmpXChg(AtomHelper, Builder, HlslOp);
        } break;
        case IntrinsicOp::IOP_InterlockedCompareStoreFloatBitwise:
        case IntrinsicOp::IOP_InterlockedCompareExchangeFloatBitwise: {
          Type *I32Ty = Type::getInt32Ty(UserCall->getContext());
          ResLoadHelper Helper(CI, RK, RC, Handle,
                               IntrinsicOp::IOP_InterlockedCompareExchange);
          AtomicHelper AtomHelper(UserCall, DXIL::OpCode::AtomicCompareExchange,
                                  Handle, Helper.Addr, /*offset*/ nullptr,
                                  I32Ty);
          translateAtomicCmpXChg(AtomHelper, Builder, HlslOp);
        } break;
        default:
          DXASSERT(0, "invalid opcode");
          break;
        }
      } else {
        DXASSERT(0, "invalid group");
      }
      UserCall->eraseFromParent();
    }
  }
}
} // namespace

static void translateHlSubscript(CallInst *CI, HLSubscriptOpcode Opcode,
                          HLOperationLowerHelper &Helper,
                          HLObjectOperationLowerHelper *PObjHelper,
                          bool &Translated) {
  if (CI->user_empty()) {
    Translated = true;
    return;
  }
  hlsl::OP *HlslOp = &Helper.HlslOp;

  Value *Ptr = CI->getArgOperand(HLOperandIndex::kSubscriptObjectOpIdx);
  if (Opcode == HLSubscriptOpcode::CBufferSubscript) {
    dxilutil::MergeGepUse(CI);
    // Resource ptr.
    Value *Handle = CI->getArgOperand(HLOperandIndex::kSubscriptObjectOpIdx);
    translateCbOperationsLegacy(Handle, CI, HlslOp, Helper.DxilTypeSys,
                                Helper.DL, PObjHelper);
    Translated = true;
    return;
  }

  if (Opcode == HLSubscriptOpcode::DoubleSubscript) {
    // Resource ptr.
    Value *Handle = Ptr;
    DXIL::ResourceKind RK = PObjHelper->getRk(Handle);
    Value *Coord = CI->getArgOperand(HLOperandIndex::kSubscriptIndexOpIdx);
    Value *MipLevel =
        CI->getArgOperand(HLOperandIndex::kDoubleSubscriptMipLevelOpIdx);

    auto U = CI->user_begin();
    DXASSERT(CI->hasOneUse(), "subscript should only have one use");
    IRBuilder<> Builder(CI);
    if (LoadInst *LdInst = dyn_cast<LoadInst>(*U)) {
      Value *Offset = UndefValue::get(Builder.getInt32Ty());
      ResLoadHelper LdHelper(LdInst, RK, Handle, Coord, Offset,
                             /*status*/ nullptr, MipLevel);
      translateBufLoad(LdHelper, RK, Builder, HlslOp, Helper.DL);
      LdInst->eraseFromParent();
    } else {
      StoreInst *StInst = cast<StoreInst>(*U);
      Value *Val = StInst->getValueOperand();
      Value *UndefI = UndefValue::get(Builder.getInt32Ty());
      translateStore(RK, Handle, Val,
                     CI->getArgOperand(HLOperandIndex::kSubscriptIndexOpIdx),
                     UndefI, Builder, HlslOp, MipLevel);
      StInst->eraseFromParent();
    }
    Translated = true;
    return;
  }

  Type *HandleTy = HlslOp->GetHandleType();
  if (Ptr->getType() == HlslOp->GetNodeRecordHandleType()) {
    DXASSERT(false, "Shouldn't get here, NodeRecord subscripts should have "
                    "been lowered in LowerRecordAccessToGetNodeRecordPtr");
    return;
  }

  if (Ptr->getType() == HandleTy) {
    // Resource ptr.
    Value *Handle = Ptr;
    DXIL::ResourceKind RK = DxilResource::Kind::Invalid;
    Type *ObjTy = nullptr;
    Type *RetTy = nullptr;
    RK = PObjHelper->getRk(Handle);
    if (RK == DxilResource::Kind::Invalid) {
      Translated = false;
      return;
    }
    ObjTy = PObjHelper->getResourceType(Handle);
    RetTy = ObjTy->getStructElementType(0);
    Translated = true;

    if (DXIL::IsStructuredBuffer(RK))
      translateStructBufSubscript(CI, Handle, /*status*/ nullptr, HlslOp, RK,
                                  Helper.DL);
    else
      translateTypedBufferSubscript(CI, Helper, PObjHelper, Translated);

    return;
  }

  Value *BasePtr = CI->getArgOperand(HLOperandIndex::kMatSubscriptMatOpIdx);
  if (isLocalVariablePtr(BasePtr) || isSharedMemPtr(BasePtr)) {
    // Translate matrix into vector of array for share memory or local
    // variable should be done in HLMatrixLowerPass
    DXASSERT_NOMSG(0);
    Translated = true;
    return;
  }

  // Other case should be take care in TranslateStructBufSubscript or
  // TranslateCBOperations.
  Translated = false;
}

static void translateSubscriptOperation(Function *F, HLOperationLowerHelper &Helper,
                                 HLObjectOperationLowerHelper *PObjHelper) {
  for (auto U = F->user_begin(); U != F->user_end();) {
    Value *User = *(U++);
    if (!isa<Instruction>(User))
      continue;
    // must be call inst
    CallInst *CI = cast<CallInst>(User);
    unsigned Opcode = GetHLOpcode(CI);
    bool Translated = true;
    translateHlSubscript(CI, static_cast<HLSubscriptOpcode>(Opcode), Helper,
                         PObjHelper, Translated);
    if (Translated) {
      // delete the call
      DXASSERT(CI->use_empty(),
               "else TranslateHLSubscript didn't replace/erase uses");
      CI->eraseFromParent();
    }
  }
}

// Create BitCast if ptr, otherwise, create alloca of new type, write to bitcast
// of alloca, and return load from alloca If bOrigAllocaTy is true: create
// alloca of old type instead, write to alloca, and return load from bitcast of
// alloca
static Instruction *bitCastValueOrPtr(Value *V, Instruction *Insert, Type *Ty,
                                      bool BOrigAllocaTy = false,
                                      const Twine &Name = "") {
  IRBuilder<> Builder(Insert);
  if (Ty->isPointerTy()) {
    // If pointer, we can bitcast directly
    return cast<Instruction>(Builder.CreateBitCast(V, Ty, Name));
  }

  // If value, we have to alloca, store to bitcast ptr, and load
  IRBuilder<> AllocaBuilder(dxilutil::FindAllocaInsertionPt(Insert));
  Type *AllocaTy = BOrigAllocaTy ? V->getType() : Ty;
  Type *OtherTy = BOrigAllocaTy ? Ty : V->getType();
  Instruction *AllocaInst = AllocaBuilder.CreateAlloca(AllocaTy);
  Instruction *BitCast = cast<Instruction>(
      Builder.CreateBitCast(AllocaInst, OtherTy->getPointerTo()));
  Builder.CreateStore(V, BOrigAllocaTy ? AllocaInst : BitCast);
  return Builder.CreateLoad(BOrigAllocaTy ? BitCast : AllocaInst, Name);
}

static Instruction *createTransposeShuffle(IRBuilder<> &Builder, Value *VecVal,
                                           unsigned ToRows, unsigned ToCols) {
  SmallVector<int, 16> CastMask(ToCols * ToRows);
  unsigned Idx = 0;
  for (unsigned R = 0; R < ToRows; R++)
    for (unsigned C = 0; C < ToCols; C++)
      CastMask[Idx++] = C * ToRows + R;
  return cast<Instruction>(
      Builder.CreateShuffleVector(VecVal, VecVal, CastMask));
}

static void translateHlBuiltinOperation(Function *F, HLOperationLowerHelper &Helper,
                                 hlsl::HLOpcodeGroup Group,
                                 HLObjectOperationLowerHelper *PObjHelper) {
  if (Group == HLOpcodeGroup::HLIntrinsic) {
    // map to dxil operations
    for (auto U = F->user_begin(); U != F->user_end();) {
      Value *User = *(U++);
      if (!isa<Instruction>(User))
        continue;
      // must be call inst
      CallInst *CI = cast<CallInst>(User);

      // Keep the instruction to lower by other function.
      bool Translated = true;

      translateBuiltinIntrinsic(CI, Helper, PObjHelper, Translated);

      if (Translated) {
        // delete the call
        DXASSERT(CI->use_empty(),
                 "else TranslateBuiltinIntrinsic didn't replace/erase uses");
        CI->eraseFromParent();
      }
    }
  } else {
    if (Group == HLOpcodeGroup::HLMatLoadStore) {
      // Both ld/st use arg1 for the pointer.
      Type *PtrTy =
          F->getFunctionType()->getParamType(HLOperandIndex::kMatLoadPtrOpIdx);

      if (PtrTy->getPointerAddressSpace() == DXIL::kTGSMAddrSpace) {
        // Translate matrix into vector of array for shared memory
        // variable should be done in HLMatrixLowerPass.
        if (!F->user_empty())
          F->getContext().emitError("Fail to lower matrix load/store.");
      } else if (PtrTy->getPointerAddressSpace() == DXIL::kDefaultAddrSpace) {
        // Default address space may be function argument in lib target
        if (!F->user_empty()) {
          for (auto U = F->user_begin(); U != F->user_end();) {
            Value *User = *(U++);
            if (!isa<Instruction>(User))
              continue;
            // must be call inst
            CallInst *CI = cast<CallInst>(User);
            IRBuilder<> Builder(CI);
            HLMatLoadStoreOpcode Opcode =
                static_cast<HLMatLoadStoreOpcode>(hlsl::GetHLOpcode(CI));
            switch (Opcode) {
            case HLMatLoadStoreOpcode::ColMatStore:
            case HLMatLoadStoreOpcode::RowMatStore: {
              Value *VecVal =
                  CI->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);
              Value *MatPtr =
                  CI->getArgOperand(HLOperandIndex::kMatStoreDstPtrOpIdx);
              MatPtr = skipAddrSpaceCast(MatPtr);
              unsigned AddrSpace =
                  cast<PointerType>(MatPtr->getType())->getAddressSpace();

              Value *CastPtr = Builder.CreateBitCast(
                  MatPtr, VecVal->getType()->getPointerTo(AddrSpace));
              Builder.CreateStore(VecVal, CastPtr);
              CI->eraseFromParent();
            } break;
            case HLMatLoadStoreOpcode::ColMatLoad:
            case HLMatLoadStoreOpcode::RowMatLoad: {
              Value *MatPtr =
                  CI->getArgOperand(HLOperandIndex::kMatLoadPtrOpIdx);
              MatPtr = skipAddrSpaceCast(MatPtr);
              unsigned AddrSpace =
                  cast<PointerType>(MatPtr->getType())->getAddressSpace();
              Value *CastPtr = Builder.CreateBitCast(
                  MatPtr, CI->getType()->getPointerTo(AddrSpace));
              Value *VecVal = Builder.CreateLoad(CastPtr);
              CI->replaceAllUsesWith(VecVal);
              CI->eraseFromParent();
            } break;
            }
          }
        }
      }
    } else if (Group == HLOpcodeGroup::HLCast) {
      // HLCast may be used on matrix value function argument in lib target
      if (!F->user_empty()) {
        for (auto U = F->user_begin(); U != F->user_end();) {
          Value *User = *(U++);
          if (!isa<Instruction>(User))
            continue;
          // must be call inst
          CallInst *CI = cast<CallInst>(User);
          IRBuilder<> Builder(CI);
          HLCastOpcode Opcode =
              static_cast<HLCastOpcode>(hlsl::GetHLOpcode(CI));
          bool BTranspose = false;
          bool BColDest = false;
          switch (Opcode) {
          case HLCastOpcode::RowMatrixToColMatrix:
            BColDest = true;
            LLVM_FALLTHROUGH;
          case HLCastOpcode::ColMatrixToRowMatrix:
            BTranspose = true;
            LLVM_FALLTHROUGH;
          case HLCastOpcode::ColMatrixToVecCast:
          case HLCastOpcode::RowMatrixToVecCast: {
            Value *MatVal =
                CI->getArgOperand(HLOperandIndex::kInitFirstArgOpIdx);
            Value *VecVal =
                bitCastValueOrPtr(MatVal, CI, CI->getType(),
                                  /*bOrigAllocaTy*/ false, MatVal->getName());
            if (BTranspose) {
              HLMatrixType MatTy = HLMatrixType::cast(MatVal->getType());
              unsigned Row = MatTy.getNumRows();
              unsigned Col = MatTy.getNumColumns();
              if (BColDest)
                std::swap(Row, Col);
              VecVal = createTransposeShuffle(Builder, VecVal, Row, Col);
            }
            CI->replaceAllUsesWith(VecVal);
            CI->eraseFromParent();
          } break;
          }
        }
      }
    } else if (Group == HLOpcodeGroup::HLSubscript) {
      translateSubscriptOperation(F, Helper, PObjHelper);
    }
    // map to math function or llvm ir
  }
}

typedef std::unordered_map<llvm::Instruction *, llvm::Value *> HandleMap;
static void translateHlExtension(Function *F,
                                 HLSLExtensionsCodegenHelper *Helper,
                                 OP &HlslOp,
                                 HLObjectOperationLowerHelper &ObjHelper) {
  // Find all calls to the function F.
  // Store the calls in a vector for now to be replaced the loop below.
  // We use a two step "find then replace" to avoid removing uses while
  // iterating.
  SmallVector<CallInst *, 8> CallsToReplace;
  for (User *U : F->users()) {
    if (CallInst *CI = dyn_cast<CallInst>(U)) {
      CallsToReplace.push_back(CI);
    }
  }

  // Get the lowering strategy to use for this intrinsic.
  llvm::StringRef LowerStrategy = GetHLLowerStrategy(F);
  HLObjectExtensionLowerHelper ExtObjHelper(ObjHelper);
  ExtensionLowering Lower(LowerStrategy, Helper, HlslOp, ExtObjHelper);

  // Replace all calls that were successfully translated.
  for (CallInst *CI : CallsToReplace) {
    Value *Result = Lower.Translate(CI);
    if (Result && Result != CI) {
      CI->replaceAllUsesWith(Result);
      CI->eraseFromParent();
    }
  }
}

namespace hlsl {

void TranslateBuiltinOperations(
    HLModule &HLM, HLSLExtensionsCodegenHelper *ExtCodegenHelper,
    std::unordered_set<Instruction *> &UpdateCounterSet) {
  HLOperationLowerHelper Helper(HLM);

  HLObjectOperationLowerHelper ObjHelper = {HLM, UpdateCounterSet};

  Module *M = HLM.GetModule();

  SmallVector<Function *, 4> NonUniformResourceIndexIntrinsics;

  // generate dxil operation
  for (iplist<Function>::iterator F : M->getFunctionList()) {
    if (F->user_empty())
      continue;
    if (!F->isDeclaration()) {
      continue;
    }
    hlsl::HLOpcodeGroup Group = hlsl::GetHLOpcodeGroup(F);
    if (Group == HLOpcodeGroup::NotHL) {
      // Nothing to do.
      continue;
    }
    if (Group == HLOpcodeGroup::HLExtIntrinsic) {
      translateHlExtension(F, ExtCodegenHelper, Helper.HlslOp, ObjHelper);
      continue;
    }
    if (Group == HLOpcodeGroup::HLIntrinsic) {
      CallInst *CI = cast<CallInst>(*F->user_begin()); // must be call inst
      unsigned Opcode = hlsl::GetHLOpcode(CI);
      if (Opcode == (unsigned)IntrinsicOp::IOP_NonUniformResourceIndex) {
        NonUniformResourceIndexIntrinsics.push_back(F);
        continue;
      }
    }
    translateHlBuiltinOperation(F, Helper, Group, &ObjHelper);
  }

  // Translate last so value placed in NonUniformSet is still valid.
  if (!NonUniformResourceIndexIntrinsics.empty()) {
    for (auto *F : NonUniformResourceIndexIntrinsics) {
      translateHlBuiltinOperation(F, Helper, HLOpcodeGroup::HLIntrinsic,
                                  &ObjHelper);
    }
  }
}

static void emitGetNodeRecordPtrAndUpdateUsers(HLOperationLowerHelper &Helper,
                                        CallInst *CI, Value *ArrayIndex) {
  IRBuilder<> Builder(CI);
  Value *OpArg = nullptr;
  Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
  OpArg = Builder.getInt32((unsigned)DXIL::OpCode::GetNodeRecordPtr);
  StructType *OrigRecordUdt =
      cast<StructType>(cast<PointerType>(CI->getType())->getElementType());
  Type *GetNodeRecordPtrRt = OrigRecordUdt;
  // Translate node record type here
  auto FindIt = Helper.LoweredTypes.find(OrigRecordUdt);
  if (FindIt != Helper.LoweredTypes.end()) {
    GetNodeRecordPtrRt = FindIt->second;
  } else {
    GetNodeRecordPtrRt = GetLoweredUDT(OrigRecordUdt, &Helper.DxilTypeSys);
    if (OrigRecordUdt != GetNodeRecordPtrRt)
      Helper.LoweredTypes[OrigRecordUdt] = GetNodeRecordPtrRt;
  }
  GetNodeRecordPtrRt =
      GetNodeRecordPtrRt->getPointerTo(DXIL::kNodeRecordAddrSpace);
  Function *GetNodeRecordPtr = Helper.HlslOp.GetOpFunc(
      DXIL::OpCode::GetNodeRecordPtr, GetNodeRecordPtrRt);
  Value *Args[] = {OpArg, Handle, ArrayIndex};
  Value *NodeRecordPtr = Builder.CreateCall(GetNodeRecordPtr, Args);
  ReplaceUsesForLoweredUDT(CI, NodeRecordPtr);
}

void LowerRecordAccessToGetNodeRecordPtr(HLModule &HLM) {
  Module *M = HLM.GetModule();
  HLOperationLowerHelper Helper(HLM);
  for (iplist<Function>::iterator F : M->getFunctionList()) {
    if (F->user_empty())
      continue;
    hlsl::HLOpcodeGroup Group = hlsl::GetHLOpcodeGroup(F);
    if (Group == HLOpcodeGroup::HLSubscript) {
      for (auto U = F->user_begin(); U != F->user_end();) {
        Value *User = *(U++);
        if (!isa<Instruction>(User))
          continue;
        // must be call inst
        CallInst *CI = cast<CallInst>(User);
        HLSubscriptOpcode Opcode =
            static_cast<HLSubscriptOpcode>(hlsl::GetHLOpcode(CI));
        if (Opcode != HLSubscriptOpcode::DefaultSubscript)
          continue;

        hlsl::OP *OP = &Helper.HlslOp;
        Value *Handle = CI->getArgOperand(HLOperandIndex::kHandleOpIdx);
        if (Handle->getType() != OP->GetNodeRecordHandleType()) {
          continue;
        }

        Value *Index = CI->getNumArgOperands() > 2
                           ? CI->getArgOperand(2)
                           : ConstantInt::get(Helper.I32Ty, 0);
        emitGetNodeRecordPtrAndUpdateUsers(Helper, CI, Index);
        CI->eraseFromParent();
      }
    }
  }
}
} // namespace hlsl
