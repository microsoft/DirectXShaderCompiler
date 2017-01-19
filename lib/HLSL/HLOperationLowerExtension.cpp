///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLOperationLowerExtension.cpp                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/HLOperationLowerExtension.h"

#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/HLOperationLower.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HlslIntrinsicOp.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_os_ostream.h"

using namespace llvm;
using namespace hlsl;

ExtensionLowering::Strategy ExtensionLowering::GetStrategy(StringRef strategy) {
  if (strategy.size() < 1)
    return Strategy::Unknown;

  switch (strategy[0]) {
    case 'n': return Strategy::NoTranslation;
    case 'r': return Strategy::Replicate;
    case 'p': return Strategy::Pack;
    default: break;
  }
  return Strategy::Unknown;
}

llvm::StringRef ExtensionLowering::GetStrategyName(Strategy strategy) {
  switch (strategy) {
    case Strategy::NoTranslation: return "n";
    case Strategy::Replicate:     return "r";
    case Strategy::Pack:          return "p";
    default: break;
  }
  return "?";
}

ExtensionLowering::ExtensionLowering(Strategy strategy, HLSLExtensionsCodegenHelper *helper) 
  : m_strategy(strategy), m_helper(helper)
  {}

ExtensionLowering::ExtensionLowering(StringRef strategy, HLSLExtensionsCodegenHelper *helper) 
  : ExtensionLowering(GetStrategy(strategy), helper)
  {}

llvm::Value *ExtensionLowering::Translate(llvm::CallInst *CI) {
  switch (m_strategy) {
  case Strategy::NoTranslation: return NoTranslation(CI);
  case Strategy::Replicate:     return Replicate(CI);
  case Strategy::Pack:          return Pack(CI);
  default: break;
  }
  return Unknown(CI);
}

llvm::Value *ExtensionLowering::Unknown(CallInst *CI) {
  assert(false && "unknown translation strategy");
  return nullptr;
}

// Interface to describe how to translate types from HL-dxil to dxil.
class FunctionTypeTranslator {
public:
  virtual Type *TranslateReturnType(CallInst *CI) = 0;
  virtual Type *TranslateArgumentType(Type *OrigArgType) = 0;
};

// Class to create the new function with the translated types for low-level dxil.
class FunctionTranslator {
public:
  template <typename TypeTranslator>
  static Function *GetLoweredFunction(CallInst *CI, ExtensionLowering &lower) {
    TypeTranslator typeTranslator;
    FunctionTranslator translator(typeTranslator, lower);
    return translator.GetLoweredFunction(CI);
  }

private:
  FunctionTypeTranslator &m_typeTranslator;
  ExtensionLowering &m_lower;

  FunctionTranslator(FunctionTypeTranslator &typeTranslator, ExtensionLowering &lower)
    : m_typeTranslator(typeTranslator)
    , m_lower(lower)
  {}

  Function *GetLoweredFunction(CallInst *CI) {
    // Ge the return type of replicated function.
    Type *RetTy = m_typeTranslator.TranslateReturnType(CI);
    if (!RetTy)
      return nullptr;

    // Get the Function type for replicated function.
    FunctionType *FTy = GetFunctionType(CI, RetTy);
    if (!FTy)
      return nullptr;

    // Create a new function that will be the replicated call.
    AttributeSet attributes = GetAttributeSet(CI);
    std::string name = m_lower.GetExtensionName(CI);
    return cast<Function>(CI->getModule()->getOrInsertFunction(name, FTy, attributes));
  }

  FunctionType *GetFunctionType(CallInst *CI, Type *RetTy) {
    // Create a new function type with the translated argument.
    SmallVector<Type *, 10> ParamTypes;
    ParamTypes.reserve(CI->getNumArgOperands());
    for (unsigned i = 0; i < CI->getNumArgOperands(); ++i) {
      Type *OrigTy = CI->getArgOperand(i)->getType();
      Type *TranslatedTy = m_typeTranslator.TranslateArgumentType(OrigTy);
      ParamTypes.push_back(TranslatedTy);
    }

    const bool IsVarArg = false;
    return FunctionType::get(RetTy, ParamTypes, IsVarArg);
  }

  AttributeSet GetAttributeSet(CallInst *CI) {
    Function *F = CI->getCalledFunction();
    AttributeSet attributes;
    auto copyAttribute = [=, &attributes](Attribute::AttrKind a) {
      if (F->hasFnAttribute(a)) {
        attributes = attributes.addAttribute(CI->getContext(), AttributeSet::FunctionIndex, a);
      }
    };
    copyAttribute(Attribute::AttrKind::ReadOnly);
    copyAttribute(Attribute::AttrKind::ReadNone);
    copyAttribute(Attribute::AttrKind::ArgMemOnly);

    return attributes;
  }
};

