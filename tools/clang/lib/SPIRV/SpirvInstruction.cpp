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
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvCopyObject)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSampledImage)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSelect)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSpecConstantBinaryOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvSpecConstantUnaryOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvStore)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvUnaryOp)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvVectorShuffle)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvArrayLength)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvRayTracingOpNV)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvDemoteToHelperInvocationEXT)
DEFINE_INVOKE_VISITOR_FOR_CLASS(SpirvRayQueryOpKHR)

#undef DEFINE_INVOKE_VISITOR_FOR_CLASS

SpirvInstruction::SpirvInstruction(Kind k, spv::Op op, QualType astType,
                                   SourceLocation loc)
    : kind(k), opcode(op), astResultType(astType), resultId(0), srcLoc(loc),
      debugName(), resultType(nullptr), resultTypeId(0),
      layoutRule(SpirvLayoutRule::Void), containsAlias(false),
      storageClass(spv::StorageClass::Function), isRValue_(false),
      isRelaxedPrecision_(false), isNonUniform_(false), isPrecise_(false) {}

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
    : SpirvInstruction(IK_Extension, spv::Op::OpExtension, QualType(), loc),
      extName(extensionName) {}

bool SpirvExtension::operator==(const SpirvExtension &that) const {
  return extName == that.extName;
}

SpirvExtInstImport::SpirvExtInstImport(SourceLocation loc,
                                       llvm::StringRef extensionName)
    : SpirvInstruction(IK_ExtInstImport, spv::Op::OpExtInstImport, QualType(),
                       loc),
      extName(extensionName) {}

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
      execModel(executionModel), entryPoint(entryPointFn), name(nameStr),
      interfaceVec(iface.begin(), iface.end()) {}

// OpExecutionMode and OpExecutionModeId instructions
SpirvExecutionMode::SpirvExecutionMode(SourceLocation loc, SpirvFunction *entry,
                                       spv::ExecutionMode em,
                                       llvm::ArrayRef<uint32_t> paramsVec,
                                       bool usesIdParams)
    : SpirvInstruction(IK_ExecutionMode,
                       usesIdParams ? spv::Op::OpExecutionModeId
                                    : spv::Op::OpExecutionMode,
                       QualType(), loc),
      entryPoint(entry), execMode(em),
      params(paramsVec.begin(), paramsVec.end()) {}

SpirvString::SpirvString(SourceLocation loc, llvm::StringRef stringLiteral)
    : SpirvInstruction(IK_String, spv::Op::OpString, QualType(), loc),
      str(stringLiteral) {}

SpirvSource::SpirvSource(SourceLocation loc, spv::SourceLanguage language,
                         uint32_t ver, SpirvString *fileString,
                         llvm::StringRef src)
    : SpirvInstruction(IK_Source, spv::Op::OpSource, QualType(), loc),
      lang(language), version(ver), file(fileString), source(src) {}

SpirvModuleProcessed::SpirvModuleProcessed(SourceLocation loc,
                                           llvm::StringRef processStr)
    : SpirvInstruction(IK_ModuleProcessed, spv::Op::OpModuleProcessed,
                       QualType(), loc),
      process(processStr) {}

SpirvDecoration::SpirvDecoration(SourceLocation loc,
                                 SpirvInstruction *targetInst,
                                 spv::Decoration decor,
                                 llvm::ArrayRef<uint32_t> p,
                                 llvm::Optional<uint32_t> idx)
    : SpirvInstruction(IK_Decoration, getDecorateOpcode(decor, idx),
                       /*type*/ {}, loc),
      target(targetInst), decoration(decor), index(idx),
      params(p.begin(), p.end()), idParams() {}

SpirvDecoration::SpirvDecoration(SourceLocation loc,
                                 SpirvInstruction *targetInst,
                                 spv::Decoration decor,
                                 llvm::StringRef strParam,
                                 llvm::Optional<uint32_t> idx)
    : SpirvInstruction(IK_Decoration, getDecorateOpcode(decor, idx),
                       /*type*/ {}, loc),
      target(targetInst), decoration(decor), index(idx), params(), idParams() {
  const auto &stringWords = string::encodeSPIRVString(strParam);
  params.insert(params.end(), stringWords.begin(), stringWords.end());
}

SpirvDecoration::SpirvDecoration(SourceLocation loc,
                                 SpirvInstruction *targetInst,
                                 spv::Decoration decor,
                                 llvm::ArrayRef<SpirvInstruction *> ids)
    : SpirvInstruction(IK_Decoration, spv::Op::OpDecorateId,
                       /*type*/ {}, loc),
      target(targetInst), decoration(decor), index(llvm::None), params(),
      idParams(ids.begin(), ids.end()) {}

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
         params == that.params && idParams == that.idParams &&
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

SpirvVariable::SpirvVariable(const SpirvType *spvType, SourceLocation loc,
                             spv::StorageClass sc, bool precise,
                             SpirvInstruction *initializerInst)
    : SpirvInstruction(IK_Variable, spv::Op::OpVariable, QualType(), loc),
      initializer(initializerInst), descriptorSet(-1), binding(-1),
      hlslUserType("") {
  setResultType(spvType);
  setStorageClass(sc);
  setPrecise(precise);
}

