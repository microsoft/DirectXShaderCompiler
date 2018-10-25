//===- SpirvInstruction.cpp - SPIR-V Instruction Representation -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file implements the in-memory representation of SPIR-V instructions.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/BitwiseCast.h"
#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvVisitor.h"
#include "clang/SPIRV/String.h"

namespace clang {
namespace spirv {

#define DEFINE_INVOKE_VISITOR_FOR_CLASS(cls)                                   \
  bool cls::invokeVisitor(Visitor *v) { return v->visit(this); }

DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvCapability)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvExtension)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvExtInstImport)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvMemoryModel)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvEntryPoint)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvExecutionMode)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvString)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSource)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvModuleProcessed)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvDecoration)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvVariable)

DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvFunctionParameter)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvLoopMerge)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSelectionMerge)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvBranch)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvBranchConditional)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvKill)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvReturn)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSwitch)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvUnreachable)

DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvAccessChain)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvAtomic)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvBarrier)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvBinaryOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvBitFieldExtract)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvBitFieldInsert)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantBoolean)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantInteger)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantFloat)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantComposite)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantNull)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvComposite)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvCompositeExtract)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvCompositeInsert)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvEmitVertex)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvEndPrimitive)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvExtInst)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvFunctionCall)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvNonUniformBinaryOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvNonUniformElect)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvNonUniformUnaryOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvImageOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvImageQuery)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvImageSparseTexelsResident)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvImageTexelPointer)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvLoad)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSampledImage)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSelect)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSpecConstantBinaryOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSpecConstantUnaryOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvStore)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvUnaryOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvVectorShuffle)

#undef DEFINE_INVOKE_VISITOR_FOR_CLASS

SpirvInstruction::SpirvInstruction(Kind k, spv::Op op, QualType type,
                                   uint32_t id, SourceLocation loc)
    : kind(k), opcode(op), astResultType(type), resultId(id), srcLoc(loc),
      debugName(), resultType(nullptr), resultTypeId(0),
      layoutRule(SpirvLayoutRule::Void) {}

SpirvCapability::SpirvCapability(SourceLocation loc, spv::Capability cap)
    : SpirvInstruction(IK_Capability, spv::Op::OpCapability, QualType(),
                       /*resultId=*/0, loc),
      capability(cap) {}

SpirvExtension::SpirvExtension(SourceLocation loc,
                               llvm::StringRef extensionName)
    : SpirvInstruction(IK_Extension, spv::Op::OpExtension, QualType(),
                       /*resultId=*/0, loc),
      extName(extensionName) {}

SpirvExtInstImport::SpirvExtInstImport(uint32_t resultId, SourceLocation loc,
                                       llvm::StringRef extensionName)
    : SpirvInstruction(IK_ExtInstImport, spv::Op::OpExtInstImport, QualType(),
                       resultId, loc),
      extName(extensionName) {}

SpirvMemoryModel::SpirvMemoryModel(spv::AddressingModel addrModel,
                                   spv::MemoryModel memModel)
    : SpirvInstruction(IK_MemoryModel, spv::Op::OpMemoryModel, QualType(),
                       /*resultId=*/0, /*SrcLoc*/ {}),
      addressModel(addrModel), memoryModel(memModel) {}

SpirvEntryPoint::SpirvEntryPoint(SourceLocation loc,
                                 spv::ExecutionModel executionModel,
                                 SpirvFunction *entryPointFn,
                                 llvm::StringRef nameStr,
                                 llvm::ArrayRef<SpirvVariable *> iface)
    : SpirvInstruction(IK_EntryPoint, spv::Op::OpMemoryModel, QualType(),
                       /*resultId=*/0, loc),
      execModel(executionModel), entryPoint(entryPointFn), name(nameStr),
      interfaceVec(iface.begin(), iface.end()) {}

// OpExecutionMode and OpExecutionModeId instructions
SpirvExecutionMode::SpirvExecutionMode(SourceLocation loc,
                                       SpirvEntryPoint *entry,
                                       spv::ExecutionMode em,
                                       llvm::ArrayRef<uint32_t> paramsVec,
                                       bool usesIdParams)
    : SpirvInstruction(IK_ExecutionMode,
                       usesIdParams ? spv::Op::OpExecutionModeId
                                    : spv::Op::OpExecutionMode,
                       QualType(), /*resultId=*/0, loc),
      entryPoint(entry), execMode(em),
      params(paramsVec.begin(), paramsVec.end()) {}

SpirvString::SpirvString(SourceLocation loc, llvm::StringRef stringLiteral)
    : SpirvInstruction(IK_String, spv::Op::OpString, QualType(),
                       /*resultId=*/0, loc),
      str(stringLiteral) {}

