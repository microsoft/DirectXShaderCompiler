//===-- SpirvInstruction.h - SPIR-V Instruction Representation --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//

#include "spirv/unified1/spirv.hpp11"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace spirv {

/// \brief The base class for representing SPIR-V instructions.
class SpirvInstruction {
public:
  virtual spv::Op getOpcode() { return opcode; }
  virtual QualType getResultType() { return resultType; }
  virtual uint32_t getResultId() { return resultId; }
  virtual clang::SourceLocation getSourceLocation() { return srcLoc; }

  // virtual SpirvVersion getSpirvVersion() = 0;
  // virtual SpirvExtension getSpirvExtension() = 0;
  // virtual llvm::StringRef getDebugName() = 0;

protected:
  SpirvInstruction(spv::Op op, QualType type, uint32_t id, SourceLocation loc)
      : opcode(op), resultType(type), resultId(id), srcLoc(loc) {}

private:
  spv::Op opcode;
  QualType resultType;
  uint32_t resultId;
  SourceLocation srcLoc;
};

/// \brief Represents SPIR-V Unary operation instructions. Includes:
/// ----------------------------------------------------------------------------
/// opTranspose     // Matrix capability
/// opDPdx
/// opDPdy
/// opFwidth
/// opDPdxFine     // DerivativeControl capability
/// opDPdyFine     // DerivativeControl capability
/// opFwidthFine   // DerivativeControl capability
/// opDPdxCoarse   // DerivativeControl capability
/// opDPdyCoarse   // DerivativeControl capability
/// opFwidthCoarse // DerivativeControl capability
/// ------------------------- Conversions --------------------------------------
/// OpConvertFToU
/// OpConvertFToS
/// OpConvertSToF
/// OpConvertUToF
/// OpUConvert
/// OpSConvert
/// OpFConvert
/// OpBitcast
/// ----------------------------------------------------------------------------
/// OpSNegate
/// OpFNegate
/// ----------------------------------------------------------------------------
/// opBitReverse
/// opBitCount
/// OpNot
/// ----------------------------- logical --------------------------------------
/// OpLogicalNot
/// OpAny
/// OpAll
/// OpIsNan
/// OpIsInf
/// OpIsFinite        // Kernel capability
/// ----------------------------------------------------------------------------
class SpirvUnaryInstr : public SpirvInstruction {
public:
  SpirvUnaryInstr(spv::Op opCode, QualType type, uint32_t resultId,
                  SourceLocation loc, uint32_t op)
      : SpirvInstruction(opCode, type, resultId, loc), operand(op) {}
  virtual uint32_t getOperand() { return operand; }

private:
  uint32_t operand;
};

/// \brief Represents SPIR-V binary operation instructions. Includes:
/// -------------------------- Arithmetic operations ---------------------------
/// OpIAdd
/// OpFAdd
/// OpISub
/// OpFSub
/// OpIMul
/// OpFMul
/// OpUDiv
/// OpSDiv
/// OpFDiv
/// OpUMod
/// OpSRem
/// OpSMod
/// OpFRem
/// OpFMod
/// OpVectorTimesScalar
/// OpMatrixTimesScalar
/// OpVectorTimesMatrix
/// OpMatrixTimesVector
/// OpMatrixTimesMatrix
/// OpOuterProduct
/// OpDot
/// ----------------------------- Shift operations -----------------------------
/// OpShiftRightLogical
/// OpShiftRightArithmetic
/// OpShiftLeftLogical
/// ------------------------  Bitwise logical operations -----------------------
/// OpBitwiseOr
/// OpBitwiseXor
/// OpBitwiseAnd
/// ----------------------------  Logical operations ---------------------------
/// OpLogicalEqual
/// OpLogicalNotEqual
/// OpLogicalOr
/// OpLogicalAnd
/// OpIEqual
/// OpINotEqual
/// OpUGreaterThan
/// OpSGreaterThan
/// OpUGreaterThanEqual
/// OpSGreaterThanEqual
/// OpULessThan
/// OpSLessThan
/// OpULessThanEqual
/// OpSLessThanEqual
/// OpFOrdEqual
/// OpFUnordEqual
/// OpFOrdNotEqual
/// OpFUnordNotEqual
/// OpFOrdLessThan
/// OpFUnordLessThan
/// OpFOrdGreaterThan
/// OpFUnordGreaterThan
/// OpFOrdLessThanEqual
/// OpFUnordLessThanEqual
/// OpFOrdGreaterThanEqual
/// OpFUnordGreaterThanEqual
/// -----------------------------  SpecConstant binary operations --------------
/// OpSpecConstantOp
/// ----------------------------------------------------------------------------
class SpirvBinaryInstr : public SpirvInstruction {
public:
  SpirvBinaryInstr(spv::Op opCode, QualType type, uint32_t resultId,
                   SourceLocation loc, uint32_t op1, uint32_t op2)
      : SpirvInstruction(opCode, type, resultId, loc), operand1(op1),
        operand2(op2) {}

  virtual uint32_t getOperand1() { return operand1; }
  virtual uint32_t getOperand2() { return operand2; }
  virtual bool isSpecConstantOp() {
    return getOpcode() == spv::Op::OpSpecConstantOp;
  }

private:
  uint32_t operand1;
  uint32_t operand2;
};

