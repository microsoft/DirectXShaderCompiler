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
#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvType.h"
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
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvCompositeConstruct)
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
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvArrayLength)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvRayTracingOpNV)

#undef DEFINE_INVOKE_VISITOR_FOR_CLASS

SpirvInstruction::SpirvInstruction(Kind k, spv::Op op, QualType astType,
                                   SourceLocation loc)
    : kind(k), opcode(op), astResultType(astType), resultId(0), srcLoc(loc),
      debugName(), resultType(nullptr), resultTypeId(0),
      layoutRule(SpirvLayoutRule::Void), containsAlias(false),
      storageClass(spv::StorageClass::Function), isRValue_(false),
      isRelaxedPrecision_(false), isNonUniform_(false), isPrecise_(false) {}

void *SpirvInstruction::operator new(std::size_t size, const SpirvContext &ctx,
                                     std::size_t extra) {
  return ::operator new(size + extra, ctx);
}

void SpirvInstruction::setDebugName(const SpirvContext &ctx,
                                    llvm::StringRef name) {
  debugName = ctx.copyString(name);
}

bool SpirvInstruction::isArithmeticInstruction() const {
  switch (opcode) {
  case spv::Op::OpSNegate:
  case spv::Op::OpFNegate:
  case spv::Op::OpIAdd:
  case spv::Op::OpFAdd:
  case spv::Op::OpISub:
  case spv::Op::OpFSub:
  case spv::Op::OpIMul:
  case spv::Op::OpFMul:
  case spv::Op::OpUDiv:
  case spv::Op::OpSDiv:
  case spv::Op::OpFDiv:
  case spv::Op::OpUMod:
  case spv::Op::OpSRem:
  case spv::Op::OpSMod:
  case spv::Op::OpFRem:
  case spv::Op::OpFMod:
  case spv::Op::OpVectorTimesScalar:
  case spv::Op::OpMatrixTimesScalar:
  case spv::Op::OpVectorTimesMatrix:
  case spv::Op::OpMatrixTimesVector:
  case spv::Op::OpMatrixTimesMatrix:
  case spv::Op::OpOuterProduct:
  case spv::Op::OpDot:
  case spv::Op::OpIAddCarry:
  case spv::Op::OpISubBorrow:
  case spv::Op::OpUMulExtended:
  case spv::Op::OpSMulExtended:
    return true;
  default:
    return false;
  }
}

SpirvCapability::SpirvCapability(SourceLocation loc, spv::Capability cap)
    : SpirvInstruction(IK_Capability, spv::Op::OpCapability, QualType(), loc),
      capability(cap) {}

bool SpirvCapability::operator==(const SpirvCapability &that) const {
  return capability == that.capability;
}

SpirvExtension::SpirvExtension(SourceLocation loc,
                               llvm::StringRef extensionName)
    : SpirvInstruction(IK_Extension, spv::Op::OpExtension, QualType(), loc) {
  std::copy(extensionName.begin(), extensionName.end(),
            getTrailingObjects<char>());
  getTrailingObjects<char>()[extensionName.size()] = '\0';
}

SpirvExtension *SpirvExtension::Create(const SpirvContext &c,
                                       SourceLocation loc,
                                       llvm::StringRef extensionName) {
  return new (c, additionalSizeToAlloc<char>(extensionName.size() + 1))
      SpirvExtension(loc, extensionName);
}

bool SpirvExtension::operator==(const SpirvExtension &that) const {
  return getExtensionName() == that.getExtensionName();
}

SpirvExtInstImport::SpirvExtInstImport(SourceLocation loc,
                                       llvm::StringRef extensionName)
    : SpirvInstruction(IK_ExtInstImport, spv::Op::OpExtInstImport, QualType(),
                       loc) {
  std::copy(extensionName.begin(), extensionName.end(),
            getTrailingObjects<char>());
  getTrailingObjects<char>()[extensionName.size()] = '\0';
}

SpirvExtInstImport *SpirvExtInstImport::Create(const SpirvContext &c,
                                               SourceLocation loc,
                                               llvm::StringRef extensionName) {
  return new (c, additionalSizeToAlloc<char>(extensionName.size() + 1))
      SpirvExtInstImport(loc, extensionName);
}

SpirvMemoryModel::SpirvMemoryModel(spv::AddressingModel addrModel,
                                   spv::MemoryModel memModel)
    : SpirvInstruction(IK_MemoryModel, spv::Op::OpMemoryModel, QualType(),
                       /*SrcLoc*/ {}),
      addressModel(addrModel), memoryModel(memModel) {}

SpirvEntryPoint::SpirvEntryPoint(SourceLocation loc,
                                 spv::ExecutionModel executionModel,
                                 SpirvFunction *entryPointFn,
                                 llvm::StringRef nameStr,
                                 llvm::ArrayRef<SpirvVariable *> iface)
    : SpirvInstruction(IK_EntryPoint, spv::Op::OpEntryPoint, QualType(), loc),
      execModel(executionModel), entryPoint(entryPointFn),
      numInterfaceVars(iface.size()) {
  std::copy(iface.begin(), iface.end(), getTrailingObjects<SpirvVariable *>());
  std::copy(nameStr.begin(), nameStr.end(), getTrailingObjects<char>());
  getTrailingObjects<char>()[nameStr.size()] = '\0';
}