///////////////////////////////////////////////////////////////////////////////
// NoTranslation Lowering.
class NoTranslationTypeTranslator : public FunctionTypeTranslator {
  virtual Type *TranslateReturnType(CallInst *CI) override {
    return CI->getType();
  }
  virtual Type *TranslateArgumentType(Type *OrigArgType) override {
    return OrigArgType;
  }
};

llvm::Value *ExtensionLowering::NoTranslation(CallInst *CI) {
  Function *NoTranslationFunction = FunctionTranslator::GetLoweredFunction<NoTranslationTypeTranslator>(CI, *this);
  if (!NoTranslationFunction)
    return nullptr;

  IRBuilder<> builder(CI);
  SmallVector<Value *, 8> args(CI->arg_operands().begin(), CI->arg_operands().end());
  return builder.CreateCall(NoTranslationFunction, args);
};

///////////////////////////////////////////////////////////////////////////////
// Replicated Lowering.
enum {
  NO_COMMON_VECTOR_SIZE = 0xFFFFFFFF,
};
// Find the vector size that will be used for replication.
// The function call will be replicated once for each element of the vector
// size.
static unsigned GetReplicatedVectorSize(llvm::CallInst *CI) {
  unsigned commonVectorSize = NO_COMMON_VECTOR_SIZE;
  Type *RetTy = CI->getType();
  if (RetTy->isVectorTy())
    commonVectorSize = RetTy->getVectorNumElements();
  for (unsigned i = 0; i < CI->getNumArgOperands(); ++i) {
    Type *Ty = CI->getArgOperand(i)->getType();
    if (Ty->isVectorTy()) {
      unsigned vectorSize = Ty->getVectorNumElements();
      if (commonVectorSize != NO_COMMON_VECTOR_SIZE && commonVectorSize != vectorSize) {
        // Inconsistent vector sizes; need a different strategy.
        return NO_COMMON_VECTOR_SIZE;
      }
      commonVectorSize = vectorSize;
    }
  }

  return commonVectorSize;
}

class ReplicatedFunctionTypeTranslator : public FunctionTypeTranslator {
  virtual Type *TranslateReturnType(CallInst *CI) override {
    unsigned commonVectorSize = GetReplicatedVectorSize(CI);
    if (commonVectorSize == NO_COMMON_VECTOR_SIZE)
      return nullptr;

    // Result should be vector or void.
    Type *RetTy = CI->getType();
    if (!RetTy->isVoidTy() && !RetTy->isVectorTy())
      return nullptr;

    if (RetTy->isVectorTy()) {
      RetTy = RetTy->getVectorElementType();
    }

    return RetTy;
  }

  virtual Type *TranslateArgumentType(Type *OrigArgType) override {
    Type *Ty = OrigArgType;
    if (Ty->isVectorTy()) {
      Ty = Ty->getVectorElementType();
    }

    return Ty;
  }

};

class ReplicateCall {
public:
  ReplicateCall(CallInst *CI, Function &ReplicatedFunction)
    : m_CI(CI)
    , m_ReplicatedFunction(ReplicatedFunction)
    , m_numReplicatedCalls(GetReplicatedVectorSize(CI))
    , m_ScalarizeArgIdx()
    , m_Args(CI->getNumArgOperands())
    , m_ReplicatedCalls(m_numReplicatedCalls)
    , m_Builder(CI)
  {
    assert(m_numReplicatedCalls != NO_COMMON_VECTOR_SIZE);
  }

  Value *Generate() {
    CollectReplicatedArguments();
    CreateReplicatedCalls();
    Value *retVal = GetReturnValue();
    return retVal;
  }

private:
  CallInst *m_CI;
  Function &m_ReplicatedFunction;
  unsigned m_numReplicatedCalls;
  SmallVector<unsigned, 10> m_ScalarizeArgIdx;
  SmallVector<Value *, 10> m_Args;
  SmallVector<Value *, 10> m_ReplicatedCalls;
  IRBuilder<> m_Builder;

  // Collect replicated arguments.
  // For non-vector arguments we can add them to the args list directly.
  // These args will be shared by each replicated call. For the vector
  // arguments we remember the position it will go in the argument list.
  // We will fill in the vector args below when we replicate the call
  // (once for each vector lane).
  void CollectReplicatedArguments() {
    for (unsigned i = 0; i < m_CI->getNumArgOperands(); ++i) {
      Type *Ty = m_CI->getArgOperand(i)->getType();
      if (Ty->isVectorTy()) {
        m_ScalarizeArgIdx.push_back(i);
      }
      else {
        m_Args[i] = m_CI->getArgOperand(i);
      }
    }
  }

