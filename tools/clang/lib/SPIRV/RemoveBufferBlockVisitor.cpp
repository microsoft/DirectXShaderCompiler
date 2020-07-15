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

namespace {

bool isBufferBlockDecorationDeprecated(
    const clang::spirv::SpirvCodeGenOptions &opts) {
  return opts.targetEnv.compare("vulkan1.2") >= 0;
}

} // end anonymous namespace

namespace clang {
namespace spirv {

bool RemoveBufferBlockVisitor::visit(SpirvModule *mod, Phase phase) {
  // If the target environment is Vulkan 1.2 or later, BufferBlock decoration is
  // deprecated and should be removed from the module.
  // Otherwise, no action is needed by this IMR visitor.
  if (phase == Visitor::Phase::Init)
    if (!isBufferBlockDecorationDeprecated(spvOptions))
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
  if (auto *ptrResultType = dyn_cast<SpirvPointerType>(inst->getResultType())) {
    if (hasStorageBufferInterfaceType(ptrResultType->getPointeeType()) &&
        ptrResultType->getStorageClass() != spv::StorageClass::StorageBuffer) {
      inst->setStorageClass(spv::StorageClass::StorageBuffer);
      inst->setResultType(context.getPointerType(
          ptrResultType->getPointeeType(), spv::StorageClass::StorageBuffer));
    }
  }

  return true;
}

} // end namespace spirv
} // end namespace clang