/// \brief Load instruction representation
class SpirvLoadInstr : public SpirvInstruction {
public:
  SpirvLoadInstr(QualType type, uint32_t resultId, SourceLocation loc,
                 uint32_t pointerId, llvm::Optional<spv::MemoryAccessMask> mask)
      : SpirvInstruction(spv::Op::OpLoad, type, resultId, loc),
        pointer(pointerId), memoryAccess(mask) {}

  virtual uint32_t getPointer() { return pointer; }
  virtual bool hasMemoryAccessSemantics() { return memoryAccess.hasValue(); }
  virtual spv::MemoryAccessMask getMemoryAccess() {
    return memoryAccess.getValue();
  }

private:
  uint32_t pointer;
  llvm::Optional<spv::MemoryAccessMask> memoryAccess;
};

/// \brief Store instruction representation
class SpirvStoreInstr : public SpirvInstruction {
public:
  SpirvStoreInstr(SourceLocation loc, uint32_t pointerId, uint32_t objectId,
                  llvm::Optional<spv::MemoryAccessMask> mask)
      : SpirvInstruction(spv::Op::OpStore, /* QualType */ {},
                         /*result id*/ 0, loc),
        pointer(pointerId), object(objectId), memoryAccess(mask) {}

  virtual uint32_t getPointer() { return pointer; }
  virtual uint32_t getObject() { return object; }
  virtual spv::MemoryAccessMask getMemoryAccess() {
    return memoryAccess.getValue();
  }
  virtual bool hasMemoryAccessSemantics() { return memoryAccess.hasValue(); }

private:
  uint32_t pointer;
  uint32_t object;
  llvm::Optional<spv::MemoryAccessMask> memoryAccess;
};

/// \brief Access Chain instruction representation (OpAccessChain)
/// Note: If needed, this class can be extended to cover Ptr access chains, and
/// InBounds access chains. These are currently not used by CodeGen.
class SpirvAccessChainInstr : public SpirvInstruction {
public:
  SpirvAccessChainInstr(QualType type, uint32_t resultId, SourceLocation loc,
                        uint32_t baseId,
                        llvm::SmallVector<uint32_t, 4> indexVec)
      : SpirvInstruction(spv::Op::OpAccessChain, type, resultId, loc),
        base(baseId), indexes(indexVec) {}
  virtual uint32_t getBase() { return base; }
  virtual llvm::ArrayRef<uint32_t> getIndexes() { return indexes; }

private:
  uint32_t base;
  llvm::SmallVector<uint32_t, 4> indexes;
};

/// \brief Select operation representation.
class SpirvSelectInstr : public SpirvInstruction {
public:
  SpirvSelectInstr(QualType type, uint32_t resultId, SourceLocation loc,
                   uint32_t cond, uint32_t trueId, uint32_t falseId)
      : SpirvInstruction(spv::Op::OpSelect, type, resultId, loc),
        condition(cond), trueObj(trueId), falseObj(falseId) {}
  virtual uint32_t getCondition() { return condition; }
  virtual uint32_t getTrueObject() { return trueObj; }
  virtual uint32_t getFalseObject() { return falseObj; }

private:
  uint32_t condition;
  uint32_t trueObj;
  uint32_t falseObj;
};

/// \brief Extension instruction
class SpirvExtensionInstr : public SpirvInstruction {
public:
  SpirvExtensionInstr(SourceLocation loc, llvm::StringRef extensionName)
      : SpirvInstruction(spv::Op::OpExtension, /* QualType */ {},
                         /* result-id */ 0, loc),
        extName(extensionName) {}
  llvm::StringRef getExtensionName() { return extName; }

private:
  std::string extName;
};

/// \brief ExtInstImport instruction
class SpirvExtInstImportInstr : public SpirvInstruction {
public:
  SpirvExtInstImportInstr(uint32_t resultId, SourceLocation loc,
                          llvm::StringRef extensionName)
      : SpirvInstruction(spv::Op::OpExtInstImport, /* QualType */ {}, resultId,
                         loc),
        extName(extensionName) {}
  llvm::StringRef getExtendedInstSetName() { return extName; }

private:
  std::string extName;
};

/// \brief ExtInst instruction
class SpirvExtInstInstr : public SpirvInstruction {
public:
  SpirvExtInstInstr(QualType type, uint32_t resultId, SourceLocation loc,
                    uint32_t setId, uint32_t inst,
                    llvm::SmallVector<uint32_t, 4> &operandsVec)
      : SpirvInstruction(spv::Op::OpExtInst, type, resultId, loc),
        instructionSetId(setId), instruction(inst), operands(operandsVec) {}
  uint32_t getInstructionSetId() { return instructionSetId; }
  uint32_t getInstruction() { return instruction; }
  llvm::ArrayRef<uint32_t> getOperands() { return operands; }

private:
  uint32_t instructionSetId;
  uint32_t instruction;
  llvm::SmallVector<uint32_t, 4> operands;
};

