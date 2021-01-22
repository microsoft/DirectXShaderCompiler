//===-- RemoveBufferBlockVisitor.cpp - RemoveBufferBlock Visitor -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "RemoveBufferBlockVisitor.h"
#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvFunction.h"

namespace clang {
namespace spirv {

bool RemoveBufferBlockVisitor::isBufferBlockDecorationDeprecated() {
  return featureManager.isTargetEnvVulkan1p2OrAbove();
}

bool RemoveBufferBlockVisitor::visit(SpirvModule *mod, Phase phase) {
  // If the target environment is Vulkan 1.2 or later, BufferBlock decoration is
  // deprecated and should be removed from the module.
  // Otherwise, no action is needed by this IMR visitor.
  if (phase == Visitor::Phase::Init)
    if (!isBufferBlockDecorationDeprecated())
      return false;

  return true;
}

bool RemoveBufferBlockVisitor::hasStorageBufferInterfaceType(
    const SpirvType *type) {
  while (type != nullptr) {
    if (const auto *structType = dyn_cast<StructType>(type)) {
      return structType->getInterfaceType() ==
             StructInterfaceType::StorageBuffer;
    } else if (const auto *elemType = dyn_cast<ArrayType>(type)) {
      type = elemType->getElementType();
    } else if (const auto *elemType = dyn_cast<RuntimeArrayType>(type)) {
      type = elemType->getElementType();
    } else {
      return false;
    }
  }
  return false;
}

bool RemoveBufferBlockVisitor::visitInstruction(SpirvInstruction *inst) {
  if (!inst->getResultType())
    return true;

  // OpAccessChain can obtain pointers to any type. Its result type is
  // OpTypePointer, and it should get the same storage class as its base.
  if (auto *accessChain = dyn_cast<SpirvAccessChain>(inst)) {
    auto *accessChainType = accessChain->getResultType();
    auto *baseType = accessChain->getBase()->getResultType();
    // The result type of OpAccessChain and the result type of its base must be
    // OpTypePointer.
    assert(isa<SpirvPointerType>(accessChainType));
    assert(isa<SpirvPointerType>(baseType));
    auto *accessChainPtr = dyn_cast<SpirvPointerType>(accessChainType);
    auto *basePtr = dyn_cast<SpirvPointerType>(baseType);
    auto baseStorageClass = basePtr->getStorageClass();
    if (accessChainPtr->getStorageClass() != baseStorageClass) {
      auto *newAccessChainType = context.getPointerType(
          accessChainPtr->getPointeeType(), baseStorageClass);
      inst->setStorageClass(baseStorageClass);
      inst->setResultType(newAccessChainType);
    }
  }

  // For all instructions, if the result type is a pointer pointing to a struct
  // with StorageBuffer interface, the storage class must be updated.
  const auto *instType = inst->getResultType();
  const auto *newInstType = instType;
  spv::StorageClass newInstStorageClass = spv::StorageClass::Max;
  if (updateStorageClass(instType, &newInstType, &newInstStorageClass)) {
    inst->setResultType(newInstType);
    inst->setStorageClass(newInstStorageClass);
  }

  return true;
}

bool RemoveBufferBlockVisitor::updateStorageClass(
    const SpirvType *type, const SpirvType **newType,
    spv::StorageClass *newStorageClass) {
  auto *ptrType = dyn_cast<SpirvPointerType>(type);
  if (ptrType == nullptr)
    return false;

  const auto *innerType = ptrType->getPointeeType();

  // For usual cases such as _ptr_Uniform_StructuredBuffer_float.
  if (hasStorageBufferInterfaceType(innerType) &&
      ptrType->getStorageClass() != spv::StorageClass::StorageBuffer) {
    *newType =
        context.getPointerType(innerType, spv::StorageClass::StorageBuffer);
    *newStorageClass = spv::StorageClass::StorageBuffer;
    return true;
  }

  // For pointer-to-pointer cases (which need legalization), we could have a
  // type like: _ptr_Function__ptr_Uniform_type_StructuredBuffer_float.
  // In such cases, we need to update the storage class for the inner pointer.
  if (const auto *innerPtrType = dyn_cast<SpirvPointerType>(innerType)) {
    if (hasStorageBufferInterfaceType(innerPtrType->getPointeeType()) &&
        innerPtrType->getStorageClass() != spv::StorageClass::StorageBuffer) {
      auto *newInnerType = context.getPointerType(
          innerPtrType->getPointeeType(), spv::StorageClass::StorageBuffer);
      *newType =
          context.getPointerType(newInnerType, ptrType->getStorageClass());
      *newStorageClass = ptrType->getStorageClass();
      return true;
    }
  }

  return false;
}

bool RemoveBufferBlockVisitor::visit(SpirvFunction *fn, Phase phase) {
  if (phase == Visitor::Phase::Init) {
    llvm::SmallVector<const SpirvType *, 4> paramTypes;
    bool updatedParamTypes = false;
    for (auto *param : fn->getParameters()) {
      const auto *paramType = param->getResultType();
      // This pass is run after all types are lowered.
      assert(paramType != nullptr);

      // Update the parameter type if needed (update storage class of pointers).
      const auto *newParamType = paramType;
      spv::StorageClass newParamSC = spv::StorageClass::Max;
      if (updateStorageClass(paramType, &newParamType, &newParamSC)) {
        param->setStorageClass(newParamSC);
        param->setResultType(newParamType);
        updatedParamTypes = true;
      }
      paramTypes.push_back(newParamType);
    }

    // Update the return type if needed (update storage class of pointers).
    const auto *returnType = fn->getReturnType();
    const auto *newReturnType = returnType;
    spv::StorageClass newReturnSC = spv::StorageClass::Max;
    bool updatedReturnType =
        updateStorageClass(returnType, &newReturnType, &newReturnSC);
    if (updatedReturnType) {
      fn->setReturnType(newReturnType);
    }

    if (updatedParamTypes || updatedReturnType) {
      fn->setFunctionType(context.getFunctionType(newReturnType, paramTypes));
    }
    return true;
  }
  return true;
}

} // end namespace spirv
} // end namespace clang