SpirvSource::SpirvSource(SourceLocation loc, spv::SourceLanguage language,
                         uint32_t ver, SpirvString *fileString,
                         llvm::StringRef src)
    : SpirvInstruction(IK_Source, spv::Op::OpSource, QualType(),
                       /*resultId=*/0, loc),
      lang(language), version(ver), file(fileString), source(src) {}

SpirvModuleProcessed::SpirvModuleProcessed(SourceLocation loc,
                                           llvm::StringRef processStr)
    : SpirvInstruction(IK_ModuleProcessed, spv::Op::OpModuleProcessed,
                       QualType(),
                       /*resultId=*/0, loc),
      process(processStr) {}

SpirvDecoration::SpirvDecoration(SourceLocation loc,
                                 SpirvInstruction *targetInst,
                                 spv::Decoration decor,
                                 llvm::ArrayRef<uint32_t> p,
                                 llvm::Optional<uint32_t> idx)
    : SpirvInstruction(IK_Decoration,
                       idx.hasValue() ? spv::Op::OpMemberDecorate
                                      : spv::Op::OpDecorate,
                       /*type*/ {}, /*id*/ 0, loc),
      target(targetInst), decoration(decor), index(idx),
      params(p.begin(), p.end()) {}

SpirvDecoration::SpirvDecoration(SourceLocation loc,
                                 SpirvInstruction *targetInst,
                                 spv::Decoration decor,
                                 llvm::StringRef strParam,
                                 llvm::Optional<uint32_t> idx)
    : SpirvInstruction(IK_Decoration,
                       idx.hasValue() ? spv::Op::OpMemberDecorate
                                      : spv::Op::OpDecorate,
                       /*type*/ {}, /*id*/ 0, loc),
      target(targetInst), decoration(decor), index(idx), params() {
  const auto &stringWords = string::encodeSPIRVString(strParam);
  params.insert(params.end(), stringWords.begin(), stringWords.end());
}

SpirvVariable::SpirvVariable(QualType resultType, uint32_t resultId,
                             SourceLocation loc, spv::StorageClass sc,
                             SpirvInstruction *initializerInst)
    : SpirvInstruction(IK_Variable, spv::Op::OpVariable, resultType, resultId,
                       loc),
      storageClass(sc), initializer(initializerInst) {}

SpirvFunctionParameter::SpirvFunctionParameter(QualType resultType,
                                               uint32_t resultId,
                                               SourceLocation loc)
    : SpirvInstruction(IK_FunctionParameter, spv::Op::OpFunctionParameter,
                       resultType, resultId, loc) {}

SpirvMerge::SpirvMerge(Kind kind, spv::Op op, SourceLocation loc,
                       SpirvBasicBlock *mergeLabel)
    : SpirvInstruction(kind, op, QualType(), /*resultId=*/0, loc),
      mergeBlock(mergeLabel) {}

SpirvLoopMerge::SpirvLoopMerge(SourceLocation loc, SpirvBasicBlock *mergeBlock,
                               SpirvBasicBlock *contTarget,
                               spv::LoopControlMask mask)
    : SpirvMerge(IK_LoopMerge, spv::Op::OpLoopMerge, loc, mergeBlock),
      continueTarget(contTarget), loopControlMask(mask) {}

SpirvSelectionMerge::SpirvSelectionMerge(SourceLocation loc,
                                         SpirvBasicBlock *mergeBlock,
                                         spv::SelectionControlMask mask)
    : SpirvMerge(IK_SelectionMerge, spv::Op::OpSelectionMerge, loc, mergeBlock),
      selControlMask(mask) {}

SpirvTerminator::SpirvTerminator(Kind kind, spv::Op op, SourceLocation loc)
    : SpirvInstruction(kind, op, QualType(), /*resultId=*/0, loc) {}

SpirvBranching::SpirvBranching(Kind kind, spv::Op op, SourceLocation loc)
    : SpirvTerminator(kind, op, loc) {}

SpirvBranch::SpirvBranch(SourceLocation loc, SpirvBasicBlock *target)
    : SpirvBranching(IK_Branch, spv::Op::OpBranch, loc), targetLabel(target) {}

SpirvBranchConditional::SpirvBranchConditional(SourceLocation loc,
                                               SpirvInstruction *cond,
                                               SpirvBasicBlock *trueInst,
                                               SpirvBasicBlock *falseInst)
    : SpirvBranching(IK_BranchConditional, spv::Op::OpBranchConditional, loc),
      condition(cond), trueLabel(trueInst), falseLabel(falseInst) {}