/// \brief Image query instructions. Includes:
/// Covers the following instructions:
/// OpImageQueryFormat  (image)
/// OpImageQueryOrder   (image)
/// OpImageQuerySize    (image)
/// OpImageQueryLevels  (image)
/// OpImageQuerySamples (image)
/// OpImageQueryLod     (image, coordinate)
/// OpImageQuerySizeLod (image, lod)
class SpirvImageQueryInstr : public SpirvInstruction {
public:
  SpirvImageQueryInstr(spv::Op op, QualType type, uint32_t resultId,
                       SourceLocation loc, uint32_t img, uint32_t lodId = 0,
                       uint32_t coordId = 0);
  uint32_t getImage() { return image; }
  uint32_t getLod() { return lod; }
  uint32_t hasLod() { return lod != 0; }
  uint32_t getCoordinate() { return coordinate; }
  uint32_t hasCoordinate() { return coordinate != 0; }

private:
  uint32_t image;
  uint32_t lod;
  uint32_t coordinate;
};

/// \brief OpImageSparseTexelsResident instruction
class SpirvImageSparseTexelsResidentInstr : public SpirvInstruction {
public:
  SpirvImageSparseTexelsResidentInstr(QualType type, uint32_t resultId,
                                      SourceLocation loc, uint32_t resCode)
      : SpirvInstruction(spv::Op::OpImageSparseTexelsResident, type, resultId,
                         loc),
        residentCode(resCode) {}
  uint32_t getResidentCode() { return residentCode; }

private:
  uint32_t residentCode;
};

/// \brief Atomic instructions. includes:
/// OpAtomicLoad           (pointer,scope,memorysemantics)
/// OpAtomicIncrement      (pointer,scope,memorysemantics)
/// OpAtomicDecrement      (pointer,scope,memorysemantics)
/// OpAtomicFlagClear      (pointer,scope,memorysemantics)
/// OpAtomicFlagTestAndSet (pointer,scope,memorysemantics)
/// OpAtomicStore          (pointer,scope,memorysemantics,value)
/// OpAtomicAnd            (pointer,scope,memorysemantics,value)
/// OpAtomicOr             (pointer,scope,memorysemantics,value)
/// OpAtomicXor            (pointer,scope,memorysemantics,value)
/// OpAtomicIAdd           (pointer,scope,memorysemantics,value)
/// OpAtomicISub           (pointer,scope,memorysemantics,value)
/// OpAtomicSMin           (pointer,scope,memorysemantics,value)
/// OpAtomicUMin           (pointer,scope,memorysemantics,value)
/// OpAtomicSMax           (pointer,scope,memorysemantics,value)
/// OpAtomicUMax           (pointer,scope,memorysemantics,value)
/// OpAtomicExchange       (pointer,scope,memorysemantics,value)
/// OpAtomicCompareExchange(pointer,scope,semaequal,semaunequal,value,comparator)
class SpirvAtomicInstr : public SpirvInstruction {
public:
  SpirvAtomicInstr(spv::Op opCode, QualType type, uint32_t resultId,
                   SourceLocation loc, uint32_t pointerId, spv::Scope,
                   spv::MemorySemanticsMask, uint32_t valueId = 0);
  SpirvAtomicInstr(spv::Op opCode, QualType type, uint32_t resultId,
                   SourceLocation loc, uint32_t pointerId, spv::Scope,
                   spv::MemorySemanticsMask semanticsEqual,
                   spv::MemorySemanticsMask semanticsUnequal, uint32_t value,
                   uint32_t comparatorId);

  virtual uint32_t getPointer() { return pointer; }
  virtual spv::Scope getScope() { return scope; }
  virtual spv::MemorySemanticsMask getMemorySemantics() {
    return memorySemantic;
  }
  virtual bool hasValue() { return value != 0; }
  virtual uint32_t getValue() { return value; }
  virtual bool hasComparator() { return comparator != 0; }
  virtual uint32_t getComparator() { return comparator; }
  virtual spv::MemorySemanticsMask getMemorySemanticsEqual() {
    return memorySemantic;
  }
  virtual spv::MemorySemanticsMask getMemorySemanticsUnequal() {
    return memorySemanticUnequal;
  }

private:
  uint32_t pointer;
  spv::Scope scope;
  spv::MemorySemanticsMask memorySemantic;
  spv::MemorySemanticsMask memorySemanticUnequal;
  uint32_t value;
  uint32_t comparator;
};

/// \brief OpVectorShuffle instruction
class SpirvVectorShuffleInstr : public SpirvInstruction {
public:
  SpirvVectorShuffleInstr(QualType type, uint32_t resultId, SourceLocation loc,
                          uint32_t vec1Id, uint32_t vec2Id,
                          llvm::SmallVector<uint32_t, 4> &componentsVec)
      : SpirvInstruction(spv::Op::OpVectorShuffle, type, resultId, loc),
        vec1(vec1Id), vec2(vec2Id), components(componentsVec) {}
  virtual uint32_t getVec1() { return vec1; }
  virtual uint32_t getVec2() { return vec2; }
  virtual llvm::ArrayRef<uint32_t> getComponents() { return components; }

private:
  uint32_t vec1;
  uint32_t vec2;
  llvm::SmallVector<uint32_t, 4> components;
};

