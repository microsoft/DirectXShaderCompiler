//===--- ModuleBuilder.cpp - SPIR-V builder implementation ----*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/ModuleBuilder.h"

#include "TypeTranslator.h"
#include "spirv/unified1/spirv.hpp11"
#include "clang/SPIRV/BitwiseCast.h"
#include "clang/SPIRV/InstBuilder.h"

namespace clang {
namespace spirv {

ModuleBuilder::ModuleBuilder(SPIRVContext *C, FeatureManager *features,
                             const SpirvCodeGenOptions &opts)
    : theContext(*C), featureManager(features), spirvOptions(opts),
      theModule(opts), theFunction(nullptr), insertPoint(nullptr),
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

uint32_t ModuleBuilder::createCompositeInsert(uint32_t resultType,
                                              uint32_t composite,
                                              llvm::ArrayRef<uint32_t> indices,
                                              uint32_t object) {
  assert(insertPoint && "null insert point");
  const uint32_t resultId = theContext.takeNextId();
  instBuilder
      .opCompositeInsert(resultType, resultId, object, composite, indices)
      .x();
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
  switch (op) {
  case spv::Op::OpImageQuerySize:
  case spv::Op::OpImageQueryLevels:
  case spv::Op::OpImageQuerySamples:
    requireCapability(spv::Capability::ImageQuery);
    break;
  default:
    // Only checking for ImageQueries, the other Ops can be ignored.
    break;
  }
  return id;
}

uint32_t ModuleBuilder::createBinaryOp(spv::Op op, uint32_t resultType,
                                       uint32_t lhs, uint32_t rhs) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder.binaryOp(op, resultType, id, lhs, rhs).x();
  insertPoint->appendInstruction(std::move(constructSite));
  switch (op) {
  case spv::Op::OpImageQueryLod:
  case spv::Op::OpImageQuerySizeLod:
    requireCapability(spv::Capability::ImageQuery);
    break;
  default:
    // Only checking for ImageQueries, the other Ops can be ignored.
    break;
  }
  return id;
}

uint32_t ModuleBuilder::createSpecConstantBinaryOp(spv::Op op,
                                                   uint32_t resultType,
                                                   uint32_t lhs, uint32_t rhs) {
  const uint32_t id = theContext.takeNextId();
  instBuilder.specConstantBinaryOp(op, resultType, id, lhs, rhs).x();
  theModule.addVariable(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createGroupNonUniformOp(spv::Op op, uint32_t resultType,
                                                uint32_t execScope) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder.groupNonUniformOp(op, resultType, id, execScope).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createGroupNonUniformUnaryOp(
    spv::Op op, uint32_t resultType, uint32_t execScope, uint32_t operand,
    llvm::Optional<spv::GroupOperation> groupOp) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder
      .groupNonUniformUnaryOp(op, resultType, id, execScope, groupOp, operand)
      .x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createGroupNonUniformBinaryOp(spv::Op op,
                                                      uint32_t resultType,
                                                      uint32_t execScope,
                                                      uint32_t operand1,
                                                      uint32_t operand2) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder
      .groupNonUniformBinaryOp(op, resultType, id, execScope, operand1,
                               operand2)
      .x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createAtomicOp(spv::Op opcode, uint32_t resultType,
                                       uint32_t orignalValuePtr,
                                       uint32_t scopeId,
                                       uint32_t memorySemanticsId,
                                       uint32_t valueToOp) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder
      .atomicOp(opcode, resultType, id, orignalValuePtr, scopeId,
                memorySemanticsId, valueToOp)
      .x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createAtomicCompareExchange(
    uint32_t resultType, uint32_t orignalValuePtr, uint32_t scopeId,
    uint32_t equalMemorySemanticsId, uint32_t unequalMemorySemanticsId,
    uint32_t valueToOp, uint32_t comparator) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder.opAtomicCompareExchange(
      resultType, id, orignalValuePtr, scopeId, equalMemorySemanticsId,
      unequalMemorySemanticsId, valueToOp, comparator);
  instBuilder.x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

spv::ImageOperandsMask ModuleBuilder::composeImageOperandsMask(
    uint32_t bias, uint32_t lod, const std::pair<uint32_t, uint32_t> &grad,
    uint32_t constOffset, uint32_t varOffset, uint32_t constOffsets,
    uint32_t sample, uint32_t minLod,
    llvm::SmallVectorImpl<uint32_t> *orderedParams) {
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

  if (varOffset) {
    mask = mask | ImageOperandsMask::Offset;
    requireCapability(spv::Capability::ImageGatherExtended);
    orderedParams->push_back(varOffset);
  }

  if (constOffsets) {
    mask = mask | ImageOperandsMask::ConstOffsets;
    requireCapability(spv::Capability::ImageGatherExtended);
    orderedParams->push_back(constOffsets);
  }

  if (sample) {
    mask = mask | ImageOperandsMask::Sample;
    orderedParams->push_back(sample);
  }

  if (minLod) {
    requireCapability(spv::Capability::MinLod);
    mask = mask | ImageOperandsMask::MinLod;
    orderedParams->push_back(minLod);
  }

  return mask;
}

uint32_t
ModuleBuilder::createImageSparseTexelsResident(uint32_t resident_code) {
  assert(insertPoint && "null insert point");
  // Result type must be a boolean
  const uint32_t result_type = getBoolType();
  const uint32_t id = theContext.takeNextId();
  instBuilder.opImageSparseTexelsResident(result_type, id, resident_code).x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createImageTexelPointer(uint32_t resultType,
                                                uint32_t imageId,
                                                uint32_t coordinate,
                                                uint32_t sample) {
  assert(insertPoint && "null insert point");
  const uint32_t id = theContext.takeNextId();
  instBuilder.opImageTexelPointer(resultType, id, imageId, coordinate, sample)
      .x();
  insertPoint->appendInstruction(std::move(constructSite));
  return id;
}

uint32_t ModuleBuilder::createImageSample(
    uint32_t texelType, uint32_t imageType, uint32_t image, uint32_t sampler,
    bool isNonUniform, uint32_t coordinate, uint32_t compareVal, uint32_t bias,
    uint32_t lod, std::pair<uint32_t, uint32_t> grad, uint32_t constOffset,
    uint32_t varOffset, uint32_t constOffsets, uint32_t sample, uint32_t minLod,
    uint32_t residencyCodeId) {
  assert(insertPoint && "null insert point");

  // The Lod and Grad image operands requires explicit-lod instructions.
  // Otherwise we use implicit-lod instructions.
  const bool isExplicit = lod || (grad.first && grad.second);
  const bool isSparse = (residencyCodeId != 0);

  // minLod is only valid with Implicit instructions and Grad instructions.
  // This means that we cannot have Lod and minLod together because Lod requires
  // explicit insturctions. So either lod or minLod or both must be zero.
  assert(lod == 0 || minLod == 0);

  uint32_t retType = texelType;
  if (isSparse) {
    requireCapability(spv::Capability::SparseResidency);
    retType = getSparseResidencyStructType(texelType);
  }

  // An OpSampledImage is required to do the image sampling.
  const uint32_t sampledImgId = theContext.takeNextId();
  const uint32_t sampledImgTy = getSampledImageType(imageType);
  instBuilder.opSampledImage(sampledImgTy, sampledImgId, image, sampler).x();
  insertPoint->appendInstruction(std::move(constructSite));

  if (isNonUniform) {
    // The sampled image will be used to access resource's memory, so we need
    // to decorate it with NonUniformEXT.
    decorateNonUniformEXT(sampledImgId);
  }

  uint32_t texelId = theContext.takeNextId();
  llvm::SmallVector<uint32_t, 4> params;
  const auto mask =
      composeImageOperandsMask(bias, lod, grad, constOffset, varOffset,
                               constOffsets, sample, minLod, &params);

  instBuilder.opImageSample(retType, texelId, sampledImgId, coordinate,
                            compareVal, mask, isExplicit, isSparse);

  for (const auto param : params)
    instBuilder.idRef(param);
  instBuilder.x();
  insertPoint->appendInstruction(std::move(constructSite));

  if (isSparse) {
    // Write the Residency Code
    const auto status = createCompositeExtract(getUint32Type(), texelId, {0});
    createStore(residencyCodeId, status);
    // Extract the real result from the struct
    texelId = createCompositeExtract(texelType, texelId, {1});
  }

  return texelId;
}

void ModuleBuilder::createImageWrite(QualType imageType, uint32_t imageId,
                                     uint32_t coordId, uint32_t texelId) {
  assert(insertPoint && "null insert point");
  requireCapability(
      TypeTranslator::getCapabilityForStorageImageReadWrite(imageType));
  instBuilder.opImageWrite(imageId, coordId, texelId, llvm::None).x();
  insertPoint->appendInstruction(std::move(constructSite));
}

uint32_t ModuleBuilder::createImageFetchOrRead(
    bool doImageFetch, uint32_t texelType, QualType imageType, uint32_t image,
    uint32_t coordinate, uint32_t lod, uint32_t constOffset, uint32_t varOffset,
    uint32_t constOffsets, uint32_t sample, uint32_t residencyCodeId) {
  assert(insertPoint && "null insert point");

  llvm::SmallVector<uint32_t, 2> params;
  const auto mask =
      llvm::Optional<spv::ImageOperandsMask>(composeImageOperandsMask(
          /*bias*/ 0, lod, std::make_pair(0, 0), constOffset, varOffset,
          constOffsets, sample, /*minLod*/ 0, &params));

  const bool isSparse = (residencyCodeId != 0);
  uint32_t retType = texelType;
  if (isSparse) {
    requireCapability(spv::Capability::SparseResidency);
    retType = getSparseResidencyStructType(texelType);
  }

  if (!doImageFetch) {
    requireCapability(
        TypeTranslator::getCapabilityForStorageImageReadWrite(imageType));
  }

  uint32_t texelId = theContext.takeNextId();
  instBuilder.opImageFetchRead(retType, texelId, image, coordinate, mask,
                               doImageFetch, isSparse);

  for (const auto param : params)
    instBuilder.idRef(param);
  instBuilder.x();
  insertPoint->appendInstruction(std::move(constructSite));

  if (isSparse) {
    // Write the Residency Code
    const auto status = createCompositeExtract(getUint32Type(), texelId, {0});
    createStore(residencyCodeId, status);
    // Extract the real result from the struct
    texelId = createCompositeExtract(texelType, texelId, {1});
  }

  return texelId;
}

uint32_t ModuleBuilder::createImageGather(
    uint32_t texelType, uint32_t imageType, uint32_t image, uint32_t sampler,
    bool isNonUniform, uint32_t coordinate, uint32_t component,
    uint32_t compareVal, uint32_t constOffset, uint32_t varOffset,
    uint32_t constOffsets, uint32_t sample, uint32_t residencyCodeId) {
  assert(insertPoint && "null insert point");

  uint32_t sparseRetType = 0;
  if (residencyCodeId) {
    requireCapability(spv::Capability::SparseResidency);
    sparseRetType = getSparseResidencyStructType(texelType);
  }

  // An OpSampledImage is required to do the image sampling.
  const uint32_t sampledImgId = theContext.takeNextId();
  const uint32_t sampledImgTy = getSampledImageType(imageType);
  instBuilder.opSampledImage(sampledImgTy, sampledImgId, image, sampler).x();
  insertPoint->appendInstruction(std::move(constructSite));

  if (isNonUniform) {
    // The sampled image will be used to access resource's memory, so we need
    // to decorate it with NonUniformEXT.
    decorateNonUniformEXT(sampledImgId);
  }

  llvm::SmallVector<uint32_t, 2> params;

  // TODO: Update ImageGather to accept minLod if necessary.
  const auto mask =
      llvm::Optional<spv::ImageOperandsMask>(composeImageOperandsMask(
          /*bias*/ 0, /*lod*/ 0, std::make_pair(0, 0), constOffset, varOffset,
          constOffsets, sample, /*minLod*/ 0, &params));
  uint32_t texelId = theContext.takeNextId();

  if (compareVal) {
    if (residencyCodeId) {
      // Note: OpImageSparseDrefGather does not take the component parameter.
      instBuilder.opImageSparseDrefGather(sparseRetType, texelId, sampledImgId,
                                          coordinate, compareVal, mask);
    } else {
      // Note: OpImageDrefGather does not take the component parameter.
      instBuilder.opImageDrefGather(texelType, texelId, sampledImgId,
                                    coordinate, compareVal, mask);
    }
  } else {
    if (residencyCodeId) {
      instBuilder.opImageSparseGather(sparseRetType, texelId, sampledImgId,
                                      coordinate, component, mask);
    } else {
      instBuilder.opImageGather(texelType, texelId, sampledImgId, coordinate,
                                component, mask);
    }
  }

  for (const auto param : params)
    instBuilder.idRef(param);
  instBuilder.x();
  insertPoint->appendInstruction(std::move(constructSite));

  if (residencyCodeId) {
    // Write the Residency Code
    const auto status = createCompositeExtract(getUint32Type(), texelId, {0});
    createStore(residencyCodeId, status);
    // Extract the real result from the struct
    texelId = createCompositeExtract(texelType, texelId, {1});
  }

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

void ModuleBuilder::createBarrier(uint32_t execution, uint32_t memory,
                                  uint32_t semantics) {
  assert(insertPoint && "null insert point");
  if (execution)
    instBuilder.opControlBarrier(execution, memory, semantics).x();
  else
    instBuilder.opMemoryBarrier(memory, semantics).x();
  insertPoint->appendInstruction(std::move(constructSite));
}

uint32_t ModuleBuilder::createBitFieldExtract(uint32_t resultType,
                                              uint32_t base, uint32_t offset,
                                              uint32_t count, bool isSigned) {
  assert(insertPoint && "null insert point");
  uint32_t resultId = theContext.takeNextId();
  if (isSigned)
    instBuilder.opBitFieldSExtract(resultType, resultId, base, offset, count);
  else
    instBuilder.opBitFieldUExtract(resultType, resultId, base, offset, count);
  instBuilder.x();
  insertPoint->appendInstruction(std::move(constructSite));
  return resultId;
}

uint32_t ModuleBuilder::createBitFieldInsert(uint32_t resultType, uint32_t base,
                                             uint32_t insert, uint32_t offset,
                                             uint32_t count) {
  assert(insertPoint && "null insert point");
  uint32_t resultId = theContext.takeNextId();
  instBuilder
      .opBitFieldInsert(resultType, resultId, base, insert, offset, count)
      .x();
  insertPoint->appendInstruction(std::move(constructSite));
  return resultId;
}

void ModuleBuilder::createEmitVertex() {
  assert(insertPoint && "null insert point");
  instBuilder.opEmitVertex().x();
  insertPoint->appendInstruction(std::move(constructSite));
}

void ModuleBuilder::createEndPrimitive() {
  assert(insertPoint && "null insert point");
  instBuilder.opEndPrimitive().x();
  insertPoint->appendInstruction(std::move(constructSite));
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

void ModuleBuilder::addExtension(Extension ext, llvm::StringRef target,
                                 SourceLocation srcLoc) {
  assert(featureManager);
  featureManager->requestExtension(ext, target, srcLoc);
  // Do not emit OpExtension if the given extension is natively supported in the
  // target environment.
  if (featureManager->isExtensionRequiredForTargetEnv(ext))
    theModule.addExtension(featureManager->getExtensionName(ext));
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
  auto found =
      std::find_if(builtinVars.begin(), builtinVars.end(),
                   [sc, builtin](const BuiltInVarInfo &varInfo) {
                     return varInfo.sc == sc && varInfo.builtIn == builtin;
                   });
  if (found != builtinVars.end()) {
    return found->variable;
  }

  const uint32_t pointerType = getPointerType(type, sc);
  const uint32_t varId = theContext.takeNextId();
  instBuilder.opVariable(pointerType, varId, sc, llvm::None).x();
  theModule.addVariable(std::move(constructSite));

  // Decorate with the specified Builtin
  const Decoration *d = Decoration::getBuiltIn(theContext, builtin);
  theModule.addDecoration(d, varId);

  builtinVars.emplace_back(sc, builtin, varId);

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
  theModule.addDecoration(d, targetId);

  d = Decoration::getBinding(theContext, bindingNumber);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateInputAttachmentIndex(uint32_t targetId,
                                                 uint32_t indexNumber) {
  const auto *d = Decoration::getInputAttachmentIndex(theContext, indexNumber);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateCounterBufferId(uint32_t mainBufferId,
                                            uint32_t counterBufferId) {
  if (spirvOptions.enableReflect) {
    addExtension(Extension::GOOGLE_hlsl_functionality1, "SPIR-V reflection",
                 {});
    theModule.addDecoration(
        Decoration::getHlslCounterBufferGOOGLE(theContext, counterBufferId),
        mainBufferId);
  }
}

void ModuleBuilder::decorateHlslSemantic(uint32_t targetId,
                                         llvm::StringRef semantic,
                                         llvm::Optional<uint32_t> memberIdx) {
  if (spirvOptions.enableReflect) {
    addExtension(Extension::GOOGLE_hlsl_functionality1, "SPIR-V reflection",
                 {});
    theModule.addDecoration(
        Decoration::getHlslSemanticGOOGLE(theContext, semantic, memberIdx),
        targetId);
  }
}

void ModuleBuilder::decorateLocation(uint32_t targetId, uint32_t location) {
  const Decoration *d =
      Decoration::getLocation(theContext, location, llvm::None);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateIndex(uint32_t targetId, uint32_t index) {
  const Decoration *d = Decoration::getIndex(theContext, index);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateSpecId(uint32_t targetId, uint32_t specId) {
  const Decoration *d = Decoration::getSpecId(theContext, specId);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateCentroid(uint32_t targetId) {
  const Decoration *d = Decoration::getCentroid(theContext);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateFlat(uint32_t targetId) {
  const Decoration *d = Decoration::getFlat(theContext);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateNoPerspective(uint32_t targetId) {
  const Decoration *d = Decoration::getNoPerspective(theContext);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateSample(uint32_t targetId) {
  const Decoration *d = Decoration::getSample(theContext);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateBlock(uint32_t targetId) {
  const Decoration *d = Decoration::getBlock(theContext);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateRelaxedPrecision(uint32_t targetId) {
  const Decoration *d = Decoration::getRelaxedPrecision(theContext);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decoratePatch(uint32_t targetId) {
  const Decoration *d = Decoration::getPatch(theContext);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateNonUniformEXT(uint32_t targetId) {
  const Decoration *d = Decoration::getNonUniformEXT(theContext);
  theModule.addDecoration(d, targetId);
}

void ModuleBuilder::decorateNoContraction(uint32_t targetId) {
  const Decoration *d = Decoration::getNoContraction(theContext);
  theModule.addDecoration(d, targetId);
}

#define IMPL_GET_PRIMITIVE_TYPE(ty)                                            \
                                                                               \
  uint32_t ModuleBuilder::get##ty##Type() {                                    \
    const Type *type = Type::get##ty(theContext);                              \
    const uint32_t typeId = theContext.getResultIdForType(type);               \
    theModule.addType(type, typeId);                                           \
    return typeId;                                                             \
  }

IMPL_GET_PRIMITIVE_TYPE(Void)
IMPL_GET_PRIMITIVE_TYPE(Bool)
IMPL_GET_PRIMITIVE_TYPE(Int32)
IMPL_GET_PRIMITIVE_TYPE(Uint32)
IMPL_GET_PRIMITIVE_TYPE(Float32)

#undef IMPL_GET_PRIMITIVE_TYPE

// Note: At the moment, Float16 capability should not be added for Vulkan 1.0.
// It is not a required capability, and adding the SPV_AMD_gpu_half_float does
// not enable this capability. Any driver that supports float16 in Vulkan 1.0
// should accept this extension.
#define IMPL_GET_PRIMITIVE_TYPE_WITH_CAPABILITY(ty, cap)                       \
                                                                               \
  uint32_t ModuleBuilder::get##ty##Type() {                                    \
    if (spv::Capability::cap == spv::Capability::Float16)                      \
      addExtension(Extension::AMD_gpu_shader_half_float, "16-bit float", {});  \
    else                                                                       \
      requireCapability(spv::Capability::cap);                                 \
    const Type *type = Type::get##ty(theContext);                              \
    const uint32_t typeId = theContext.getResultIdForType(type);               \
    theModule.addType(type, typeId);                                           \
    return typeId;                                                             \
  }

IMPL_GET_PRIMITIVE_TYPE_WITH_CAPABILITY(Int64, Int64)
IMPL_GET_PRIMITIVE_TYPE_WITH_CAPABILITY(Uint64, Int64)
IMPL_GET_PRIMITIVE_TYPE_WITH_CAPABILITY(Float64, Float64)
IMPL_GET_PRIMITIVE_TYPE_WITH_CAPABILITY(Int16, Int16)
IMPL_GET_PRIMITIVE_TYPE_WITH_CAPABILITY(Uint16, Int16)
IMPL_GET_PRIMITIVE_TYPE_WITH_CAPABILITY(Float16, Float16)

#undef IMPL_GET_PRIMITIVE_TYPE_WITH_CAPABILITY

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

uint32_t ModuleBuilder::getMatType(QualType elemType, uint32_t colType,
                                   uint32_t colCount,
                                   Type::DecorationSet decorations) {
  // NOTE: According to Item "Data rules" of SPIR-V Spec 2.16.1 "Universal
  // Validation Rules":
  //   Matrix types can only be parameterized with floating-point types.
  //
  // So we need special handling of non-fp matrices. We emulate non-fp
  // matrices as an array of vectors.
  if (!elemType->isFloatingType())
    return getArrayType(colType, getConstantUint32(colCount), decorations);

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
                             llvm::ArrayRef<llvm::StringRef> fieldNames,
                             Type::DecorationSet decorations) {
  const Type *type =
      Type::getStruct(theContext, fieldTypes, structName, decorations);
  bool isRegistered = false;
  const uint32_t typeId = theContext.getResultIdForType(type, &isRegistered);
  theModule.addType(type, typeId);
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

uint32_t ModuleBuilder::getSparseResidencyStructType(uint32_t type) {
  const auto uintType = getUint32Type();
  return getStructType({uintType, type}, "SparseResidencyStruct",
                       {"Residency.Code", "Result.Type"});
}

uint32_t ModuleBuilder::getArrayType(uint32_t elemType, uint32_t count,
                                     Type::DecorationSet decorations) {
  const Type *type = Type::getArray(theContext, elemType, count, decorations);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  return typeId;
}

uint32_t ModuleBuilder::getRuntimeArrayType(uint32_t elemType,
                                            Type::DecorationSet decorations) {
  const Type *type = Type::getRuntimeArray(theContext, elemType, decorations);
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
                                     uint32_t depth, bool isArray, uint32_t ms,
                                     uint32_t sampled,
                                     spv::ImageFormat format) {
  const Type *type = Type::getImage(theContext, sampledType, dim, depth,
                                    isArray, ms, sampled, format);
  bool isRegistered = false;
  const uint32_t typeId = theContext.getResultIdForType(type, &isRegistered);
  theModule.addType(type, typeId);

  switch (format) {
  case spv::ImageFormat::Rg32f:
  case spv::ImageFormat::Rg16f:
  case spv::ImageFormat::R11fG11fB10f:
  case spv::ImageFormat::R16f:
  case spv::ImageFormat::Rgba16:
  case spv::ImageFormat::Rgb10A2:
  case spv::ImageFormat::Rg16:
  case spv::ImageFormat::Rg8:
  case spv::ImageFormat::R16:
  case spv::ImageFormat::R8:
  case spv::ImageFormat::Rgba16Snorm:
  case spv::ImageFormat::Rg16Snorm:
  case spv::ImageFormat::Rg8Snorm:
  case spv::ImageFormat::R16Snorm:
  case spv::ImageFormat::R8Snorm:
  case spv::ImageFormat::Rg32i:
  case spv::ImageFormat::Rg16i:
  case spv::ImageFormat::Rg8i:
  case spv::ImageFormat::R16i:
  case spv::ImageFormat::R8i:
  case spv::ImageFormat::Rgb10a2ui:
  case spv::ImageFormat::Rg32ui:
  case spv::ImageFormat::Rg16ui:
  case spv::ImageFormat::Rg8ui:
  case spv::ImageFormat::R16ui:
  case spv::ImageFormat::R8ui:
    requireCapability(spv::Capability::StorageImageExtendedFormats);
    break;
  default:
    // Only image formats requiring extended formats are relevant. The rest just
    // pass through.
    break;
  }

  if (dim == spv::Dim::Dim1D) {
    if (sampled == 2u) {
      requireCapability(spv::Capability::Image1D);
    } else {
      requireCapability(spv::Capability::Sampled1D);
    }
  } else if (dim == spv::Dim::Buffer) {
    requireCapability(spv::Capability::SampledBuffer);
  } else if (dim == spv::Dim::SubpassData) {
    requireCapability(spv::Capability::InputAttachment);
  }

  if (isArray && ms) {
    requireCapability(spv::Capability::ImageMSArray);
  }

  // Skip constructing the debug name if we have already done it before.
  if (!isRegistered) {
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
  }

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

uint32_t ModuleBuilder::getByteAddressBufferType(bool isRW) {
  // Create a uint RuntimeArray with Array Stride of 4.
  const uint32_t uintType = getUint32Type();
  const auto *arrStride4 = Decoration::getArrayStride(theContext, 4u);
  const Type *raType =
      Type::getRuntimeArray(theContext, uintType, {arrStride4});
  const uint32_t raTypeId = theContext.getResultIdForType(raType);
  theModule.addType(raType, raTypeId);

  // Create a struct containing the runtime array as its only member.
  // The struct must also be decorated as BufferBlock. The offset decoration
  // should also be applied to the first (only) member. NonWritable decoration
  // should also be applied to the first member if isRW is true.
  llvm::SmallVector<const Decoration *, 3> typeDecs;
  typeDecs.push_back(Decoration::getBufferBlock(theContext));
  typeDecs.push_back(Decoration::getOffset(theContext, 0, 0));
  if (!isRW)
    typeDecs.push_back(Decoration::getNonWritable(theContext, 0));

  const Type *type = Type::getStruct(theContext, {raTypeId}, "", typeDecs);
  const uint32_t typeId = theContext.getResultIdForType(type);
  theModule.addType(type, typeId);
  theModule.addDebugName(typeId, isRW ? "type.RWByteAddressBuffer"
                                      : "type.ByteAddressBuffer");
  return typeId;
}

uint32_t ModuleBuilder::getConstantBool(bool value, bool isSpecConst) {
  if (isSpecConst) {
    const uint32_t constId = theContext.takeNextId();
    if (value) {
      instBuilder.opSpecConstantTrue(getBoolType(), constId).x();
    } else {
      instBuilder.opSpecConstantFalse(getBoolType(), constId).x();
    }

    theModule.addVariable(std::move(constructSite));
    return constId;
  }

  const uint32_t typeId = getBoolType();
  const Constant *constant = value ? Constant::getTrue(theContext, typeId)
                                   : Constant::getFalse(theContext, typeId);

  const uint32_t constId = theContext.getResultIdForConstant(constant);
  theModule.addConstant(constant, constId);
  return constId;
}

#define IMPL_GET_PRIMITIVE_CONST(builderTy, cppTy)                             \
                                                                               \
  uint32_t ModuleBuilder::getConstant##builderTy(cppTy value) {                \
    const uint32_t typeId = get##builderTy##Type();                            \
    const Constant *constant =                                                 \
        Constant::get##builderTy(theContext, typeId, value);                   \
    const uint32_t constId = theContext.getResultIdForConstant(constant);      \
    theModule.addConstant(constant, constId);                                  \
    return constId;                                                            \
  }

#define IMPL_GET_PRIMITIVE_CONST_SPEC_CONST(builderTy, cppTy)                  \
                                                                               \
  uint32_t ModuleBuilder::getConstant##builderTy(cppTy value,                  \
                                                 bool isSpecConst) {           \
    if (isSpecConst) {                                                         \
      const uint32_t constId = theContext.takeNextId();                        \
      instBuilder                                                              \
          .opSpecConstant(get##builderTy##Type(), constId,                     \
                          cast::BitwiseCast<uint32_t>(value))                  \
          .x();                                                                \
      theModule.addVariable(std::move(constructSite));                         \
      return constId;                                                          \
    }                                                                          \
                                                                               \
    const uint32_t typeId = get##builderTy##Type();                            \
    const Constant *constant =                                                 \
        Constant::get##builderTy(theContext, typeId, value);                   \
    const uint32_t constId = theContext.getResultIdForConstant(constant);      \
    theModule.addConstant(constant, constId);                                  \
    return constId;                                                            \
  }

IMPL_GET_PRIMITIVE_CONST(Int16, int16_t)
IMPL_GET_PRIMITIVE_CONST_SPEC_CONST(Int32, int32_t)
IMPL_GET_PRIMITIVE_CONST(Uint16, uint16_t)
IMPL_GET_PRIMITIVE_CONST_SPEC_CONST(Uint32, uint32_t)
IMPL_GET_PRIMITIVE_CONST(Float16, int16_t)
IMPL_GET_PRIMITIVE_CONST_SPEC_CONST(Float32, float)
IMPL_GET_PRIMITIVE_CONST(Float64, double)
IMPL_GET_PRIMITIVE_CONST(Int64, int64_t)
IMPL_GET_PRIMITIVE_CONST(Uint64, uint64_t)

#undef IMPL_GET_PRIMITIVE_CONST
#undef IMPL_GET_PRIMITIVE_CONST_SPEC_CONST

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

void ModuleBuilder::debugLine(uint32_t file, uint32_t line, uint32_t column) {
  instBuilder.opLine(file, line, column).x();
  insertPoint->appendInstruction(std::move(constructSite));
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