SpirvKill::SpirvKill(SourceLocation loc)
    : SpirvTerminator(IK_Kill, spv::Op::OpKill, loc) {}

SpirvReturn::SpirvReturn(SourceLocation loc, SpirvInstruction *retVal)
    : SpirvTerminator(IK_Return,
                      retVal ? spv::Op::OpReturnValue : spv::Op::OpReturn, loc),
      returnValue(retVal) {}

SpirvSwitch::SpirvSwitch(
    SourceLocation loc, SpirvInstruction *selectorInst,
    SpirvBasicBlock *defaultLbl,
    llvm::ArrayRef<std::pair<uint32_t, SpirvBasicBlock *>> &targetsVec)
    : SpirvBranching(IK_Switch, spv::Op::OpSwitch, loc), selector(selectorInst),
      defaultLabel(defaultLbl), targets(targetsVec.begin(), targetsVec.end()) {}

// Switch instruction methods.
SpirvBasicBlock *SpirvSwitch::getTargetLabelForLiteral(uint32_t lit) const {
  for (auto pair : targets)
    if (pair.first == lit)
      return pair.second;
  return defaultLabel;
}

llvm::ArrayRef<SpirvBasicBlock *> SpirvSwitch::getTargetBranches() const {
  llvm::SmallVector<SpirvBasicBlock *, 4> branches;
  for (auto pair : targets)
    branches.push_back(pair.second);
  branches.push_back(defaultLabel);
  return branches;
}

SpirvUnreachable::SpirvUnreachable(SourceLocation loc)
    : SpirvTerminator(IK_Unreachable, spv::Op::OpUnreachable, loc) {}

SpirvAccessChain::SpirvAccessChain(QualType resultType, uint32_t resultId,
                                   SourceLocation loc,
                                   SpirvInstruction *baseInst,
                                   llvm::ArrayRef<SpirvInstruction *> indexVec)
    : SpirvInstruction(IK_AccessChain, spv::Op::OpAccessChain, resultType,
                       resultId, loc),
      base(baseInst), indices(indexVec.begin(), indexVec.end()) {}

SpirvAtomic::SpirvAtomic(spv::Op op, QualType resultType, uint32_t resultId,
                         SourceLocation loc, SpirvInstruction *pointerInst,
                         spv::Scope s, spv::MemorySemanticsMask mask,
                         SpirvInstruction *valueInst)
    : SpirvInstruction(IK_Atomic, op, resultType, resultId, loc),
      pointer(pointerInst), scope(s), memorySemantic(mask),
      memorySemanticUnequal(spv::MemorySemanticsMask::MaskNone),
      value(valueInst), comparator(nullptr) {
  assert(
      op == spv::Op::OpAtomicLoad || op == spv::Op::OpAtomicIIncrement ||
      op == spv::Op::OpAtomicIDecrement || op == spv::Op::OpAtomicFlagClear ||
      op == spv::Op::OpAtomicFlagTestAndSet || op == spv::Op::OpAtomicStore ||
      op == spv::Op::OpAtomicAnd || op == spv::Op::OpAtomicOr ||
      op == spv::Op::OpAtomicXor || op == spv::Op::OpAtomicIAdd ||
      op == spv::Op::OpAtomicISub || op == spv::Op::OpAtomicSMin ||
      op == spv::Op::OpAtomicUMin || op == spv::Op::OpAtomicSMax ||
      op == spv::Op::OpAtomicUMax || op == spv::Op::OpAtomicExchange);
}

SpirvAtomic::SpirvAtomic(spv::Op op, QualType resultType, uint32_t resultId,
                         SourceLocation loc, SpirvInstruction *pointerInst,
                         spv::Scope s, spv::MemorySemanticsMask semanticsEqual,
                         spv::MemorySemanticsMask semanticsUnequal,
                         SpirvInstruction *valueInst,
                         SpirvInstruction *comparatorInst)
    : SpirvInstruction(IK_Atomic, op, resultType, resultId, loc),
      pointer(pointerInst), scope(s), memorySemantic(semanticsEqual),
      memorySemanticUnequal(semanticsUnequal), value(valueInst),
      comparator(comparatorInst) {
  assert(op == spv::Op::OpAtomicExchange);
}

SpirvBarrier::SpirvBarrier(SourceLocation loc, spv::Scope memScope,
                           spv::MemorySemanticsMask memSemantics,
                           llvm::Optional<spv::Scope> execScope)
    : SpirvInstruction(IK_Barrier,
                       execScope.hasValue() ? spv::Op::OpMemoryBarrier
                                            : spv::Op::OpControlBarrier,
                       QualType(), /*resultId=*/0, loc),
      memoryScope(memScope), memorySemantics(memSemantics),
      executionScope(execScope) {}