SpirvEntryPoint *
SpirvEntryPoint::Create(const SpirvContext &c, SourceLocation loc,
                        spv::ExecutionModel executionModel,
                        SpirvFunction *entryPoint, llvm::StringRef nameStr,
                        llvm::ArrayRef<SpirvVariable *> iface) {
  return new (c, additionalSizeToAlloc<SpirvVariable *, char>(
                     iface.size(), nameStr.size() + 1))
      SpirvEntryPoint(loc, executionModel, entryPoint, nameStr, iface);
}

// OpExecutionMode and OpExecutionModeId instructions
SpirvExecutionMode::SpirvExecutionMode(SourceLocation loc, SpirvFunction *entry,
                                       spv::ExecutionMode em,
                                       llvm::ArrayRef<uint32_t> paramsVec,
                                       bool usesIdParams)
    : SpirvInstruction(IK_ExecutionMode,
                       usesIdParams ? spv::Op::OpExecutionModeId
                                    : spv::Op::OpExecutionMode,
                       QualType(), loc),
      entryPoint(entry), execMode(em), numParams(paramsVec.size()) {
  std::copy(paramsVec.begin(), paramsVec.end(), getTrailingObjects<uint32_t>());
}

SpirvExecutionMode *
SpirvExecutionMode::Create(const SpirvContext &c, SourceLocation loc,
                           SpirvFunction *entry, spv::ExecutionMode em,
                           llvm::ArrayRef<uint32_t> paramsVec,
                           bool usesIdParams) {
  return new (c, additionalSizeToAlloc<uint32_t>(paramsVec.size()))
      SpirvExecutionMode(loc, entry, em, paramsVec, usesIdParams);
}

SpirvString::SpirvString(SourceLocation loc, llvm::StringRef stringLiteral)
    : SpirvInstruction(IK_String, spv::Op::OpString, QualType(), loc) {
  std::copy(stringLiteral.begin(), stringLiteral.end(),
            getTrailingObjects<char>());
  getTrailingObjects<char>()[stringLiteral.size()] = '\0';
}

SpirvString *SpirvString::Create(const SpirvContext &c, SourceLocation loc,
                                 llvm::StringRef stringLiteral) {
  return new (c, additionalSizeToAlloc<char>(stringLiteral.size() + 1))
      SpirvString(loc, stringLiteral);
}

SpirvSource::SpirvSource(SourceLocation loc, spv::SourceLanguage language,
                         uint32_t ver, SpirvString *fileString,
                         llvm::StringRef src)
    : SpirvInstruction(IK_Source, spv::Op::OpSource, QualType(), loc),
      lang(language), version(ver), file(fileString) {
  std::copy(src.begin(), src.end(), getTrailingObjects<char>());
  getTrailingObjects<char>()[src.size()] = '\0';
}

SpirvSource *SpirvSource::Create(const SpirvContext &c, SourceLocation loc,
                                 spv::SourceLanguage language, uint32_t ver,
                                 SpirvString *file, llvm::StringRef src) {
  return new (c, additionalSizeToAlloc<char>(src.size() + 1))
      SpirvSource(loc, language, ver, file, src);
}

SpirvModuleProcessed::SpirvModuleProcessed(SourceLocation loc,
                                           llvm::StringRef processStr)
    : SpirvInstruction(IK_ModuleProcessed, spv::Op::OpModuleProcessed,
                       QualType(), loc) {
  std::copy(processStr.begin(), processStr.end(), getTrailingObjects<char>());
  getTrailingObjects<char>()[processStr.size()] = '\0';
}

SpirvModuleProcessed *SpirvModuleProcessed::Create(const SpirvContext &c,
                                                   SourceLocation loc,
                                                   llvm::StringRef processStr) {
  return new (c, additionalSizeToAlloc<char>(processStr.size() + 1))
      SpirvModuleProcessed(loc, processStr);
}

SpirvDecoration::SpirvDecoration(SourceLocation loc,
                                 SpirvInstruction *targetInst,
                                 spv::Decoration decor,
                                 llvm::ArrayRef<uint32_t> p,
                                 llvm::Optional<uint32_t> idx)
    : SpirvInstruction(IK_Decoration, getDecorateOpcode(decor, idx),
                       /*type*/ {}, loc),
      target(targetInst), decoration(decor), index(idx), numParams(p.size()),
      numIdParams(0) {
  std::copy(p.begin(), p.end(), getTrailingObjects<uint32_t>());
}

