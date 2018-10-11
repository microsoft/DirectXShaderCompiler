//===-- SpirvInstruction.h - SPIR-V Instruction Representation --*- C++ -*-===//
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
#include "clang/SPIRV/SpirvVisitor.h"

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
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvName)
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
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvComposite)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvCompositeExtract)
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
    : kind(k), opcode(op), resultType(type), resultId(id), srcLoc(loc) {}

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
                                 uint32_t entryPointId, llvm::StringRef nameStr,
                                 llvm::ArrayRef<uint32_t> iface)
    : SpirvInstruction(IK_EntryPoint, spv::Op::OpMemoryModel, QualType(),
                       /*resultId=*/0, loc),
      execModel(executionModel), entryPoint(entryPointId), name(nameStr),
      interfaceVec(iface.begin(), iface.end()) {}

// OpExecutionMode and OpExecutionModeId instructions
SpirvExecutionMode::SpirvExecutionMode(SourceLocation loc, uint32_t entryId,
                                       spv::ExecutionMode em,
                                       llvm::ArrayRef<uint32_t> paramsVec,
                                       bool usesIdParams)
    : SpirvInstruction(IK_ExecutionMode,
                       usesIdParams ? spv::Op::OpExecutionModeId
                                    : spv::Op::OpExecutionMode,
                       QualType(), /*resultId=*/0, loc),
      entryPointId(entryId), execMode(em),
      params(paramsVec.begin(), paramsVec.end()) {}

SpirvString::SpirvString(SourceLocation loc, llvm::StringRef stringLiteral)
    : SpirvInstruction(IK_String, spv::Op::OpString, QualType(),
                       /*resultId=*/0, loc),
      str(stringLiteral) {}

SpirvSource::SpirvSource(SourceLocation loc, spv::SourceLanguage language,
                         uint32_t ver, uint32_t fileId, llvm::StringRef src)
    : SpirvInstruction(IK_Source, spv::Op::OpSource, QualType(),
                       /*resultId=*/0, loc),
      lang(language), version(ver), file(fileId), source(src) {}

SpirvName::SpirvName(SourceLocation loc, uint32_t targetId,
                     llvm::StringRef nameStr,
                     llvm::Optional<uint32_t> memberIndex)
    : SpirvInstruction(IK_Name, spv::Op::OpMemberName, QualType(),
                       /*resultId=*/0, loc),
      target(targetId), member(memberIndex), name(nameStr) {}

SpirvModuleProcessed::SpirvModuleProcessed(SourceLocation loc,
                                           llvm::StringRef processStr)
    : SpirvInstruction(IK_ModuleProcessed, spv::Op::OpModuleProcessed,
                       QualType(),
                       /*resultId=*/0, loc),
      process(processStr) {}

SpirvDecoration::SpirvDecoration(SourceLocation loc, uint32_t targetId,
                                 spv::Decoration decor,
                                 llvm::ArrayRef<uint32_t> p,
                                 llvm::Optional<uint32_t> idx)
    : SpirvInstruction(IK_Decoration,
                       index.hasValue() ? spv::Op::OpMemberDecorate
                                        : spv::Op::OpDecorate,
                       /*type*/ {}, /*id*/ 0, loc),
      target(targetId), decoration(decor), params(p.begin(), p.end()),
      index(idx) {}

SpirvVariable::SpirvVariable(QualType resultType, uint32_t resultId,
                             SourceLocation loc, spv::StorageClass sc,
                             uint32_t initializerId)
    : SpirvInstruction(IK_Variable, spv::Op::OpVariable, resultType, resultId,
                       loc),
      storageClass(sc), initializer(initializerId) {}

SpirvFunctionParameter::SpirvFunctionParameter(QualType resultType,
                                               uint32_t resultId,
                                               SourceLocation loc)
    : SpirvInstruction(IK_FunctionParameter, spv::Op::OpFunctionParameter,
                       resultType, resultId, loc) {}

SpirvMerge::SpirvMerge(Kind kind, spv::Op op, SourceLocation loc,
                       uint32_t mergeBlockId)
    : SpirvInstruction(kind, op, QualType(), /*resultId=*/0, loc),
      mergeBlock(mergeBlockId) {}

SpirvLoopMerge::SpirvLoopMerge(SourceLocation loc, uint32_t mergeBlock,
                               uint32_t contTarget, spv::LoopControlMask mask)
    : SpirvMerge(IK_LoopMerge, spv::Op::OpLoopMerge, loc, mergeBlock),
      continueTarget(contTarget), loopControlMask(mask) {}

SpirvSelectionMerge::SpirvSelectionMerge(SourceLocation loc,
                                         uint32_t mergeBlock,
                                         spv::SelectionControlMask mask)
    : SpirvMerge(IK_SelectionMerge, spv::Op::OpSelectionMerge, loc, mergeBlock),
      selControlMask(mask) {}