SpirvBinaryOp::SpirvBinaryOp(spv::Op opcode, QualType resultType,
                             uint32_t resultId, SourceLocation loc,
                             SpirvInstruction *op1, SpirvInstruction *op2)
    : SpirvInstruction(IK_BinaryOp, opcode, resultType, resultId, loc),
      operand1(op1), operand2(op2) {}

SpirvBitField::SpirvBitField(Kind kind, spv::Op op, QualType resultType,
                             uint32_t resultId, SourceLocation loc,
                             SpirvInstruction *baseInst,
                             SpirvInstruction *offsetInst,
                             SpirvInstruction *countInst)
    : SpirvInstruction(kind, op, resultType, resultId, loc), base(baseInst),
      offset(offsetInst), count(countInst) {}

SpirvBitFieldExtract::SpirvBitFieldExtract(
    QualType resultType, uint32_t resultId, SourceLocation loc,
    SpirvInstruction *baseInst, SpirvInstruction *offsetInst,
    SpirvInstruction *countInst, bool isSigned)
    : SpirvBitField(
          IK_BitFieldExtract,
          isSigned ? spv::Op::OpBitFieldSExtract : spv::Op::OpBitFieldUExtract,
          resultType, resultId, loc, baseInst, offsetInst, countInst) {}

SpirvBitFieldInsert::SpirvBitFieldInsert(QualType resultType, uint32_t resultId,
                                         SourceLocation loc,
                                         SpirvInstruction *baseInst,
                                         SpirvInstruction *insertInst,
                                         SpirvInstruction *offsetInst,
                                         SpirvInstruction *countInst)
    : SpirvBitField(IK_BitFieldInsert, spv::Op::OpBitFieldInsert, resultType,
                    resultId, loc, baseInst, offsetInst, countInst),
      insert(insertInst) {}

SpirvComposite::SpirvComposite(
    QualType resultType, uint32_t resultId, SourceLocation loc,
    llvm::ArrayRef<SpirvInstruction *> constituentsVec, bool isConstant,
    bool isSpecConstant)
    : SpirvInstruction(IK_Composite,
                       isSpecConstant
                           ? spv::Op::OpSpecConstantComposite
                           : isConstant ? spv::Op::OpConstantComposite
                                        : spv::Op::OpCompositeConstruct,
                       resultType, resultId, loc),
      consituents(constituentsVec.begin(), constituentsVec.end()) {}

SpirvConstant::SpirvConstant(Kind kind, spv::Op op, QualType resultType,
                             uint32_t resultId, SourceLocation loc)
    : SpirvInstruction(kind, op, resultType, resultId, loc) {}

SpirvConstantBoolean::SpirvConstantBoolean(bool val, QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc)
    : SpirvConstant(IK_ConstantBoolean,
                    val ? spv::Op::OpConstantTrue : spv::Op::OpConstantFalse,
                    resultType, resultId, loc),
      value(val) {}

SpirvConstantInteger::SpirvConstantInteger(uint16_t val, QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc)
    : SpirvConstant(IK_ConstantInteger, spv::Op::OpConstant, resultType,
                    resultId, loc),
      bitwidth(16), value(static_cast<uint64_t>(val)) {
  assert(resultType->isUnsignedIntegerType());
}

SpirvConstantInteger::SpirvConstantInteger(int16_t val, QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc)
    : SpirvConstant(IK_ConstantInteger, spv::Op::OpConstant, resultType,
                    resultId, loc),
      bitwidth(16), value(static_cast<uint64_t>(val)) {
  assert(resultType->isSignedIntegerType());
}

SpirvConstantInteger::SpirvConstantInteger(uint32_t val, QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc)
    : SpirvConstant(IK_ConstantInteger, spv::Op::OpConstant, resultType,
                    resultId, loc),
      bitwidth(32), value(static_cast<uint64_t>(val)) {
  assert(resultType->isUnsignedIntegerType());
}

SpirvConstantInteger::SpirvConstantInteger(int32_t val, QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc)
    : SpirvConstant(IK_ConstantInteger, spv::Op::OpConstant, resultType,
                    resultId, loc),
      bitwidth(32), value(static_cast<uint64_t>(val)) {
  assert(resultType->isSignedIntegerType());
}

SpirvConstantInteger::SpirvConstantInteger(uint64_t val, QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc)
    : SpirvConstant(IK_ConstantInteger, spv::Op::OpConstant, resultType,
                    resultId, loc),
      bitwidth(64), value(val) {
  assert(resultType->isUnsignedIntegerType());
}