SpirvDecoration *SpirvDecoration::Create(const SpirvContext &c,
                                         SourceLocation loc,
                                         SpirvInstruction *target,
                                         spv::Decoration decor,
                                         llvm::ArrayRef<uint32_t> p,
                                         llvm::Optional<uint32_t> idx) {
  return new (c,
              additionalSizeToAlloc<SpirvInstruction *, uint32_t>(0, p.size()))
      SpirvDecoration(loc, target, decor, p, idx);
}

SpirvDecoration::SpirvDecoration(SourceLocation loc,
                                 SpirvInstruction *targetInst,
                                 spv::Decoration decor,
                                 llvm::StringRef strParam,
                                 llvm::Optional<uint32_t> idx)
    : SpirvInstruction(IK_Decoration, getDecorateOpcode(decor, idx),
                       /*type*/ {}, loc),
      target(targetInst), decoration(decor), index(idx), numIdParams(0) {
  const auto stringWords = string::encodeSPIRVString(strParam);
  numParams = stringWords.size();
  std::copy(stringWords.begin(), stringWords.end(),
            getTrailingObjects<uint32_t>());
}

SpirvDecoration *SpirvDecoration::Create(const SpirvContext &c,
                                         SourceLocation loc,
                                         SpirvInstruction *targetInst,
                                         spv::Decoration decor,
                                         llvm::StringRef strParam,
                                         llvm::Optional<uint32_t> idx) {
  return new (c, additionalSizeToAlloc<SpirvInstruction *, uint32_t>(
                     0, strParam.size() / 4 + 1))
      SpirvDecoration(loc, targetInst, decor, strParam, idx);
}

SpirvDecoration::SpirvDecoration(SourceLocation loc,
                                 SpirvInstruction *targetInst,
                                 spv::Decoration decor,
                                 llvm::ArrayRef<SpirvInstruction *> ids)
    : SpirvInstruction(IK_Decoration, spv::Op::OpDecorateId,
                       /*type*/ {}, loc),
      target(targetInst), decoration(decor), index(llvm::None), numParams(0),
      numIdParams(ids.size()) {
  std::copy(ids.begin(), ids.end(), getTrailingObjects<SpirvInstruction *>());
}

SpirvDecoration *
SpirvDecoration::Create(const SpirvContext &c, SourceLocation loc,
                        SpirvInstruction *targetInst, spv::Decoration decor,
                        llvm::ArrayRef<SpirvInstruction *> ids) {
  return new (
      c, additionalSizeToAlloc<SpirvInstruction *, uint32_t>(ids.size(), 0))
      SpirvDecoration(loc, targetInst, decor, ids);
}

spv::Op SpirvDecoration::getDecorateOpcode(
    spv::Decoration decoration, const llvm::Optional<uint32_t> &memberIndex) {
  if (decoration == spv::Decoration::HlslSemanticGOOGLE ||
      decoration == spv::Decoration::UserTypeGOOGLE)
    return memberIndex.hasValue() ? spv::Op::OpMemberDecorateStringGOOGLE
                                  : spv::Op::OpDecorateStringGOOGLE;

  return memberIndex.hasValue() ? spv::Op::OpMemberDecorate
                                : spv::Op::OpDecorate;
}

bool SpirvDecoration::operator==(const SpirvDecoration &that) const {
  return target == that.target && decoration == that.decoration &&
         getParams() == that.getParams() &&
         getIdParams() == that.getIdParams() &&
         index.hasValue() == that.index.hasValue() &&
         (!index.hasValue() || index.getValue() == that.index.getValue());
}

SpirvVariable::SpirvVariable(QualType resultType, SourceLocation loc,
                             spv::StorageClass sc, bool precise,
                             SpirvInstruction *initializerInst)
    : SpirvInstruction(IK_Variable, spv::Op::OpVariable, resultType, loc),
      initializer(initializerInst), descriptorSet(-1), binding(-1),
      hlslUserType("") {
  setStorageClass(sc);
  setPrecise(precise);
}

void SpirvVariable::setHlslUserType(const SpirvContext &ctx,
                                    llvm::StringRef userType) {
  hlslUserType = ctx.copyString(userType);
}

SpirvFunctionParameter::SpirvFunctionParameter(QualType resultType,
                                               bool isPrecise,
                                               SourceLocation loc)
    : SpirvInstruction(IK_FunctionParameter, spv::Op::OpFunctionParameter,
                       resultType, loc) {
  setPrecise(isPrecise);
}

SpirvMerge::SpirvMerge(Kind kind, spv::Op op, SourceLocation loc,
                       SpirvBasicBlock *mergeLabel)
    : SpirvInstruction(kind, op, QualType(), loc), mergeBlock(mergeLabel) {}

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
    : SpirvInstruction(kind, op, QualType(), loc) {}

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
      defaultLabel(defaultLbl), numTargets(targetsVec.size()) {
  std::copy(targetsVec.begin(), targetsVec.end(),
            getTrailingObjects<std::pair<uint32_t, SpirvBasicBlock *>>());
}

