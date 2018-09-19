//===-- SpirvInstruction.h - SPIR-V Instruction -----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//

#include "spirv/unified1/GLSL.std.450.h"
#include "spirv/unified1/spirv.hpp11"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace spirv {

/// \brief The base class for representing SPIR-V instructions.
class SpirvInstruction {
public:
  spv::Op getOpcode() const { return opcode; }
  QualType getResultType() const { return resultType; }
  uint32_t getResultId() const { return resultId; }
  clang::SourceLocation getSourceLocation() const { return srcLoc; }

  virtual ~SpirvInstruction() = default;

protected:
  SpirvInstruction(spv::Op op, QualType type, uint32_t id, SourceLocation loc)
      : opcode(op), resultType(type), resultId(id), srcLoc(loc) {}

private:
  spv::Op opcode;
  QualType resultType;
  uint32_t resultId;
  SourceLocation srcLoc;
};

/// \brief Represents SPIR-V unary operation instructions. Includes:
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
/// ------------------------- Conversion operations ----------------------------
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
/// ----------------------------- Logical operations ---------------------------
/// OpLogicalNot
/// OpAny
/// OpAll
/// OpIsNan
/// OpIsInf
/// OpIsFinite        // Kernel capability
/// ----------------------------------------------------------------------------
class SpirvUnaryOp : public SpirvInstruction {
public:
  SpirvUnaryOp(spv::Op opCode, QualType type, uint32_t resultId,
               SourceLocation loc, uint32_t op)
      : SpirvInstruction(opCode, type, resultId, loc), operand(op) {}
  uint32_t getOperand() const { return operand; }

private:
  uint32_t operand;
};

/// \brief OpSpecConstantOp instruction where the operation is unary.
class SpirvSpecConstantUnaryOp : public SpirvUnaryOp {
public:
  SpirvSpecConstantUnaryOp(spv::Op specConstantOp, QualType type,
                           uint32_t resultId, SourceLocation loc,
                           uint32_t operand)
      : SpirvUnaryOp(spv::Op::OpSpecConstantOp, type, resultId, loc, operand),
        specOp(specConstantOp) {}

  spv::Op getSpecOpcode() const { return specOp; }

private:
  spv::Op specOp;
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
/// -------------------------- Shift operations --------------------------------
/// OpShiftRightLogical
/// OpShiftRightArithmetic
/// OpShiftLeftLogical
/// -------------------------- Bitwise logical operations ----------------------
/// OpBitwiseOr
/// OpBitwiseXor
/// OpBitwiseAnd
/// -------------------------- Logical operations ------------------------------
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
/// ----------------------------------------------------------------------------
class SpirvBinaryOp : public SpirvInstruction {
public:
  SpirvBinaryOp(spv::Op opCode, QualType type, uint32_t resultId,
                SourceLocation loc, uint32_t op1, uint32_t op2)
      : SpirvInstruction(opCode, type, resultId, loc), operand1(op1),
        operand2(op2) {}

  uint32_t getOperand1() const { return operand1; }
  uint32_t getOperand2() const { return operand2; }
  bool isSpecConstantOp() const {
    return getOpcode() == spv::Op::OpSpecConstantOp;
  }

private:
  uint32_t operand1;
  uint32_t operand2;
};

/// \brief OpSpecConstantOp instruction where the operation is binary.
class SpirvSpecConstantBinaryOp : public SpirvBinaryOp {
public:
  SpirvSpecConstantBinaryOp(spv::Op specConstantOp, QualType type,
                            uint32_t resultId, SourceLocation loc,
                            uint32_t operand1, uint32_t operand2)
      : SpirvBinaryOp(spv::Op::OpSpecConstantOp, type, resultId, loc, operand1,
                      operand2),
        specOp(specConstantOp) {}

  spv::Op getSpecOpcode() const { return specOp; }

private:
  spv::Op specOp;
};

/// \brief Load instruction representation
class SpirvLoad : public SpirvInstruction {
public:
  SpirvLoad(QualType type, uint32_t resultId, SourceLocation loc,
            uint32_t pointerId, llvm::Optional<spv::MemoryAccessMask> mask)
      : SpirvInstruction(spv::Op::OpLoad, type, resultId, loc),
        pointer(pointerId), memoryAccess(mask) {}