/// \brief OpSource and OpSourceContinued instruction
class SpirvSourceInstr : public SpirvInstruction {
public:
  SpirvSourceInstr(SourceLocation loc, spv::SourceLanguage language,
                   uint32_t ver, llvm::Optional<uint32_t> fileId,
                   llvm::StringRef src)
      : SpirvInstruction(spv::Op::OpSource, /*QualType*/ {}, /*result-id*/ 0,
                         loc),
        lang(language), version(ver), file(fileId), source(src) {}

  spv::SourceLanguage getSourceLanguage() { return lang; }
  uint32_t getVersion() { return version; }
  bool hasFileId() { return file.hasValue(); }
  uint32_t getFileId() { return file.getValue(); }
  llvm::StringRef getSource() { return source; }

private:
  spv::SourceLanguage lang;
  uint32_t version;
  llvm::Optional<uint32_t> file;
  std::string source;
};

/// \brief OpSourceExtension instruction
class SpirvSourceExtensionInstr : public SpirvInstruction {
public:
  SpirvSourceExtensionInstr(SourceLocation loc, llvm::StringRef ext)
      : SpirvInstruction(spv::Op::OpSourceExtension, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        extension(ext) {}
  llvm::StringRef getExtension() { return extension; }

private:
  std::string extension;
};

/// \brief OpName instruction
class SpirvNameInstr : public SpirvInstruction {
public:
  SpirvNameInstr(SourceLocation loc, uint32_t targetId, llvm::StringRef nameStr)
      : SpirvInstruction(spv::Op::OpName, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        target(targetId), name(nameStr) {}
  uint32_t getTarget() { return target; }
  llvm::StringRef getName() { return name; }

private:
  uint32_t target;
  std::string name;
};

/// \brief OpMemberName instruction
class SpirvMemberNameInstr : public SpirvInstruction {
public:
  SpirvMemberNameInstr(SourceLocation loc, uint32_t structTypeId,
                       uint32_t memberNumber, llvm::StringRef nameStr)
      : SpirvInstruction(spv::Op::OpMemberName, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        targetType(structTypeId), member(memberNumber), name(nameStr) {}
  uint32_t getTargetType() { return targetType; }
  uint32_t getMember() { return member; }
  llvm::StringRef getName() { return name; }

private:
  uint32_t targetType;
  uint32_t member;
  std::string name;
};

/// \brief OpString instruction
class SpirvStringInstr : public SpirvInstruction {
public:
  SpirvStringInstr(SourceLocation loc, llvm::StringRef stringLiteral)
      : SpirvInstruction(spv::Op::OpString, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        str(stringLiteral) {}
  llvm::StringRef getString() { return str; }

private:
  std::string str;
};

/// \brief OpLine instruction
class SpirvLineInstr : public SpirvInstruction {
public:
  SpirvLineInstr(SourceLocation loc, uint32_t fileId, uint32_t lineLiteral,
                 uint32_t columnLiteral)
      : SpirvInstruction(spv::Op::OpLine, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        file(fileId), line(lineLiteral), column(columnLiteral) {}
  uint32_t getFileId() { return file; }
  uint32_t getLine() { return line; }
  uint32_t getColumn() { return column; }

private:
  uint32_t file;
  uint32_t line;
  uint32_t column;
};

/// \brief OpModuleProcessed instruction
class SpirvModuleProcessedInstr : public SpirvInstruction {
public:
  SpirvModuleProcessedInstr(SourceLocation loc, llvm::StringRef processStr)
      : SpirvInstruction(spv::Op::OpModuleProcessed, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        process(processStr) {}
  llvm::StringRef getProcess() { return process; }

private:
  std::string process;
};

/// \brief OpMemoryModel instruction
class SpirvMemoryModelInstr : public SpirvInstruction {
public:
  SpirvMemoryModelInstr(spv::AddressingModel addrModel,
                        spv::MemoryModel memModel)
      : SpirvInstruction(spv::Op::OpMemoryModel, /*QualType*/ {},
                         /*result-id*/ 0, /*SrcLoc*/ {}),
        addressModel(addrModel), memoryModel(memModel) {}