SpirvSwitch *SpirvSwitch::Create(
    const SpirvContext &c, SourceLocation loc, SpirvInstruction *selector,
    SpirvBasicBlock *defaultLabel,
    llvm::ArrayRef<std::pair<uint32_t, SpirvBasicBlock *>> &targetsVec) {
  return new (c, additionalSizeToAlloc<std::pair<uint32_t, SpirvBasicBlock *>>(
                     targetsVec.size()))
      SpirvSwitch(loc, selector, defaultLabel, targetsVec);
}

// Switch instruction methods.
SpirvBasicBlock *SpirvSwitch::getTargetLabelForLiteral(uint32_t lit) const {
  for (auto pair : getTargets())
    if (pair.first == lit)
      return pair.second;
  return defaultLabel;
}

llvm::ArrayRef<SpirvBasicBlock *> SpirvSwitch::getTargetBranches() const {
  llvm::SmallVector<SpirvBasicBlock *, 4> branches;
  for (auto pair : getTargets())
    branches.push_back(pair.second);
  branches.push_back(defaultLabel);
  return branches;
}

SpirvUnreachable::SpirvUnreachable(SourceLocation loc)
    : SpirvTerminator(IK_Unreachable, spv::Op::OpUnreachable, loc) {}

SpirvAccessChain::SpirvAccessChain(QualType resultType, SourceLocation loc,
                                   SpirvInstruction *baseInst,
                                   llvm::ArrayRef<SpirvInstruction *> indexVec)
    : SpirvInstruction(IK_AccessChain, spv::Op::OpAccessChain, resultType, loc),
      base(baseInst), numIndices(indexVec.size()) {
  std::copy(indexVec.begin(), indexVec.end(),
            getTrailingObjects<SpirvInstruction *>());
}

SpirvAccessChain *
SpirvAccessChain::Create(const SpirvContext &c, QualType resultType,
                         SourceLocation loc, SpirvInstruction *base,
                         llvm::ArrayRef<SpirvInstruction *> indexVec) {
  return new (c, additionalSizeToAlloc<SpirvInstruction *>(indexVec.size()))
      SpirvAccessChain(resultType, loc, base, indexVec);
}

SpirvAtomic::SpirvAtomic(spv::Op op, QualType resultType, SourceLocation loc,
                         SpirvInstruction *pointerInst, spv::Scope s,
                         spv::MemorySemanticsMask mask,
                         SpirvInstruction *valueInst)
    : SpirvInstruction(IK_Atomic, op, resultType, loc), pointer(pointerInst),
      scope(s), memorySemantic(mask),
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

SpirvAtomic::SpirvAtomic(spv::Op op, QualType resultType, SourceLocation loc,
                         SpirvInstruction *pointerInst, spv::Scope s,
                         spv::MemorySemanticsMask semanticsEqual,
                         spv::MemorySemanticsMask semanticsUnequal,
                         SpirvInstruction *valueInst,
                         SpirvInstruction *comparatorInst)
    : SpirvInstruction(IK_Atomic, op, resultType, loc), pointer(pointerInst),
      scope(s), memorySemantic(semanticsEqual),
      memorySemanticUnequal(semanticsUnequal), value(valueInst),
      comparator(comparatorInst) {
  assert(op == spv::Op::OpAtomicCompareExchange);
}

SpirvBarrier::SpirvBarrier(SourceLocation loc, spv::Scope memScope,
                           spv::MemorySemanticsMask memSemantics,
                           llvm::Optional<spv::Scope> execScope)
    : SpirvInstruction(IK_Barrier,
                       execScope.hasValue() ? spv::Op::OpControlBarrier
                                            : spv::Op::OpMemoryBarrier,
                       QualType(), loc),
      memoryScope(memScope), memorySemantics(memSemantics),
      executionScope(execScope) {}

SpirvBinaryOp::SpirvBinaryOp(spv::Op opcode, QualType resultType,
                             SourceLocation loc, SpirvInstruction *op1,
                             SpirvInstruction *op2)
    : SpirvInstruction(IK_BinaryOp, opcode, resultType, loc), operand1(op1),
      operand2(op2) {}

SpirvBitField::SpirvBitField(Kind kind, spv::Op op, QualType resultType,
                             SourceLocation loc, SpirvInstruction *baseInst,
                             SpirvInstruction *offsetInst,
                             SpirvInstruction *countInst)
    : SpirvInstruction(kind, op, resultType, loc), base(baseInst),
      offset(offsetInst), count(countInst) {}

SpirvBitFieldExtract::SpirvBitFieldExtract(
    QualType resultType, SourceLocation loc, SpirvInstruction *baseInst,
    SpirvInstruction *offsetInst, SpirvInstruction *countInst, bool isSigned)
    : SpirvBitField(IK_BitFieldExtract,
                    isSigned ? spv::Op::OpBitFieldSExtract
                             : spv::Op::OpBitFieldUExtract,
                    resultType, loc, baseInst, offsetInst, countInst) {}

