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
#include "DebugTypeVisitor.h"
#include "EmitVisitor.h"
#include "LiteralTypeVisitor.h"
#include "LowerTypeVisitor.h"
#include "NonUniformVisitor.h"
#include "PreciseVisitor.h"
#include "RelaxedPrecisionVisitor.h"
#include "RemoveBufferBlockVisitor.h"
#include "SortDebugInfoVisitor.h"
#include "clang/SPIRV/AstTypeProbe.h"

namespace clang {
namespace spirv {

SpirvBuilder::SpirvBuilder(ASTContext &ac, SpirvContext &ctx,
                           const SpirvCodeGenOptions &opt)
    : astContext(ac), context(ctx), mod(llvm::make_unique<SpirvModule>()),
      function(nullptr), spirvOptions(opt), builtinVars(), debugNone(nullptr),
      nullDebugExpr(nullptr), stringLiterals() {}

SpirvFunction *SpirvBuilder::createSpirvFunction(QualType returnType,
                                                 SourceLocation loc,
                                                 llvm::StringRef name,
                                                 bool isPrecise) {
  auto *fn = new (context) SpirvFunction(returnType, loc, name, isPrecise);
  mod->addFunction(fn);
  return fn;
}

SpirvFunction *SpirvBuilder::beginFunction(QualType returnType,
                                           SourceLocation loc,
                                           llvm::StringRef funcName,
                                           bool isPrecise,
                                           SpirvFunction *func) {
  assert(!function && "found nested function");
  if (func) {
    function = func;
    function->setAstReturnType(returnType);
    function->setSourceLocation(loc);
    function->setFunctionName(funcName);
    function->setPrecise(isPrecise);
  } else {
    function = createSpirvFunction(returnType, loc, funcName, isPrecise);
  }

  return function;
}

SpirvFunctionParameter *SpirvBuilder::addFnParam(QualType ptrType,
                                                 bool isPrecise,
                                                 SourceLocation loc,
                                                 llvm::StringRef name) {
  assert(function && "found detached parameter");
  SpirvFunctionParameter *param = nullptr;
  if (isBindlessOpaqueArray(ptrType)) {
    // If it is a bindless array of an opaque type, we have to use
    // a pointer to a pointer of the runtime array.
    param = new (context) SpirvFunctionParameter(
        context.getPointerType(ptrType, spv::StorageClass::UniformConstant),
        isPrecise, loc);
  } else {
    param = new (context) SpirvFunctionParameter(ptrType, isPrecise, loc);
  }
  param->setStorageClass(spv::StorageClass::Function);
  param->setDebugName(name);
  function->addParameter(param);
  return param;
}

SpirvVariable *SpirvBuilder::addFnVar(QualType valueType, SourceLocation loc,
                                      llvm::StringRef name, bool isPrecise,
                                      SpirvInstruction *init) {
  assert(function && "found detached local variable");
  SpirvVariable *var = nullptr;
  if (isBindlessOpaqueArray(valueType)) {
    // If it is a bindless array of an opaque type, we have to use
    // a pointer to a pointer of the runtime array.
    var = new (context) SpirvVariable(
        context.getPointerType(valueType, spv::StorageClass::UniformConstant),
        loc, spv::StorageClass::Function, isPrecise, init);
  } else {
    var = new (context) SpirvVariable(
        valueType, loc, spv::StorageClass::Function, isPrecise, init);
  }
  var->setDebugName(name);
  function->addVariable(var);
  return var;
}

void SpirvBuilder::endFunction() {
  assert(function && "no active function");
  mod->addFunctionToListOfSortedModuleFunctions(function);
  function = nullptr;
  insertPoint = nullptr;
}

SpirvBasicBlock *SpirvBuilder::createBasicBlock(llvm::StringRef name) {
  assert(function && "found detached basic block");
  auto *bb = new (context) SpirvBasicBlock(name);
  function->addBasicBlock(bb);
  if (auto *scope = context.getCurrentLexicalScope())
    bb->setDebugScope(new (context) SpirvDebugScope(scope));
  return bb;
}

SpirvDebugScope *SpirvBuilder::createDebugScope(SpirvDebugInstruction *scope) {
  assert(insertPoint && "null insert point");
  auto *dbgScope = new (context) SpirvDebugScope(scope);
  insertPoint->addInstruction(dbgScope);
  return dbgScope;
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

SpirvCompositeConstruct *SpirvBuilder::createCompositeConstruct(
    QualType resultType, llvm::ArrayRef<SpirvInstruction *> constituents,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvCompositeConstruct(resultType, loc, constituents);
  insertPoint->addInstruction(instruction);
  if (!constituents.empty()) {
    instruction->setLayoutRule(constituents[0]->getLayoutRule());
  }
  return instruction;
}

SpirvCompositeExtract *SpirvBuilder::createCompositeExtract(
    QualType resultType, SpirvInstruction *composite,
    llvm::ArrayRef<uint32_t> indexes, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvCompositeExtract(resultType, loc, composite, indexes);
  instruction->setRValue();
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvCompositeInsert *SpirvBuilder::createCompositeInsert(
    QualType resultType, SpirvInstruction *composite,
    llvm::ArrayRef<uint32_t> indices, SpirvInstruction *object,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvCompositeInsert(resultType, loc, composite, object, indices);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvVectorShuffle *SpirvBuilder::createVectorShuffle(
    QualType resultType, SpirvInstruction *vector1, SpirvInstruction *vector2,
    llvm::ArrayRef<uint32_t> selectors, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvVectorShuffle(resultType, loc, vector1, vector2, selectors);
  instruction->setRValue();
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvLoad *SpirvBuilder::createLoad(QualType resultType,
                                    SpirvInstruction *pointer,
                                    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvLoad(resultType, loc, pointer);
  instruction->setStorageClass(pointer->getStorageClass());
  instruction->setLayoutRule(pointer->getLayoutRule());
  instruction->setRValue(true);

  if (pointer->containsAliasComponent() &&
      isAKindOfStructuredOrByteBuffer(resultType)) {
    instruction->setStorageClass(spv::StorageClass::Uniform);
    // Now it is a pointer to the global resource, which is lvalue.
    instruction->setRValue(false);
    // Set to false to indicate that we've performed dereference over the
    // pointer-to-pointer and now should fallback to the normal path
    instruction->setContainsAliasComponent(false);
  }

  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvCopyObject *SpirvBuilder::createCopyObject(QualType resultType,
                                                SpirvInstruction *pointer,
                                                SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvCopyObject(resultType, loc, pointer);
  instruction->setStorageClass(pointer->getStorageClass());
  instruction->setLayoutRule(pointer->getLayoutRule());
  // The result of OpCopyObject is always an rvalue.
  instruction->setRValue(true);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvLoad *SpirvBuilder::createLoad(const SpirvType *resultType,
                                    SpirvInstruction *pointer,
                                    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvLoad(/*QualType*/ {}, loc, pointer);
  instruction->setResultType(resultType);
  instruction->setStorageClass(pointer->getStorageClass());
  // Special case for legalization. We could have point-to-pointer types.
  // For example:
  //
  // %var = OpVariable %_ptr_Private__ptr_Uniform_type_X Private
  // %1 = OpLoad %_ptr_Uniform_type_X %var
  //
  // Loading from %var should result in Uniform storage class, not Private.
  if (const auto *ptrType = dyn_cast<SpirvPointerType>(resultType)) {
    instruction->setStorageClass(ptrType->getStorageClass());
  }

  instruction->setLayoutRule(pointer->getLayoutRule());
  instruction->setRValue(true);
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
      new (context) SpirvFunctionCall(returnType, loc, func, params);
  instruction->setRValue(func->isRValue());
  instruction->setContainsAliasComponent(func->constainsAliasComponent());

  if (func->constainsAliasComponent() &&
      isAKindOfStructuredOrByteBuffer(returnType)) {
    instruction->setStorageClass(spv::StorageClass::Uniform);
    // Now it is a pointer to the global resource, which is lvalue.
    instruction->setRValue(false);
    // Set to false to indicate that we've performed dereference over the
    // pointer-to-pointer and now should fallback to the normal path
    instruction->setContainsAliasComponent(false);
  }

  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvAccessChain *
SpirvBuilder::createAccessChain(QualType resultType, SpirvInstruction *base,
                                llvm::ArrayRef<SpirvInstruction *> indexes,
                                SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvAccessChain(resultType, loc, base, indexes);
  instruction->setStorageClass(base->getStorageClass());
  instruction->setLayoutRule(base->getLayoutRule());
  instruction->setContainsAliasComponent(base->containsAliasComponent());

  // If doing an access chain into a structured or byte address buffer, make
  // sure the layout rule is sBufferLayoutRule.
  if (base->hasAstResultType() &&
      isAKindOfStructuredOrByteBuffer(base->getAstResultType()))
    instruction->setLayoutRule(spirvOptions.sBufferLayoutRule);

  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvUnaryOp *SpirvBuilder::createUnaryOp(spv::Op op, QualType resultType,
                                          SpirvInstruction *operand,
                                          SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvUnaryOp(op, resultType, loc, operand);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvBinaryOp *SpirvBuilder::createBinaryOp(spv::Op op, QualType resultType,
                                            SpirvInstruction *lhs,
                                            SpirvInstruction *rhs,
                                            SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvBinaryOp(op, resultType, loc, lhs, rhs);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvSpecConstantBinaryOp *SpirvBuilder::createSpecConstantBinaryOp(
    spv::Op op, QualType resultType, SpirvInstruction *lhs,
    SpirvInstruction *rhs, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvSpecConstantBinaryOp(op, resultType, loc, lhs, rhs);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvNonUniformElect *SpirvBuilder::createGroupNonUniformElect(
    spv::Op op, QualType resultType, spv::Scope execScope, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvNonUniformElect(resultType, loc, execScope);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvNonUniformUnaryOp *SpirvBuilder::createGroupNonUniformUnaryOp(
    SourceLocation loc, spv::Op op, QualType resultType, spv::Scope execScope,
    SpirvInstruction *operand, llvm::Optional<spv::GroupOperation> groupOp) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvNonUniformUnaryOp(op, resultType, loc, execScope, groupOp, operand);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvNonUniformBinaryOp *SpirvBuilder::createGroupNonUniformBinaryOp(
    spv::Op op, QualType resultType, spv::Scope execScope,
    SpirvInstruction *operand1, SpirvInstruction *operand2,
    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context) SpirvNonUniformBinaryOp(
      op, resultType, loc, execScope, operand1, operand2);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvAtomic *SpirvBuilder::createAtomicOp(
    spv::Op opcode, QualType resultType, SpirvInstruction *originalValuePtr,
    spv::Scope scope, spv::MemorySemanticsMask memorySemantics,
    SpirvInstruction *valueToOp, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction =
      new (context) SpirvAtomic(opcode, resultType, loc, originalValuePtr,
                                scope, memorySemantics, valueToOp);
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
      SpirvAtomic(spv::Op::OpAtomicCompareExchange, resultType, loc,
                  originalValuePtr, scope, equalMemorySemantics,
                  unequalMemorySemantics, valueToOp, comparator);
  insertPoint->addInstruction(instruction);
  return instruction;
}

SpirvSampledImage *SpirvBuilder::createSampledImage(QualType imageType,
                                                    SpirvInstruction *image,
                                                    SpirvInstruction *sampler,
                                                    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *sampledImage =
      new (context) SpirvSampledImage(imageType, loc, image, sampler);
  insertPoint->addInstruction(sampledImage);
  return sampledImage;
}

SpirvImageTexelPointer *SpirvBuilder::createImageTexelPointer(
    QualType resultType, SpirvInstruction *image, SpirvInstruction *coordinate,
    SpirvInstruction *sample, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *instruction = new (context)
      SpirvImageTexelPointer(resultType, loc, image, coordinate, sample);
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
    SpirvInstruction *sampler, SpirvInstruction *coordinate,
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
  auto *sampledImage = createSampledImage(imageType, image, sampler, loc);

  const auto mask = composeImageOperandsMask(
      bias, lod, grad, constOffset, varOffset, constOffsets, sample, minLod);

  auto *imageSampleInst = new (context)
      SpirvImageOp(op, texelType, loc, sampledImage, coordinate, mask,
                   compareVal, bias, lod, grad.first, grad.second, constOffset,
                   varOffset, constOffsets, sample, minLod);
  insertPoint->addInstruction(imageSampleInst);

  if (isSparse) {
    // Write the Residency Code
    const auto status = createCompositeExtract(astContext.UnsignedIntTy,
                                               imageSampleInst, {0}, loc);
    createStore(residencyCode, status, loc);
    // Extract the real result from the struct
    return createCompositeExtract(texelType, imageSampleInst, {1}, loc);
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
      op, texelType, loc, image, coordinate, mask,
      /*dref*/ nullptr, /*bias*/ nullptr, lod, /*gradDx*/ nullptr,
      /*gradDy*/ nullptr, constOffset, varOffset, constOffsets, sample);
  insertPoint->addInstruction(fetchOrReadInst);

  if (isSparse) {
    // Write the Residency Code
    const auto status = createCompositeExtract(astContext.UnsignedIntTy,
                                               fetchOrReadInst, {0}, loc);
    createStore(residencyCode, status, loc);
    // Extract the real result from the struct
    return createCompositeExtract(texelType, fetchOrReadInst, {1}, loc);
  }

  return fetchOrReadInst;
}

void SpirvBuilder::createImageWrite(QualType imageType, SpirvInstruction *image,
                                    SpirvInstruction *coord,
                                    SpirvInstruction *texel,
                                    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *writeInst = new (context) SpirvImageOp(
      spv::Op::OpImageWrite, imageType, loc, image, coord,
      spv::ImageOperandsMask::MaskNone,
      /*dref*/ nullptr, /*bias*/ nullptr, /*lod*/ nullptr, /*gradDx*/ nullptr,
      /*gradDy*/ nullptr, /*constOffset*/ nullptr, /*varOffset*/ nullptr,
      /*constOffsets*/ nullptr, /*sample*/ nullptr, /*minLod*/ nullptr,
      /*component*/ nullptr, texel);
  insertPoint->addInstruction(writeInst);
}

SpirvInstruction *SpirvBuilder::createImageGather(
    QualType texelType, QualType imageType, SpirvInstruction *image,
    SpirvInstruction *sampler, SpirvInstruction *coordinate,
    SpirvInstruction *component, SpirvInstruction *compareVal,
    SpirvInstruction *constOffset, SpirvInstruction *varOffset,
    SpirvInstruction *constOffsets, SpirvInstruction *sample,
    SpirvInstruction *residencyCode, SourceLocation loc) {
  assert(insertPoint && "null insert point");

  // An OpSampledImage is required to do the image sampling.
  auto *sampledImage = createSampledImage(imageType, image, sampler, loc);

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
      op, texelType, loc, sampledImage, coordinate, mask, compareVal,
      /*bias*/ nullptr, /*lod*/ nullptr, /*gradDx*/ nullptr,
      /*gradDy*/ nullptr, constOffset, varOffset, constOffsets, sample,
      /*minLod*/ nullptr, component);
  insertPoint->addInstruction(imageInstruction);

  if (residencyCode) {
    // Write the Residency Code
    const auto status = createCompositeExtract(astContext.UnsignedIntTy,
                                               imageInstruction, {0}, loc);
    createStore(residencyCode, status, loc);
    // Extract the real result from the struct
    return createCompositeExtract(texelType, imageInstruction, {1}, loc);
  }

  return imageInstruction;
}

SpirvImageSparseTexelsResident *
SpirvBuilder::createImageSparseTexelsResident(SpirvInstruction *residentCode,
                                              SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context)
      SpirvImageSparseTexelsResident(astContext.BoolTy, loc, residentCode);
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

  auto *inst = new (context) SpirvImageQuery(opcode, resultType, loc, image,
                                             lodParam, coordinateParam);
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
      SpirvSelect(resultType, loc, condition, trueValue, falseValue);
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
                                SourceLocation loc, SpirvBasicBlock *mergeBB,
                                SpirvBasicBlock *continueBB,
                                spv::LoopControlMask loopControl) {
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
    SpirvBasicBlock *falseLabel, SourceLocation loc,
    SpirvBasicBlock *mergeLabel, SpirvBasicBlock *continueLabel,
    spv::SelectionControlMask selectionControl,
    spv::LoopControlMask loopControl) {
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

SpirvInstruction *
SpirvBuilder::createGLSLExtInst(QualType resultType, GLSLstd450 inst,
                                llvm::ArrayRef<SpirvInstruction *> operands,
                                SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *extInst = new (context) SpirvExtInst(
      resultType, loc, getExtInstSet("GLSL.std.450"), inst, operands);
  insertPoint->addInstruction(extInst);
  return extInst;
}

SpirvInstruction *
SpirvBuilder::createGLSLExtInst(const SpirvType *resultType, GLSLstd450 inst,
                                llvm::ArrayRef<SpirvInstruction *> operands,
                                SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *extInst = new (context) SpirvExtInst(
      /*QualType*/ {}, loc, getExtInstSet("GLSL.std.450"), inst, operands);
  extInst->setResultType(resultType);
  insertPoint->addInstruction(extInst);
  return extInst;
}

SpirvInstruction *SpirvBuilder::createNonSemanticDebugPrintfExtInst(
    QualType resultType, NonSemanticDebugPrintfInstructions instId,
    llvm::ArrayRef<SpirvInstruction *> operands, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *extInst = new (context)
      SpirvExtInst(resultType, loc, getExtInstSet("NonSemantic.DebugPrintf"),
                   instId, operands);
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
  auto *inst = new (context)
      SpirvBitFieldInsert(resultType, loc, base, insert, offset, count);
  insertPoint->addInstruction(inst);
  return inst;
}

SpirvBitFieldExtract *SpirvBuilder::createBitFieldExtract(
    QualType resultType, SpirvInstruction *base, SpirvInstruction *offset,
    SpirvInstruction *count, bool isSigned, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context)
      SpirvBitFieldExtract(resultType, loc, base, offset, count, isSigned);
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
  auto *inst =
      new (context) SpirvArrayLength(resultType, loc, structure, arrayMember);
  insertPoint->addInstruction(inst);
  return inst;
}

SpirvInstruction *
SpirvBuilder::createRayTracingOpsNV(spv::Op opcode, QualType resultType,
                                    ArrayRef<SpirvInstruction *> operands,
                                    SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst =
      new (context) SpirvRayTracingOpNV(resultType, opcode, operands, loc);
  insertPoint->addInstruction(inst);
  return inst;
}

SpirvInstruction *
SpirvBuilder::createDemoteToHelperInvocationEXT(SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context) SpirvDemoteToHelperInvocationEXT(loc);
  insertPoint->addInstruction(inst);
  return inst;
}

SpirvDebugSource *SpirvBuilder::createDebugSource(llvm::StringRef file,
                                                  llvm::StringRef text) {
  auto *inst = new (context) SpirvDebugSource(file, text);
  mod->addDebugInfo(inst);
  return inst;
}

SpirvDebugCompilationUnit *
SpirvBuilder::createDebugCompilationUnit(SpirvDebugSource *source) {
  auto *inst = new (context) SpirvDebugCompilationUnit(
      /*version*/ 1, /*DWARF version*/ 4, source);
  mod->addDebugInfo(inst);
  return inst;
}

SpirvDebugLexicalBlock *
SpirvBuilder::createDebugLexicalBlock(SpirvDebugSource *source, uint32_t line,
                                      uint32_t column,
                                      SpirvDebugInstruction *parent) {
  assert(insertPoint && "null insert point");
  auto *inst =
      new (context) SpirvDebugLexicalBlock(source, line, column, parent);
  mod->addDebugInfo(inst);
  return inst;
}

SpirvDebugLocalVariable *SpirvBuilder::createDebugLocalVariable(
    QualType debugQualType, llvm::StringRef varName, SpirvDebugSource *src,
    uint32_t line, uint32_t column, SpirvDebugInstruction *parentScope,
    uint32_t flags, llvm::Optional<uint32_t> argNumber) {
  auto *inst = new (context) SpirvDebugLocalVariable(
      debugQualType, varName, src, line, column, parentScope, flags, argNumber);
  mod->addDebugInfo(inst);
  return inst;
}

SpirvDebugGlobalVariable *SpirvBuilder::createDebugGlobalVariable(
    QualType debugType, llvm::StringRef varName, SpirvDebugSource *src,
    uint32_t line, uint32_t column, SpirvDebugInstruction *parentScope,
    llvm::StringRef linkageName, SpirvVariable *var, uint32_t flags,
    llvm::Optional<SpirvInstruction *> staticMemberDebugType) {
  auto *inst = new (context) SpirvDebugGlobalVariable(
      debugType, varName, src, line, column, parentScope, linkageName, var,
      flags, staticMemberDebugType);
  mod->addDebugInfo(inst);
  return inst;
}

SpirvDebugInfoNone *SpirvBuilder::getOrCreateDebugInfoNone() {
  if (debugNone)
    return debugNone;

  debugNone = new (context) SpirvDebugInfoNone();
  mod->addDebugInfo(debugNone);
  return debugNone;
}

SpirvDebugExpression *SpirvBuilder::getOrCreateNullDebugExpression() {
  if (nullDebugExpr)
    return nullDebugExpr;

  nullDebugExpr = new (context) SpirvDebugExpression();
  mod->addDebugInfo(nullDebugExpr);
  return nullDebugExpr;
}

SpirvDebugDeclare *SpirvBuilder::createDebugDeclare(
    SpirvDebugLocalVariable *dbgVar, SpirvInstruction *var,
    llvm::Optional<SpirvDebugExpression *> dbgExpr) {
  auto *decl = new (context)
      SpirvDebugDeclare(dbgVar, var,
                        dbgExpr.hasValue() ? dbgExpr.getValue()
                                           : getOrCreateNullDebugExpression());
  if (isa<SpirvFunctionParameter>(var)) {
    assert(function && "found detached parameter");
    function->addParameterDebugDeclare(decl);
  } else {
    assert(insertPoint && "null insert point");
    insertPoint->addInstruction(decl);
  }
  return decl;
}

SpirvDebugFunction *SpirvBuilder::createDebugFunction(
    const FunctionDecl *decl, llvm::StringRef name, SpirvDebugSource *src,
    uint32_t line, uint32_t column, SpirvDebugInstruction *parentScope,
    llvm::StringRef linkageName, uint32_t flags, uint32_t scopeLine,
    SpirvFunction *fn) {
  auto *inst = new (context) SpirvDebugFunction(
      name, src, line, column, parentScope, linkageName, flags, scopeLine, fn);
  mod->addDebugInfo(inst);
  context.registerDebugFunctionForDecl(decl, inst);
  return inst;
}

SpirvInstruction *
SpirvBuilder::createRayQueryOpsKHR(spv::Op opcode, QualType resultType,
                                   ArrayRef<SpirvInstruction *> operands,
                                   bool cullFlags, SourceLocation loc) {
  assert(insertPoint && "null insert point");
  auto *inst = new (context)
      SpirvRayQueryOpKHR(resultType, opcode, operands, cullFlags, loc);
  insertPoint->addInstruction(inst);
  return inst;
}

void SpirvBuilder::addModuleProcessed(llvm::StringRef process) {
  mod->addModuleProcessed(new (context) SpirvModuleProcessed({}, process));
}

SpirvExtInstImport *SpirvBuilder::getExtInstSet(llvm::StringRef extName) {
  SpirvExtInstImport *set = mod->getExtInstSet(extName);
  if (!set) {
    // The extended instruction set is likely required for several different
    // reasons. We can't pinpoint the source location for one specific function.
    set = new (context) SpirvExtInstImport(/*SourceLocation*/ {}, extName);
    mod->addExtInstSet(set);
  }
  return set;
}

SpirvExtInstImport *SpirvBuilder::getOpenCLDebugInfoExtInstSet() {
  return getExtInstSet("OpenCL.DebugInfo.100");
}

SpirvVariable *SpirvBuilder::addStageIOVar(QualType type,
                                           spv::StorageClass storageClass,
                                           std::string name, bool isPrecise,
                                           SourceLocation loc) {
  // Note: We store the underlying type in the variable, *not* the pointer type.
  auto *var = new (context) SpirvVariable(type, loc, storageClass, isPrecise);
  var->setDebugName(name);
  mod->addVariable(var);
  return var;
}

SpirvVariable *SpirvBuilder::addStageBuiltinVar(QualType type,
                                                spv::StorageClass storageClass,
                                                spv::BuiltIn builtin,
                                                bool isPrecise,
                                                SourceLocation loc) {
  // If the built-in variable has already been added (via a built-in alias),
  // return the existing variable.
  auto found = std::find_if(
      builtinVars.begin(), builtinVars.end(),
      [storageClass, builtin](const BuiltInVarInfo &varInfo) {
        return varInfo.sc == storageClass && varInfo.builtIn == builtin;
      });
  if (found != builtinVars.end()) {
    return found->variable;
  }

  // Note: We store the underlying type in the variable, *not* the pointer type.
  auto *var = new (context) SpirvVariable(type, loc, storageClass, isPrecise);
  mod->addVariable(var);

  // Decorate with the specified Builtin
  auto *decor = new (context) SpirvDecoration(
      loc, var, spv::Decoration::BuiltIn, {static_cast<uint32_t>(builtin)});
  mod->addDecoration(decor);

  // Add variable to cache.
  builtinVars.emplace_back(storageClass, builtin, var);

  return var;
}

SpirvVariable *
SpirvBuilder::addModuleVar(QualType type, spv::StorageClass storageClass,
                           bool isPrecise, llvm::StringRef name,
                           llvm::Optional<SpirvInstruction *> init,
                           SourceLocation loc) {
  assert(storageClass != spv::StorageClass::Function);
  // Note: We store the underlying type in the variable, *not* the pointer type.
  auto *var =
      new (context) SpirvVariable(type, loc, storageClass, isPrecise,
                                  init.hasValue() ? init.getValue() : nullptr);
  var->setDebugName(name);
  mod->addVariable(var);
  return var;
}

SpirvVariable *SpirvBuilder::addModuleVar(
    const SpirvType *type, spv::StorageClass storageClass, bool isPrecise,
    llvm::StringRef name, llvm::Optional<SpirvInstruction *> init,
    SourceLocation loc) {
  assert(storageClass != spv::StorageClass::Function);
  // Note: We store the underlying type in the variable, *not* the pointer type.
  auto *var =
      new (context) SpirvVariable(/*QualType*/ {}, loc, storageClass, isPrecise,
                                  init.hasValue() ? init.getValue() : nullptr);
  var->setResultType(type);
  var->setDebugName(name);
  mod->addVariable(var);
  return var;
}

void SpirvBuilder::decorateLocation(SpirvInstruction *target,
                                    uint32_t location) {
  auto *decor =
      new (context) SpirvDecoration(target->getSourceLocation(), target,
                                    spv::Decoration::Location, {location});
  mod->addDecoration(decor);
}

void SpirvBuilder::decorateIndex(SpirvInstruction *target, uint32_t index,
                                 SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::Index, {index});
  mod->addDecoration(decor);
}

void SpirvBuilder::decorateDSetBinding(SpirvVariable *target,
                                       uint32_t setNumber,
                                       uint32_t bindingNumber) {
  const SourceLocation srcLoc = target->getSourceLocation();
  auto *dset = new (context) SpirvDecoration(
      srcLoc, target, spv::Decoration::DescriptorSet, {setNumber});
  mod->addDecoration(dset);

  auto *binding = new (context) SpirvDecoration(
      srcLoc, target, spv::Decoration::Binding, {bindingNumber});

  target->setDescriptorSetNo(setNumber);
  target->setBindingNo(bindingNumber);

  mod->addDecoration(binding);
}

void SpirvBuilder::decorateSpecId(SpirvInstruction *target, uint32_t specId,
                                  SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::SpecId, {specId});
  mod->addDecoration(decor);
}

void SpirvBuilder::decorateInputAttachmentIndex(SpirvInstruction *target,
                                                uint32_t indexNumber,
                                                SourceLocation srcLoc) {
  auto *decor = new (context) SpirvDecoration(
      srcLoc, target, spv::Decoration::InputAttachmentIndex, {indexNumber});
  mod->addDecoration(decor);
}

void SpirvBuilder::decorateCounterBuffer(SpirvInstruction *mainBuffer,
                                         SpirvInstruction *counterBuffer,
                                         SourceLocation srcLoc) {
  if (spirvOptions.enableReflect) {
    auto *decor = new (context) SpirvDecoration(
        srcLoc, mainBuffer, spv::Decoration::HlslCounterBufferGOOGLE,
        {counterBuffer});
    mod->addDecoration(decor);
  }
}

void SpirvBuilder::decorateHlslSemantic(SpirvInstruction *target,
                                        llvm::StringRef semantic,
                                        llvm::Optional<uint32_t> memberIdx) {
  if (spirvOptions.enableReflect) {
    auto *decor = new (context) SpirvDecoration(
        target->getSourceLocation(), target,
        spv::Decoration::HlslSemanticGOOGLE, semantic, memberIdx);
    mod->addDecoration(decor);
  }
}

void SpirvBuilder::decorateCentroid(SpirvInstruction *target,
                                    SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::Centroid);
  mod->addDecoration(decor);
}

void SpirvBuilder::decorateFlat(SpirvInstruction *target,
                                SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::Flat);
  mod->addDecoration(decor);
}

void SpirvBuilder::decorateNoPerspective(SpirvInstruction *target,
                                         SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::NoPerspective);
  mod->addDecoration(decor);
}

void SpirvBuilder::decorateSample(SpirvInstruction *target,
                                  SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::Sample);
  mod->addDecoration(decor);
}

void SpirvBuilder::decoratePatch(SpirvInstruction *target,
                                 SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::Patch);
  mod->addDecoration(decor);
}

void SpirvBuilder::decorateNoContraction(SpirvInstruction *target,
                                         SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::NoContraction);
  mod->addDecoration(decor);
}

void SpirvBuilder::decoratePerPrimitiveNV(SpirvInstruction *target,
                                          SourceLocation srcLoc) {
  auto *decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::PerPrimitiveNV);
  mod->addDecoration(decor);
}

void SpirvBuilder::decoratePerTaskNV(SpirvInstruction *target, uint32_t offset,
                                     SourceLocation srcLoc) {
  auto *decor =
      new (context) SpirvDecoration(srcLoc, target, spv::Decoration::PerTaskNV);
  mod->addDecoration(decor);
  decor = new (context)
      SpirvDecoration(srcLoc, target, spv::Decoration::Offset, {offset});
  mod->addDecoration(decor);
}

SpirvConstant *SpirvBuilder::getConstantInt(QualType type, llvm::APInt value,
                                            bool specConst) {
  // We do not reuse existing constant integers. Just create a new one.
  auto *intConst = new (context) SpirvConstantInteger(type, value, specConst);
  mod->addConstant(intConst);
  return intConst;
}

SpirvConstant *SpirvBuilder::getConstantFloat(QualType type,
                                              llvm::APFloat value,
                                              bool specConst) {
  // We do not reuse existing constant floats. Just create a new one.
  auto *floatConst = new (context) SpirvConstantFloat(type, value, specConst);
  mod->addConstant(floatConst);
  return floatConst;
}

SpirvConstant *SpirvBuilder::getConstantBool(bool value, bool specConst) {
  // We do not care about making unique constants at this point.
  auto *boolConst =
      new (context) SpirvConstantBoolean(astContext.BoolTy, value, specConst);
  mod->addConstant(boolConst);
  return boolConst;
}

SpirvConstant *
SpirvBuilder::getConstantComposite(QualType compositeType,
                                   llvm::ArrayRef<SpirvConstant *> constituents,
                                   bool specConst) {
  // We do not care about making unique constants at this point.
  auto *compositeConst = new (context)
      SpirvConstantComposite(compositeType, constituents, specConst);
  mod->addConstant(compositeConst);
  return compositeConst;
}

SpirvConstant *SpirvBuilder::getConstantNull(QualType type) {
  // We do not care about making unique constants at this point.
  auto *nullConst = new (context) SpirvConstantNull(type);
  mod->addConstant(nullConst);
  return nullConst;
}

SpirvString *SpirvBuilder::getString(llvm::StringRef str) {
  // Reuse an existing instruction if possible.
  auto iter = stringLiterals.find(str.str());
  if (iter != stringLiterals.end())
    return iter->second;

  // Create a SpirvString instruction
  auto *instr = new (context) SpirvString(/* SourceLocation */ {}, str);
  instr->setRValue();
  stringLiterals[str.str()] = instr;
  mod->addString(instr);
  return instr;
}

std::vector<uint32_t> SpirvBuilder::takeModule() {
  // Run necessary visitor passes first
  LiteralTypeVisitor literalTypeVisitor(astContext, context, spirvOptions);
  LowerTypeVisitor lowerTypeVisitor(astContext, context, spirvOptions);
  CapabilityVisitor capabilityVisitor(astContext, context, spirvOptions, *this);
  RelaxedPrecisionVisitor relaxedPrecisionVisitor(context, spirvOptions);
  PreciseVisitor preciseVisitor(context, spirvOptions);
  NonUniformVisitor nonUniformVisitor(context, spirvOptions);
  RemoveBufferBlockVisitor removeBufferBlockVisitor(context, spirvOptions);
  EmitVisitor emitVisitor(astContext, context, spirvOptions);

  mod->invokeVisitor(&literalTypeVisitor, true);

  // Propagate NonUniform decorations
  mod->invokeVisitor(&nonUniformVisitor);

  // Lower types
  mod->invokeVisitor(&lowerTypeVisitor);

  // Generate debug types (if needed)
  if (spirvOptions.debugInfoRich) {
    DebugTypeVisitor debugTypeVisitor(astContext, context, spirvOptions, *this,
                                      lowerTypeVisitor);
    SortDebugInfoVisitor sortDebugInfoVisitor(context, spirvOptions);
    mod->invokeVisitor(&debugTypeVisitor);
    mod->invokeVisitor(&sortDebugInfoVisitor);
  }

  // Add necessary capabilities and extensions
  mod->invokeVisitor(&capabilityVisitor);

  // Propagate RelaxedPrecision decorations
  mod->invokeVisitor(&relaxedPrecisionVisitor);

  // Propagate NoContraction decorations
  mod->invokeVisitor(&preciseVisitor, true);

  // Remove BufferBlock decoration if necessary (this decoration is deprecated
  // after SPIR-V 1.3).
  mod->invokeVisitor(&removeBufferBlockVisitor);

  // Emit SPIR-V
  mod->invokeVisitor(&emitVisitor);

  return emitVisitor.takeBinary();
}

} // end namespace spirv
} // end namespace clang