  spv::AddressingModel getAddressingModel() { return addressModel; }
  spv::MemoryModel getMemoryModel() { return memoryModel; }

private:
  spv::AddressingModel addressModel;
  spv::MemoryModel memoryModel;
};

/// \brief OpEntryPoint instruction
class SpirvEntryPointInstr : public SpirvInstruction {
public:
  SpirvEntryPointInstr(SourceLocation loc, spv::ExecutionModel executionModel,
                       uint32_t entryPointId, llvm::StringRef nameStr,
                       llvm::SmallVector<uint32_t, 4> &iface)
      : SpirvInstruction(spv::Op::OpMemoryModel, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        execModel(executionModel), entryPoint(entryPointId), name(nameStr),
        interfaceVec(iface) {}
  spv::ExecutionModel getExecModel() { return execModel; }
  uint32_t getEntryPointId() { return entryPoint; }
  llvm::StringRef getEntryPointName() { return name; }
  llvm::ArrayRef<uint32_t> getInterface() { return interfaceVec; }

private:
  spv::ExecutionModel execModel;
  uint32_t entryPoint;
  std::string name;
  llvm::SmallVector<uint32_t, 4> interfaceVec;
};

/// \brief OpExecutionMode and OpExecutionModeId instructions
class SpirvExecutionModeInstr : public SpirvInstruction {
public:
  SpirvExecutionModeInstr(SourceLocation loc, uint32_t entryPointId,
                          spv::ExecutionMode,
                          llvm::SmallVector<uint32_t, 4> &params,
                          bool usesIdParams);
  uint32_t getEntryPointId() { return entryPointId; }
  spv::ExecutionMode getExecutionMode() { return execMode; }

private:
  uint32_t entryPointId;
  spv::ExecutionMode execMode;
  llvm::SmallVector<uint32_t, 4> params;
};

/// \brief OpCapability instruction
class SpirvCapabilityInstr : public SpirvInstruction {
public:
  SpirvCapabilityInstr(SourceLocation loc, spv::Capability cap)
      : SpirvInstruction(spv::Op::OpCapability, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        capability(cap) {}
  spv::Capability getCapability() { return capability; }

private:
  spv::Capability capability;
};

/// \brief OpFunction instruction
class SpirvFunctionInstr : public SpirvInstruction {
public:
  SpirvFunctionInstr(QualType type, uint32_t resultId, SourceLocation loc,
                     spv::FunctionControlMask controlMask,
                     uint32_t functionType)
      : SpirvInstruction(spv::Op::OpFunction, type, resultId, loc),
        mask(controlMask), fnType(functionType) {}

  spv::FunctionControlMask getFnControlMask() { return mask; }
  uint32_t getFnType() { return fnType; }

private:
  spv::FunctionControlMask mask;
  uint32_t fnType;
};

/// \brief OpMemoryBarrier and OpControlBarrier instructions
class SpirvBarrierInstr : public SpirvInstruction {
public:
  SpirvBarrierInstr(SourceLocation loc, spv::Scope memoryScope,
                    spv::MemorySemanticsMask memorySemantics,
                    spv::Scope executionScope = spv::Scope::Max);

  spv::Scope getMemoryScope() { return memoryScope; }
  spv::MemorySemanticsMask getMemorySemantics() { return memorySemantics; }
  bool isControlBarrier() { return getOpcode() == spv::Op::OpControlBarrier; }
  spv::Scope getExecutionScope() { return executionScope; }

private:
  spv::Scope memoryScope;
  spv::MemorySemanticsMask memorySemantics;
  spv::Scope executionScope;
};

/// \brief Merge instructions include OpLoopMerge and OpSelectionMerge
class SpirvMergeInstr : public SpirvInstruction {
public:
  virtual uint32_t getMergeBlock();

protected:
  SpirvMergeInstr(spv::Op op, SourceLocation loc, uint32_t mergeBlockId)
      : SpirvInstruction(op, /*QualType*/ {}, /*result-id*/ 0, loc),
        mergeBlock(mergeBlockId) {}

private:
  uint32_t mergeBlock;
};
class SpirvLoopMergeInstr : public SpirvMergeInstr {
public:
  SpirvLoopMergeInstr(SourceLocation loc, uint32_t mergeBlock,
                      uint32_t contTarget, spv::LoopControlMask mask)
      : SpirvMergeInstr(spv::Op::OpLoopMerge, loc, mergeBlock),
        continueTarget(contTarget), loopControlMask(mask) {}

  virtual uint32_t getContinueTarget() { return continueTarget; }
  virtual spv::LoopControlMask getLoopControlMask() { return loopControlMask; }

private:
  uint32_t continueTarget;
  spv::LoopControlMask loopControlMask;
};
class SpirvSelectionMergeInstr : public SpirvMergeInstr {
public:
  SpirvSelectionMergeInstr(SourceLocation loc, uint32_t mergeBlock,
                           spv::SelectionControlMask mask)
      : SpirvMergeInstr(spv::Op::OpSelectionMerge, loc, mergeBlock),
        selControlMask(mask) {}
  virtual spv::SelectionControlMask getSelectionControlMask() {
    return selControlMask;
  }

private:
  spv::SelectionControlMask selControlMask;
};

/// \brief Termination instructions are instructions that end a basic block.
/// These instructions include:
/// * OpBranch, OpBranchConditional, OpSwitch
/// * OpReturn, OpReturnValue, OpKill, OpUnreachable
/// The first group (branching instructions) also include information on
/// possible branches that will be taken next.
class SpirvTerminationInstr : public SpirvInstruction {
protected:
  SpirvTerminationInstr(spv::Op op, SourceLocation loc)
      : SpirvInstruction(op, /*QualType*/ {}, /*result-id*/ 0, loc) {}
};

/// \brief Base class for branching instructions
class SpirvBranchingInstr : public SpirvTerminationInstr {
public:
  virtual llvm::SmallVector<uint32_t, 4> getTargetBranches() = 0;

protected:
  SpirvBranchingInstr(spv::Op op, SourceLocation loc)
      : SpirvTerminationInstr(op, loc) {}
};

/// \brief OpBranch instruction
class SpirvBranchInstr : public SpirvBranchingInstr {
public:
  SpirvBranchInstr(SourceLocation loc, uint32_t target)
      : SpirvBranchingInstr(spv::Op::OpBranch, loc), targetLabel(target) {}