SpirvBitFieldInsert::SpirvBitFieldInsert(QualType resultType,
                                         SourceLocation loc,
                                         SpirvInstruction *baseInst,
                                         SpirvInstruction *insertInst,
                                         SpirvInstruction *offsetInst,
                                         SpirvInstruction *countInst)
    : SpirvBitField(IK_BitFieldInsert, spv::Op::OpBitFieldInsert, resultType,
                    loc, baseInst, offsetInst, countInst),
      insert(insertInst) {}

SpirvCompositeConstruct::SpirvCompositeConstruct(
    QualType resultType, SourceLocation loc,
    llvm::ArrayRef<SpirvInstruction *> constituentsVec)
    : SpirvInstruction(IK_CompositeConstruct, spv::Op::OpCompositeConstruct,
                       resultType, loc),
      numConstituents(constituentsVec.size()) {
  std::copy(constituentsVec.begin(), constituentsVec.end(),
            getTrailingObjects<SpirvInstruction *>());
}

SpirvCompositeConstruct *SpirvCompositeConstruct::Create(
    const SpirvContext &c, QualType resultType, SourceLocation loc,
    llvm::ArrayRef<SpirvInstruction *> constituentsVec) {
  return new (c,
              additionalSizeToAlloc<SpirvInstruction *>(constituentsVec.size()))
      SpirvCompositeConstruct(resultType, loc, constituentsVec);
}

SpirvConstant::SpirvConstant(Kind kind, spv::Op op, const SpirvType *spvType)
    : SpirvInstruction(kind, op, QualType(),
                       /*SourceLocation*/ {}) {
  setResultType(spvType);
}

SpirvConstant::SpirvConstant(Kind kind, spv::Op op, QualType resultType)
    : SpirvInstruction(kind, op, resultType,
                       /*SourceLocation*/ {}) {}

bool SpirvConstant::isSpecConstant() const {
  return opcode == spv::Op::OpSpecConstant ||
         opcode == spv::Op::OpSpecConstantTrue ||
         opcode == spv::Op::OpSpecConstantFalse ||
         opcode == spv::Op::OpSpecConstantComposite;
}

SpirvConstantBoolean::SpirvConstantBoolean(QualType type, bool val,
                                           bool isSpecConst)
    : SpirvConstant(IK_ConstantBoolean,
                    val ? (isSpecConst ? spv::Op::OpSpecConstantTrue
                                       : spv::Op::OpConstantTrue)
                        : (isSpecConst ? spv::Op::OpSpecConstantFalse
                                       : spv::Op::OpConstantFalse),
                    type),
      value(val) {}

bool SpirvConstantBoolean::operator==(const SpirvConstantBoolean &that) const {
  return resultType == that.resultType && astResultType == that.astResultType &&
         value == that.value && opcode == that.opcode;
}

SpirvConstantInteger::SpirvConstantInteger(QualType type, llvm::APInt val,
                                           bool isSpecConst)
    : SpirvConstant(IK_ConstantInteger,
                    isSpecConst ? spv::Op::OpSpecConstant : spv::Op::OpConstant,
                    type),
      value(val) {
  assert(type->isIntegerType());
}

bool SpirvConstantInteger::operator==(const SpirvConstantInteger &that) const {
  return resultType == that.resultType && astResultType == that.astResultType &&
         value == that.value && opcode == that.opcode;
}

SpirvConstantFloat::SpirvConstantFloat(QualType type, llvm::APFloat val,
                                       bool isSpecConst)
    : SpirvConstant(IK_ConstantFloat,
                    isSpecConst ? spv::Op::OpSpecConstant : spv::Op::OpConstant,
                    type),
      value(val) {
  assert(type->isFloatingType());
}

bool SpirvConstantFloat::operator==(const SpirvConstantFloat &that) const {
  return resultType == that.resultType && astResultType == that.astResultType &&
         value.bitwiseIsEqual(that.value) && opcode == that.opcode;
}

SpirvConstantComposite::SpirvConstantComposite(
    QualType type, llvm::ArrayRef<SpirvConstant *> constituentsVec,
    bool isSpecConst)
    : SpirvConstant(IK_ConstantComposite,
                    isSpecConst ? spv::Op::OpSpecConstantComposite
                                : spv::Op::OpConstantComposite,
                    type),
      numConstituents(constituentsVec.size()) {
  std::copy(constituentsVec.begin(), constituentsVec.end(),
            getTrailingObjects<SpirvConstant *>());
}

SpirvConstantComposite *
SpirvConstantComposite::Create(const SpirvContext &c, QualType type,
                               llvm::ArrayRef<SpirvConstant *> constituents,
                               bool isSpecConst) {
  return new (c, additionalSizeToAlloc<SpirvConstant *>(constituents.size()))
      SpirvConstantComposite(type, constituents, isSpecConst);
}

SpirvConstantNull::SpirvConstantNull(QualType type)
    : SpirvConstant(IK_ConstantNull, spv::Op::OpConstantNull, type) {}