  uint32_t getPointer() const { return pointer; }
  bool hasMemoryAccessSemantics() const { return memoryAccess.hasValue(); }
  spv::MemoryAccessMask getMemoryAccess() const {
    return memoryAccess.getValue();
  }

private:
  uint32_t pointer;
  llvm::Optional<spv::MemoryAccessMask> memoryAccess;
};

/// \brief Store instruction representation
class SpirvStore : public SpirvInstruction {
public:
  SpirvStore(SourceLocation loc, uint32_t pointerId, uint32_t objectId,
             llvm::Optional<spv::MemoryAccessMask> mask)
      : SpirvInstruction(spv::Op::OpStore, /* QualType */ {}, /*result-id*/ 0,
                         loc),
        pointer(pointerId), object(objectId), memoryAccess(mask) {}

  uint32_t getPointer() const { return pointer; }
  uint32_t getObject() const { return object; }
  bool hasMemoryAccessSemantics() const { return memoryAccess.hasValue(); }
  spv::MemoryAccessMask getMemoryAccess() const {
    return memoryAccess.getValue();
  }

private:
  uint32_t pointer;
  uint32_t object;
  llvm::Optional<spv::MemoryAccessMask> memoryAccess;
};

/// \brief Access Chain instruction representation (OpAccessChain)
/// Note: If needed, this class can be extended to cover Ptr access chains, and
/// InBounds access chains. These are currently not used by CodeGen.
class SpirvAccessChain : public SpirvInstruction {
public:
  SpirvAccessChain(QualType type, uint32_t resultId, SourceLocation loc,
                   uint32_t baseId, llvm::ArrayRef<uint32_t> indexVec)
      : SpirvInstruction(spv::Op::OpAccessChain, type, resultId, loc),
        base(baseId), indices(indexVec.begin(), indexVec.end()) {}
  uint32_t getBase() const { return base; }
  llvm::ArrayRef<uint32_t> getIndexes() const { return indices; }

private:
  uint32_t base;
  llvm::SmallVector<uint32_t, 4> indices;
};

/// \brief Select operation representation.
class SpirvSelect : public SpirvInstruction {
public:
  SpirvSelect(QualType type, uint32_t resultId, SourceLocation loc,
              uint32_t cond, uint32_t trueId, uint32_t falseId)
      : SpirvInstruction(spv::Op::OpSelect, type, resultId, loc),
        condition(cond), trueObject(trueId), falseObject(falseId) {}
  uint32_t getCondition() const { return condition; }
  uint32_t getTrueObject() const { return trueObject; }
  uint32_t getFalseObject() const { return falseObject; }

private:
  uint32_t condition;
  uint32_t trueObject;
  uint32_t falseObject;
};

/// \brief Extension instruction
class SpirvExtension : public SpirvInstruction {
public:
  SpirvExtension(SourceLocation loc, llvm::StringRef extensionName)
      : SpirvInstruction(spv::Op::OpExtension, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        extName(extensionName) {}
  llvm::StringRef getExtensionName() const { return extName; }

private:
  std::string extName;
};

/// \brief ExtInstImport instruction
class SpirvExtInstImport : public SpirvInstruction {
public:
  SpirvExtInstImport(uint32_t resultId, SourceLocation loc,
                     llvm::StringRef extensionName = "GLSL.std.450")
      : SpirvInstruction(spv::Op::OpExtInstImport, /* QualType */ {}, resultId,
                         loc),
        extName(extensionName) {}
  llvm::StringRef getExtendedInstSetName() const { return extName; }

private:
  std::string extName;
};

/// \brief ExtInst instruction
class SpirvExtInst : public SpirvInstruction {
public:
  SpirvExtInst(QualType type, uint32_t resultId, SourceLocation loc,
               uint32_t setId, GLSLstd450 inst,
               llvm::ArrayRef<uint32_t> operandsVec)
      : SpirvInstruction(spv::Op::OpExtInst, type, resultId, loc),
        instructionSetId(setId), instruction(inst),
        operands(operandsVec.begin(), operandsVec.end()) {}
  uint32_t getInstructionSetId() const { return instructionSetId; }
  GLSLstd450 getInstruction() const { return instruction; }
  llvm::ArrayRef<uint32_t> getOperands() const { return operands; }

private:
  uint32_t instructionSetId;
  GLSLstd450 instruction;
  llvm::SmallVector<uint32_t, 4> operands;
};

/// \brief Image query instructions:
/// Covers the following instructions:
/// OpImageQueryFormat  (image)
/// OpImageQueryOrder   (image)
/// OpImageQuerySize    (image)
/// OpImageQueryLevels  (image)
/// OpImageQuerySamples (image)
/// OpImageQueryLod     (image, coordinate)
/// OpImageQuerySizeLod (image, lod)
class SpirvImageQuery : public SpirvInstruction {
public:
  SpirvImageQuery(spv::Op op, QualType type, uint32_t resultId,
                  SourceLocation loc, uint32_t img, uint32_t lodId = 0,
                  uint32_t coordId = 0);
  uint32_t getImage() const { return image; }
  uint32_t hasLod() const { return lod != 0; }
  uint32_t getLod() const { return lod; }
  uint32_t hasCoordinate() const { return coordinate != 0; }
  uint32_t getCoordinate() const { return coordinate; }

private:
  uint32_t image;
  uint32_t lod;
  uint32_t coordinate;
};

/// \brief OpImageSparseTexelsResident instruction
class SpirvImageSparseTexelsResident : public SpirvInstruction {
public:
  SpirvImageSparseTexelsResident(QualType type, uint32_t resultId,
                                 SourceLocation loc, uint32_t resCode)
      : SpirvInstruction(spv::Op::OpImageSparseTexelsResident, type, resultId,
                         loc),
        residentCode(resCode) {}
  uint32_t getResidentCode() const { return residentCode; }

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
class SpirvAtomic : public SpirvInstruction {
public:
  SpirvAtomic(spv::Op opCode, QualType type, uint32_t resultId,
              SourceLocation loc, uint32_t pointerId, spv::Scope,
              spv::MemorySemanticsMask, uint32_t valueId = 0);
  SpirvAtomic(spv::Op opCode, QualType type, uint32_t resultId,
              SourceLocation loc, uint32_t pointerId, spv::Scope,
              spv::MemorySemanticsMask semanticsEqual,
              spv::MemorySemanticsMask semanticsUnequal, uint32_t value,
              uint32_t comparatorId);