SpirvConstantInteger::SpirvConstantInteger(int64_t val, QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc)
    : SpirvConstant(IK_ConstantInteger, spv::Op::OpConstant, resultType,
                    resultId, loc),
      bitwidth(64), value(static_cast<uint64_t>(val)) {
  assert(resultType->isSignedIntegerType());
}

uint16_t SpirvConstantInteger::getUnsignedInt16Value() const {
  assert(!isSigned());
  assert(bitwidth == 16);
  return static_cast<uint16_t>(value);
}

int16_t SpirvConstantInteger::getSignedInt16Value() const {
  assert(isSigned());
  assert(bitwidth == 16);
  return static_cast<int16_t>(value);
}

uint32_t SpirvConstantInteger::getUnsignedInt32Value() const {
  assert(!isSigned());
  assert(bitwidth == 32);
  return static_cast<uint32_t>(value);
}

int32_t SpirvConstantInteger::getSignedInt32Value() const {
  assert(isSigned());
  assert(bitwidth == 32);
  return static_cast<int32_t>(value);
}

uint64_t SpirvConstantInteger::getUnsignedInt64Value() const {
  assert(!isSigned());
  assert(bitwidth == 64);
  return value;
}

int64_t SpirvConstantInteger::getSignedInt64Value() const {
  assert(isSigned());
  assert(bitwidth == 64);
  return static_cast<int64_t>(value);
}

SpirvConstantFloat::SpirvConstantFloat(uint16_t val, QualType resultType,
                                       uint32_t resultId, SourceLocation loc)
    : SpirvConstant(IK_ConstantFloat, spv::Op::OpConstant, resultType, resultId,
                    loc),
      bitwidth(16), value(static_cast<uint64_t>(val)) {}

SpirvConstantFloat::SpirvConstantFloat(float val, QualType resultType,
                                       uint32_t resultId, SourceLocation loc)
    : SpirvConstant(IK_ConstantFloat, spv::Op::OpConstant, resultType, resultId,
                    loc),
      bitwidth(32),
      value(static_cast<uint64_t>(cast::BitwiseCast<uint32_t, float>(val))) {}

SpirvConstantFloat::SpirvConstantFloat(double val, QualType resultType,
                                       uint32_t resultId, SourceLocation loc)
    : SpirvConstant(IK_ConstantFloat, spv::Op::OpConstant, resultType, resultId,
                    loc),
      bitwidth(64), value(cast::BitwiseCast<uint64_t, double>(val)) {}

uint16_t SpirvConstantFloat::getValue16() const {
  assert(bitwidth == 16);
  return static_cast<uint16_t>(value);
}

float SpirvConstantFloat::getValue32() const {
  assert(bitwidth == 32);
  return cast::BitwiseCast<float, uint32_t>(static_cast<uint32_t>(value));
}

double SpirvConstantFloat::getValue64() const {
  assert(bitwidth == 64);
  return cast::BitwiseCast<double, uint64_t>(value);
}

SpirvConstantComposite::SpirvConstantComposite(
    llvm::ArrayRef<SpirvConstant *> constituentsVec, QualType resultType,
    uint32_t resultId, SourceLocation loc)
    : SpirvConstant(IK_ConstantComposite, spv::Op::OpConstantComposite,
                    resultType, resultId, loc),
      constituents(constituentsVec) {}

SpirvConstantNull::SpirvConstantNull(QualType resultType, uint32_t resultId,
                                     SourceLocation loc)
    : SpirvConstant(IK_ConstantNull, spv::Op::OpConstantNull, resultType,
                    resultId, loc) {}

SpirvCompositeExtract::SpirvCompositeExtract(QualType resultType,
                                             uint32_t resultId,
                                             SourceLocation loc,
                                             SpirvInstruction *compositeInst,
                                             llvm::ArrayRef<uint32_t> indexVec)
    : SpirvInstruction(IK_CompositeExtract, spv::Op::OpCompositeExtract,
                       resultType, resultId, loc),
      composite(compositeInst), indices(indexVec.begin(), indexVec.end()) {}

SpirvCompositeInsert::SpirvCompositeInsert(QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc,
                                           SpirvInstruction *compositeInst,
                                           SpirvInstruction *objectInst,
                                           llvm::ArrayRef<uint32_t> indexVec)
    : SpirvInstruction(IK_CompositeInsert, spv::Op::OpCompositeInsert,
                       resultType, resultId, loc),
      composite(compositeInst), object(objectInst),
      indices(indexVec.begin(), indexVec.end()) {}