bool SpirvConstantNull::operator==(const SpirvConstantNull &that) const {
  return opcode == that.opcode && resultType == that.resultType &&
         astResultType == that.astResultType;
}

SpirvCompositeExtract::SpirvCompositeExtract(QualType resultType,
                                             SourceLocation loc,
                                             SpirvInstruction *compositeInst,
                                             llvm::ArrayRef<uint32_t> indexVec)
    : SpirvInstruction(IK_CompositeExtract, spv::Op::OpCompositeExtract,
                       resultType, loc),
      composite(compositeInst), numIndices(indexVec.size()) {
  std::copy(indexVec.begin(), indexVec.end(), getTrailingObjects<uint32_t>());
}

SpirvCompositeExtract *
SpirvCompositeExtract::Create(const SpirvContext &c, QualType resultType,
                              SourceLocation loc, SpirvInstruction *composite,
                              llvm::ArrayRef<uint32_t> indices) {
  return new (c, additionalSizeToAlloc<uint32_t>(indices.size()))
      SpirvCompositeExtract(resultType, loc, composite, indices);
}

SpirvCompositeInsert::SpirvCompositeInsert(QualType resultType,
                                           SourceLocation loc,
                                           SpirvInstruction *compositeInst,
                                           SpirvInstruction *objectInst,
                                           llvm::ArrayRef<uint32_t> indexVec)
    : SpirvInstruction(IK_CompositeInsert, spv::Op::OpCompositeInsert,
                       resultType, loc),
      composite(compositeInst), object(objectInst),
      numIndices(indexVec.size()) {
  std::copy(indexVec.begin(), indexVec.end(), getTrailingObjects<uint32_t>());
}

SpirvCompositeInsert *
SpirvCompositeInsert::Create(const SpirvContext &c, QualType resultType,
                             SourceLocation loc, SpirvInstruction *composite,
                             SpirvInstruction *object,
                             llvm::ArrayRef<uint32_t> indices) {
  return new (c, additionalSizeToAlloc<uint32_t>(indices.size()))
      SpirvCompositeInsert(resultType, loc, composite, object, indices);
}

SpirvEmitVertex::SpirvEmitVertex(SourceLocation loc)
    : SpirvInstruction(IK_EmitVertex, spv::Op::OpEmitVertex, QualType(), loc) {}

SpirvEndPrimitive::SpirvEndPrimitive(SourceLocation loc)
    : SpirvInstruction(IK_EndPrimitive, spv::Op::OpEndPrimitive, QualType(),
                       loc) {}

SpirvExtInst::SpirvExtInst(QualType resultType, SourceLocation loc,
                           SpirvExtInstImport *set, GLSLstd450 inst,
                           llvm::ArrayRef<SpirvInstruction *> operandsVec)
    : SpirvInstruction(IK_ExtInst, spv::Op::OpExtInst, resultType, loc),
      instructionSet(set), instruction(inst), numOperands(operandsVec.size()) {
  std::copy(operandsVec.begin(), operandsVec.end(),
            getTrailingObjects<SpirvInstruction *>());
}

SpirvExtInst *
SpirvExtInst::Create(const SpirvContext &c, QualType resultType,
                     SourceLocation loc, SpirvExtInstImport *set,
                     GLSLstd450 inst,
                     llvm::ArrayRef<SpirvInstruction *> operandsVec) {
  return new (c, additionalSizeToAlloc<SpirvInstruction *>(operandsVec.size()))
      SpirvExtInst(resultType, loc, set, inst, operandsVec);
}

SpirvFunctionCall::SpirvFunctionCall(QualType resultType, SourceLocation loc,
                                     SpirvFunction *fn,
                                     llvm::ArrayRef<SpirvInstruction *> argsVec)
    : SpirvInstruction(IK_FunctionCall, spv::Op::OpFunctionCall, resultType,
                       loc),
      function(fn), numArgs(argsVec.size()) {
  std::copy(argsVec.begin(), argsVec.end(),
            getTrailingObjects<SpirvInstruction *>());
}

SpirvFunctionCall *
SpirvFunctionCall::Create(const SpirvContext &c, QualType resultType,
                          SourceLocation loc, SpirvFunction *function,
                          llvm::ArrayRef<SpirvInstruction *> argsVec) {
  return new (c, additionalSizeToAlloc<SpirvInstruction *>(argsVec.size()))
      SpirvFunctionCall(resultType, loc, function, argsVec);
}

SpirvGroupNonUniformOp::SpirvGroupNonUniformOp(Kind kind, spv::Op op,
                                               QualType resultType,
                                               SourceLocation loc,
                                               spv::Scope scope)
    : SpirvInstruction(kind, op, resultType, loc), execScope(scope) {}