  // Create replicated calls.
  // Replicate the call once for each element of the replicated vector size.
  void CreateReplicatedCalls() {
    for (unsigned vecIdx = 0; vecIdx < m_numReplicatedCalls; vecIdx++) {
      for (unsigned i = 0, e = m_ScalarizeArgIdx.size(); i < e; ++i) {
        unsigned argIdx = m_ScalarizeArgIdx[i];
        Value *arg = m_CI->getArgOperand(argIdx);
        m_Args[argIdx] = m_Builder.CreateExtractElement(arg, vecIdx);
      }
      Value *EltOP = m_Builder.CreateCall(&m_ReplicatedFunction, m_Args);
      m_ReplicatedCalls[vecIdx] = EltOP;
    }
  }

  // Get the final replicated value.
  // If the function is a void type then return (arbitrarily) the first call.
  // We do not return nullptr because that indicates a failure to replicate.
  // If the function is a vector type then aggregate all of the replicated
  // call values into a new vector.
  Value *GetReturnValue() {
    if (m_CI->getType()->isVoidTy())
      return m_ReplicatedCalls.back();

      Value *retVal = llvm::UndefValue::get(m_CI->getType());
      for (unsigned i = 0; i < m_ReplicatedCalls.size(); ++i)
        retVal = m_Builder.CreateInsertElement(retVal, m_ReplicatedCalls[i], i);

    return retVal;
  }
};

Value *ExtensionLowering::TranslateReplicating(CallInst *CI, Function *ReplicatedFunction) {
  if (!ReplicatedFunction)
    return nullptr;

  ReplicateCall replicate(CI, *ReplicatedFunction);
  return replicate.Generate();
}

Value *ExtensionLowering::Replicate(CallInst *CI) {
  Function *ReplicatedFunction = FunctionTranslator::GetLoweredFunction<ReplicatedFunctionTypeTranslator>(CI, *this);
  return TranslateReplicating(CI, ReplicatedFunction);
}

///////////////////////////////////////////////////////////////////////////////
// Packed Lowering.
class PackCall {
public:
  PackCall(CallInst *CI, Function &PackedFunction)
    : m_CI(CI)
    , m_packedFunction(PackedFunction)
    , m_builder(CI)
  {}

  Value *Generate() {
    SmallVector<Value *, 10> args;
    PackArgs(args);
    Value *result = CreateCall(args);
    return UnpackResult(result);
  }
  
  static StructType *ConvertVectorTypeToStructType(Type *vecTy) {
    assert(vecTy->isVectorTy());
    Type *elementTy = vecTy->getVectorElementType();
    unsigned numElements = vecTy->getVectorNumElements();
    SmallVector<Type *, 4> elements;
    for (unsigned i = 0; i < numElements; ++i)
      elements.push_back(elementTy);

    return StructType::get(vecTy->getContext(), elements);
  }

private:
  CallInst *m_CI;
  Function &m_packedFunction;
  IRBuilder<> m_builder;

  void PackArgs(SmallVectorImpl<Value*> &args) {
    args.clear();
    for (Value *arg : m_CI->arg_operands()) {
      if (arg->getType()->isVectorTy())
        arg = PackVectorIntoStruct(m_builder, arg);
      args.push_back(arg);
    }
  }

  Value *CreateCall(const SmallVectorImpl<Value*> &args) {
    return m_builder.CreateCall(&m_packedFunction, args);
  }

  Value *UnpackResult(Value *result) {
    if (result->getType()->isStructTy()) {
      result = PackStructIntoVector(m_builder, result);
    }
    return result;
  }

  static VectorType *ConvertStructTypeToVectorType(Type *structTy) {
    assert(structTy->isStructTy());
    return VectorType::get(structTy->getStructElementType(0), structTy->getStructNumElements());
  }

  static Value *PackVectorIntoStruct(IRBuilder<> &builder, Value *vec) {
    StructType *structTy = ConvertVectorTypeToStructType(vec->getType());
    Value *packed = UndefValue::get(structTy);

    unsigned numElements = structTy->getStructNumElements();
    for (unsigned i = 0; i < numElements; ++i) {
      Value *element = builder.CreateExtractElement(vec, i);
      packed = builder.CreateInsertValue(packed, element, { i });
    }

    return packed;
  }

