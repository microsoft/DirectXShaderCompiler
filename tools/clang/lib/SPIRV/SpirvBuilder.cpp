//===--- SpirvBuilder.cpp - SPIR-V Builder Implementation --------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvBuilder.h"
#include "CapabilityVisitor.h"
#include "LiteralTypeVisitor.h"
#include "TypeTranslator.h"
#include "clang/SPIRV/EmitVisitor.h"
#include "clang/SPIRV/LowerTypeVisitor.h"

namespace clang {
namespace spirv {

SpirvBuilder::SpirvBuilder(ASTContext &ac, SpirvContext &ctx,
                           FeatureManager *fm, const SpirvCodeGenOptions &opt)
    : astContext(ac), context(ctx), module(nullptr), function(nullptr),
      featureManager(fm), spirvOptions(opt) {
  module = new (context) SpirvModule;
}

SpirvFunction *SpirvBuilder::beginFunction(QualType returnType,
                                           SpirvType *functionType,
                                           SourceLocation loc,
                                           llvm::StringRef funcName,
                                           SpirvFunction *func) {
  assert(!function && "found nested function");
  if (func) {
    function = func;
    function->setAstReturnType(returnType);
    function->setFunctionType(functionType);
    function->setSourceLocation(loc);
    function->setFunctionName(funcName);
  } else {
    function = new (context)
        SpirvFunction(returnType, functionType, /*id*/ 0,
                      spv::FunctionControlMask::MaskNone, loc, funcName);
  }

  return function;
}

SpirvFunctionParameter *SpirvBuilder::addFnParam(QualType ptrType,
                                                 SourceLocation loc,
                                                 llvm::StringRef name) {
  assert(function && "found detached parameter");
  auto *param = new (context) SpirvFunctionParameter(ptrType, /*id*/ 0, loc);
  param->setStorageClass(spv::StorageClass::Function);
  param->setDebugName(name);
  function->addParameter(param);
  return param;
}

SpirvFunctionParameter *SpirvBuilder::addFnParam(const SpirvType *ptrType,
                                                 SourceLocation loc,
                                                 llvm::StringRef name) {
  assert(function && "found detached parameter");
  auto *param =
      new (context) SpirvFunctionParameter(/*QualType*/ {}, /*id*/ 0, loc);
  param->setResultType(ptrType);
  param->setDebugName(name);
  function->addParameter(param);
  return param;
}

SpirvVariable *SpirvBuilder::addFnVar(QualType valueType, SourceLocation loc,
                                      llvm::StringRef name,
                                      SpirvInstruction *init) {
  assert(function && "found detached local variable");
  auto *var = new (context) SpirvVariable(valueType, /*id*/ 0, loc,
                                          spv::StorageClass::Function, init);
  var->setDebugName(name);
  function->addVariable(var);
  return var;
}

void SpirvBuilder::endFunction() {
  assert(function && "no active function");

  // Move all basic blocks into the current function.
  // TODO: we should adjust the order the basic blocks according to
  // SPIR-V validation rules.
  for (auto *bb : basicBlocks) {
    function->addBasicBlock(bb);
  }
  basicBlocks.clear();

  module->addFunction(function);
  function = nullptr;
  insertPoint = nullptr;
}

SpirvBasicBlock *SpirvBuilder::createBasicBlock(llvm::StringRef name) {
  assert(function && "found detached basic block");
  auto *bb = new (context) SpirvBasicBlock(/*id*/ 0, name);
  basicBlocks.push_back(bb);
  return bb;
}

void SpirvBuilder::addSuccessor(SpirvBasicBlock *successorBB) {
  assert(insertPoint && "null insert point");
  insertPoint->addSuccessor(successorBB);
}

void SpirvBuilder::setMergeTarget(SpirvBasicBlock *mergeLabel) {
  assert(insertPoint && "null insert point");
  insertPoint->setMergeTarget(mergeLabel);
}

void SpirvBuilder::setContinueTarget(SpirvBasicBlock *continueLabel) {
  assert(insertPoint && "null insert point");
  insertPoint->setContinueTarget(continueLabel);
}

SpirvComposite *SpirvBuilder::createCompositeConstruct(
    QualType resultType, llvm::ArrayRef<SpirvInstruction *> constituents,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvComposite(resultType, /*id*/ 0, loc, constituents);
  insertPoint->addInstruction(instruction);
  if (!constituents.empty()) {
    instruction->setLayoutRule(constituents[0]->getLayoutRule());
  }
  return instruction;
}

SpirvComposite *SpirvBuilder::createCompositeConstruct(
    const SpirvType *resultType,
    llvm::ArrayRef<SpirvInstruction *> constituents, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvComposite(/*QualType*/ {}, /*id*/ 0, loc, constituents);
  instruction->setResultType(resultType);
  if (!constituents.empty()) {
    instruction->setLayoutRule(constituents[0]->getLayoutRule());
  }
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvCompositeExtract *SpirvBuilder::createCompositeExtract(
    QualType resultType, SpirvInstruction *composite,
    llvm::ArrayRef<uint32_t> indexes, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvCompositeExtract(resultType, /*id*/ 0, loc, composite, indexes);
  instruction->setRValue();
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvCompositeInsert *SpirvBuilder::createCompositeInsert(
    QualType resultType, SpirvInstruction *composite,
    llvm::ArrayRef<uint32_t> indices, SpirvInstruction *object,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvCompositeInsert(
      resultType, /*id*/ 0, loc, composite, object, indices);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvVectorShuffle *SpirvBuilder::createVectorShuffle(
    QualType resultType, SpirvInstruction *vector1, SpirvInstruction *vector2,
    llvm::ArrayRef<uint32_t> selectors, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvVectorShuffle(
      resultType, /*id*/ 0, loc, vector1, vector2, selectors);
  instruction->setRValue();
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvLoad *SpirvBuilder::createLoad(QualType resultType,
                                    SpirvInstruction *pointer,
                                    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvLoad(resultType, /*id*/ 0, loc, pointer);
  instruction->setStorageClass(pointer->getStorageClass());
  instruction->setRValue();
  instruction->setLayoutRule(pointer->getLayoutRule());
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvLoad *SpirvBuilder::createLoad(const SpirvType *resultType,
                                    SpirvInstruction *pointer,
                                    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvLoad(/*QualType*/ {}, /*id*/ 0, loc, pointer);
  instruction->setResultType(resultType);
  instruction->setStorageClass(pointer->getStorageClass());
  instruction->setRValue();
  instruction->setLayoutRule(pointer->getLayoutRule());
  insertPoint->addInstruction(instruction);
  return instruction;
}

void SpirvBuilder::createStore(SpirvInstruction *address,
                               SpirvInstruction *value, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvStore(loc, address, value);
  insertPoint->addInstruction(instruction);
}

SpirvFunctionCall *
SpirvBuilder::createFunctionCall(QualType returnType, SpirvFunction *func,
                                 llvm::ArrayRef<SpirvInstruction *> params,
                                 SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvFunctionCall(returnType, /*id*/ 0, loc, func, params);
  instruction->setRValue();
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvAccessChain *
SpirvBuilder::createAccessChain(QualType resultType, SpirvInstruction *base,
                                llvm::ArrayRef<SpirvInstruction *> indexes,
                                SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvAccessChain(resultType, /*id*/ 0, loc, base, indexes);
  instruction->setStorageClass(base->getStorageClass());
  instruction->setLayoutRule(base->getLayoutRule());
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvAccessChain *SpirvBuilder::createAccessChain(
    const SpirvType *resultType, SpirvInstruction *base,
    llvm::ArrayRef<SpirvInstruction *> indexes, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvAccessChain(/*QualType*/ {}, /*id*/ 0, loc, base, indexes);
  instruction->setResultType(resultType);
  instruction->setStorageClass(base->getStorageClass());
  instruction->setLayoutRule(base->getLayoutRule());
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvUnaryOp *SpirvBuilder::createUnaryOp(spv::Op op, QualType resultType,
                                          SpirvInstruction *operand,
                                          SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvUnaryOp(op, resultType, /*id*/ 0, loc, operand);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvBinaryOp *SpirvBuilder::createBinaryOp(spv::Op op, QualType resultType,
                                            SpirvInstruction *lhs,
                                            SpirvInstruction *rhs,
                                            SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvBinaryOp(op, resultType, /*id*/ 0, loc, lhs, rhs);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvBinaryOp *SpirvBuilder::createBinaryOp(spv::Op op,
                                            const SpirvType *resultType,
                                            SpirvInstruction *lhs,
                                            SpirvInstruction *rhs,
                                            SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvBinaryOp(op, /*QualType*/ {}, /*id*/ 0, loc, lhs, rhs);
  instruction->setResultType(resultType);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvSpecConstantBinaryOp *SpirvBuilder::createSpecConstantBinaryOp(
    spv::Op op, QualType resultType, SpirvInstruction *lhs,
    SpirvInstruction *rhs, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvSpecConstantBinaryOp(op, resultType, /*id*/ 0, loc, lhs, rhs);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvNonUniformElect *SpirvBuilder::createGroupNonUniformElect(
    spv::Op op, QualType resultType, spv::Scope execScope, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvNonUniformElect(resultType, /*id*/ 0, loc, execScope);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvNonUniformUnaryOp *SpirvBuilder::createGroupNonUniformUnaryOp(
    spv::Op op, QualType resultType, spv::Scope execScope,
    SpirvInstruction *operand, llvm::Optional<spv::GroupOperation> groupOp,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvNonUniformUnaryOp(
      op, resultType, /*id*/ 0, loc, execScope, groupOp, operand);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvNonUniformBinaryOp *SpirvBuilder::createGroupNonUniformBinaryOp(
    spv::Op op, QualType resultType, spv::Scope execScope,
    SpirvInstruction *operand1, SpirvInstruction *operand2,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvNonUniformBinaryOp(
      op, resultType, /*id*/ 0, loc, execScope, operand1, operand2);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvAtomic *SpirvBuilder::createAtomicOp(
    spv::Op opcode, QualType resultType, SpirvInstruction *originalValuePtr,
    spv::Scope scope, spv::MemorySemanticsMask memorySemantics,
    SpirvInstruction *valueToOp, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvAtomic(opcode, resultType, /*id*/ 0, loc, originalValuePtr, scope,
                  memorySemantics, valueToOp);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvAtomic *SpirvBuilder::createAtomicCompareExchange(
    QualType resultType, SpirvInstruction *originalValuePtr, spv::Scope scope,
    spv::MemorySemanticsMask equalMemorySemantics,
    spv::MemorySemanticsMask unequalMemorySemantics,
    SpirvInstruction *valueToOp, SpirvInstruction *comparator,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvAtomic(spv::Op::OpAtomicCompareExchange, resultType, /*id*/ 0, loc,
                  originalValuePtr, scope, equalMemorySemantics,
                  unequalMemorySemantics, valueToOp, comparator);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvImageTexelPointer *SpirvBuilder::createImageTexelPointer(
    QualType resultType, SpirvInstruction *image, SpirvInstruction *coordinate,
    SpirvInstruction *sample, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvImageTexelPointer(
      resultType, /*id*/ 0, loc, image, coordinate, sample);
  insertPoint->addInstruction(instruction);
  return instruction;
}

spv::ImageOperandsMask SpirvBuilder::composeImageOperandsMask(
    SpirvInstruction *bias, SpirvInstruction *lod,
    const std::pair<SpirvInstruction *, SpirvInstruction *> &grad,
    SpirvInstruction *constOffset, SpirvInstruction *varOffset,
    SpirvInstruction *constOffsets, SpirvInstruction *sample,
    SpirvInstruction *minLod) {
  using spv::ImageOperandsMask;
  // SPIR-V Image Operands from least significant bit to most significant bit
  // Bias, Lod, Grad, ConstOffset, Offset, ConstOffsets, Sample, MinLod

  auto mask = ImageOperandsMask::MaskNone;
  if (bias) {
    mask = mask | ImageOperandsMask::Bias;
  }
  if (lod) {
    mask = mask | ImageOperandsMask::Lod;
  }
  if (grad.first && grad.second) {
    mask = mask | ImageOperandsMask::Grad;
  }
  if (constOffset) {
    mask = mask | ImageOperandsMask::ConstOffset;
  }
  if (varOffset) {
    mask = mask | ImageOperandsMask::Offset;
  }
  if (constOffsets) {
    mask = mask | ImageOperandsMask::ConstOffsets;
  }
  if (sample) {
    mask = mask | ImageOperandsMask::Sample;
  }
  if (minLod) {
    mask = mask | ImageOperandsMask::MinLod;
  }
  return mask;
}

SpirvInstruction *SpirvBuilder::createImageSample(
    QualType texelType, QualType imageType, SpirvInstruction *image,
    SpirvInstruction *sampler, bool isNonUniform, SpirvInstruction *coordinate,
    SpirvInstruction *compareVal, SpirvInstruction *bias, SpirvInstruction *lod,
    std::pair<SpirvInstruction *, SpirvInstruction *> grad,
    SpirvInstruction *constOffset, SpirvInstruction *varOffset,
    SpirvInstruction *constOffsets, SpirvInstruction *sample,
    SpirvInstruction *minLod, SpirvInstruction *residencyCode,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");

  // The Lod and Grad image operands requires explicit-lod instructions.
  // Otherwise we use implicit-lod instructions.
  const bool isExplicit = lod || (grad.first && grad.second);
  const bool isSparse = (residencyCode != nullptr);

  spv::Op op = spv::Op::Max;
  if (compareVal) {
    op = isExplicit ? (isSparse ? spv::Op::OpImageSparseSampleDrefExplicitLod
                                : spv::Op::OpImageSampleDrefExplicitLod)
                    : (isSparse ? spv::Op::OpImageSparseSampleDrefImplicitLod
                                : spv::Op::OpImageSampleDrefImplicitLod);
  } else {
    op = isExplicit ? (isSparse ? spv::Op::OpImageSparseSampleExplicitLod
                                : spv::Op::OpImageSampleExplicitLod)
                    : (isSparse ? spv::Op::OpImageSparseSampleImplicitLod
                                : spv::Op::OpImageSampleImplicitLod);
  }

  // minLod is only valid with Implicit instructions and Grad instructions.
  // This means that we cannot have Lod and minLod together because Lod requires
  // explicit insturctions. So either lod or minLod or both must be zero.
  assert(lod == nullptr || minLod == nullptr);

  // An OpSampledImage is required to do the image sampling.
  auto *sampledImage =
      new (context) SpirvSampledImage(imageType, /*id*/ 0, loc, image, sampler);
  insertPoint->addInstruction(sampledImage);

  if (isNonUniform) {
    // The sampled image will be used to access resource's memory, so we need
    // to decorate it with NonUniformEXT.
    decorateNonUniformEXT(sampledImage, loc);
  }

  const auto mask = composeImageOperandsMask(
      bias, lod, grad, constOffset, varOffset, constOffsets, sample, minLod);

  auto *imageSampleInst = new (context)
      SpirvImageOp(op, texelType, /*id*/ 0, loc, sampledImage, coordinate, mask,
                   compareVal, bias, lod, grad.first, grad.second, constOffset,
                   varOffset, constOffsets, sample, minLod);
  insertPoint->addInstruction(imageSampleInst);

  if (isSparse) {
    // Write the Residency Code
    const auto status =
        createCompositeExtract(astContext.UnsignedIntTy, imageSampleInst, {0});
    createStore(residencyCode, status, loc);
    // Extract the real result from the struct
    return createCompositeExtract(texelType, imageSampleInst, {1});
  }

  return imageSampleInst;
}

SpirvInstruction *SpirvBuilder::createImageFetchOrRead(
    bool doImageFetch, QualType texelType, QualType imageType,
    SpirvInstruction *image, SpirvInstruction *coordinate,
    SpirvInstruction *lod, SpirvInstruction *constOffset,
    SpirvInstruction *varOffset, SpirvInstruction *constOffsets,
    SpirvInstruction *sample, SpirvInstruction *residencyCode,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");

  const auto mask = composeImageOperandsMask(
      /*bias*/ nullptr, lod, std::make_pair(nullptr, nullptr), constOffset,
      varOffset, constOffsets, sample, /*minLod*/ nullptr);

  const bool isSparse = (residencyCode != nullptr);

  spv::Op op =
      doImageFetch
          ? (isSparse ? spv::Op::OpImageSparseFetch : spv::Op::OpImageFetch)
          : (isSparse ? spv::Op::OpImageSparseRead : spv::Op::OpImageRead);

  auto *fetchOrReadInst = new (context) SpirvImageOp(
      op, texelType, /*id*/ 0, loc, image, coordinate, mask,
      /*dref*/ nullptr, /*bias*/ nullptr, lod, /*gradDx*/ nullptr,
      /*gradDy*/ nullptr, constOffset, varOffset, constOffsets, sample);
  insertPoint->addInstruction(fetchOrReadInst);

  if (isSparse) {
    // Write the Residency Code
    const auto status =
        createCompositeExtract(astContext.UnsignedIntTy, fetchOrReadInst, {0});
    createStore(residencyCode, status, loc);
    // Extract the real result from the struct
    return createCompositeExtract(texelType, fetchOrReadInst, {1});
  }

  return fetchOrReadInst;
}

void SpirvBuilder::createImageWrite(QualType imageType, SpirvInstruction *image,
                                    SpirvInstruction *coord,
                                    SpirvInstruction *texel,
                                    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *writeInst = new (context) SpirvImageOp(
      spv::Op::OpImageWrite, imageType, /*id*/ 0, loc, image, coord,
      spv::ImageOperandsMask::MaskNone,
      /*dref*/ nullptr, /*bias*/ nullptr, /*lod*/ nullptr, /*gradDx*/ nullptr,
      /*gradDy*/ nullptr, /*constOffset*/ nullptr, /*varOffset*/ nullptr,
      /*constOffsets*/ nullptr, /*sample*/ nullptr, /*minLod*/ nullptr,
      /*component*/ nullptr, texel);
  insertPoint->addInstruction(writeInst);
}

SpirvInstruction *SpirvBuilder::createImageGather(
    QualType texelType, QualType imageType, SpirvInstruction *image,
    SpirvInstruction *sampler, bool isNonUniform, SpirvInstruction *coordinate,
    SpirvInstruction *component, SpirvInstruction *compareVal,
    SpirvInstruction *constOffset, SpirvInstruction *varOffset,
    SpirvInstruction *constOffsets, SpirvInstruction *sample,
    SpirvInstruction *residencyCode, SourceLocation loc) {
  assert(insertPoint && "null insert point");

  // An OpSampledImage is required to do the image sampling.
  auto *sampledImage =
      new (context) SpirvSampledImage(imageType, /*id*/ 0, loc, image, sampler);
  insertPoint->addInstruction(sampledImage);

  if (isNonUniform) {
    // The sampled image will be used to access resource's memory, so we need
    // to decorate it with NonUniformEXT.
    decorateNonUniformEXT(sampledImage, loc);
  }

  // TODO: Update ImageGather to accept minLod if necessary.
  const auto mask = composeImageOperandsMask(
      /*bias*/ nullptr, /*lod*/ nullptr, std::make_pair(nullptr, nullptr),
      constOffset, varOffset, constOffsets, sample, /*minLod*/ nullptr);

  spv::Op op = compareVal ? (residencyCode ? spv::Op::OpImageSparseDrefGather
                                           : spv::Op::OpImageDrefGather)
                          : (residencyCode ? spv::Op::OpImageSparseGather
                                           : spv::Op::OpImageGather);

  // Note: OpImageSparseDrefGather and OpImageDrefGather do not take the
  // component parameter.
  if (compareVal)
    component = nullptr;

  auto *imageInstruction = new (context) SpirvImageOp(
      op, texelType, /*id*/ 0, loc, sampledImage, coordinate, mask, compareVal,
      /*bias*/ nullptr, /*lod*/ nullptr, /*gradDx*/ nullptr,
      /*gradDy*/ nullptr, constOffset, varOffset, constOffsets, sample,
      /*minLod*/ nullptr, component);
  insertPoint->addInstruction(imageInstruction);

  if (residencyCode) {
    // Write the Residency Code
    const auto status =
        createCompositeExtract(astContext.UnsignedIntTy, imageInstruction, {0});
    createStore(residencyCode, status);
    // Extract the real result from the struct
    return createCompositeExtract(texelType, imageInstruction, {1});
  }

  return imageInstruction;
}

SpirvImageSparseTexelsResident *
SpirvBuilder::createImageSparseTexelsResident(SpirvInstruction *residentCode,
                                              SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context) SpirvImageSparseTexelsResident(
      astContext.BoolTy, /*id*/ 0, loc, residentCode);
  insertPoint->addInstruction(inst);
  return inst;
}

SpirvImageQuery *SpirvBuilder::createImageQuery(spv::Op opcode,
                                                QualType resultType,
                                                SourceLocation loc,
                                                SpirvInstruction *image,
                                                SpirvInstruction *lod) {
  assert(insertPoint && "null insert point");
  SpirvInstruction *lodParam = nullptr;
  SpirvInstruction *coordinateParam = nullptr;
  if (opcode == spv::Op::OpImageQuerySizeLod)
    lodParam = lod;
  if (opcode == spv::Op::OpImageQueryLod)
    coordinateParam = lod;

  auto *inst =
      new (context) SpirvImageQuery(opcode, resultType, /*result-id*/ 0, loc,
                                    image, lodParam, coordinateParam);
  insertPoint->addInstruction(inst);
  return inst;
}

SpirvSelect *SpirvBuilder::createSelect(QualType resultType,
                                        SpirvInstruction *condition,
                                        SpirvInstruction *trueValue,
                                        SpirvInstruction *falseValue,
                                        SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context)
      SpirvSelect(resultType, /*id*/ 0, loc, condition, trueValue, falseValue);
  insertPoint->addInstruction(inst);
  return inst;
}

void SpirvBuilder::createSwitch(
    SpirvBasicBlock *mergeLabel, SpirvInstruction *selector,
    SpirvBasicBlock *defaultLabel,
    llvm::ArrayRef<std::pair<uint32_t, SpirvBasicBlock *>> target,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  // Create the OpSelectioMerege.
  auto *selectionMerge = new (context)
      SpirvSelectionMerge(loc, mergeLabel, spv::SelectionControlMask::MaskNone);
  insertPoint->addInstruction(selectionMerge);

  // Create the OpSwitch.
  auto *switchInst =
      new (context) SpirvSwitch(loc, selector, defaultLabel, target);
  insertPoint->addInstruction(switchInst);
}

void SpirvBuilder::createKill(SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *kill = new (context) SpirvKill(loc);
  insertPoint->addInstruction(kill);
}

void SpirvBuilder::createBranch(SpirvBasicBlock *targetLabel,
                                SpirvBasicBlock *mergeBB,
                                SpirvBasicBlock *continueBB,
                                spv::LoopControlMask loopControl,
                                SourceLocation loc) {
  assert(insertPoint && "null insert point");

  if (mergeBB && continueBB) {
    auto *loopMerge =
        new (context) SpirvLoopMerge(loc, mergeBB, continueBB, loopControl);
    insertPoint->addInstruction(loopMerge);
  }

  auto *branch = new (context) SpirvBranch(loc, targetLabel);
  insertPoint->addInstruction(branch);
}

void SpirvBuilder::createConditionalBranch(
    SpirvInstruction *condition, SpirvBasicBlock *trueLabel,
    SpirvBasicBlock *falseLabel, SpirvBasicBlock *mergeLabel,
    SpirvBasicBlock *continueLabel, spv::SelectionControlMask selectionControl,
    spv::LoopControlMask loopControl, SourceLocation loc) {
  assert(insertPoint && "null insert point");

  if (mergeLabel) {
    if (continueLabel) {
      auto *loopMerge = new (context)
          SpirvLoopMerge(loc, mergeLabel, continueLabel, loopControl);
      insertPoint->addInstruction(loopMerge);
    } else {
      auto *selectionMerge =
          new (context) SpirvSelectionMerge(loc, mergeLabel, selectionControl);
      insertPoint->addInstruction(selectionMerge);
    }
  }

  auto *branchConditional = new (context)
      SpirvBranchConditional(loc, condition, trueLabel, falseLabel);
  insertPoint->addInstruction(branchConditional);
}

void SpirvBuilder::createReturn(SourceLocation loc) {
  assert(insertPoint && "null insert point");
  insertPoint->addInstruction(new (context) SpirvReturn(loc));
}

void SpirvBuilder::createReturnValue(SpirvInstruction *value,
                                     SourceLocation loc) {
  assert(insertPoint && "null insert point");
  insertPoint->addInstruction(new (context) SpirvReturn(loc, value));
}

SpirvInstruction *SpirvBuilder::createExtInst(
    QualType resultType, SpirvExtInstImport *set, GLSLstd450 inst,
    llvm::ArrayRef<SpirvInstruction *> operands, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *extInst = new (context)
      SpirvExtInst(resultType, /*id*/ 0, loc, set, inst, operands);
  insertPoint->addInstruction(extInst);
  return extInst;
}

SpirvInstruction *SpirvBuilder::createExtInst(
    const SpirvType *resultType, SpirvExtInstImport *set, GLSLstd450 inst,
    llvm::ArrayRef<SpirvInstruction *> operands, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *extInst = new (context)
      SpirvExtInst(/*QualType*/ {}, /*id*/ 0, loc, set, inst, operands);
  extInst->setResultType(resultType);
  insertPoint->addInstruction(extInst);
  return extInst;
}

void SpirvBuilder::createBarrier(spv::Scope memoryScope,
                                 spv::MemorySemanticsMask memorySemantics,
                                 llvm::Optional<spv::Scope> exec,
                                 SourceLocation loc) {
  assert(insertPoint && "null insert point");
  SpirvBarrier *barrier =
      new (context) SpirvBarrier(loc, memoryScope, memorySemantics, exec);
  insertPoint->addInstruction(barrier);
}

SpirvBitFieldInsert *SpirvBuilder::createBitFieldInsert(
    QualType resultType, SpirvInstruction *base, SpirvInstruction *insert,
    SpirvInstruction *offset, SpirvInstruction *count, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context) SpirvBitFieldInsert(resultType, /*id*/ 0, loc,
                                                 base, insert, offset, count);
  insertPoint->addInstruction(inst);
  return inst;
}

SpirvBitFieldExtract *SpirvBuilder::createBitFieldExtract(
    QualType resultType, SpirvInstruction *base, SpirvInstruction *offset,
    SpirvInstruction *count, bool isSigned, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context) SpirvBitFieldExtract(
      resultType, /*id*/ 0, loc, base, offset, count, isSigned);
  insertPoint->addInstruction(inst);
  return inst;
}

void SpirvBuilder::createEmitVertex(SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context) SpirvEmitVertex(loc);
  insertPoint->addInstruction(inst);
}

void SpirvBuilder::createEndPrimitive(SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context) SpirvEndPrimitive(loc);
  insertPoint->addInstruction(inst);
}

SpirvArrayLength *SpirvBuilder::createArrayLength(QualType resultType,
                                                  SourceLocation loc,
                                                  SpirvInstruction *structure,
                                                  uint32_t arrayMember) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context) SpirvArrayLength(resultType, /*result-id*/ 0, loc,
                                              structure, arrayMember);
  insertPoint->addInstruction(inst);
  return inst;
}

void SpirvBuilder::createLineInfo(SpirvString *file, uint32_t line,
                                  uint32_t column) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context) SpirvLineInfo(file, line, column);
  insertPoint->addInstruction(inst);
}

void SpirvBuilder::addExtension(Extension ext, llvm::StringRef target,
                                SourceLocation loc) {
  // TODO: The extension management should be removed from here and added as a
  // separate pass.

  if (existingExtensions.insert(ext)) {
    assert(featureManager);
    featureManager->requestExtension(ext, target, loc);
    // Do not emit OpExtension if the given extension is natively supported in
    // the target environment.
    if (featureManager->isExtensionRequiredForTargetEnv(ext))
      module->addExtension(new (context) SpirvExtension(
          loc, featureManager->getExtensionName(ext)));
  }
}

void SpirvBuilder::addModuleProcessed(llvm::StringRef process) {
  module->addModuleProcessed(new (context) SpirvModuleProcessed({}, process));
}

SpirvExtInstImport *SpirvBuilder::getGLSLExtInstSet(SourceLocation loc) {
  SpirvExtInstImport *glslSet = module->getGLSLExtInstSet();
  if (!glslSet) {
    glslSet = new (context) SpirvExtInstImport(/*id*/ 0, loc, "GLSL.std.450");
    module->addExtInstSet(glslSet);
  }
  return glslSet;
}

SpirvVariable *SpirvBuilder::addStageIOVar(QualType type,
                                           spv::StorageClass storageClass,
                                           std::string name,
                                           SourceLocation loc) {
  // Note: We store the underlying type in the variable, *not* the pointer type.
  auto *var = new (context) SpirvVariable(type, /*id*/ 0, loc, storageClass);
  var->setDebugName(name);
  module->addVariable(var);
  return var;
}

SpirvVariable *SpirvBuilder::addStageBuiltinVar(const SpirvType *type,
                                                spv::StorageClass storageClass,
                                                spv::BuiltIn builtin,
                                                SourceLocation loc) {
  // Note: We store the underlying type in the variable, *not* the pointer type.
  auto *var =
      new (context) SpirvVariable(/*QualType*/ {}, /*id*/ 0, loc, storageClass);
  var->setResultType(type);
  module->addVariable(var);

  // Decorate with the specified Builtin
  auto *decor = new (context) SpirvDecoration(
      loc, var, spv::Decoration::BuiltIn, {static_cast<uint32_t>(builtin)});
  module->addDecoration(decor);

  return var;
}

SpirvVariable *SpirvBuilder::addStageBuiltinVar(QualType type,
                                                spv::StorageClass storageClass,
                                                spv::BuiltIn builtin,
                                                SourceLocation loc) {
  // Note: We store the underlying type in the variable, *not* the pointer type.
  auto *var = new (context) SpirvVariable(type, /*id*/ 0, loc, storageClass);
  module->addVariable(var);

  // Decorate with the specified Builtin
  auto *decor = new (context) SpirvDecoration(
      loc, var, spv::Decoration::BuiltIn, {static_cast<uint32_t>(builtin)});
  module->addDecoration(decor);

  return var;
}

SpirvVariable *SpirvBuilder::addModuleVar(
    QualType type, spv::StorageClass storageClass, llvm::StringRef name,
    llvm::Optional<SpirvInstruction *> init, SourceLocation loc) {
  assert(storageClass != spv::StorageClass::Function);
  // Note: We store the underlying type in the variable, *not* the pointer type.
  auto *var =
      new (context) SpirvVariable(type, /*id*/ 0, loc, storageClass,
                                  init.hasValue() ? init.getValue() : nullptr);
  var->setDebugName(name);
  module->addVariable(var);
  return var;
}

SpirvVariable *SpirvBuilder::addModuleVar(
    const SpirvType *type, spv::StorageClass storageClass, llvm::StringRef name,
    llvm::Optional<SpirvInstruction *> init, SourceLocation loc) {
  assert(storageClass != spv::StorageClass::Function);
  // Note: We store the underlying type in the variable, *not* the pointer type.
  auto *var =
      new (context) SpirvVariable(/*QualType*/ {}, /*id*/ 0, loc, storageClass,
                                  init.hasValue() ? init.getValue() : nullptr);
  var->setResultType(type);
  var->setDebugName(name);
  module->addVariable(var);
  return var;
}

void SpirvBuilder::decorateLocation(SpirvInstruction *target, uint32_t location,
                                    SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::Location, {location});
  module->addDecoration(decor);
}

void SpirvBuilder::decorateIndex(SpirvInstruction *target, uint32_t index,
                                 SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::Index, {index});
  module->addDecoration(decor);
}

void SpirvBuilder::decorateDSetBinding(SpirvInstruction *target,
                                       uint32_t setNumber,
                                       uint32_t bindingNumber,
                                       SourceLocation srcLoc) {
  auto *dset = new (context) SpirvDecoration(
      srcLoc, target, spv::Decoration::DescriptorSet, {setNumber});
  module->addDecoration(dset);

  auto *binding = new (context) SpirvDecoration(
      srcLoc, target, spv::Decoration::Binding, {bindingNumber});
  module->addDecoration(binding);
}

void SpirvBuilder::decorateSpecId(SpirvInstruction *target, uint32_t specId,
                                  SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::SpecId, {specId});
  module->addDecoration(decor);
}

void SpirvBuilder::decorateInputAttachmentIndex(SpirvInstruction *target,
                                                uint32_t indexNumber,
                                                SourceLocation srcLoc) {
  auto *decor = new (context) SpirvDecoration(
      srcLoc, target, spv::Decoration::InputAttachmentIndex, {indexNumber});
  module->addDecoration(decor);
}

void SpirvBuilder::decorateCounterBuffer(SpirvInstruction *mainBuffer,
                                         SpirvInstruction *counterBuffer,
                                         SourceLocation srcLoc) {
  if (spirvOptions.enableReflect) {
    addExtension(Extension::GOOGLE_hlsl_functionality1, "SPIR-V reflection",
                 srcLoc);
    auto *decor = new (context) SpirvDecoration(
        srcLoc, mainBuffer, spv::Decoration::HlslCounterBufferGOOGLE,
        {counterBuffer});
    module->addDecoration(decor);
  }
}

void SpirvBuilder::decorateHlslSemantic(SpirvInstruction *target,
                                        llvm::StringRef semantic,
                                        llvm::Optional<uint32_t> memberIdx,
                                        SourceLocation srcLoc) {
  if (spirvOptions.enableReflect) {
    addExtension(Extension::GOOGLE_hlsl_functionality1, "SPIR-V reflection",
                 srcLoc);
    auto *decor = new (context)
        SpirvDecoration(srcLoc, target, spv::Decoration::HlslSemanticGOOGLE,
                        semantic, memberIdx);
    module->addDecoration(decor);
  }
}

void SpirvBuilder::decorateCentroid(SpirvInstruction *target,
                                    SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::Centroid);
  module->addDecoration(decor);
}

void SpirvBuilder::decorateFlat(SpirvInstruction *target,
                                SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::Flat);
  module->addDecoration(decor);
}

void SpirvBuilder::decorateNoPerspective(SpirvInstruction *target,
                                         SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::NoPerspective);
  module->addDecoration(decor);
}

void SpirvBuilder::decorateSample(SpirvInstruction *target,
                                  SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::Sample);
  module->addDecoration(decor);
}

void SpirvBuilder::decorateBlock(SpirvInstruction *target,
                                 SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::Block);
  module->addDecoration(decor);
}

void SpirvBuilder::decorateRelaxedPrecision(SpirvInstruction *target,
                                            SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::RelaxedPrecision);
  module->addDecoration(decor);
}