SpirvNonUniformBinaryOp::SpirvNonUniformBinaryOp(
    spv::Op op, QualType resultType, SourceLocation loc, spv::Scope scope,
    SpirvInstruction *arg1Inst, SpirvInstruction *arg2Inst)
    : SpirvGroupNonUniformOp(IK_GroupNonUniformBinaryOp, op, resultType, loc,
                             scope),
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
                                           SourceLocation loc, spv::Scope scope)
    : SpirvGroupNonUniformOp(IK_GroupNonUniformElect,
                             spv::Op::OpGroupNonUniformElect, resultType, loc,
                             scope) {}

SpirvNonUniformUnaryOp::SpirvNonUniformUnaryOp(
    spv::Op op, QualType resultType, SourceLocation loc, spv::Scope scope,
    llvm::Optional<spv::GroupOperation> group, SpirvInstruction *argInst)
    : SpirvGroupNonUniformOp(IK_GroupNonUniformUnaryOp, op, resultType, loc,
                             scope),
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
    spv::Op op, QualType resultType, SourceLocation loc,
    SpirvInstruction *imageInst, SpirvInstruction *coordinateInst,
    spv::ImageOperandsMask mask, SpirvInstruction *drefInst,
    SpirvInstruction *biasInst, SpirvInstruction *lodInst,
    SpirvInstruction *gradDxInst, SpirvInstruction *gradDyInst,
    SpirvInstruction *constOffsetInst, SpirvInstruction *offsetInst,
    SpirvInstruction *constOffsetsInst, SpirvInstruction *sampleInst,
    SpirvInstruction *minLodInst, SpirvInstruction *componentInst,
    SpirvInstruction *texelToWriteInst)
    : SpirvInstruction(IK_ImageOp, op, resultType, loc), image(imageInst),
      coordinate(coordinateInst), dref(drefInst), bias(biasInst), lod(lodInst),
      gradDx(gradDxInst), gradDy(gradDyInst), constOffset(constOffsetInst),
      offset(offsetInst), constOffsets(constOffsetsInst), sample(sampleInst),
      minLod(minLodInst), component(componentInst),
      texelToWrite(texelToWriteInst), operandsMask(mask) {
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
    assert(lod || (gradDx && gradDy));
  }
  if (op == spv::Op::OpImageSampleDrefImplicitLod ||
      op == spv::Op::OpImageSampleDrefExplicitLod ||
      op == spv::Op::OpImageSparseSampleDrefImplicitLod ||
      op == spv::Op::OpImageSparseSampleDrefExplicitLod ||
      op == spv::Op::OpImageDrefGather ||
      op == spv::Op::OpImageSparseDrefGather) {
    assert(dref);
  }
  if (op == spv::Op::OpImageWrite) {
    assert(texelToWrite);
  }
  if (op == spv::Op::OpImageGather || op == spv::Op::OpImageSparseGather) {
    assert(component);
  }
}

bool SpirvImageOp::isSparse() const {
  return opcode == spv::Op::OpImageSparseSampleImplicitLod ||
         opcode == spv::Op::OpImageSparseSampleExplicitLod ||
         opcode == spv::Op::OpImageSparseSampleDrefImplicitLod ||
         opcode == spv::Op::OpImageSparseSampleDrefExplicitLod ||
         opcode == spv::Op::OpImageSparseFetch ||
         opcode == spv::Op::OpImageSparseGather ||
         opcode == spv::Op::OpImageSparseDrefGather ||
         opcode == spv::Op::OpImageSparseRead;
}

SpirvImageQuery::SpirvImageQuery(spv::Op op, QualType resultType,
                                 SourceLocation loc, SpirvInstruction *img,
                                 SpirvInstruction *lodInst,
                                 SpirvInstruction *coordInst)
    : SpirvInstruction(IK_ImageQuery, op, resultType, loc), image(img),
      lod(lodInst), coordinate(coordInst) {
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
    QualType resultType, SourceLocation loc, SpirvInstruction *resCode)
    : SpirvInstruction(IK_ImageSparseTexelsResident,
                       spv::Op::OpImageSparseTexelsResident, resultType, loc),
      residentCode(resCode) {}

SpirvImageTexelPointer::SpirvImageTexelPointer(QualType resultType,
                                               SourceLocation loc,
                                               SpirvInstruction *imageInst,
                                               SpirvInstruction *coordinateInst,
                                               SpirvInstruction *sampleInst)
    : SpirvInstruction(IK_ImageTexelPointer, spv::Op::OpImageTexelPointer,
                       resultType, loc),
      image(imageInst), coordinate(coordinateInst), sample(sampleInst) {}

SpirvLoad::SpirvLoad(QualType resultType, SourceLocation loc,
                     SpirvInstruction *pointerInst,
                     llvm::Optional<spv::MemoryAccessMask> mask)
    : SpirvInstruction(IK_Load, spv::Op::OpLoad, resultType, loc),
      pointer(pointerInst), memoryAccess(mask) {}

SpirvSampledImage::SpirvSampledImage(QualType resultType, SourceLocation loc,
                                     SpirvInstruction *imageInst,
                                     SpirvInstruction *samplerInst)
    : SpirvInstruction(IK_SampledImage, spv::Op::OpSampledImage, resultType,
                       loc),
      image(imageInst), sampler(samplerInst) {}

