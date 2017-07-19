//===--- ModuleBuilder.cpp - SPIR-V builder implementation ----*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/ModuleBuilder.h"

#include "spirv/1.0//spirv.hpp11"
#include "clang/SPIRV/InstBuilder.h"
#include "llvm/llvm_assert/assert.h"

namespace clang {
namespace spirv {

ModuleBuilder::ModuleBuilder(SPIRVContext *C)
    : theContext(*C), theModule(), theFunction(nullptr), insertPoint(nullptr),
      instBuilder(nullptr) {
  instBuilder.setConsumer([this](std::vector<uint32_t> &&words) {
    this->constructSite = std::move(words);
  });
}

uint32_t ModuleBuilder::beginFunction(uint32_t funcType, uint32_t returnType) {
  if (theFunction) {
    assert(false && "found nested function");
    return 0;
  }

  const uint32_t fId = theContext.takeNextId();

  theFunction = llvm::make_unique<Function>(
      returnType, fId, spv::FunctionControlMask::MaskNone, funcType);

  return fId;
}

uint32_t ModuleBuilder::addFnParameter(uint32_t type) {
  assert(theFunction && "found detached parameter");

  const uint32_t paramId = theContext.takeNextId();
  theFunction->addParameter(type, paramId);
  return paramId;
}

bool ModuleBuilder::endFunction() {
  if (theFunction == nullptr) {
    assert(false && "no active function");
    return false;
  }

  // Move all basic blocks into the current function.
  // TODO: we should adjust the order the basic blocks according to
  // SPIR-V validation rules.
  for (auto &bb : basicBlocks) {
    theFunction->addBasicBlock(std::move(bb.second));
  }
  basicBlocks.clear();

  theModule.addFunction(std::move(theFunction));
  theFunction.reset(nullptr);

  insertPoint = nullptr;

  return true;
}

uint32_t ModuleBuilder::createBasicBlock() {
  if (theFunction == nullptr) {
    assert(false && "found detached basic block");
    return 0;
  }

  const uint32_t labelId = theContext.takeNextId();
  basicBlocks[labelId] = llvm::make_unique<BasicBlock>(labelId);

  return labelId;
}

bool ModuleBuilder::setInsertPoint(uint32_t labelId) {
  auto it = basicBlocks.find(labelId);
  if (it == basicBlocks.end()) {
    assert(false && "invalid <label-id>");
    return false;
  }
  insertPoint = it->second.get();
  return true;
}

uint32_t ModuleBuilder::createLoad(uint32_t resultType, uint32_t pointer) {
  assert(insertPoint && "null insert point");
  const uint32_t resultId = theContext.takeNextId();
  instBuilder.opLoad(resultType, resultId, pointer, llvm::None).x();
  insertPoint->addInstruction(std::move(constructSite));
  return resultId;
}

void ModuleBuilder::createStore(uint32_t address, uint32_t value) {
  assert(insertPoint && "null insert point");
  instBuilder.opStore(address, value, llvm::None).x();
  insertPoint->addInstruction(std::move(constructSite));
}

void ModuleBuilder::createReturn() {
  assert(insertPoint && "null insert point");
  instBuilder.opReturn().x();
  insertPoint->addInstruction(std::move(constructSite));
}

void ModuleBuilder::createReturnValue(uint32_t value) {
  assert(insertPoint && "null insert point");
  instBuilder.opReturnValue(value).x();
  insertPoint->addInstruction(std::move(constructSite));
}

uint32_t ModuleBuilder::getVoidType() {
  const Type *type = Type::getVoid(theContext);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t ModuleBuilder::getFloatType() {
  const Type *type = Type::getFloat32(theContext);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t ModuleBuilder::getVec2Type(uint32_t elemType) {
  const Type *type = Type::getVec2(theContext, elemType);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t ModuleBuilder::getVec3Type(uint32_t elemType) {
  const Type *type = Type::getVec3(theContext, elemType);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t ModuleBuilder::getVec4Type(uint32_t elemType) {
  const Type *type = Type::getVec4(theContext, elemType);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t
ModuleBuilder::getFunctionType(uint32_t returnType,
                               const std::vector<uint32_t> &paramTypes) {
  const Type *type = Type::getFunction(theContext, returnType, paramTypes);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t ModuleBuilder::getPointerType(uint32_t pointeeType,
                                       spv::StorageClass storageClass) {
  const Type *type = Type::getPointer(theContext, storageClass, pointeeType);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t
ModuleBuilder::addStageIOVariable(uint32_t type, spv::StorageClass storageClass,
                                  llvm::Optional<uint32_t> initilizer) {
  const uint32_t pointerType = getPointerType(type, storageClass);
  const uint32_t varId = theContext.takeNextId();
  instBuilder.opVariable(pointerType, varId, storageClass, initilizer).x();
  theModule.addVariable(std::move(constructSite));
  return varId;
}

void ModuleBuilder::decorateLocation(uint32_t targetId, uint32_t location) {
  const Decoration *d =
      Decoration::getLocation(theContext, location, llvm::None);
  theModule.addDecoration(*d, targetId);
}

std::vector<uint32_t> ModuleBuilder::takeModule() {
  theModule.setBound(theContext.getNextId());

  std::vector<uint32_t> binary;
  auto ib = InstBuilder([&binary](std::vector<uint32_t> &&words) {
    binary.insert(binary.end(), words.begin(), words.end());
  });

  theModule.take(&ib);
  return std::move(binary);
}

} // end namespace spirv
} // end namespace clang