SpirvTerminator::SpirvTerminator(Kind kind, spv::Op op, SourceLocation loc)
    : SpirvInstruction(kind, op, QualType(), /*resultId=*/0, loc) {}

SpirvBranching::SpirvBranching(Kind kind, spv::Op op, SourceLocation loc)
    : SpirvTerminator(kind, op, loc) {}

SpirvBranch::SpirvBranch(SourceLocation loc, uint32_t target)
    : SpirvBranching(IK_Branch, spv::Op::OpBranch, loc), targetLabel(target) {}

SpirvBranchConditional::SpirvBranchConditional(SourceLocation loc,
                                               uint32_t cond,
                                               uint32_t trueLabelId,
                                               uint32_t falseLabelId)
    : SpirvBranching(IK_BranchConditional, spv::Op::OpBranchConditional, loc),
      condition(cond), trueLabel(trueLabelId), falseLabel(falseLabelId) {}

SpirvKill::SpirvKill(SourceLocation loc)
    : SpirvTerminator(IK_Kill, spv::Op::OpKill, loc) {}

SpirvReturn::SpirvReturn(SourceLocation loc, uint32_t retVal)
    : SpirvTerminator(IK_Return,
                      retVal ? spv::Op::OpReturnValue : spv::Op::OpReturn, loc),
      returnValue(retVal) {}

SpirvSwitch::SpirvSwitch(
    SourceLocation loc, uint32_t selectorId, uint32_t defaultLabelId,
    llvm::ArrayRef<std::pair<uint32_t, uint32_t>> &targetsVec)
    : SpirvBranching(IK_Switch, spv::Op::OpSwitch, loc), selector(selectorId),
      defaultLabel(defaultLabelId),
      targets(targetsVec.begin(), targetsVec.end()) {}

// Switch instruction methods.
uint32_t SpirvSwitch::getTargetLabelForLiteral(uint32_t lit) const {
  for (auto pair : targets)
    if (pair.first == lit)
      return pair.second;
  return defaultLabel;
}

llvm::ArrayRef<uint32_t> SpirvSwitch::getTargetBranches() const {
  llvm::SmallVector<uint32_t, 4> branches;
  for (auto pair : targets)
    branches.push_back(pair.second);
  branches.push_back(defaultLabel);
  return branches;
}

SpirvUnreachable::SpirvUnreachable(SourceLocation loc)
    : SpirvTerminator(IK_Unreachable, spv::Op::OpUnreachable, loc) {}

SpirvAccessChain::SpirvAccessChain(QualType resultType, uint32_t resultId,
                                   SourceLocation loc, uint32_t baseId,
                                   llvm::ArrayRef<uint32_t> indexVec)
    : SpirvInstruction(IK_AccessChain, spv::Op::OpAccessChain, resultType,
                       resultId, loc),
      base(baseId), indices(indexVec.begin(), indexVec.end()) {}

SpirvAtomic::SpirvAtomic(spv::Op op, QualType resultType, uint32_t resultId,
                         SourceLocation loc, uint32_t pointerId, spv::Scope s,
                         spv::MemorySemanticsMask mask, uint32_t valueId)
    : SpirvInstruction(IK_Atomic, op, resultType, resultId, loc),
      pointer(pointerId), scope(s), memorySemantic(mask),
      memorySemanticUnequal(spv::MemorySemanticsMask::MaskNone), value(valueId),
      comparator(0) {
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
                         SourceLocation loc, uint32_t pointerId, spv::Scope s,
                         spv::MemorySemanticsMask semanticsEqual,
                         spv::MemorySemanticsMask semanticsUnequal,
                         uint32_t valueId, uint32_t comparatorId)
    : SpirvInstruction(IK_Atomic, op, resultType, resultId, loc),
      pointer(pointerId), scope(s), memorySemantic(semanticsEqual),
      memorySemanticUnequal(semanticsUnequal), value(valueId),
      comparator(comparatorId) {
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
                             uint32_t op1, uint32_t op2)
    : SpirvInstruction(IK_BinaryOp, opcode, resultType, resultId, loc),
      operand1(op1), operand2(op2) {}

SpirvBitField::SpirvBitField(Kind kind, spv::Op op, QualType resultType,
                             uint32_t resultId, SourceLocation loc,
                             uint32_t baseId, uint32_t offsetId,
                             uint32_t countId)
    : SpirvInstruction(kind, op, resultType, resultId, loc), base(baseId),
      offset(offsetId), count(countId) {}