SpirvSelect::SpirvSelect(QualType resultType, SourceLocation loc,
                         SpirvInstruction *cond, SpirvInstruction *trueInst,
                         SpirvInstruction *falseInst)
    : SpirvInstruction(IK_Select, spv::Op::OpSelect, resultType, loc),
      condition(cond), trueObject(trueInst), falseObject(falseInst) {}

SpirvSpecConstantBinaryOp::SpirvSpecConstantBinaryOp(spv::Op specConstantOp,
                                                     QualType resultType,
                                                     SourceLocation loc,
                                                     SpirvInstruction *op1,
                                                     SpirvInstruction *op2)
    : SpirvInstruction(IK_SpecConstantBinaryOp, spv::Op::OpSpecConstantOp,
                       resultType, loc),
      specOp(specConstantOp), operand1(op1), operand2(op2) {}

SpirvSpecConstantUnaryOp::SpirvSpecConstantUnaryOp(spv::Op specConstantOp,
                                                   QualType resultType,
                                                   SourceLocation loc,
                                                   SpirvInstruction *op)
    : SpirvInstruction(IK_SpecConstantUnaryOp, spv::Op::OpSpecConstantOp,
                       resultType, loc),
      specOp(specConstantOp), operand(op) {}

SpirvStore::SpirvStore(SourceLocation loc, SpirvInstruction *pointerInst,
                       SpirvInstruction *objectInst,
                       llvm::Optional<spv::MemoryAccessMask> mask)
    : SpirvInstruction(IK_Store, spv::Op::OpStore, QualType(), loc),
      pointer(pointerInst), object(objectInst), memoryAccess(mask) {}

SpirvUnaryOp::SpirvUnaryOp(spv::Op opcode, QualType resultType,
                           SourceLocation loc, SpirvInstruction *op)
    : SpirvInstruction(IK_UnaryOp, opcode, resultType, loc), operand(op) {}

bool SpirvUnaryOp::isConversionOp() const {
  return opcode == spv::Op::OpConvertFToU || opcode == spv::Op::OpConvertFToS ||
         opcode == spv::Op::OpConvertSToF || opcode == spv::Op::OpConvertUToF ||
         opcode == spv::Op::OpUConvert || opcode == spv::Op::OpSConvert ||
         opcode == spv::Op::OpFConvert || opcode == spv::Op::OpQuantizeToF16 ||
         opcode == spv::Op::OpBitcast;
}

SpirvVectorShuffle::SpirvVectorShuffle(QualType resultType, SourceLocation loc,
                                       SpirvInstruction *vec1Inst,
                                       SpirvInstruction *vec2Inst,
                                       llvm::ArrayRef<uint32_t> componentsVec)
    : SpirvInstruction(IK_VectorShuffle, spv::Op::OpVectorShuffle, resultType,
                       loc),
      vec1(vec1Inst), vec2(vec2Inst), numComponents(componentsVec.size()) {
  std::copy(componentsVec.begin(), componentsVec.end(),
            getTrailingObjects<uint32_t>());
}

SpirvVectorShuffle *
SpirvVectorShuffle::Create(const SpirvContext &c, QualType resultType,
                           SourceLocation loc, SpirvInstruction *vec1,
                           SpirvInstruction *vec2,
                           llvm::ArrayRef<uint32_t> componentsVec) {
  return new (c, additionalSizeToAlloc<uint32_t>(componentsVec.size()))
      SpirvVectorShuffle(resultType, loc, vec1, vec2, componentsVec);
}

SpirvArrayLength::SpirvArrayLength(QualType resultType, SourceLocation loc,
                                   SpirvInstruction *structure_,
                                   uint32_t memberLiteral)
    : SpirvInstruction(IK_ArrayLength, spv::Op::OpArrayLength, resultType, loc),
      structure(structure_), arrayMember(memberLiteral) {}

SpirvRayTracingOpNV::SpirvRayTracingOpNV(
    QualType resultType, spv::Op opcode,
    llvm::ArrayRef<SpirvInstruction *> vecOperands, SourceLocation loc)
    : SpirvInstruction(IK_RayTracingOpNV, opcode, resultType, loc),
      numOperands(vecOperands.size()) {
  std::copy(vecOperands.begin(), vecOperands.end(),
            getTrailingObjects<SpirvInstruction *>());
}

SpirvRayTracingOpNV *SpirvRayTracingOpNV::Create(
    const SpirvContext &c, QualType resultType, spv::Op opcode,
    llvm::ArrayRef<SpirvInstruction *> vecOperands, SourceLocation loc) {
  return new (c, additionalSizeToAlloc<SpirvInstruction *>(vecOperands.size()))
      SpirvRayTracingOpNV(resultType, opcode, vecOperands, loc);
}
} // namespace spirv
} // namespace clang