  uint32_t getTargetLabel() { return targetLabel; }

  // Returns all possible branches that could be taken by the branching
  // instruction.
  virtual llvm::SmallVector<uint32_t, 4> getTargetBranches() {
    return {targetLabel};
  }

private:
  uint32_t targetLabel;
};

/// \brief OpBranchConditional instruction
class SpirvBranchConditionalInstr : public SpirvBranchingInstr {
public:
  SpirvBranchConditionalInstr(SourceLocation loc, uint32_t cond,
                              uint32_t trueLabelId, uint32_t falseLabelId)
      : SpirvBranchingInstr(spv::Op::OpBranchConditional, loc), condition(cond),
        trueLabel(trueLabelId), falseLabel(falseLabelId) {}

  virtual llvm::SmallVector<uint32_t, 4> getTargetBranches() {
    return {trueLabel, falseLabel};
  }
  uint32_t getCondition() { return condition; }
  uint32_t getTrueLabel() { return trueLabel; }
  uint32_t getFalseLabel() { return falseLabel; }

private:
  uint32_t condition;
  uint32_t trueLabel;
  uint32_t falseLabel;
};

/// \brief Switch instruction
class SpirvSwitchInstr : public SpirvBranchingInstr {
public:
  SpirvSwitchInstr(
      SourceLocation loc, uint32_t selectorId, uint32_t defaultLabelId,
      llvm::SmallVector<std::pair<uint32_t, uint32_t>, 4> &targetsVec)
      : SpirvBranchingInstr(spv::Op::OpSwitch, loc), selector(selectorId),
        defaultLabel(defaultLabelId), targets(targetsVec) {}

  virtual uint32_t getSelector() { return selector; }
  virtual uint32_t getDefaultLabel() { return defaultLabel; }
  virtual llvm::ArrayRef<std::pair<uint32_t, uint32_t>> getTargets() {
    return targets;
  }
  // Returns the branch label that will be taken for the given literal.
  virtual uint32_t getTargetLabelForLiteral(uint32_t);
  // Returns all possible branches that could be taken by the switch statement.
  virtual llvm::SmallVector<uint32_t, 4> getTargetBranches();

private:
  uint32_t selector;
  uint32_t defaultLabel;
  llvm::SmallVector<std::pair<uint32_t, uint32_t>, 4> targets;
};

/// \brief OpKill instruction
class SpirvKillInstr : public SpirvTerminationInstr {
public:
  SpirvKillInstr(SourceLocation loc)
      : SpirvTerminationInstr(spv::Op::OpKill, loc) {}
};

/// \brief OpUnreachable instruction
class SpirvUnreachableInstr : public SpirvTerminationInstr {
public:
  SpirvUnreachableInstr(SourceLocation loc)
      : SpirvTerminationInstr(spv::Op::OpUnreachable, loc) {}
};

/// \brief OpReturn and OpReturnValue instructions
class SpirvReturnInstr : public SpirvTerminationInstr {
public:
  SpirvReturnInstr(SourceLocation loc, uint32_t retVal = 0)
      : SpirvTerminationInstr(
            retVal ? spv::Op::OpReturnValue : spv::Op::OpReturn, loc),
        returnValue(retVal) {}
  bool hasReturnValue() { return returnValue != 0; }
  uint32_t getReturnValue() { return returnValue; }

private:
  uint32_t returnValue;
};

/// \brief Composition instructions include: opConstantComposite,
/// opSpecConstantComposite, and opCompositeConstruct
class SpirvCompositeInstr : public SpirvInstruction {
public:
  SpirvCompositeInstr(QualType type, uint32_t resultId, SourceLocation loc,
                      llvm::SmallVector<uint32_t, 4> &constituentsVec,
                      bool isConstant = false, bool isSpecConstant = false);