  static Value *PackStructIntoVector(IRBuilder<> &builder, Value *strukt) {
    Type *vecTy = ConvertStructTypeToVectorType(strukt->getType());
    Value *packed = UndefValue::get(vecTy);

    unsigned numElements = vecTy->getVectorNumElements();
    for (unsigned i = 0; i < numElements; ++i) {
      Value *element = builder.CreateExtractValue(strukt, i);
      packed = builder.CreateInsertElement(packed, element, { i });
    }

    return packed;
  }
};

class PackedFunctionTypeTranslator : public FunctionTypeTranslator {
  virtual Type *TranslateReturnType(CallInst *CI) override {
    return TranslateIfVector(CI->getType());
  }
  virtual Type *TranslateArgumentType(Type *OrigArgType) override {
    return TranslateIfVector(OrigArgType);
  }

  Type *TranslateIfVector(Type *ty) {
    if (ty->isVectorTy())
      ty = PackCall::ConvertVectorTypeToStructType(ty);
    return ty;
  }
};

Value *ExtensionLowering::Pack(CallInst *CI) {
  Function *PackedFunction = FunctionTranslator::GetLoweredFunction<PackedFunctionTypeTranslator>(CI, *this);
  if (!PackedFunction)
    return nullptr;

  PackCall pack(CI, *PackedFunction);
  Value *result = pack.Generate();
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// Computing Extension Names.

// Compute the name to use for the intrinsic function call once it is lowered to dxil.
// First checks to see if we have a custom name from the codegen helper and if not
// chooses a default name based on the lowergin strategy.
class ExtensionName {
public:
  ExtensionName(CallInst *CI, ExtensionLowering::Strategy strategy, HLSLExtensionsCodegenHelper *helper)
    : m_CI(CI)
    , m_strategy(strategy)
    , m_helper(helper)
  {}

  std::string Get() {
    std::string name;
    if (m_helper)
      name = GetCustomExtensionName(m_CI, *m_helper);

    if (!HasCustomExtensionName(name))
      name = GetDefaultCustomExtensionName(m_CI, ExtensionLowering::GetStrategyName(m_strategy));

    return name;
  }

private:
  CallInst *m_CI;
  ExtensionLowering::Strategy m_strategy;
  HLSLExtensionsCodegenHelper *m_helper;

  static std::string GetCustomExtensionName(CallInst *CI, HLSLExtensionsCodegenHelper &helper) {
    unsigned opcode = GetHLOpcode(CI);
    std::string name = helper.GetIntrinsicName(opcode);
    ReplaceOverloadMarkerWithTypeName(name, CI);

    return name;
  }

  static std::string GetDefaultCustomExtensionName(CallInst *CI, StringRef strategyName) {
    return (Twine(CI->getCalledFunction()->getName()) + "." + Twine(strategyName)).str();
  }

  static bool HasCustomExtensionName(const std::string name) {
    return name.size() > 0;
  }

  // Choose the (return value or argument) type that determines the overload type
  // for the intrinsic call.
  // For now we take the return type as the overload. If the return is void we
  // take the first (non-opcode) argument as the overload type. We could extend the
  // $o sytnax in the extension name to explicitly specify the overload slot (e.g.
  // $o:3 would say the overload type is determined by parameter 3.
  static Type *SelectOverloadSlot(CallInst *CI) {
    Type *ty = CI->getType();
    if (ty->isVoidTy()) {
      if (CI->getNumArgOperands() > 1)
        ty = CI->getArgOperand(1)->getType(); // First non-opcode argument.
    }

    return ty;
  }

  static Type *GetOverloadType(CallInst *CI) {
    Type *ty = SelectOverloadSlot(CI);
    if (ty->isVectorTy())
      ty = ty->getVectorElementType();

    return ty;
  }

  static std::string GetTypeName(Type *ty) {
      std::string typeName;
      llvm::raw_string_ostream os(typeName);
      ty->print(os);
      os.flush();
      return typeName;
  }

  static std::string GetOverloadTypeName(CallInst *CI) {
    Type *ty = GetOverloadType(CI);
    return GetTypeName(ty);
  }

  // Find the occurence of the overload marker $o and replace it the the overload type name.
  static void ReplaceOverloadMarkerWithTypeName(std::string &functionName, CallInst *CI) {
    const char *OverloadMarker = "$o";
    const size_t OverloadMarkerLength = 2;

    size_t pos = functionName.find(OverloadMarker);
    if (pos != std::string::npos) {
      std::string typeName = GetOverloadTypeName(CI);
      functionName.replace(pos, OverloadMarkerLength, typeName);
    }
  }
};

std::string ExtensionLowering::GetExtensionName(llvm::CallInst *CI) {
  ExtensionName name(CI, m_strategy, m_helper);
  return name.Get();
}
