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

namespace clang {
namespace spirv {

// Image query instructions constructor.
SpirvImageQuery::SpirvImageQuery(spv::Op op, QualType type, uint32_t resultId,
                                 SourceLocation loc, uint32_t img,
                                 uint32_t lodId, uint32_t coordId)
    : SpirvInstruction(op, type, resultId, loc), image(img), lod(lodId),
      coordinate(coordId) {
  assert(op == spv::Op::OpImageQueryFormat || spv::Op::OpImageQueryOrder ||
         spv::Op::OpImageQuerySize || spv::Op::OpImageQueryLevels ||
         spv::Op::OpImageQuerySamples || spv::Op::OpImageQueryLod ||
         spv::Op::OpImageQuerySizeLod);
  if (lodId)
    assert(op == spv::Op::OpImageQuerySizeLod);
  if (coordId)
    assert(op == spv::Op::OpImageQueryLod);
}

// Atomic instructions
SpirvAtomic::SpirvAtomic(spv::Op op, QualType type, uint32_t resultId,
                         SourceLocation loc, uint32_t pointerId, spv::Scope s,
                         spv::MemorySemanticsMask mask, uint32_t valueId)
    : SpirvInstruction(op, type, resultId, loc), pointer(pointerId), scope(s),
      memorySemantic(mask),
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
SpirvAtomic::SpirvAtomic(spv::Op op, QualType type, uint32_t resultId,
                         SourceLocation loc, uint32_t pointerId, spv::Scope s,
                         spv::MemorySemanticsMask semanticsEqual,
                         spv::MemorySemanticsMask semanticsUnequal,
                         uint32_t valueId, uint32_t comparatorId)
    : SpirvInstruction(op, type, resultId, loc), pointer(pointerId), scope(s),
      memorySemantic(semanticsEqual), memorySemanticUnequal(semanticsUnequal),
      value(valueId), comparator(comparatorId) {
  assert(op == spv::Op::OpAtomicExchange);
}

// OpExecutionMode and OpExecutionModeId instructions
SpirvExecutionMode::SpirvExecutionMode(SourceLocation loc, uint32_t entryId,
                                       spv::ExecutionMode em,
                                       llvm::ArrayRef<uint32_t> paramsVec,
                                       bool usesIdParams)
    : SpirvInstruction(usesIdParams ? spv::Op::OpExecutionModeId
                                    : spv::Op::OpExecutionMode,
                       /*QualType*/ {}, /*result-id*/ 0, loc),
      entryPointId(entryId), execMode(em),
      params(paramsVec.begin(), paramsVec.end()) {}

// OpMemoryBarrier and OpControlBarrier instructions.
SpirvBarrier::SpirvBarrier(SourceLocation loc, spv::Scope memScope,
                           spv::MemorySemanticsMask memSemantics,
                           spv::Scope execScope)
    : SpirvInstruction(execScope == spv::Scope::Max ? spv::Op::OpMemoryBarrier
                                                    : spv::Op::OpControlBarrier,
                       /*QualType*/ {},
                       /*result-id*/ 0, loc),
      memoryScope(memScope), memorySemantics(memSemantics),
      executionScope(execScope) {}

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

// OpCompositeConstruct, OpConstantComposite, and OpSpecConstantComposite
// instructions
SpirvComposite::SpirvComposite(QualType type, uint32_t resultId,
                               SourceLocation loc,
                               llvm::ArrayRef<uint32_t> constituentsVec,
                               bool isConstant, bool isSpecConstant)
    : SpirvInstruction(isSpecConstant
                           ? spv::Op::OpSpecConstantComposite
                           : isConstant ? spv::Op::OpConstantComposite
                                        : spv::Op::OpCompositeConstruct,
                       type, resultId, loc),
      consituents(constituentsVec.begin(), constituentsVec.end()) {}

// Non-uniform unary instructions
SpirvNonUniformUnaryOp::SpirvNonUniformUnaryOp(
    spv::Op op, QualType type, uint32_t resultId, SourceLocation loc,
    spv::Scope scope, llvm::Optional<spv::GroupOperation> group, uint32_t argId)
    : SpirvGroupNonUniformOp(op, type, resultId, loc, scope), arg(argId),
      groupOp(group) {
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

// Non-uniform binary instructions
SpirvNonUniformBinaryOp::SpirvNonUniformBinaryOp(
    spv::Op op, QualType type, uint32_t resultId, SourceLocation loc,
    spv::Scope scope, uint32_t arg1Id, uint32_t arg2Id)
    : SpirvGroupNonUniformOp(op, type, resultId, loc, scope), arg1(arg1Id),
      arg2(arg2Id) {
  assert(op == spv::Op::OpGroupNonUniformBroadcast ||
         op == spv::Op::OpGroupNonUniformBallotBitExtract ||
         op == spv::Op::OpGroupNonUniformShuffle ||
         op == spv::Op::OpGroupNonUniformShuffleXor ||
         op == spv::Op::OpGroupNonUniformShuffleUp ||
         op == spv::Op::OpGroupNonUniformShuffleDown ||
         op == spv::Op::OpGroupNonUniformQuadBroadcast ||
         op == spv::Op::OpGroupNonUniformQuadSwap);
}

// Image instructions
SpirvImageOp::SpirvImageOp(spv::Op op, QualType type, uint32_t resultId,
                           SourceLocation loc, uint32_t imageId,
                           uint32_t coordinateId, spv::ImageOperandsMask mask,
                           uint32_t drefId, uint32_t biasId, uint32_t lodId,
                           uint32_t gradDxId, uint32_t gradDyId,
                           uint32_t constOffsetId, uint32_t offsetId,
                           uint32_t constOffsetsId, uint32_t sampleId,
                           uint32_t minLodId, uint32_t componentId,
                           uint32_t texelToWriteId)
    : SpirvInstruction(op, type, resultId, loc), image(imageId),
      coordinate(coordinateId), dref(drefId), bias(biasId), lod(lodId),
      gradDx(gradDxId), gradDy(gradDyId), constOffset(constOffsetId),
      offset(offsetId), constOffsets(constOffsetsId), sample(sampleId),
      minLod(minLodId), component(componentId), texelToWrite(texelToWriteId),
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

} // namespace spirv
} // namespace clang