  bool isConstantComposite() {
    return getOpcode() == spv::Op::OpConstantComposite;
  }
  bool isSpecConstantComposite() {
    return getOpcode() == spv::Op::OpSpecConstantComposite;
  }
  llvm::ArrayRef<uint32_t> getConstituents() { return consituents; }

private:
  llvm::SmallVector<uint32_t, 4> consituents;
};

/// \brief Extraction instruction (OpCompositeExtract)
class SpirvExtractInstr : public SpirvInstruction {
public:
  SpirvExtractInstr(QualType type, uint32_t resultId, SourceLocation loc,
                    uint32_t compositeId,
                    llvm::SmallVector<uint32_t, 4> &indexesVec)
      : SpirvInstruction(spv::Op::OpCompositeExtract, type, resultId, loc),
        composite(compositeId), indexes(indexesVec) {}
  uint32_t getComposite() { return composite; }
  llvm::ArrayRef<uint32_t> getIndexes() { return indexes; }

private:
  uint32_t composite;
  llvm::SmallVector<uint32_t, 4> indexes;
};

/// \brief BitField instructions include: OpBitFieldInsert, OpBitFieldSExtract,
/// and OpBitFieldUExtract
class SpirvBitFieldInstr : public SpirvInstruction {
public:
  virtual uint32_t getBase() { return base; }
  virtual uint32_t getOffset() { return offset; }
  virtual uint32_t getCount() { return count; }

protected:
  SpirvBitFieldInstr(spv::Op op, QualType type, uint32_t resultId,
                     SourceLocation loc, uint32_t baseId, uint32_t offsetId,
                     uint32_t countId)
      : SpirvInstruction(op, type, resultId, loc), base(base), offset(offsetId),
        count(countId) {}

private:
  uint32_t base;
  uint32_t offset;
  uint32_t count;
};
class SpirvBitFieldInsertInstr : public SpirvBitFieldInstr {
public:
  SpirvBitFieldInsertInstr(QualType type, uint32_t resultId, SourceLocation loc,
                           uint32_t baseId, uint32_t insertId,
                           uint32_t offsetId, uint32_t countId)
      : SpirvBitFieldInstr(spv::Op::OpBitFieldInsert, type, resultId, loc,
                           baseId, offsetId, countId),
        insert(insertId) {}
  uint32_t getInsert() { return insert; }

private:
  uint32_t insert;
};
class SpirvBitFieldExtractInstr : public SpirvBitFieldInstr {
public:
  SpirvBitFieldExtractInstr(QualType type, uint32_t resultId,
                            SourceLocation loc, uint32_t baseId,
                            uint32_t offsetId, uint32_t countId, bool isSigned)
      : SpirvBitFieldInstr(isSigned ? spv::Op::OpBitFieldSExtract
                                    : spv::Op::OpBitFieldUExtract,
                           type, resultId, loc, baseId, offsetId, countId) {}
  uint32_t isSigned() { return getOpcode() == spv::Op::OpBitFieldSExtract; }
};

/// \brief OpFunctionCall instruction
class SpirvFunctionCallInstr : public SpirvInstruction {
public:
  SpirvFunctionCallInstr(QualType type, uint32_t resultId, SourceLocation loc,
                         uint32_t fnId, llvm::SmallVector<uint32_t, 4> &argsVec)
      : SpirvInstruction(spv::Op::OpFunctionCall, type, resultId, loc),
        function(fnId), args(argsVec) {}
  uint32_t getFunction() { return function; }
  llvm::ArrayRef<uint32_t> getArgs() { return args; }

private:
  uint32_t function;
  llvm::SmallVector<uint32_t, 4> args;
};

/// \brief OpVariable instruction
class SpirvVariableInstr : public SpirvInstruction {
public:
  SpirvVariableInstr(QualType type, uint32_t resultId, SourceLocation loc,
                     spv::StorageClass sc, uint32_t initializerId = 0)
      : SpirvInstruction(spv::Op::OpVariable, type, resultId, loc),
        storageClass(sc), initializer(initializerId) {}

  bool hasInitializer() { return initializer != 0; }
  uint32_t getInitializer() { return initializer; }
  spv::StorageClass getStorageClass() { return storageClass; }

private:
  spv::StorageClass storageClass;
  uint32_t initializer;
};

/// \brief OpImageTexelPointer instruction
class SpirvImageTexelPointerInstr : public SpirvInstruction {
public:
  SpirvImageTexelPointerInstr(QualType type, uint32_t resultId,
                              SourceLocation loc, uint32_t imageId,
                              uint32_t coordinateId, uint32_t sampleId)
      : SpirvInstruction(spv::Op::OpImageTexelPointer, type, resultId, loc),
        image(imageId), coordinate(coordinateId), sample(sampleId) {}

