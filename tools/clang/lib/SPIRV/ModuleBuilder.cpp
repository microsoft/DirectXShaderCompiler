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
      instBuilder(nullptr), glslExtSetId(0) {
  instBuilder.setConsumer([this](std::vector<uint32_t> &&words) {
    this->constructSite = std::move(words);
  });
}

std::vector<uint32_t> ModuleBuilder::takeModule() {
  theModule.setBound(theContext.getNextId());

  std::vector<uint32_t> binary;
  auto ib = InstBuilder([&binary](std::vector<uint32_t> &&words) {
    binary.insert(binary.end(), words.begin(), words.end());
  });

  theModule.take(&ib);
  return binary;
}

uint32_t ModuleBuilder::beginFunction(uint32_t funcType, uint32_t returnType,
                                      llvm::StringRef funcName, uint32_t fId) {
  if (theFunction) {
    assert(false && "found nested function");
    return 0;
  }

  // If the caller doesn't supply a function <result-id>, we need to get one.
  if (!fId)
    fId = theContext.takeNextId();

  theFunction = llvm::make_unique<Function>(
      returnType, fId, spv::FunctionControlMask::MaskNone, funcType);
  theModule.addDebugName(fId, funcName);

  return fId;
}

uint32_t ModuleBuilder::addFnParam(uint32_t ptrType, llvm::StringRef name) {
  assert(theFunction && "found detached parameter");

  const uint32_t paramId = theContext.takeNextId();
  theFunction->addParameter(ptrType, paramId);
  theModule.addDebugName(paramId, name);

  return paramId;
}