SpirvFunctionParameter::SpirvFunctionParameter(QualType resultType,
                                               bool isPrecise,
                                               SourceLocation loc)
    : SpirvInstruction(IK_FunctionParameter, spv::Op::OpFunctionParameter,
                       resultType, loc) {
  setPrecise(isPrecise);
}

SpirvFunctionParameter::SpirvFunctionParameter(const SpirvType *spvType,
                                               bool isPrecise,
                                               SourceLocation loc)
    : SpirvInstruction(IK_FunctionParameter, spv::Op::OpFunctionParameter,
                       QualType(), loc) {
  setResultType(spvType);
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

SpirvAccessChain::SpirvAccessChain(QualType resultType, SourceLocation loc,
                                   SpirvInstruction *baseInst,
                                   llvm::ArrayRef<SpirvInstruction *> indexVec)
    : SpirvInstruction(IK_AccessChain, spv::Op::OpAccessChain, resultType, loc),
      base(baseInst), indices(indexVec.begin(), indexVec.end()) {}

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
      consituents(constituentsVec.begin(), constituentsVec.end()) {}

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
      constituents(constituentsVec.begin(), constituentsVec.end()) {}

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
      composite(compositeInst), indices(indexVec.begin(), indexVec.end()) {}

SpirvCompositeInsert::SpirvCompositeInsert(QualType resultType,
                                           SourceLocation loc,
                                           SpirvInstruction *compositeInst,
                                           SpirvInstruction *objectInst,
                                           llvm::ArrayRef<uint32_t> indexVec)
    : SpirvInstruction(IK_CompositeInsert, spv::Op::OpCompositeInsert,
                       resultType, loc),
      composite(compositeInst), object(objectInst),
      indices(indexVec.begin(), indexVec.end()) {}

SpirvEmitVertex::SpirvEmitVertex(SourceLocation loc)
    : SpirvInstruction(IK_EmitVertex, spv::Op::OpEmitVertex, QualType(), loc) {}

SpirvEndPrimitive::SpirvEndPrimitive(SourceLocation loc)
    : SpirvInstruction(IK_EndPrimitive, spv::Op::OpEndPrimitive, QualType(),
                       loc) {}

SpirvExtInst::SpirvExtInst(QualType resultType, SourceLocation loc,
                           SpirvExtInstImport *set, uint32_t inst,
                           llvm::ArrayRef<SpirvInstruction *> operandsVec)
    : SpirvInstruction(IK_ExtInst, spv::Op::OpExtInst, resultType, loc),
      instructionSet(set), instruction(inst),
      operands(operandsVec.begin(), operandsVec.end()) {}

SpirvFunctionCall::SpirvFunctionCall(QualType resultType, SourceLocation loc,
                                     SpirvFunction *fn,
                                     llvm::ArrayRef<SpirvInstruction *> argsVec)
    : SpirvInstruction(IK_FunctionCall, spv::Op::OpFunctionCall, resultType,
                       loc),
      function(fn), args(argsVec.begin(), argsVec.end()) {}

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

SpirvCopyObject::SpirvCopyObject(QualType resultType, SourceLocation loc,
                                 SpirvInstruction *pointerInst)
    : SpirvInstruction(IK_CopyObject, spv::Op::OpCopyObject, resultType, loc),
      pointer(pointerInst) {}

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
      vec1(vec1Inst), vec2(vec2Inst),
      components(componentsVec.begin(), componentsVec.end()) {}

SpirvArrayLength::SpirvArrayLength(QualType resultType, SourceLocation loc,
                                   SpirvInstruction *structure_,
                                   uint32_t memberLiteral)
    : SpirvInstruction(IK_ArrayLength, spv::Op::OpArrayLength, resultType, loc),
      structure(structure_), arrayMember(memberLiteral) {}

SpirvRayTracingOpNV::SpirvRayTracingOpNV(
    QualType resultType, spv::Op opcode,
    llvm::ArrayRef<SpirvInstruction *> vecOperands, SourceLocation loc)
    : SpirvInstruction(IK_RayTracingOpNV, opcode, resultType, loc),
      operands(vecOperands.begin(), vecOperands.end()) {}

SpirvDemoteToHelperInvocationEXT::SpirvDemoteToHelperInvocationEXT(
    SourceLocation loc)
    : SpirvInstruction(IK_DemoteToHelperInvocationEXT,
                       spv::Op::OpDemoteToHelperInvocationEXT, /*QualType*/ {},
                       loc) {}

SpirvRayQueryOpKHR::SpirvRayQueryOpKHR(
    QualType resultType, spv::Op opcode,
    llvm::ArrayRef<SpirvInstruction *> vecOperands, bool flags,
    SourceLocation loc)
    : SpirvInstruction(IK_RayQueryOpKHR, opcode, resultType, loc),
      operands(vecOperands.begin(), vecOperands.end()), cullFlags(flags) {}
} // namespace spirv
} // namespace clang