SpirvEmitVertex::SpirvEmitVertex(SourceLocation loc)
    : SpirvInstruction(IK_EmitVertex, spv::Op::OpEmitVertex, QualType(),
                       /*resultId=*/0, loc) {}

SpirvEndPrimitive::SpirvEndPrimitive(SourceLocation loc)
    : SpirvInstruction(IK_EndPrimitive, spv::Op::OpEndPrimitive, QualType(),
                       /*resultId=*/0, loc) {}

SpirvExtInst::SpirvExtInst(QualType resultType, uint32_t resultId,
                           SourceLocation loc, SpirvExtInstImport *set,
                           GLSLstd450 inst,
                           llvm::ArrayRef<SpirvInstruction *> operandsVec)
    : SpirvInstruction(IK_ExtInst, spv::Op::OpExtInst, resultType, resultId,
                       loc),
      instructionSet(set), instruction(inst),
      operands(operandsVec.begin(), operandsVec.end()) {}

SpirvFunctionCall::SpirvFunctionCall(QualType resultType, uint32_t resultId,
                                     SourceLocation loc, SpirvFunction *fn,
                                     llvm::ArrayRef<SpirvInstruction *> argsVec)
    : SpirvInstruction(IK_FunctionCall, spv::Op::OpFunctionCall, resultType,
                       resultId, loc),
      function(fn), args(argsVec.begin(), argsVec.end()) {}

SpirvGroupNonUniformOp::SpirvGroupNonUniformOp(Kind kind, spv::Op op,
                                               QualType resultType,
                                               uint32_t resultId,
                                               SourceLocation loc,
                                               spv::Scope scope)
    : SpirvInstruction(kind, op, resultType, resultId, loc), execScope(scope) {}

SpirvNonUniformBinaryOp::SpirvNonUniformBinaryOp(
    spv::Op op, QualType resultType, uint32_t resultId, SourceLocation loc,
    spv::Scope scope, SpirvInstruction *arg1Inst, SpirvInstruction *arg2Inst)
    : SpirvGroupNonUniformOp(IK_GroupNonUniformBinaryOp, op, resultType,
                             resultId, loc, scope),
      arg1(arg1Inst), arg2(arg2Inst) {
  assert(op == spv::Op::OpGroupNonUniformBroadcast ||
         op == spv::Op::OpGroupNonUniformBallotBitExtract ||
         op == spv::Op::OpGroupNonUniformShuffle ||
         op == spv::Op::OpGroupNonUniformShuffleXor ||
         op == spv::Op::OpGroupNonUniformShuffleUp ||
         op == spv::Op::OpGroupNonUniformShuffleDown ||
         op == spv::Op::OpGroupNonUniformQuadBroadcast ||
         op == spv::Op::OpGroupNonUniformQuadSwap);
}

SpirvNonUniformElect::SpirvNonUniformElect(QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc, spv::Scope scope)
    : SpirvGroupNonUniformOp(IK_GroupNonUniformElect,
                             spv::Op::OpGroupNonUniformElect, resultType,
                             resultId, loc, scope) {}

SpirvNonUniformUnaryOp::SpirvNonUniformUnaryOp(
    spv::Op op, QualType resultType, uint32_t resultId, SourceLocation loc,
    spv::Scope scope, llvm::Optional<spv::GroupOperation> group,
    SpirvInstruction *argInst)
    : SpirvGroupNonUniformOp(IK_GroupNonUniformUnaryOp, op, resultType,
                             resultId, loc, scope),
      arg(argInst), groupOp(group) {
  assert(op == spv::Op::OpGroupNonUniformAll ||
         op == spv::Op::OpGroupNonUniformAny ||
         op == spv::Op::OpGroupNonUniformAllEqual ||
         op == spv::Op::OpGroupNonUniformBroadcastFirst ||
         op == spv::Op::OpGroupNonUniformBallot ||
         op == spv::Op::OpGroupNonUniformInverseBallot ||
         op == spv::Op::OpGroupNonUniformBallotBitCount ||
         op == spv::Op::OpGroupNonUniformBallotFindLSB ||
         op == spv::Op::OpGroupNonUniformBallotFindMSB ||
         op == spv::Op::OpGroupNonUniformIAdd ||
         op == spv::Op::OpGroupNonUniformFAdd ||
         op == spv::Op::OpGroupNonUniformIMul ||
         op == spv::Op::OpGroupNonUniformFMul ||
         op == spv::Op::OpGroupNonUniformSMin ||
         op == spv::Op::OpGroupNonUniformUMin ||
         op == spv::Op::OpGroupNonUniformFMin ||
         op == spv::Op::OpGroupNonUniformSMax ||
         op == spv::Op::OpGroupNonUniformUMax ||
         op == spv::Op::OpGroupNonUniformFMax ||
         op == spv::Op::OpGroupNonUniformBitwiseAnd ||
         op == spv::Op::OpGroupNonUniformBitwiseOr ||
         op == spv::Op::OpGroupNonUniformBitwiseXor ||
         op == spv::Op::OpGroupNonUniformLogicalAnd ||
         op == spv::Op::OpGroupNonUniformLogicalOr ||
         op == spv::Op::OpGroupNonUniformLogicalXor);
}