uint32_t ModuleBuilder::addFnVar(uint32_t varType, llvm::StringRef name,
                                 llvm::Optional<uint32_t> init) {
  assert(theFunction && "found detached local variable");

  const uint32_t ptrType = getPointerType(varType, spv::StorageClass::Function);
  const uint32_t varId = theContext.takeNextId();
  theFunction->addVariable(ptrType, varId, init);
  theModule.addDebugName(varId, name);
  return varId;
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

uint32_t ModuleBuilder::createBasicBlock(llvm::StringRef name,
                                         bool isReachable) {
  if (theFunction == nullptr) {
    assert(false && "found detached basic block");
    return 0;
  }

  const uint32_t labelId = theContext.takeNextId();
  basicBlocks[labelId] = llvm::make_unique<BasicBlock>(labelId, isReachable);

  // OpName instructions should not be added for unreachable basic blocks
  // because such blocks are *not* discovered by BlockReadableOrderVisitor and
  // therefore they are not emitted.
  // The newly created basic block is unreachable if specified by the caller,
  // or, if this block is being created by a block that is already unreachable.
  if (isReachable && (!insertPoint || insertPoint->isReachable()))
    theModule.addDebugName(labelId, name);

  return labelId;
}

void ModuleBuilder::addSuccessor(uint32_t successorLabel) {
  assert(insertPoint && "null insert point");
  insertPoint->addSuccessor(getBasicBlock(successorLabel));
}

void ModuleBuilder::setMergeTarget(uint32_t mergeLabel) {
  assert(insertPoint && "null insert point");
  insertPoint->setMergeTarget(getBasicBlock(mergeLabel));
}

void ModuleBuilder::setContinueTarget(uint32_t continueLabel) {
  assert(insertPoint && "null insert point");
  insertPoint->setContinueTarget(getBasicBlock(continueLabel));
}

void ModuleBuilder::setInsertPoint(uint32_t labelId) {
  insertPoint = getBasicBlock(labelId);
}

uint32_t
ModuleBuilder::createCompositeConstruct(uint32_t resultType,
                                        llvm::ArrayRef<uint32_t> constituents) {
  assert(insertPoint && "null insert point");
  const uint32_t resultId = theContext.takeNextId();
  instBuilder.opCompositeConstruct(resultType, resultId, constituents).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return resultId;
}

uint32_t
ModuleBuilder::createCompositeExtract(uint32_t resultType, uint32_t composite,
                                      llvm::ArrayRef<uint32_t> indexes) {
  assert(insertPoint && "null insert point");
  const uint32_t resultId = theContext.takeNextId();
  instBuilder.opCompositeExtract(resultType, resultId, composite, indexes).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return resultId;
}

uint32_t
ModuleBuilder::createVectorShuffle(uint32_t resultType, uint32_t vector1,
                                   uint32_t vector2,
                                   llvm::ArrayRef<uint32_t> selectors) {
  assert(insertPoint && "null insert point");
  const uint32_t resultId = theContext.takeNextId();
  instBuilder.opVectorShuffle(resultType, resultId, vector1, vector2, selectors)
      .x();
  insertPoint->appendInstruction(std::move(constructSite));
  return resultId;
}

uint32_t ModuleBuilder::createLoad(uint32_t resultType, uint32_t pointer) {
  assert(insertPoint && "null insert point");
  const uint32_t resultId = theContext.takeNextId();
  instBuilder.opLoad(resultType, resultId, pointer, llvm::None).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return resultId;
}

void ModuleBuilder::createStore(uint32_t address, uint32_t value) {
  assert(insertPoint && "null insert point");
  instBuilder.opStore(address, value, llvm::None).x();
  insertPoint->appendInstruction(std::move(constructSite));
}

uint32_t ModuleBuilder::createFunctionCall(uint32_t returnType,
                                           uint32_t functionId,
                                           llvm::ArrayRef<uint32_t> params) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder.opFunctionCall(returnType, id, functionId, params).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createAccessChain(uint32_t resultType, uint32_t base,
                                          llvm::ArrayRef<uint32_t> indexes) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder.opAccessChain(resultType, id, base, indexes).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createUnaryOp(spv::Op op, uint32_t resultType,
                                      uint32_t operand) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder.unaryOp(op, resultType, id, operand).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createBinaryOp(spv::Op op, uint32_t resultType,
                                       uint32_t lhs, uint32_t rhs) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder.binaryOp(op, resultType, id, lhs, rhs).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createSelect(uint32_t resultType, uint32_t condition,
                                     uint32_t trueValue, uint32_t falseValue) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder.opSelect(resultType, id, condition, trueValue, falseValue).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

void ModuleBuilder::createSwitch(
    uint32_t mergeLabel, uint32_t selector, uint32_t defaultLabel,
    llvm::ArrayRef<std::pair<uint32_t, uint32_t>> target) {
  assert(insertPoint && "null insert point");
  // Create the OpSelectioMerege.
  instBuilder.opSelectionMerge(mergeLabel, spv::SelectionControlMask::MaskNone)
      .x();
  insertPoint->appendInstruction(std::move(constructSite));

  // Create the OpSwitch.
  instBuilder.opSwitch(selector, defaultLabel, target).x();
  insertPoint->appendInstruction(std::move(constructSite));
}

void ModuleBuilder::createBranch(uint32_t targetLabel, uint32_t mergeBB,
                                 uint32_t continueBB,
                                 spv::LoopControlMask loopControl) {
  assert(insertPoint && "null insert point");

  if (mergeBB && continueBB) {
    instBuilder.opLoopMerge(mergeBB, continueBB, loopControl).x();
    insertPoint->appendInstruction(std::move(constructSite));
  }

  instBuilder.opBranch(targetLabel).x();
  insertPoint->appendInstruction(std::move(constructSite));
}

void ModuleBuilder::createConditionalBranch(
    uint32_t condition, uint32_t trueLabel, uint32_t falseLabel,
    uint32_t mergeLabel, uint32_t continueLabel,
    spv::SelectionControlMask selectionControl,
    spv::LoopControlMask loopControl) {
  assert(insertPoint && "null insert point");

  if (mergeLabel) {
    if (continueLabel) {
      instBuilder.opLoopMerge(mergeLabel, continueLabel, loopControl).x();
      insertPoint->appendInstruction(std::move(constructSite));
    } else {
      instBuilder.opSelectionMerge(mergeLabel, selectionControl).x();
      insertPoint->appendInstruction(std::move(constructSite));
    }
  }

  instBuilder.opBranchConditional(condition, trueLabel, falseLabel, {}).x();
  insertPoint->appendInstruction(std::move(constructSite));
}

void ModuleBuilder::createReturn() {
  assert(insertPoint && "null insert point");
  instBuilder.opReturn().x();
  insertPoint->appendInstruction(std::move(constructSite));
}

void ModuleBuilder::createReturnValue(uint32_t value) {
  assert(insertPoint && "null insert point");
  instBuilder.opReturnValue(value).x();
  insertPoint->appendInstruction(std::move(constructSite));
}

uint32_t ModuleBuilder::createExtInst(uint32_t resultType, uint32_t setId,
                                      uint32_t instId,
                                      llvm::ArrayRef<uint32_t> operands) {
  assert(insertPoint && "null insert point");
  uint32_t resultId = theContext.takeNextId();
  instBuilder.opExtInst(resultType, resultId, setId, instId, operands).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return resultId;
}

void ModuleBuilder::addExecutionMode(uint32_t entryPointId,
                                     spv::ExecutionMode em,
                                     const std::vector<uint32_t> &params) {
  instBuilder.opExecutionMode(entryPointId, em);
  for (const auto &param : params) {
    instBuilder.literalInteger(param);
  }
  instBuilder.x();
  theModule.addExecutionMode(std::move(constructSite));
}

uint32_t ModuleBuilder::getGLSLExtInstSet() {
  if (glslExtSetId == 0) {
    glslExtSetId = theContext.takeNextId();
    theModule.addExtInstSet(glslExtSetId, "GLSL.std.450");
  }
  return glslExtSetId;
}

uint32_t ModuleBuilder::addStageIOVar(uint32_t type,
                                      spv::StorageClass storageClass) {
  const uint32_t pointerType = getPointerType(type, storageClass);
  const uint32_t varId = theContext.takeNextId();
  instBuilder.opVariable(pointerType, varId, storageClass, llvm::None).x();
  theModule.addVariable(std::move(constructSite));
  return varId;
}

uint32_t ModuleBuilder::addStageBuiltinVar(uint32_t type, spv::StorageClass sc,
                                           spv::BuiltIn builtin) {
  const uint32_t pointerType = getPointerType(type, sc);
  const uint32_t varId = theContext.takeNextId();
  instBuilder.opVariable(pointerType, varId, sc, llvm::None).x();
  theModule.addVariable(std::move(constructSite));

  // Decorate with the specified Builtin
  const Decoration *d = Decoration::getBuiltIn(theContext, builtin);
  theModule.addDecoration(*d, varId);

  return varId;
}

uint32_t ModuleBuilder::addFileVar(uint32_t type, llvm::StringRef name,
                                   llvm::Optional<uint32_t> init) {
  const uint32_t pointerType = getPointerType(type, spv::StorageClass::Private);
  const uint32_t varId = theContext.takeNextId();
  instBuilder.opVariable(pointerType, varId, spv::StorageClass::Private, init)
      .x();
  theModule.addVariable(std::move(constructSite));
  theModule.addDebugName(varId, name);
  return varId;
}

void ModuleBuilder::decorateLocation(uint32_t targetId, uint32_t location) {
  const Decoration *d =
      Decoration::getLocation(theContext, location, llvm::None);
  theModule.addDecoration(*d, targetId);
}

#define IMPL_GET_PRIMITIVE_TYPE(ty)                                            \
  \
uint32_t ModuleBuilder::get##ty##Type() {                                      \
    const Type *type = Type::get##ty(theContext);                              \
    const uint32_t typeId = theContext.getResultIdForType(type);               \
    theModule.addType(type, typeId);                                           \
    return typeId;                                                             \
  \
}

