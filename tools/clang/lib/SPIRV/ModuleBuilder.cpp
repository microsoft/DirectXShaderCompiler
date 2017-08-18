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

uint32_t ModuleBuilder::createBasicBlock(llvm::StringRef name) {
  if (theFunction == nullptr) {
    assert(false && "found detached basic block");
    return 0;
  }

  const uint32_t labelId = theContext.takeNextId();
  basicBlocks[labelId] = llvm::make_unique<BasicBlock>(labelId, name);
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

namespace {
spv::ImageOperandsMask composeImageOperandsMask(
    uint32_t bias, uint32_t lod, const std::pair<uint32_t, uint32_t> &grad,
    uint32_t constOffset, llvm::SmallVectorImpl<uint32_t> *orderedParams) {
  using spv::ImageOperandsMask;
  // SPIR-V Image Operands from least significant bit to most significant bit
  // Bias, Lod, Grad, ConstOffset, Offset, ConstOffsets, Sample, MinLod

  auto mask = ImageOperandsMask::MaskNone;
  orderedParams->clear();

  if (bias) {
    mask = mask | ImageOperandsMask::Bias;
    orderedParams->push_back(bias);
  }

  if (lod) {
    mask = mask | ImageOperandsMask::Lod;
    orderedParams->push_back(lod);
  }

  if (grad.first && grad.second) {
    mask = mask | ImageOperandsMask::Grad;
    orderedParams->push_back(grad.first);
    orderedParams->push_back(grad.second);
  }

  if (constOffset) {
    mask = mask | ImageOperandsMask::ConstOffset;
    orderedParams->push_back(constOffset);
  }

  return mask;
}
} // anonymous namespace

uint32_t ModuleBuilder::createImageSample(uint32_t texelType,
                                          uint32_t imageType, uint32_t image,
                                          uint32_t sampler, uint32_t coordinate,
                                          uint32_t bias, uint32_t lod,
                                          std::pair<uint32_t, uint32_t> grad,
                                          uint32_t offset) {
  assert(insertPoint && "null insert point");

  // An OpSampledImage is required to do the image sampling.
  const uint32_t sampledImgId = theContext.takeNextId();
  const uint32_t sampledImgTy = getSampledImageType(imageType);
  instBuilder.opSampledImage(sampledImgTy, sampledImgId, image, sampler).x();
  insertPoint->appendInstruction(std::move(constructSite));

  const uint32_t texelId = theContext.takeNextId();
  llvm::SmallVector<uint32_t, 4> params;
  const auto mask = composeImageOperandsMask(bias, lod, grad, offset, &params);
  // The Lod and Grad image operands requires explicit-lod instructions.
  if (lod || (grad.first && grad.second)) {
    instBuilder.opImageSampleExplicitLod(texelType, texelId, sampledImgId,
                                         coordinate, mask);
  } else {
    instBuilder.opImageSampleImplicitLod(
        texelType, texelId, sampledImgId, coordinate,
        llvm::Optional<spv::ImageOperandsMask>(mask));
  }
  for (const auto param : params)
    instBuilder.idRef(param);
  instBuilder.x();
  insertPoint->appendInstruction(std::move(constructSite));

  return texelId;
}

uint32_t ModuleBuilder::createImageFetch(uint32_t texelType, uint32_t image,
                                         uint32_t coordinate, uint32_t lod,
                                         uint32_t offset) {
  assert(insertPoint && "null insert point");

  llvm::SmallVector<uint32_t, 2> params;
  const auto mask =
      llvm::Optional<spv::ImageOperandsMask>(composeImageOperandsMask(
          /*bias*/ 0, lod, std::make_pair(0, 0), offset, &params));

  const uint32_t texelId = theContext.takeNextId();
  instBuilder.opImageFetch(texelType, texelId, image, coordinate, mask);
  for (const auto param : params)
    instBuilder.idRef(param);
  instBuilder.x();
  insertPoint->appendInstruction(std::move(constructSite));

  return texelId;
}

uint32_t ModuleBuilder::createImageGather(uint32_t texelType,
                                          uint32_t imageType, uint32_t image,
                                          uint32_t sampler, uint32_t coordinate,
                                          uint32_t component, uint32_t offset) {
  assert(insertPoint && "null insert point");

  // An OpSampledImage is required to do the image sampling.
  const uint32_t sampledImgId = theContext.takeNextId();
  const uint32_t sampledImgTy = getSampledImageType(imageType);
  instBuilder.opSampledImage(sampledImgTy, sampledImgId, image, sampler).x();
  insertPoint->appendInstruction(std::move(constructSite));

  llvm::SmallVector<uint32_t, 2> params;

  const auto mask =
      llvm::Optional<spv::ImageOperandsMask>(composeImageOperandsMask(
          /*bias*/ 0, /*lod*/ 0, std::make_pair(0, 0), offset, &params));
  const uint32_t texelId = theContext.takeNextId();
  instBuilder.opImageGather(texelType, texelId, sampledImgId, coordinate,
                            component, mask);
  for (const auto param : params)
    instBuilder.idRef(param);
  instBuilder.x();
  insertPoint->appendInstruction(std::move(constructSite));

  return texelId;
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

void ModuleBuilder::createKill() {
  assert(insertPoint && "null insert point");
  assert(!isCurrentBasicBlockTerminated());
  instBuilder.opKill().x();
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
                                     llvm::ArrayRef<uint32_t> params) {
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
                                      spv::StorageClass storageClass,
                                      std::string name) {
  const uint32_t pointerType = getPointerType(type, storageClass);
  const uint32_t varId = theContext.takeNextId();
  instBuilder.opVariable(pointerType, varId, storageClass, llvm::None).x();
  theModule.addVariable(std::move(constructSite));
  theModule.addDebugName(varId, name);
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

uint32_t ModuleBuilder::addModuleVar(uint32_t type, spv::StorageClass sc,
                                     llvm::StringRef name,
                                     llvm::Optional<uint32_t> init) {
  assert(sc != spv::StorageClass::Function);

  // TODO: basically duplicated code of addFileVar()
  const uint32_t pointerType = getPointerType(type, sc);
  const uint32_t varId = theContext.takeNextId();
  instBuilder.opVariable(pointerType, varId, sc, init).x();
  theModule.addVariable(std::move(constructSite));
  theModule.addDebugName(varId, name);
  return varId;
}

void ModuleBuilder::decorateDSetBinding(uint32_t targetId, uint32_t setNumber,
                                        uint32_t bindingNumber) {
  const auto *d = Decoration::getDescriptorSet(theContext, setNumber);
  theModule.addDecoration(*d, targetId);

  d = Decoration::getBinding(theContext, bindingNumber);
  theModule.addDecoration(*d, targetId);
}

void ModuleBuilder::decorateLocation(uint32_t targetId, uint32_t location) {
  const Decoration *d =
      Decoration::getLocation(theContext, location, llvm::None);
  theModule.addDecoration(*d, targetId);
}

void ModuleBuilder::decorate(uint32_t targetId, spv::Decoration decoration) {
  const Decoration *d = nullptr;
  switch (decoration) {
  case spv::Decoration::Centroid:
    d = Decoration::getCentroid(theContext);
    break;
  case spv::Decoration::Flat:
    d = Decoration::getFlat(theContext);
    break;
  case spv::Decoration::NoPerspective:
    d = Decoration::getNoPerspective(theContext);
    break;
  case spv::Decoration::Sample:
    d = Decoration::getSample(theContext);
    break;
  }

  assert(d && "unimplemented decoration");
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

uint32_t ModuleBuilder::getArrayType(uint32_t elemType, uint32_t count) {
  const Type *type = Type::getArray(theContext, elemType, count);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t ModuleBuilder::getFunctionType(uint32_t returnType,
                                        llvm::ArrayRef<uint32_t> paramTypes) {
  const Type *type = Type::getFunction(theContext, returnType, paramTypes);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t ModuleBuilder::getImageType(uint32_t sampledType, spv::Dim dim,
                                     bool isArray) {
  const Type *type = Type::getImage(theContext, sampledType, dim,
                                    /*depth*/ 0, isArray, /*ms*/ 0,
                                    /*sampled*/ 1, spv::ImageFormat::Unknown);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);

  const char *dimStr = "";
  switch (dim) {
  case spv::Dim::Dim1D:
    dimStr = "1d.";
    break;
  case spv::Dim::Dim2D:
    dimStr = "2d.";
    break;
  case spv::Dim::Dim3D:
    dimStr = "3d.";
    break;
  case spv::Dim::Cube:
    dimStr = "cube.";
    break;
  case spv::Dim::Rect:
    dimStr = "rect.";
    break;
  case spv::Dim::Buffer:
    dimStr = "buffer.";
    break;
  case spv::Dim::SubpassData:
    dimStr = "subpass.";
    break;
  default:
    break;
  }
  std::string name =
      std::string("type.") + dimStr + "image" + (isArray ? ".array" : "");
  theModule.addDebugName(typeId, name);

  return typeId;
}

uint32_t ModuleBuilder::getSamplerType() {
  const Type *type = Type::getSampler(theContext);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  theModule.addDebugName(typeId, "type.sampler");
  return typeId;
}

uint32_t ModuleBuilder::getSampledImageType(uint32_t imageType) {
  const Type *type = Type::getSampledImage(theContext, imageType);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  theModule.addDebugName(typeId, "type.sampled.image");
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