SpirvBitFieldExtract::SpirvBitFieldExtract(QualType resultType,
                                           uint32_t resultId,
                                           SourceLocation loc, uint32_t baseId,
                                           uint32_t offsetId, uint32_t countId,
                                           bool isSigned)
    : SpirvBitField(IK_BitFieldExtract,
                    isSigned ? spv::Op::OpBitFieldSExtract
                             : spv::Op::OpBitFieldUExtract,
                    resultType, resultId, loc, baseId, offsetId, countId) {}

SpirvBitFieldInsert::SpirvBitFieldInsert(QualType resultType, uint32_t resultId,
                                         SourceLocation loc, uint32_t baseId,
                                         uint32_t insertId, uint32_t offsetId,
                                         uint32_t countId)
    : SpirvBitField(IK_BitFieldInsert, spv::Op::OpBitFieldInsert, resultType,
                    resultId, loc, baseId, offsetId, countId),
      insert(insertId) {}

SpirvComposite::SpirvComposite(QualType resultType, uint32_t resultId,
                               SourceLocation loc,
                               llvm::ArrayRef<uint32_t> constituentsVec,
                               bool isConstant, bool isSpecConstant)
    : SpirvInstruction(IK_Composite,
                       isSpecConstant
                           ? spv::Op::OpSpecConstantComposite
                           : isConstant ? spv::Op::OpConstantComposite
                                        : spv::Op::OpCompositeConstruct,
                       resultType, resultId, loc),
      consituents(constituentsVec.begin(), constituentsVec.end()) {}

SpirvCompositeExtract::SpirvCompositeExtract(QualType resultType,
                                             uint32_t resultId,
                                             SourceLocation loc,
                                             uint32_t compositeId,
                                             llvm::ArrayRef<uint32_t> indexVec)
    : SpirvInstruction(IK_CompositeExtract, spv::Op::OpCompositeExtract,
                       resultType, resultId, loc),
      composite(compositeId), indices(indexVec.begin(), indexVec.end()) {}

SpirvExtInst::SpirvExtInst(QualType resultType, uint32_t resultId,
                           SourceLocation loc, uint32_t setId, GLSLstd450 inst,
                           llvm::ArrayRef<uint32_t> operandsVec)
    : SpirvInstruction(IK_ExtInst, spv::Op::OpExtInst, resultType, resultId,
                       loc),
      instructionSetId(setId), instruction(inst),
      operands(operandsVec.begin(), operandsVec.end()) {}

SpirvFunctionCall::SpirvFunctionCall(QualType resultType, uint32_t resultId,
                                     SourceLocation loc, uint32_t fnId,
                                     llvm::ArrayRef<uint32_t> argsVec)
    : SpirvInstruction(IK_FunctionCall, spv::Op::OpFunctionCall, resultType,
                       resultId, loc),
      function(fnId), args(argsVec.begin(), argsVec.end()) {}

SpirvGroupNonUniformOp::SpirvGroupNonUniformOp(Kind kind, spv::Op op,
                                               QualType resultType,
                                               uint32_t resultId,
                                               SourceLocation loc,
                                               spv::Scope scope)
    : SpirvInstruction(kind, op, resultType, resultId, loc), execScope(scope) {}

