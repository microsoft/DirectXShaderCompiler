//===--- ModuleBuilder.cpp - SPIR-V builder implementation ----*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/ModuleBuilder.h"

#include "clang/SPIRV/InstBuilder.h"
#include "llvm/llvm_assert/assert.h"
#include "spirv/1.0//spirv.hpp11"

namespace clang {
namespace spirv {

ModuleBuilder::ModuleBuilder(SPIRVContext *C)
    : theContext(*C), theModule(), theFunction(llvm::None),
      theBasicBlock(llvm::None), instBuilder(nullptr) {
  instBuilder.setConsumer([this](std::vector<uint32_t> &&words) {
    this->constructSite = std::move(words);
  });
}

ModuleBuilder::Status ModuleBuilder::beginModule() {
  if (!theModule.isEmpty() || theFunction.hasValue() ||
      theBasicBlock.hasValue())
    return Status::ErrNestedModule;

  return Status::Success;
}

ModuleBuilder::Status ModuleBuilder::endModule() {
  theModule.setBound(theContext.getNextId());
  return Status::Success;
}

ModuleBuilder::Status ModuleBuilder::beginFunction(uint32_t funcType,
                                                   uint32_t returnType) {
  if (theFunction.hasValue())
    return Status::ErrNestedFunction;

  theFunction = llvm::Optional<Function>(
      Function(returnType, theContext.takeNextId(),
               spv::FunctionControlMask::MaskNone, funcType));

  return Status::Success;
}

ModuleBuilder::Status ModuleBuilder::endFunction() {
  if (theBasicBlock.hasValue())
    return Status::ErrActiveBasicBlock;
  if (!theFunction.hasValue())
    return Status::ErrNoActiveFunction;

  theModule.addFunction(std::move(theFunction.getValue()));
  theFunction.reset();

  return Status::Success;
}

ModuleBuilder::Status ModuleBuilder::beginBasicBlock() {
  if (theBasicBlock.hasValue())
    return Status::ErrNestedBasicBlock;
  if (!theFunction.hasValue())
    return Status::ErrDetachedBasicBlock;

  theBasicBlock =
      llvm::Optional<BasicBlock>(BasicBlock(theContext.takeNextId()));

  return Status::Success;
}

ModuleBuilder::Status ModuleBuilder::endBasicBlockWithReturn() {
  if (!theBasicBlock.hasValue())
    return Status::ErrNoActiveBasicBlock;

  instBuilder.opReturn().x();
  theBasicBlock.getValue().addInstruction(std::move(constructSite));

  return endBasicBlock();
}

ModuleBuilder::Status ModuleBuilder::endBasicBlock() {
  theFunction.getValue().addBasicBlock(std::move(theBasicBlock.getValue()));
  theBasicBlock.reset();
  return Status::Success;
}

std::vector<uint32_t> ModuleBuilder::takeModule() {
  std::vector<uint32_t> binary;
  auto ib = InstBuilder([&binary](std::vector<uint32_t> &&words) {
    binary.insert(binary.end(), words.begin(), words.end());
  });

  theModule.take(&ib);
  return std::move(binary);
}

} // end namespace spirv
} // end namespace clang