SpirvImageOp::SpirvImageOp(
    spv::Op op, QualType resultType, uint32_t resultId, SourceLocation loc,
    SpirvInstruction *imageInst, SpirvInstruction *coordinateInst,
    spv::ImageOperandsMask mask, SpirvInstruction *drefInst,
    SpirvInstruction *biasInst, SpirvInstruction *lodInst,
    SpirvInstruction *gradDxInst, SpirvInstruction *gradDyInst,
    SpirvInstruction *constOffsetInst, SpirvInstruction *offsetInst,
    SpirvInstruction *constOffsetsInst, SpirvInstruction *sampleInst,
    SpirvInstruction *minLodInst, SpirvInstruction *componentInst,
    SpirvInstruction *texelToWriteInst)
    : SpirvInstruction(IK_ImageOp, op, resultType, resultId, loc),
      image(imageInst), coordinate(coordinateInst), dref(drefInst),
      bias(biasInst), lod(lodInst), gradDx(gradDxInst), gradDy(gradDyInst),
      constOffset(constOffsetInst), offset(offsetInst),
      constOffsets(constOffsetsInst), sample(sampleInst), minLod(minLodInst),
      component(componentInst), texelToWrite(texelToWriteInst),
      operandsMask(mask) {
  assert(op == spv::Op::OpImageSampleImplicitLod ||
         op == spv::Op::OpImageSampleExplicitLod ||
         op == spv::Op::OpImageSampleDrefImplicitLod ||
         op == spv::Op::OpImageSampleDrefExplicitLod ||
         op == spv::Op::OpImageSparseSampleImplicitLod ||
         op == spv::Op::OpImageSparseSampleExplicitLod ||
         op == spv::Op::OpImageSparseSampleDrefImplicitLod ||
         op == spv::Op::OpImageSparseSampleDrefExplicitLod ||
         op == spv::Op::OpImageFetch || op == spv::Op::OpImageSparseFetch ||
         op == spv::Op::OpImageGather || op == spv::Op::OpImageSparseGather ||
         op == spv::Op::OpImageDrefGather ||
         op == spv::Op::OpImageSparseDrefGather || op == spv::Op::OpImageRead ||
         op == spv::Op::OpImageSparseRead || op == spv::Op::OpImageWrite);

  if (op == spv::Op::OpImageSampleExplicitLod ||
      op == spv::Op::OpImageSampleDrefExplicitLod ||
      op == spv::Op::OpImageSparseSampleExplicitLod ||
      op == spv::Op::OpImageSparseSampleDrefExplicitLod) {
    assert(lod != 0);
  }
  if (op == spv::Op::OpImageSampleDrefImplicitLod ||
      op == spv::Op::OpImageSampleDrefExplicitLod ||
      op == spv::Op::OpImageSparseSampleDrefImplicitLod ||
      op == spv::Op::OpImageSparseSampleDrefExplicitLod ||
      op == spv::Op::OpImageDrefGather ||
      op == spv::Op::OpImageSparseDrefGather) {
    assert(dref != 0);
  }
  if (op == spv::Op::OpImageWrite) {
    assert(texelToWrite != 0);
  }
  if (op == spv::Op::OpImageGather || op == spv::Op::OpImageSparseGather) {
    assert(component != 0);
  }
}

SpirvImageQuery::SpirvImageQuery(spv::Op op, QualType resultType,
                                 uint32_t resultId, SourceLocation loc,
                                 SpirvInstruction *img,
                                 SpirvInstruction *lodInst,
                                 SpirvInstruction *coordInst)
    : SpirvInstruction(IK_ImageQuery, op, resultType, resultId, loc),
      image(img), lod(lodInst), coordinate(coordInst) {
  assert(op == spv::Op::OpImageQueryFormat ||
         op == spv::Op::OpImageQueryOrder || op == spv::Op::OpImageQuerySize ||
         op == spv::Op::OpImageQueryLevels ||
         op == spv::Op::OpImageQuerySamples || op == spv::Op::OpImageQueryLod ||
         op == spv::Op::OpImageQuerySizeLod);
  if (lodInst)
    assert(op == spv::Op::OpImageQuerySizeLod);
  if (coordInst)
    assert(op == spv::Op::OpImageQueryLod);
}