SpirvNonUniformBinaryOp::SpirvNonUniformBinaryOp(
    spv::Op op, QualType resultType, uint32_t resultId, SourceLocation loc,
    spv::Scope scope, uint32_t arg1Id, uint32_t arg2Id)
    : SpirvGroupNonUniformOp(IK_GroupNonUniformBinaryOp, op, resultType,
                             resultId, loc, scope),
      arg1(arg1Id), arg2(arg2Id) {
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
    spv::Scope scope, llvm::Optional<spv::GroupOperation> group, uint32_t argId)
    : SpirvGroupNonUniformOp(IK_GroupNonUniformUnaryOp, op, resultType,
                             resultId, loc, scope),
      arg(argId), groupOp(group) {
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

SpirvImageOp::SpirvImageOp(spv::Op op, QualType resultType, uint32_t resultId,
                           SourceLocation loc, uint32_t imageId,
                           uint32_t coordinateId, spv::ImageOperandsMask mask,
                           uint32_t drefId, uint32_t biasId, uint32_t lodId,
                           uint32_t gradDxId, uint32_t gradDyId,
                           uint32_t constOffsetId, uint32_t offsetId,
                           uint32_t constOffsetsId, uint32_t sampleId,
                           uint32_t minLodId, uint32_t componentId,
                           uint32_t texelToWriteId)
    : SpirvInstruction(IK_ImageOp, op, resultType, resultId, loc),
      image(imageId), coordinate(coordinateId), dref(drefId), bias(biasId),
      lod(lodId), gradDx(gradDxId), gradDy(gradDyId),
      constOffset(constOffsetId), offset(offsetId),
      constOffsets(constOffsetsId), sample(sampleId), minLod(minLodId),
      component(componentId), texelToWrite(texelToWriteId), operandsMask(mask) {
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
                                 uint32_t img, uint32_t lodId, uint32_t coordId)
    : SpirvInstruction(IK_ImageQuery, op, resultType, resultId, loc),
      image(img), lod(lodId), coordinate(coordId) {
  assert(op == spv::Op::OpImageQueryFormat ||
         op == spv::Op::OpImageQueryOrder || op == spv::Op::OpImageQuerySize ||
         op == spv::Op::OpImageQueryLevels ||
         op == spv::Op::OpImageQuerySamples || op == spv::Op::OpImageQueryLod ||
         op == spv::Op::OpImageQuerySizeLod);
  if (lodId)
    assert(op == spv::Op::OpImageQuerySizeLod);
  if (coordId)
    assert(op == spv::Op::OpImageQueryLod);
}

SpirvImageSparseTexelsResident::SpirvImageSparseTexelsResident(
    QualType resultType, uint32_t resultId, SourceLocation loc,
    uint32_t resCode)
    : SpirvInstruction(IK_ImageSparseTexelsResident,
                       spv::Op::OpImageSparseTexelsResident, resultType,
                       resultId, loc),
      residentCode(resCode) {}

SpirvImageTexelPointer::SpirvImageTexelPointer(
    QualType resultType, uint32_t resultId, SourceLocation loc,
    uint32_t imageId, uint32_t coordinateId, uint32_t sampleId)
    : SpirvInstruction(IK_ImageTexelPointer, spv::Op::OpImageTexelPointer,
                       resultType, resultId, loc),
      image(imageId), coordinate(coordinateId), sample(sampleId) {}

SpirvLoad::SpirvLoad(QualType resultType, uint32_t resultId, SourceLocation loc,
                     uint32_t pointerId,
                     llvm::Optional<spv::MemoryAccessMask> mask)
    : SpirvInstruction(IK_Load, spv::Op::OpLoad, resultType, resultId, loc),
      pointer(pointerId), memoryAccess(mask) {}

SpirvSampledImage::SpirvSampledImage(QualType resultType, uint32_t resultId,
                                     SourceLocation loc, uint32_t imageId,
                                     uint32_t samplerId)
    : SpirvInstruction(IK_SampledImage, spv::Op::OpSampledImage, resultType,
                       resultId, loc),
      image(imageId), sampler(samplerId) {}

SpirvSelect::SpirvSelect(QualType resultType, uint32_t resultId,
                         SourceLocation loc, uint32_t cond, uint32_t trueId,
                         uint32_t falseId)
    : SpirvInstruction(IK_Select, spv::Op::OpSelect, resultType, resultId, loc),
      condition(cond), trueObject(trueId), falseObject(falseId) {}

SpirvSpecConstantBinaryOp::SpirvSpecConstantBinaryOp(spv::Op specConstantOp,
                                                     QualType resultType,
                                                     uint32_t resultId,
                                                     SourceLocation loc,
                                                     uint32_t op1, uint32_t op2)
    : SpirvInstruction(IK_SpecConstantBinaryOp, spv::Op::OpSpecConstantOp,
                       resultType, resultId, loc),
      specOp(specConstantOp), operand1(op1), operand2(op2) {}

SpirvSpecConstantUnaryOp::SpirvSpecConstantUnaryOp(spv::Op specConstantOp,
                                                   QualType resultType,
                                                   uint32_t resultId,
                                                   SourceLocation loc,
                                                   uint32_t op)
    : SpirvInstruction(IK_SpecConstantUnaryOp, spv::Op::OpSpecConstantOp,
                       resultType, resultId, loc),
      specOp(specConstantOp), operand(op) {}

SpirvStore::SpirvStore(SourceLocation loc, uint32_t pointerId,
                       uint32_t objectId,
                       llvm::Optional<spv::MemoryAccessMask> mask)
    : SpirvInstruction(IK_Store, spv::Op::OpStore, QualType(),
                       /*resultId=*/0, loc),
      pointer(pointerId), object(objectId), memoryAccess(mask) {}

SpirvUnaryOp::SpirvUnaryOp(spv::Op opcode, QualType resultType,
                           uint32_t resultId, SourceLocation loc, uint32_t op)
    : SpirvInstruction(IK_UnaryOp, opcode, resultType, resultId, loc),
      operand(op) {}

SpirvVectorShuffle::SpirvVectorShuffle(QualType resultType, uint32_t resultId,
                                       SourceLocation loc, uint32_t vec1Id,
                                       uint32_t vec2Id,
                                       llvm::ArrayRef<uint32_t> componentsVec)
    : SpirvInstruction(IK_VectorShuffle, spv::Op::OpVectorShuffle, resultType,
                       resultId, loc),
      vec1(vec1Id), vec2(vec2Id),
      components(componentsVec.begin(), componentsVec.end()) {}

} // namespace spirv
} // namespace clang