  uint32_t getPointer() const { return pointer; }
  spv::Scope getScope() const { return scope; }
  spv::MemorySemanticsMask getMemorySemantics() const { return memorySemantic; }
  bool hasValue() const { return value != 0; }
  uint32_t getValue() const { return value; }
  bool hasComparator() const { return comparator != 0; }
  uint32_t getComparator() const { return comparator; }
  spv::MemorySemanticsMask getMemorySemanticsEqual() const {
    return memorySemantic;
  }
  spv::MemorySemanticsMask getMemorySemanticsUnequal() const {
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
class SpirvVectorShuffle : public SpirvInstruction {
public:
  SpirvVectorShuffle(QualType type, uint32_t resultId, SourceLocation loc,
                     uint32_t vec1Id, uint32_t vec2Id,
                     llvm::ArrayRef<uint32_t> componentsVec)
      : SpirvInstruction(spv::Op::OpVectorShuffle, type, resultId, loc),
        vec1(vec1Id), vec2(vec2Id),
        components(componentsVec.begin(), componentsVec.end()) {}
  uint32_t getVec1() const { return vec1; }
  uint32_t getVec2() const { return vec2; }
  llvm::ArrayRef<uint32_t> getComponents() const { return components; }

private:
  uint32_t vec1;
  uint32_t vec2;
  llvm::SmallVector<uint32_t, 4> components;
};

/// \brief OpSource and OpSourceContinued instruction
class SpirvSource : public SpirvInstruction {
public:
  SpirvSource(SourceLocation loc, spv::SourceLanguage language, uint32_t ver,
              uint32_t fileId, llvm::StringRef src)
      : SpirvInstruction(spv::Op::OpSource, /*QualType*/ {}, /*result-id*/ 0,
                         loc),
        lang(language), version(ver), file(fileId), source(src) {}

  spv::SourceLanguage getSourceLanguage() const { return lang; }
  uint32_t getVersion() const { return version; }
  bool hasFileId() const { return file != 0; }
  uint32_t getFileId() const { return file; }
  llvm::StringRef getSource() const { return source; }

private:
  spv::SourceLanguage lang;
  uint32_t version;
  uint32_t file;
  std::string source;
};

/// \brief OpName instruction
class SpirvName : public SpirvInstruction {
public:
  SpirvName(SourceLocation loc, uint32_t targetId, llvm::StringRef nameStr)
      : SpirvInstruction(spv::Op::OpName, /*QualType*/ {}, /*result-id*/ 0,
                         loc),
        target(targetId), name(nameStr) {}
  uint32_t getTarget() const { return target; }
  llvm::StringRef getName() const { return name; }

private:
  uint32_t target;
  std::string name;
};

/// \brief OpMemberName instruction
class SpirvMemberName : public SpirvInstruction {
public:
  SpirvMemberName(SourceLocation loc, uint32_t structTypeId,
                  uint32_t memberNumber, llvm::StringRef nameStr)
      : SpirvInstruction(spv::Op::OpMemberName, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        targetType(structTypeId), member(memberNumber), name(nameStr) {}
  uint32_t getTargetType() const { return targetType; }
  uint32_t getMember() const { return member; }
  llvm::StringRef getName() const { return name; }

private:
  uint32_t targetType;
  uint32_t member;
  std::string name;
};

/// \brief OpString instruction
class SpirvString : public SpirvInstruction {
public:
  SpirvString(SourceLocation loc, llvm::StringRef stringLiteral)
      : SpirvInstruction(spv::Op::OpString, /*QualType*/ {}, /*result-id*/ 0,
                         loc),
        str(stringLiteral) {}
  llvm::StringRef getString() const { return str; }

private:
  std::string str;
};

/// \brief OpLine instruction
class SpirvLine : public SpirvInstruction {
public:
  SpirvLine(SourceLocation loc, uint32_t fileId, uint32_t lineLiteral,
            uint32_t columnLiteral)
      : SpirvInstruction(spv::Op::OpLine, /*QualType*/ {}, /*result-id*/ 0,
                         loc),
        file(fileId), line(lineLiteral), column(columnLiteral) {}
  uint32_t getFileId() const { return file; }
  uint32_t getLine() const { return line; }
  uint32_t getColumn() const { return column; }

private:
  uint32_t file;
  uint32_t line;
  uint32_t column;
};

/// \brief OpModuleProcessed instruction
class SpirvModuleProcessed : public SpirvInstruction {
public:
  SpirvModuleProcessed(SourceLocation loc, llvm::StringRef processStr)
      : SpirvInstruction(spv::Op::OpModuleProcessed, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        process(processStr) {}
  llvm::StringRef getProcess() const { return process; }

private:
  std::string process;
};

/// \brief OpMemoryModel instruction
class SpirvMemoryModel : public SpirvInstruction {
public:
  SpirvMemoryModel(spv::AddressingModel addrModel, spv::MemoryModel memModel)
      : SpirvInstruction(spv::Op::OpMemoryModel, /*QualType*/ {},
                         /*result-id*/ 0, /*SrcLoc*/ {}),
        addressModel(addrModel), memoryModel(memModel) {}

  spv::AddressingModel getAddressingModel() const { return addressModel; }
  spv::MemoryModel getMemoryModel() const { return memoryModel; }

private:
  spv::AddressingModel addressModel;
  spv::MemoryModel memoryModel;
};

/// \brief OpEntryPoint instruction
class SpirvEntryPoint : public SpirvInstruction {
public:
  SpirvEntryPoint(SourceLocation loc, spv::ExecutionModel executionModel,
                  uint32_t entryPointId, llvm::StringRef nameStr,
                  llvm::ArrayRef<uint32_t> iface)
      : SpirvInstruction(spv::Op::OpMemoryModel, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        execModel(executionModel), entryPoint(entryPointId), name(nameStr),
        interfaceVec(iface.begin(), iface.end()) {}
  spv::ExecutionModel getExecModel() const { return execModel; }
  uint32_t getEntryPointId() const { return entryPoint; }
  llvm::StringRef getEntryPointName() const { return name; }
  llvm::ArrayRef<uint32_t> getInterface() const { return interfaceVec; }

private:
  spv::ExecutionModel execModel;
  uint32_t entryPoint;
  std::string name;
  llvm::SmallVector<uint32_t, 8> interfaceVec;
};

/// \brief OpExecutionMode and OpExecutionModeId instructions
class SpirvExecutionMode : public SpirvInstruction {
public:
  SpirvExecutionMode(SourceLocation loc, uint32_t entryPointId,
                     spv::ExecutionMode, llvm::ArrayRef<uint32_t> params,
                     bool usesIdParams);
  uint32_t getEntryPointId() const { return entryPointId; }
  spv::ExecutionMode getExecutionMode() const { return execMode; }

private:
  uint32_t entryPointId;
  spv::ExecutionMode execMode;
  llvm::SmallVector<uint32_t, 4> params;
};

/// \brief OpCapability instruction
class SpirvCapability : public SpirvInstruction {
public:
  SpirvCapability(SourceLocation loc, spv::Capability cap)
      : SpirvInstruction(spv::Op::OpCapability, /*QualType*/ {},
                         /*result-id*/ 0, loc),
        capability(cap) {}
  spv::Capability getCapability() const { return capability; }

private:
  spv::Capability capability;
};

/// \brief OpMemoryBarrier and OpControlBarrier instructions
class SpirvBarrier : public SpirvInstruction {
public:
  SpirvBarrier(SourceLocation loc, spv::Scope memoryScope,
               spv::MemorySemanticsMask memorySemantics,
               spv::Scope executionScope = spv::Scope::Max);

  spv::Scope getMemoryScope() const { return memoryScope; }
  spv::MemorySemanticsMask getMemorySemantics() const {
    return memorySemantics;
  }
  bool isControlBarrier() const {
    return getOpcode() == spv::Op::OpControlBarrier;
  }
  spv::Scope getExecutionScope() const { return executionScope; }

private:
  spv::Scope memoryScope;
  spv::MemorySemanticsMask memorySemantics;
  spv::Scope executionScope;
};

/// \brief Merge instructions include OpLoopMerge and OpSelectionMerge
class SpirvMerge : public SpirvInstruction {
public:
  uint32_t getMergeBlock() const { return mergeBlock; }

protected:
  SpirvMerge(spv::Op op, SourceLocation loc, uint32_t mergeBlockId)
      : SpirvInstruction(op, /*QualType*/ {}, /*result-id*/ 0, loc),
        mergeBlock(mergeBlockId) {}

private:
  uint32_t mergeBlock;
};
class SpirvLoopMerge : public SpirvMerge {
public:
  SpirvLoopMerge(SourceLocation loc, uint32_t mergeBlock, uint32_t contTarget,
                 spv::LoopControlMask mask)
      : SpirvMerge(spv::Op::OpLoopMerge, loc, mergeBlock),
        continueTarget(contTarget), loopControlMask(mask) {}

  uint32_t getContinueTarget() const { return continueTarget; }
  spv::LoopControlMask getLoopControlMask() const { return loopControlMask; }

private:
  uint32_t continueTarget;
  spv::LoopControlMask loopControlMask;
};
class SpirvSelectionMerge : public SpirvMerge {
public:
  SpirvSelectionMerge(SourceLocation loc, uint32_t mergeBlock,
                      spv::SelectionControlMask mask)
      : SpirvMerge(spv::Op::OpSelectionMerge, loc, mergeBlock),
        selControlMask(mask) {}
  spv::SelectionControlMask getSelectionControlMask() const {
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
class SpirvTerminator : public SpirvInstruction {
protected:
  SpirvTerminator(spv::Op op, SourceLocation loc)
      : SpirvInstruction(op, /*QualType*/ {}, /*result-id*/ 0, loc) {}
};

/// \brief Base class for branching instructions
class SpirvBranching : public SpirvTerminator {
public:
  virtual llvm::ArrayRef<uint32_t> getTargetBranches() const = 0;

protected:
  SpirvBranching(spv::Op op, SourceLocation loc) : SpirvTerminator(op, loc) {}
};

/// \brief OpBranch instruction
class SpirvBranch : public SpirvBranching {
public:
  SpirvBranch(SourceLocation loc, uint32_t target)
      : SpirvBranching(spv::Op::OpBranch, loc), targetLabel(target) {}

  uint32_t getTargetLabel() const { return targetLabel; }

  // Returns all possible branches that could be taken by the branching
  // instruction.
  llvm::ArrayRef<uint32_t> getTargetBranches() const { return {targetLabel}; }

private:
  uint32_t targetLabel;
};

/// \brief OpBranchConditional instruction
class SpirvBranchConditional : public SpirvBranching {
public:
  SpirvBranchConditional(SourceLocation loc, uint32_t cond,
                         uint32_t trueLabelId, uint32_t falseLabelId)
      : SpirvBranching(spv::Op::OpBranchConditional, loc), condition(cond),
        trueLabel(trueLabelId), falseLabel(falseLabelId) {}

  llvm::ArrayRef<uint32_t> getTargetBranches() const {
    return {trueLabel, falseLabel};
  }
  uint32_t getCondition() const { return condition; }
  uint32_t getTrueLabel() const { return trueLabel; }
  uint32_t getFalseLabel() const { return falseLabel; }

private:
  uint32_t condition;
  uint32_t trueLabel;
  uint32_t falseLabel;
};

/// \brief Switch instruction
class SpirvSwitch : public SpirvBranching {
public:
  SpirvSwitch(SourceLocation loc, uint32_t selectorId, uint32_t defaultLabelId,
              llvm::ArrayRef<std::pair<uint32_t, uint32_t>> &targetsVec)
      : SpirvBranching(spv::Op::OpSwitch, loc), selector(selectorId),
        defaultLabel(defaultLabelId),
        targets(targetsVec.begin(), targetsVec.end()) {}

  uint32_t getSelector() const { return selector; }
  uint32_t getDefaultLabel() const { return defaultLabel; }
  llvm::ArrayRef<std::pair<uint32_t, uint32_t>> getTargets() const {
    return targets;
  }
  // Returns the branch label that will be taken for the given literal.
  uint32_t getTargetLabelForLiteral(uint32_t) const;
  // Returns all possible branches that could be taken by the switch statement.
  llvm::ArrayRef<uint32_t> getTargetBranches() const;

private:
  uint32_t selector;
  uint32_t defaultLabel;
  llvm::SmallVector<std::pair<uint32_t, uint32_t>, 4> targets;
};

/// \brief OpKill instruction
class SpirvKill : public SpirvTerminator {
public:
  SpirvKill(SourceLocation loc) : SpirvTerminator(spv::Op::OpKill, loc) {}
};

/// \brief OpUnreachable instruction
class SpirvUnreachable : public SpirvTerminator {
public:
  SpirvUnreachable(SourceLocation loc)
      : SpirvTerminator(spv::Op::OpUnreachable, loc) {}
};

/// \brief OpReturn and OpReturnValue instructions
class SpirvReturn : public SpirvTerminator {
public:
  SpirvReturn(SourceLocation loc, uint32_t retVal = 0)
      : SpirvTerminator(retVal ? spv::Op::OpReturnValue : spv::Op::OpReturn,
                        loc),
        returnValue(retVal) {}
  bool hasReturnValue() const { return returnValue != 0; }
  uint32_t getReturnValue() const { return returnValue; }

private:
  uint32_t returnValue;
};

/// \brief Composition instructions include: opConstantComposite,
/// opSpecConstantComposite, and opCompositeConstruct
class SpirvComposite : public SpirvInstruction {
public:
  SpirvComposite(QualType type, uint32_t resultId, SourceLocation loc,
                 llvm::ArrayRef<uint32_t> constituentsVec,
                 bool isConstant = false, bool isSpecConstant = false);

  bool isConstantComposite() const {
    return getOpcode() == spv::Op::OpConstantComposite;
  }
  bool isSpecConstantComposite() const {
    return getOpcode() == spv::Op::OpSpecConstantComposite;
  }
  llvm::ArrayRef<uint32_t> getConstituents() const { return consituents; }

private:
  llvm::SmallVector<uint32_t, 4> consituents;
};

/// \brief Extraction instruction (OpCompositeExtract)
class SpirvExtract : public SpirvInstruction {
public:
  SpirvExtract(QualType type, uint32_t resultId, SourceLocation loc,
               uint32_t compositeId, llvm::ArrayRef<uint32_t> indexVec)
      : SpirvInstruction(spv::Op::OpCompositeExtract, type, resultId, loc),
        composite(compositeId), indices(indexVec.begin(), indexVec.end()) {}
  uint32_t getComposite() const { return composite; }
  llvm::ArrayRef<uint32_t> getIndexes() const { return indices; }

private:
  uint32_t composite;
  llvm::SmallVector<uint32_t, 4> indices;
};

/// \brief BitField instructions include: OpBitFieldInsert, OpBitFieldSExtract,
/// and OpBitFieldUExtract
class SpirvBitField : public SpirvInstruction {
public:
  virtual uint32_t getBase() const { return base; }
  virtual uint32_t getOffset() const { return offset; }
  virtual uint32_t getCount() const { return count; }

protected:
  SpirvBitField(spv::Op op, QualType type, uint32_t resultId,
                SourceLocation loc, uint32_t baseId, uint32_t offsetId,
                uint32_t countId)
      : SpirvInstruction(op, type, resultId, loc), base(base), offset(offsetId),
        count(countId) {}

private:
  uint32_t base;
  uint32_t offset;
  uint32_t count;
};
class SpirvBitFieldInsert : public SpirvBitField {
public:
  SpirvBitFieldInsert(QualType type, uint32_t resultId, SourceLocation loc,
                      uint32_t baseId, uint32_t insertId, uint32_t offsetId,
                      uint32_t countId)
      : SpirvBitField(spv::Op::OpBitFieldInsert, type, resultId, loc, baseId,
                      offsetId, countId),
        insert(insertId) {}
  uint32_t getInsert() const { return insert; }

private:
  uint32_t insert;
};
class SpirvBitFieldExtract : public SpirvBitField {
public:
  SpirvBitFieldExtract(QualType type, uint32_t resultId, SourceLocation loc,
                       uint32_t baseId, uint32_t offsetId, uint32_t countId,
                       bool isSigned)
      : SpirvBitField(isSigned ? spv::Op::OpBitFieldSExtract
                               : spv::Op::OpBitFieldUExtract,
                      type, resultId, loc, baseId, offsetId, countId) {}
  uint32_t isSigned() const {
    return getOpcode() == spv::Op::OpBitFieldSExtract;
  }
};

/// \brief OpFunctionCall instruction
class SpirvFunctionCall : public SpirvInstruction {
public:
  SpirvFunctionCall(QualType type, uint32_t resultId, SourceLocation loc,
                    uint32_t fnId, llvm::ArrayRef<uint32_t> argsVec)
      : SpirvInstruction(spv::Op::OpFunctionCall, type, resultId, loc),
        function(fnId), args(argsVec.begin(), argsVec.end()) {}
  uint32_t getFunction() const { return function; }
  llvm::ArrayRef<uint32_t> getArgs() const { return args; }

private:
  uint32_t function;
  llvm::SmallVector<uint32_t, 4> args;
};

/// \brief OpVariable instruction
class SpirvVariable : public SpirvInstruction {
public:
  SpirvVariable(QualType type, uint32_t resultId, SourceLocation loc,
                spv::StorageClass sc, uint32_t initializerId = 0)
      : SpirvInstruction(spv::Op::OpVariable, type, resultId, loc),
        storageClass(sc), initializer(initializerId) {}

  bool hasInitializer() const { return initializer != 0; }
  uint32_t getInitializer() const { return initializer; }
  spv::StorageClass getStorageClass() const { return storageClass; }

private:
  spv::StorageClass storageClass;
  uint32_t initializer;
};

/// \brief OpImageTexelPointer instruction
class SpirvImageTexelPointer : public SpirvInstruction {
public:
  SpirvImageTexelPointer(QualType type, uint32_t resultId, SourceLocation loc,
                         uint32_t imageId, uint32_t coordinateId,
                         uint32_t sampleId)
      : SpirvInstruction(spv::Op::OpImageTexelPointer, type, resultId, loc),
        image(imageId), coordinate(coordinateId), sample(sampleId) {}

  uint32_t getImage() const { return image; }
  uint32_t getCoordinate() const { return coordinate; }
  uint32_t getSample() const { return sample; }

private:
  uint32_t image;
  uint32_t coordinate;
  uint32_t sample;
};

/// \brief Base for OpGroupNonUniform* instructions
class SpirvGroupNonUniformOp : public SpirvInstruction {
protected:
  SpirvGroupNonUniformOp(spv::Op op, QualType type, uint32_t resultId,
                         SourceLocation loc, spv::Scope scope)
      : SpirvInstruction(op, type, resultId, loc), execScope(scope) {}

private:
  spv::Scope execScope;
};
/// \brief OpGroupNonUniformElect instruction. This is currently the only
/// non-uniform instruction that takes no other arguments.
class SpirvNonUniformElect : public SpirvGroupNonUniformOp {
public:
  SpirvNonUniformElect(QualType type, uint32_t resultId, SourceLocation loc,
                       spv::Scope scope)
      : SpirvGroupNonUniformOp(spv::Op::OpGroupNonUniformElect, type, resultId,
                               loc, scope) {}
};
/// \brief OpGroupNonUniform* unary instructions.
class SpirvNonUniformUnaryOp : public SpirvGroupNonUniformOp {
public:
  SpirvNonUniformUnaryOp(spv::Op op, QualType type, uint32_t resultId,
                         SourceLocation loc, spv::Scope scope,
                         llvm::Optional<spv::GroupOperation> group,
                         uint32_t argId);

private:
  uint32_t arg;
  llvm::Optional<spv::GroupOperation> groupOp;
};
/// \brief OpGroupNonUniform* binary instructions.
class SpirvNonUniformBinaryOp : public SpirvGroupNonUniformOp {
public:
  SpirvNonUniformBinaryOp(spv::Op op, QualType type, uint32_t resultId,
                          SourceLocation loc, spv::Scope scope, uint32_t arg1Id,
                          uint32_t arg2Id);

private:
  uint32_t arg1;
  uint32_t arg2;
};

/// \brief OpLabel instruction
class SpirvLabel : SpirvInstruction {
public:
  SpirvLabel(uint32_t resultId, SourceLocation loc)
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
class SpirvImageOp : public SpirvInstruction {
public:
  SpirvImageOp(spv::Op op, QualType type, uint32_t resultId, SourceLocation loc,
               uint32_t imageId, uint32_t coordinateId,
               spv::ImageOperandsMask mask, uint32_t drefId = 0,
               uint32_t biasId = 0, uint32_t lodId = 0, uint32_t gradDxId = 0,
               uint32_t gradDyId = 0, uint32_t constOffsetId = 0,
               uint32_t offsetId = 0, uint32_t constOffsetsId = 0,
               uint32_t sampleId = 0, uint32_t minLodId = 0,
               uint32_t componentId = 0, uint32_t texelToWriteId = 0);
  uint32_t getImage() const { return image; }
  uint32_t getCoordinate() const { return coordinate; }
  spv::ImageOperandsMask getImageOperandsMask() const { return operandsMask; }
  uint32_t getDref() const { return dref; }
  uint32_t getBias() const { return bias; }
  uint32_t getLod() const { return lod; }
  uint32_t getGradDx() const { return gradDx; }
  uint32_t getGradDy() const { return gradDy; }
  std::pair<uint32_t, uint32_t> getGrad() const {
    return std::make_pair(gradDx, gradDy);
  }
  uint32_t getConstOffset() const { return constOffset; }
  uint32_t getOffset() const { return offset; }
  uint32_t getConstOffsets() const { return constOffsets; }
  uint32_t getSample() const { return sample; }
  uint32_t getMinLod() const { return minLod; }
  uint32_t getComponent() const { return component; }
  uint32_t getTexelToWrite() const { return texelToWrite; }

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
class SpirvSampledImage : public SpirvInstruction {
public:
  SpirvSampledImage(QualType type, uint32_t resultId, SourceLocation loc,
                    uint32_t imageId, uint32_t samplerId)
      : SpirvInstruction(spv::Op::OpSampledImage, type, resultId, loc),
        image(imageId), sampler(samplerId) {}

  uint32_t getImage() const { return image; }
  uint32_t getSampler() const { return sampler; }

private:
  uint32_t image;
  uint32_t sampler;
};

} // namespace spirv
} // namespace clang