void SpirvBuilder::decoratePatch(SpirvInstruction *target,
                                 SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::Patch);
  module->addDecoration(decor);
}

void SpirvBuilder::decorateNonUniformEXT(SpirvInstruction *target,
                                         SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::NonUniformEXT);
  module->addDecoration(decor);
}

SpirvConstant *SpirvBuilder::getConstantInt(QualType type, llvm::APInt value,
                                            bool specConst) {
  // We do not reuse existing constant integers. Just create a new one.
  auto *intConst = new (context) SpirvConstantInteger(type, value, specConst);
  module->addConstant(intConst);
  return intConst;
}

SpirvConstant *SpirvBuilder::getConstantFloat(QualType type,
                                              llvm::APFloat value,
                                              bool specConst) {
  // We do not reuse existing constant floats. Just create a new one.
  auto *floatConst = new (context) SpirvConstantFloat(type, value, specConst);
  module->addConstant(floatConst);
  return floatConst;
}

SpirvConstant *SpirvBuilder::getConstantBool(bool value, bool specConst) {
  // We do not care about making unique constants at this point.
  auto *boolConst = new (context)
      SpirvConstantBoolean(context.getBoolType(), value, specConst);
  module->addConstant(boolConst);
  return boolConst;
}

SpirvConstant *
SpirvBuilder::getConstantComposite(QualType compositeType,
                                   llvm::ArrayRef<SpirvConstant *> constituents,
                                   bool specConst) {
  // We do not care about making unique constants at this point.
  auto *compositeConst = new (context)
      SpirvConstantComposite(compositeType, constituents, specConst);
  module->addConstant(compositeConst);
  return compositeConst;
}

SpirvConstant *SpirvBuilder::getConstantNull(QualType type) {
  // We do not care about making unique constants at this point.
  auto *nullConst = new (context) SpirvConstantNull(type);
  module->addConstant(nullConst);
  return nullConst;
}

std::vector<uint32_t> SpirvBuilder::takeModule() {
  // Run necessary visitor passes first
  LiteralTypeVisitor literalTypeVisitor(astContext, context, spirvOptions);
  LowerTypeVisitor lowerTypeVisitor(astContext, context, spirvOptions);
  CapabilityVisitor capabilityVisitor(context, spirvOptions, *this);
  EmitVisitor emitVisitor(astContext, context, spirvOptions);

  module->invokeVisitor(&literalTypeVisitor, true);

  // Lower types
  module->invokeVisitor(&lowerTypeVisitor);

  // Add necessary capabilities and extensions
  module->invokeVisitor(&capabilityVisitor);

  // Emit SPIR-V
  module->invokeVisitor(&emitVisitor);

  return emitVisitor.takeBinary();
}

} // end namespace spirv
} // end namespace clang