IMPL_GET_PRIMITIVE_TYPE(Void)
IMPL_GET_PRIMITIVE_TYPE(Bool)
IMPL_GET_PRIMITIVE_TYPE(Int32)
IMPL_GET_PRIMITIVE_TYPE(Uint32)
IMPL_GET_PRIMITIVE_TYPE(Float32)

#undef IMPL_GET_PRIMITIVE_TYPE

uint32_t ModuleBuilder::getVecType(uint32_t elemType, uint32_t elemCount) {
  const Type *type = nullptr;
  switch (elemCount) {
  case 2:
    type = Type::getVec2(theContext, elemType);
    break;
  case 3:
    type = Type::getVec3(theContext, elemType);
    break;
  case 4:
    type = Type::getVec4(theContext, elemType);
    break;
  default:
    assert(false && "unhandled vector size");
    // Error found. Return 0 as the <result-id> directly.
    return 0;
  }

  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);

  return typeId;
}

uint32_t ModuleBuilder::getMatType(uint32_t colType, uint32_t colCount) {
  const Type *type = Type::getMatrix(theContext, colType, colCount);
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
ModuleBuilder::getStructType(llvm::ArrayRef<uint32_t> fieldTypes,
                             llvm::StringRef structName,
                             llvm::ArrayRef<llvm::StringRef> fieldNames) {
  const Type *type = Type::getStruct(theContext, fieldTypes);
  bool isRegistered = false;
  const uint32_t typeId = theContext.getResultIdForType(type, &isRegistered);
  theModule.addType(type, typeId);
  // TODO: Probably we should check duplication and do nothing if trying to add
  // the same debug name for the same entity in addDebugName().
  if (!isRegistered) {
    theModule.addDebugName(typeId, structName);
    if (!fieldNames.empty()) {
      assert(fieldNames.size() == fieldTypes.size());
      for (uint32_t i = 0; i < fieldNames.size(); ++i)
        theModule.addDebugName(typeId, fieldNames[i],
                               llvm::Optional<uint32_t>(i));
    }
  }
  return typeId;
}

uint32_t ModuleBuilder::getFunctionType(uint32_t returnType,
                                        llvm::ArrayRef<uint32_t> paramTypes) {
  const Type *type = Type::getFunction(theContext, returnType, paramTypes);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t ModuleBuilder::getConstantBool(bool value) {
  const uint32_t typeId = getBoolType();
  const Constant *constant = value ? Constant::getTrue(theContext, typeId)
                                   : Constant::getFalse(theContext, typeId);

  const uint32_t constId = theContext.getResultIdForConstant(constant);
  theModule.addConstant(constant, constId);
  return constId;
}

#define IMPL_GET_PRIMITIVE_CONST(builderTy, cppTy)                             \
  \
uint32_t ModuleBuilder::getConstant##builderTy(cppTy value) {                  \
    const uint32_t typeId = get##builderTy##Type();                            \
    const Constant *constant =                                                 \
        Constant::get##builderTy(theContext, typeId, value);                   \
    const uint32_t constId = theContext.getResultIdForConstant(constant);      \
    theModule.addConstant(constant, constId);                                  \
    return constId;                                                            \
  \
}

IMPL_GET_PRIMITIVE_CONST(Int32, int32_t)
IMPL_GET_PRIMITIVE_CONST(Uint32, uint32_t)
IMPL_GET_PRIMITIVE_CONST(Float32, float)

#undef IMPL_GET_PRIMITIVE_VALUE

uint32_t
ModuleBuilder::getConstantComposite(uint32_t typeId,
                                    llvm::ArrayRef<uint32_t> constituents) {
  const Constant *constant =
      Constant::getComposite(theContext, typeId, constituents);
  const uint32_t constId = theContext.getResultIdForConstant(constant);
  theModule.addConstant(constant, constId);
  return constId;
}

uint32_t ModuleBuilder::getConstantNull(uint32_t typeId) {
  const Constant *constant = Constant::getNull(theContext, typeId);
  const uint32_t constId = theContext.getResultIdForConstant(constant);
  theModule.addConstant(constant, constId);
  return constId;
}

BasicBlock *ModuleBuilder::getBasicBlock(uint32_t labelId) {
  auto it = basicBlocks.find(labelId);
  if (it == basicBlocks.end()) {
    assert(false && "invalid <label-id>");
    return nullptr;
  }

  return it->second.get();
}

} // end namespace spirv
} // end namespace clang