SpirvImageSparseTexelsResident::SpirvImageSparseTexelsResident(
    QualType resultType, uint32_t resultId, SourceLocation loc,
    SpirvInstruction *resCode)
    : SpirvInstruction(IK_ImageSparseTexelsResident,
                       spv::Op::OpImageSparseTexelsResident, resultType,
                       resultId, loc),
      residentCode(resCode) {}

SpirvImageTexelPointer::SpirvImageTexelPointer(QualType resultType,
                                               uint32_t resultId,
                                               SourceLocation loc,
                                               SpirvInstruction *imageInst,
                                               SpirvInstruction *coordinateInst,
                                               SpirvInstruction *sampleInst)
    : SpirvInstruction(IK_ImageTexelPointer, spv::Op::OpImageTexelPointer,
                       resultType, resultId, loc),
      image(imageInst), coordinate(coordinateInst), sample(sampleInst) {}

SpirvLoad::SpirvLoad(QualType resultType, uint32_t resultId, SourceLocation loc,
                     SpirvInstruction *pointerInst,
                     llvm::Optional<spv::MemoryAccessMask> mask)
    : SpirvInstruction(IK_Load, spv::Op::OpLoad, resultType, resultId, loc),
      pointer(pointerInst), memoryAccess(mask) {}

SpirvSampledImage::SpirvSampledImage(QualType resultType, uint32_t resultId,
                                     SourceLocation loc,
                                     SpirvInstruction *imageInst,
                                     SpirvInstruction *samplerInst)
    : SpirvInstruction(IK_SampledImage, spv::Op::OpSampledImage, resultType,
                       resultId, loc),
      image(imageInst), sampler(samplerInst) {}

SpirvSelect::SpirvSelect(QualType resultType, uint32_t resultId,
                         SourceLocation loc, SpirvInstruction *cond,
                         SpirvInstruction *trueInst,
                         SpirvInstruction *falseInst)
    : SpirvInstruction(IK_Select, spv::Op::OpSelect, resultType, resultId, loc),
      condition(cond), trueObject(trueInst), falseObject(falseInst) {}

SpirvSpecConstantBinaryOp::SpirvSpecConstantBinaryOp(
    spv::Op specConstantOp, QualType resultType, uint32_t resultId,
    SourceLocation loc, SpirvInstruction *op1, SpirvInstruction *op2)
    : SpirvInstruction(IK_SpecConstantBinaryOp, spv::Op::OpSpecConstantOp,
                       resultType, resultId, loc),
      specOp(specConstantOp), operand1(op1), operand2(op2) {}

SpirvSpecConstantUnaryOp::SpirvSpecConstantUnaryOp(spv::Op specConstantOp,
                                                   QualType resultType,
                                                   uint32_t resultId,
                                                   SourceLocation loc,
                                                   SpirvInstruction *op)
    : SpirvInstruction(IK_SpecConstantUnaryOp, spv::Op::OpSpecConstantOp,
                       resultType, resultId, loc),
      specOp(specConstantOp), operand(op) {}

SpirvStore::SpirvStore(SourceLocation loc, SpirvInstruction *pointerInst,
                       SpirvInstruction *objectInst,
                       llvm::Optional<spv::MemoryAccessMask> mask)
    : SpirvInstruction(IK_Store, spv::Op::OpStore, QualType(),
                       /*resultId=*/0, loc),
      pointer(pointerInst), object(objectInst), memoryAccess(mask) {}

SpirvUnaryOp::SpirvUnaryOp(spv::Op opcode, QualType resultType,
                           uint32_t resultId, SourceLocation loc,
                           SpirvInstruction *op)
    : SpirvInstruction(IK_UnaryOp, opcode, resultType, resultId, loc),
      operand(op) {}

SpirvVectorShuffle::SpirvVectorShuffle(QualType resultType, uint32_t resultId,
                                       SourceLocation loc,
                                       SpirvInstruction *vec1Inst,
                                       SpirvInstruction *vec2Inst,
                                       llvm::ArrayRef<uint32_t> componentsVec)
    : SpirvInstruction(IK_VectorShuffle, spv::Op::OpVectorShuffle, resultType,
                       resultId, loc),
      vec1(vec1Inst), vec2(vec2Inst),
      components(componentsVec.begin(), componentsVec.end()) {}

} // namespace spirv
} // namespace clang