  uint32_t getImage() { return image; }
  uint32_t getCoordinate() { return coordinate; }
  uint32_t getSample() { return sample; }

private:
  uint32_t image;
  uint32_t coordinate;
  uint32_t sample;
};

/// \brief Base for OpGroupNonUniform* instructions
class SpirvGroupNonUniformInstr : public SpirvInstruction {
protected:
  SpirvGroupNonUniformInstr(spv::Op op, QualType type, uint32_t resultId,
                            SourceLocation loc, spv::Scope scope)
      : SpirvInstruction(op, type, resultId, loc), execScope(scope) {}

private:
  spv::Scope execScope;
};
/// \brief OpGroupNonUniformElect instruction. This is currently the only
/// non-uniform instruction that takes no other arguments.
class SpirvNonUniformElectInstr : public SpirvGroupNonUniformInstr {
public:
  SpirvNonUniformElectInstr(QualType type, uint32_t resultId,
                            SourceLocation loc, spv::Scope scope)
      : SpirvGroupNonUniformInstr(spv::Op::OpGroupNonUniformElect, type,
                                  resultId, loc, scope) {}
};
/// \brief OpGroupNonUniform* unary instructions.
class SpirvNonUniformUnaryInstr : public SpirvGroupNonUniformInstr {
public:
  SpirvNonUniformUnaryInstr(spv::Op op, QualType type, uint32_t resultId,
                            SourceLocation loc, spv::Scope scope,
                            llvm::Optional<spv::GroupOperation> group,
                            uint32_t argId);

private:
  uint32_t arg;
  llvm::Optional<spv::GroupOperation> groupOp;
};
/// \brief OpGroupNonUniform* binary instructions.
class SpirvNonUniformBinaryInstr : public SpirvGroupNonUniformInstr {
public:
  SpirvNonUniformBinaryInstr(spv::Op op, QualType type, uint32_t resultId,
                             SourceLocation loc, spv::Scope scope,
                             uint32_t arg1Id, uint32_t arg2Id);

private:
  uint32_t arg1;
  uint32_t arg2;
};

/// \brief OpLabel instruction
class SpirvLabelInstr : SpirvInstruction {
public:
  SpirvLabelInstr(uint32_t resultId, SourceLocation loc)
      : SpirvInstruction(spv::Op::OpLabel, /*QualType*/ {}, resultId, loc) {}
};

/// \brief Image instructions. These include:
///
/// OpImageSampleImplicitLod          (image, coordinate, mask, args)
/// OpImageSampleExplicitLod          (image, coordinate, mask, args, lod)
/// OpImageSampleDrefImplicitLod      (image, coordinate, mask, args, dref)
/// OpImageSampleDrefExplicitLod      (image, coordinate, mask, args, dref, lod)
/// OpImageSparseSampleImplicitLod    (image, coordinate, mask, args)
/// OpImageSparseSampleExplicitLod    (image, coordinate, mask, args, lod)
/// OpImageSparseSampleDrefImplicitLod(image, coordinate, mask, args, dref)
/// OpImageSparseSampleDrefExplicitLod(image, coordinate, mask, args, dref, lod)
///
/// OpImageFetch                      (image, coordinate, mask, args)
/// OpImageSparseFetch                (image, coordinate, mask, args)
/// OpImageGather                     (image, coordinate, mask, args, component)
/// OpImageSparseGather               (image, coordinate, mask, args, component)
/// OpImageDrefGather                 (image, coordinate, mask, args, dref)
/// OpImageSparseDrefGather           (image, coordinate, mask, args, dref)
/// OpImageRead                       (image, coordinate, mask, args)
/// OpImageSparseRead                 (image, coordinate, mask, args)
/// OpImageWrite                      (image, coordinate, mask, args, texel)
///
/// Image operands can include:
/// Bias, Lod, Grad (pair), ConstOffset, Offset, ConstOffsets, Sample, MinLod.
///
class SpirvImageInstr : public SpirvInstruction {
public:
  SpirvImageInstr(spv::Op op, QualType type, uint32_t resultId,
                  SourceLocation loc, uint32_t imageId, uint32_t coordinateId,
                  spv::ImageOperandsMask mask, uint32_t drefId = 0,
                  uint32_t biasId = 0, uint32_t lodId = 0,
                  uint32_t gradDxId = 0, uint32_t gradDyId = 0,
                  uint32_t constOffsetId = 0, uint32_t offsetId = 0,
                  uint32_t constOffsetsId = 0, uint32_t sampleId = 0,
                  uint32_t minLodId = 0, uint32_t componentId = 0,
                  uint32_t texelToWriteId = 0);
  uint32_t getImage() { return image; }
  uint32_t getCoordinate() { return coordinate; }
  spv::ImageOperandsMask getImageOperandsMask() { return operandsMask; }
  uint32_t getDref() { return dref; }
  uint32_t getBias() { return bias; }
  uint32_t getLod() { return lod; }
  uint32_t getGradDx() { return gradDx; }
  uint32_t getGradDy() { return gradDy; }
  std::pair<uint32_t, uint32_t> getGrad() {
    return std::make_pair(gradDx, gradDy);
  }
  uint32_t getConstOffset() { return constOffset; }
  uint32_t getOffset() { return offset; }
  uint32_t getConstOffsets() { return constOffsets; }
  uint32_t getSample() { return sample; }
  uint32_t getMinLod() { return minLod; }
  uint32_t getComponent() { return component; }
  uint32_t getTexelToWrite() { return texelToWrite; }

private:
  uint32_t image;
  uint32_t coordinate;
  uint32_t dref;
  uint32_t bias;
  uint32_t lod;
  uint32_t gradDx;
  uint32_t gradDy;
  uint32_t constOffset;
  uint32_t offset;
  uint32_t constOffsets;
  uint32_t sample;
  uint32_t minLod;
  uint32_t component;
  uint32_t texelToWrite;
  spv::ImageOperandsMask operandsMask;
};

/// \brief OpSampledImage instruction (Create a sampled image, containing both a
/// sampler and an image).
class SpirvSampledImageInstr : public SpirvInstruction {
public:
  SpirvSampledImageInstr(QualType type, uint32_t resultId, SourceLocation loc,
                         uint32_t imageId, uint32_t samplerId)
      : SpirvInstruction(spv::Op::OpSampledImage, type, resultId, loc),
        image(imageId), sampler(samplerId) {}

  uint32_t getImage() { return image; }
  uint32_t getSampler() { return sampler; }

private:
  uint32_t image;
  uint32_t sampler;
};

} // namespace spirv
} // namespace clang
