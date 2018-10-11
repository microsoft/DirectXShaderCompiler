//===-- SpirvInstruction.h - SPIR-V Instruction -----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVINSTRUCTION_H
#define LLVM_CLANG_SPIRV_SPIRVINSTRUCTION_H

#include "dxc/Support/SPIRVOptions.h"
#include "spirv/unified1/GLSL.std.450.h"
#include "spirv/unified1/spirv.hpp11"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"

namespace clang {
namespace spirv {

class Visitor;

/// \brief The base class for representing SPIR-V instructions.
class SpirvInstruction {
public:
  enum Kind {
    // "Metadata" kinds
    // In the order of logical layout

    IK_Capability,      // OpCapability
    IK_Extension,       // OpExtension
    IK_ExtInstImport,   // OpExtInstImport
    IK_MemoryModel,     // OpMemoryModel
    IK_EntryPoint,      // OpEntryPoint
    IK_ExecutionMode,   // OpExecutionMode
    IK_String,          // OpString (debug)
    IK_Source,          // OpSource (debug)
    IK_Name,            // Op*Name (debug)
    IK_ModuleProcessed, // OpModuleProcessed (debug)
    IK_Decoration,      // Op*Decorate
    IK_Type,            // OpType*
    IK_Constant,        // OpConstant*
    IK_Variable,        // OpVariable

    // Function structure kinds

    IK_FunctionParameter, // OpFunctionParameter

    // The following section is for merge instructions.
    // Used by LLVM-style RTTI; order matters.
    IK_LoopMerge,      // OpLoopMerge
    IK_SelectionMerge, // OpSelectionMerge

    // The following section is for termination instructions.
    // Used by LLVM-style RTTI; order matters.
    IK_Branch,            // OpBranch
    IK_BranchConditional, // OpBranchConditional
    IK_Kill,              // OpKill
    IK_Return,            // OpReturn*
    IK_Switch,            // OpSwitch
    IK_Unreachable,       // OpUnreachable

    // Normal instruction kinds
    // In alphabetical order

    IK_AccessChain,      // OpAccessChain
    IK_Atomic,           // OpAtomic*
    IK_Barrier,          // Op*Barrier
    IK_BinaryOp,         // Binary operations
    IK_BitFieldExtract,  // OpBitFieldExtract
    IK_BitFieldInsert,   // OpBitFieldInsert
    IK_Composite,        // Op*Composite
    IK_CompositeExtract, // OpCompositeExtract
    IK_ExtInst,          // OpExtInst
    IK_FunctionCall,     // OpFunctionCall

    // The following section is for group non-uniform instructions.
    // Used by LLVM-style RTTI; order matters.
    IK_GroupNonUniformBinaryOp, // Group non-uniform binary operations
    IK_GroupNonUniformElect,    // OpGroupNonUniformElect
    IK_GroupNonUniformUnaryOp,  // Group non-uniform unary operations

    IK_ImageOp,                   // OpImage*
    IK_ImageQuery,                // OpImageQuery*
    IK_ImageSparseTexelsResident, // OpImageSparseTexelsResident
    IK_ImageTexelPointer,         // OpImageTexelPointer
    IK_Load,                      // OpLoad
    IK_SampledImage,              // OpSampledImage
    IK_Select,                    // OpSelect
    IK_SpecConstantBinaryOp,      // SpecConstant binary operations
    IK_SpecConstantUnaryOp,       // SpecConstant unary operations
    IK_Store,                     // OpStore
    IK_UnaryOp,                   // Unary operations
    IK_VectorShuffle,             // OpVectorShuffle
  };

  virtual ~SpirvInstruction() = default;

  // Invokes SPIR-V visitor on this instruction.
  virtual bool invokeVisitor(Visitor *) = 0;

  Kind getKind() const { return kind; }
  spv::Op getopcode() const { return opcode; }
  QualType getResultType() const { return resultType; }

  // TODO: The QualType should be lowered to a SPIR-V type and the result-id of
  // the SPIR-V type should be stored somewhere (either in SpirvInstruction or
  // in a map in SpirvModule). The id of the result type should be retreived and
  // returned by this method.
  uint32_t getResultTypeId() const { return 0; }

  // TODO: The responsibility of assigning the result-id of an instruction
  // shouldn't be on the instruction itself.
  uint32_t getResultId() const { return resultId; }

  clang::SourceLocation getSourceLocation() const { return srcLoc; }

protected:
  // Forbid creating SpirvInstruction directly
  SpirvInstruction(Kind kind, spv::Op opcode, QualType resultType,
                   uint32_t resultId, SourceLocation loc);

private:
  const Kind kind;

  spv::Op opcode;
  QualType resultType;
  uint32_t resultId;
  SourceLocation srcLoc;
};

#define DECLARE_INVOKE_VISITOR_FOR_CLASS(cls)                                  \
  bool invokeVisitor(Visitor *v) override;

/// \brief OpCapability instruction
class SpirvCapability : public SpirvInstruction {
public:
  SpirvCapability(SourceLocation loc, spv::Capability cap);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Capability;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvCapability)

  spv::Capability getCapability() const { return capability; }

private:
  spv::Capability capability;
};

/// \brief Extension instruction
class SpirvExtension : public SpirvInstruction {
public:
  SpirvExtension(SourceLocation loc, llvm::StringRef extensionName);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Extension;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvExtension)

  llvm::StringRef getExtensionName() const { return extName; }

private:
  std::string extName;
};

/// \brief ExtInstImport instruction
class SpirvExtInstImport : public SpirvInstruction {
public:
  SpirvExtInstImport(uint32_t resultId, SourceLocation loc,
                     llvm::StringRef extensionName = "GLSL.std.450");

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ExtInstImport;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvExtInstImport)

  llvm::StringRef getExtendedInstSetName() const { return extName; }

private:
  std::string extName;
};

/// \brief OpMemoryModel instruction
class SpirvMemoryModel : public SpirvInstruction {
public:
  SpirvMemoryModel(spv::AddressingModel addrModel, spv::MemoryModel memModel);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_MemoryModel;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvMemoryModel)

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
                  llvm::ArrayRef<uint32_t> iface);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_EntryPoint;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvEntryPoint)

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

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ExecutionMode;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvExecutionMode)

  uint32_t getEntryPointId() const { return entryPointId; }
  spv::ExecutionMode getExecutionMode() const { return execMode; }
  llvm::ArrayRef<uint32_t> getParams() const { return params; }

private:
  uint32_t entryPointId;
  spv::ExecutionMode execMode;
  llvm::SmallVector<uint32_t, 4> params;
};

/// \brief OpString instruction
class SpirvString : public SpirvInstruction {
public:
  SpirvString(SourceLocation loc, llvm::StringRef stringLiteral);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_String;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvString)

  llvm::StringRef getString() const { return str; }

private:
  std::string str;
};

/// \brief OpSource and OpSourceContinued instruction
class SpirvSource : public SpirvInstruction {
public:
  SpirvSource(SourceLocation loc, spv::SourceLanguage language, uint32_t ver,
              uint32_t fileId, llvm::StringRef src);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Source;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSource)

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

/// \brief OpMemberName instruction
class SpirvName : public SpirvInstruction {
public:
  SpirvName(SourceLocation loc, uint32_t targetId, llvm::StringRef nameStr,
            llvm::Optional<uint32_t> memberIndex);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Name;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvName)

  uint32_t getTarget() const { return target; }
  bool isForMember() const { return member.hasValue(); }
  uint32_t getMember() const { return member.getValue(); }
  llvm::StringRef getName() const { return name; }

private:
  uint32_t target;
  llvm::Optional<uint32_t> member;
  std::string name;
};

/// \brief OpModuleProcessed instruction
class SpirvModuleProcessed : public SpirvInstruction {
public:
  SpirvModuleProcessed(SourceLocation loc, llvm::StringRef processStr);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ModuleProcessed;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvModuleProcessed)

  llvm::StringRef getProcess() const { return process; }

private:
  std::string process;
};

/// \brief OpDecorate and OpMemberDecorate instructions
class SpirvDecoration : public SpirvInstruction {
public:
  SpirvDecoration(SourceLocation loc, uint32_t target, spv::Decoration decor,
                  llvm::ArrayRef<uint32_t> params,
                  llvm::Optional<uint32_t> index);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Decoration;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvDecoration)

  // Returns the <result-id> of the target of the decoration. It may be the id
  // of an object or the id of a structure type whose member is being decorated.
  uint32_t getTarget() const { return target; }

  spv::Decoration getDecoration() const { return decoration; }
  llvm::ArrayRef<uint32_t> getParams() const { return params; }
  bool isMemberDecoration() const { return index.hasValue(); }
  uint32_t getMemberIndex() const { return index.getValue(); }

private:
  uint32_t target;
  spv::Decoration decoration;
  llvm::Optional<uint32_t> index;
  llvm::SmallVector<uint32_t, 4> params;
};

/// \brief OpVariable instruction
class SpirvVariable : public SpirvInstruction {
public:
  SpirvVariable(QualType resultType, uint32_t resultId, SourceLocation loc,
                spv::StorageClass sc, uint32_t initializerId = 0);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Variable;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvVariable)

  bool hasInitializer() const { return initializer != 0; }
  uint32_t getInitializer() const { return initializer; }
  spv::StorageClass getStorageClass() const { return storageClass; }

private:
  spv::StorageClass storageClass;
  uint32_t initializer;
};

class SpirvFunctionParameter : public SpirvInstruction {
public:
  SpirvFunctionParameter(QualType resultType, uint32_t resultId,
                         SourceLocation loc);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_FunctionParameter;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvFunctionParameter)
};

/// \brief Merge instructions include OpLoopMerge and OpSelectionMerge
class SpirvMerge : public SpirvInstruction {
public:
  uint32_t getMergeBlock() const { return mergeBlock; }

protected:
  SpirvMerge(Kind kind, spv::Op opcode, SourceLocation loc,
             uint32_t mergeBlockId);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_LoopMerge ||
           inst->getKind() == IK_SelectionMerge;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvMerge)

private:
  uint32_t mergeBlock;
};

class SpirvLoopMerge : public SpirvMerge {
public:
  SpirvLoopMerge(SourceLocation loc, uint32_t mergeBlock, uint32_t contTarget,
                 spv::LoopControlMask mask);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_LoopMerge;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvLoopMerge)

  uint32_t getContinueTarget() const { return continueTarget; }
  spv::LoopControlMask getLoopControlMask() const { return loopControlMask; }

private:
  uint32_t continueTarget;
  spv::LoopControlMask loopControlMask;
};

class SpirvSelectionMerge : public SpirvMerge {
public:
  SpirvSelectionMerge(SourceLocation loc, uint32_t mergeBlock,
                      spv::SelectionControlMask mask);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_SelectionMerge;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSelectionMerge)

  spv::SelectionControlMask getSelectionControlMask() const {
    return selControlMask;
  }

private:
  spv::SelectionControlMask selControlMask;
};

/// \brief Termination instructions are instructions that end a basic block.
///
/// These instructions include:
///
/// * OpBranch, OpBranchConditional, OpSwitch
/// * OpReturn, OpReturnValue, OpKill, OpUnreachable
///
/// The first group (branching instructions) also include information on
/// possible branches that will be taken next.
class SpirvTerminator : public SpirvInstruction {
public:
  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() >= IK_Branch && inst->getKind() <= IK_Unreachable;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvTerminator)

protected:
  SpirvTerminator(Kind kind, spv::Op opcode, SourceLocation loc);
};

/// \brief Base class for branching instructions
class SpirvBranching : public SpirvTerminator {
public:
  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() >= IK_Branch &&
           inst->getKind() <= IK_BranchConditional;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBranching)

  virtual llvm::ArrayRef<uint32_t> getTargetBranches() const = 0;

protected:
  SpirvBranching(Kind kind, spv::Op opcode, SourceLocation loc);
};

/// \brief OpBranch instruction
class SpirvBranch : public SpirvBranching {
public:
  SpirvBranch(SourceLocation loc, uint32_t target);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Branch;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBranch)

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
                         uint32_t trueLabelId, uint32_t falseLabelId);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_BranchConditional;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBranchConditional)

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

/// \brief OpKill instruction
class SpirvKill : public SpirvTerminator {
public:
  SpirvKill(SourceLocation loc);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Kill;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvKill)
};

/// \brief OpReturn and OpReturnValue instructions
class SpirvReturn : public SpirvTerminator {
public:
  SpirvReturn(SourceLocation loc, uint32_t retVal = 0);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Return;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvReturn)

  bool hasReturnValue() const { return returnValue != 0; }
  uint32_t getReturnValue() const { return returnValue; }

private:
  uint32_t returnValue;
};

/// \brief Switch instruction
class SpirvSwitch : public SpirvBranching {
public:
  SpirvSwitch(SourceLocation loc, uint32_t selectorId, uint32_t defaultLabelId,
              llvm::ArrayRef<std::pair<uint32_t, uint32_t>> &targetsVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Switch;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSwitch)

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

/// \brief OpUnreachable instruction
class SpirvUnreachable : public SpirvTerminator {
public:
  SpirvUnreachable(SourceLocation loc);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Unreachable;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvUnreachable)
};

/// \brief Access Chain instruction representation (OpAccessChain)
///
/// Note: If needed, this class can be extended to cover Ptr access chains,
/// and InBounds access chains. These are currently not used by CodeGen.
class SpirvAccessChain : public SpirvInstruction {
public:
  SpirvAccessChain(QualType resultType, uint32_t resultId, SourceLocation loc,
                   uint32_t baseId, llvm::ArrayRef<uint32_t> indexVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_AccessChain;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvAccessChain)

  uint32_t getBase() const { return base; }
  llvm::ArrayRef<uint32_t> getIndexes() const { return indices; }

private:
  uint32_t base;
  llvm::SmallVector<uint32_t, 4> indices;
};

/// \brief Atomic instructions.
///
/// This class includes:
/// OpAtomicLoad           (pointer, scope, memorysemantics)
/// OpAtomicIncrement      (pointer, scope, memorysemantics)
/// OpAtomicDecrement      (pointer, scope, memorysemantics)
/// OpAtomicFlagClear      (pointer, scope, memorysemantics)
/// OpAtomicFlagTestAndSet (pointer, scope, memorysemantics)
/// OpAtomicStore          (pointer, scope, memorysemantics, value)
/// OpAtomicAnd            (pointer, scope, memorysemantics, value)
/// OpAtomicOr             (pointer, scope, memorysemantics, value)
/// OpAtomicXor            (pointer, scope, memorysemantics, value)
/// OpAtomicIAdd           (pointer, scope, memorysemantics, value)
/// OpAtomicISub           (pointer, scope, memorysemantics, value)
/// OpAtomicSMin           (pointer, scope, memorysemantics, value)
/// OpAtomicUMin           (pointer, scope, memorysemantics, value)
/// OpAtomicSMax           (pointer, scope, memorysemantics, value)
/// OpAtomicUMax           (pointer, scope, memorysemantics, value)
/// OpAtomicExchange       (pointer, scope, memorysemantics, value)
/// OpAtomicCompareExchange(pointer, scope, semaequal, semaunequal,
///                         value, comparator)
class SpirvAtomic : public SpirvInstruction {
public:
  SpirvAtomic(spv::Op opcode, QualType resultType, uint32_t resultId,
              SourceLocation loc, uint32_t pointerId, spv::Scope,
              spv::MemorySemanticsMask, uint32_t valueId = 0);
  SpirvAtomic(spv::Op opcode, QualType resultType, uint32_t resultId,
              SourceLocation loc, uint32_t pointerId, spv::Scope,
              spv::MemorySemanticsMask semanticsEqual,
              spv::MemorySemanticsMask semanticsUnequal, uint32_t value,
              uint32_t comparatorId);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Atomic;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvAtomic)

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

/// \brief OpMemoryBarrier and OpControlBarrier instructions
class SpirvBarrier : public SpirvInstruction {
public:
  SpirvBarrier(SourceLocation loc, spv::Scope memoryScope,
               spv::MemorySemanticsMask memorySemantics,
               llvm::Optional<spv::Scope> executionScope = llvm::None);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Barrier;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBarrier)

  spv::Scope getMemoryScope() const { return memoryScope; }
  spv::MemorySemanticsMask getMemorySemantics() const {
    return memorySemantics;
  }
  bool isControlBarrier() const { return hasExecutionScope(); }
  bool hasExecutionScope() const { return executionScope.hasValue(); }
  spv::Scope getExecutionScope() const { return executionScope.getValue(); }

private:
  spv::Scope memoryScope;
  spv::MemorySemanticsMask memorySemantics;
  llvm::Optional<spv::Scope> executionScope;
};

/// \brief Represents SPIR-V binary operation instructions.
///
/// This class includes:
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
  SpirvBinaryOp(spv::Op opcode, QualType resultType, uint32_t resultId,
                SourceLocation loc, uint32_t op1, uint32_t op2);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_BinaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBinaryOp)

  uint32_t getOperand1() const { return operand1; }
  uint32_t getOperand2() const { return operand2; }
  bool isSpecConstantOp() const {
    return getopcode() == spv::Op::OpSpecConstantOp;
  }

private:
  uint32_t operand1;
  uint32_t operand2;
};

/// \brief BitField instructions
///
/// This class includes OpBitFieldInsert, OpBitFieldSExtract,
//  and OpBitFieldUExtract.
class SpirvBitField : public SpirvInstruction {
public:
  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_BitFieldExtract ||
           inst->getKind() == IK_BitFieldInsert;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBitField)

  virtual uint32_t getBase() const { return base; }
  virtual uint32_t getOffset() const { return offset; }
  virtual uint32_t getCount() const { return count; }

protected:
  SpirvBitField(Kind kind, spv::Op opcode, QualType resultType,
                uint32_t resultId, SourceLocation loc, uint32_t baseId,
                uint32_t offsetId, uint32_t countId);

private:
  uint32_t base;
  uint32_t offset;
  uint32_t count;
};

class SpirvBitFieldExtract : public SpirvBitField {
public:
  SpirvBitFieldExtract(QualType resultType, uint32_t resultId,
                       SourceLocation loc, uint32_t baseId, uint32_t offsetId,
                       uint32_t countId, bool isSigned);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_BitFieldExtract;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBitFieldExtract)

  uint32_t isSigned() const {
    return getopcode() == spv::Op::OpBitFieldSExtract;
  }
};

class SpirvBitFieldInsert : public SpirvBitField {
public:
  SpirvBitFieldInsert(QualType resultType, uint32_t resultId,
                      SourceLocation loc, uint32_t baseId, uint32_t insertId,
                      uint32_t offsetId, uint32_t countId);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_BitFieldInsert;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBitFieldInsert)

  uint32_t getInsert() const { return insert; }

private:
  uint32_t insert;
};

/// \brief Composition instructions
///
/// This class includes OpConstantComposite, OpSpecConstantComposite,
/// and OpCompositeConstruct.
class SpirvComposite : public SpirvInstruction {
public:
  SpirvComposite(QualType resultType, uint32_t resultId, SourceLocation loc,
                 llvm::ArrayRef<uint32_t> constituentsVec,
                 bool isConstant = false, bool isSpecConstant = false);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Composite;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvComposite)

  bool isConstantComposite() const {
    return getopcode() == spv::Op::OpConstantComposite;
  }
  bool isSpecConstantComposite() const {
    return getopcode() == spv::Op::OpSpecConstantComposite;
  }
  llvm::ArrayRef<uint32_t> getConstituents() const { return consituents; }

private:
  llvm::SmallVector<uint32_t, 4> consituents;
};

/// \brief Extraction instruction (OpCompositeExtract)
class SpirvCompositeExtract : public SpirvInstruction {
public:
  SpirvCompositeExtract(QualType resultType, uint32_t resultId,
                        SourceLocation loc, uint32_t compositeId,
                        llvm::ArrayRef<uint32_t> indices);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_CompositeExtract;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvCompositeExtract)

  uint32_t getComposite() const { return composite; }
  llvm::ArrayRef<uint32_t> getIndexes() const { return indices; }

private:
  uint32_t composite;
  llvm::SmallVector<uint32_t, 4> indices;
};

/// \brief ExtInst instruction
class SpirvExtInst : public SpirvInstruction {
public:
  SpirvExtInst(QualType resultType, uint32_t resultId, SourceLocation loc,
               uint32_t setId, GLSLstd450 inst,
               llvm::ArrayRef<uint32_t> operandsVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ExtInst;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvExtInst)

  uint32_t getInstructionSetId() const { return instructionSetId; }
  GLSLstd450 getInstruction() const { return instruction; }
  llvm::ArrayRef<uint32_t> getOperands() const { return operands; }

private:
  uint32_t instructionSetId;
  GLSLstd450 instruction;
  llvm::SmallVector<uint32_t, 4> operands;
};

/// \brief OpFunctionCall instruction
class SpirvFunctionCall : public SpirvInstruction {
public:
  SpirvFunctionCall(QualType resultType, uint32_t resultId, SourceLocation loc,
                    uint32_t fnId, llvm::ArrayRef<uint32_t> argsVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_FunctionCall;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvFunctionCall)

  uint32_t getFunction() const { return function; }
  llvm::ArrayRef<uint32_t> getArgs() const { return args; }

private:
  uint32_t function;
  llvm::SmallVector<uint32_t, 4> args;
};

/// \brief Base for OpGroupNonUniform* instructions
class SpirvGroupNonUniformOp : public SpirvInstruction {
public:
  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() >= IK_GroupNonUniformBinaryOp &&
           inst->getKind() <= IK_GroupNonUniformUnaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvGroupNonUniformOp)

  spv::Scope getExecutionScope() const { return execScope; }

protected:
  SpirvGroupNonUniformOp(Kind kind, spv::Op opcode, QualType resultType,
                         uint32_t resultId, SourceLocation loc,
                         spv::Scope scope);

private:
  spv::Scope execScope;
};

/// \brief OpGroupNonUniform* binary instructions.
class SpirvNonUniformBinaryOp : public SpirvGroupNonUniformOp {
public:
  SpirvNonUniformBinaryOp(spv::Op opcode, QualType resultType,
                          uint32_t resultId, SourceLocation loc,
                          spv::Scope scope, uint32_t arg1Id, uint32_t arg2Id);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_GroupNonUniformBinaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvNonUniformBinaryOp)

  uint32_t getArg1() const { return arg1; }
  uint32_t getArg2() const { return arg2; }

private:
  uint32_t arg1;
  uint32_t arg2;
};

/// \brief OpGroupNonUniformElect instruction. This is currently the only
/// non-uniform instruction that takes no other arguments.
class SpirvNonUniformElect : public SpirvGroupNonUniformOp {
public:
  SpirvNonUniformElect(QualType resultType, uint32_t resultId,
                       SourceLocation loc, spv::Scope scope);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_GroupNonUniformElect;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvNonUniformElect)
};

/// \brief OpGroupNonUniform* unary instructions.
class SpirvNonUniformUnaryOp : public SpirvGroupNonUniformOp {
public:
  SpirvNonUniformUnaryOp(spv::Op opcode, QualType resultType, uint32_t resultId,
                         SourceLocation loc, spv::Scope scope,
                         llvm::Optional<spv::GroupOperation> group,
                         uint32_t argId);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_GroupNonUniformUnaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvNonUniformUnaryOp)

  uint32_t getArg() const { return arg; }
  bool hasGroupOp() const { return groupOp.hasValue(); }
  spv::GroupOperation getGroupOp() const { return groupOp.getValue(); }

private:
  uint32_t arg;
  llvm::Optional<spv::GroupOperation> groupOp;
};

/// \brief Image instructions.
///
/// This class includes:
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
  SpirvImageOp(spv::Op opcode, QualType resultType, uint32_t resultId,
               SourceLocation loc, uint32_t imageId, uint32_t coordinateId,
               spv::ImageOperandsMask mask, uint32_t drefId = 0,
               uint32_t biasId = 0, uint32_t lodId = 0, uint32_t gradDxId = 0,
               uint32_t gradDyId = 0, uint32_t constOffsetId = 0,
               uint32_t offsetId = 0, uint32_t constOffsetsId = 0,
               uint32_t sampleId = 0, uint32_t minLodId = 0,
               uint32_t componentId = 0, uint32_t texelToWriteId = 0);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ImageOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvImageOp)

  uint32_t getImage() const { return image; }
  uint32_t getCoordinate() const { return coordinate; }
  spv::ImageOperandsMask getImageOperandsMask() const { return operandsMask; }

  bool hasDref() const { return dref != 0; }
  bool hasBias() const { return bias != 0; }
  bool hasLod() const { return lod != 0; }
  bool hasGrad() const { return gradDx != 0 && gradDy != 0; }
  bool hasConstOffset() const { return constOffset != 0; }
  bool hasOffset() const { return offset != 0; }
  bool hasConstOffsets() const { return constOffsets != 0; }
  bool hasSample() const { return sample != 0; }
  bool hasMinLod() const { return minLod != 0; }
  bool hasComponent() const { return component != 0; }
  bool isImageWrite() const { return texelToWrite != 0; }

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

/// \brief Image query instructions:
///
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
  SpirvImageQuery(spv::Op opcode, QualType resultType, uint32_t resultId,
                  SourceLocation loc, uint32_t img, uint32_t lodId = 0,
                  uint32_t coordId = 0);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ImageQuery;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvImageQuery)

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
  SpirvImageSparseTexelsResident(QualType resultType, uint32_t resultId,
                                 SourceLocation loc, uint32_t resCode);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ImageSparseTexelsResident;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvImageSparseTexelsResident)

  uint32_t getResidentCode() const { return residentCode; }

private:
  uint32_t residentCode;
};

/// \brief OpImageTexelPointer instruction
class SpirvImageTexelPointer : public SpirvInstruction {
public:
  SpirvImageTexelPointer(QualType resultType, uint32_t resultId,
                         SourceLocation loc, uint32_t imageId,
                         uint32_t coordinateId, uint32_t sampleId);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ImageTexelPointer;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvImageTexelPointer)

  uint32_t getImage() const { return image; }
  uint32_t getCoordinate() const { return coordinate; }
  uint32_t getSample() const { return sample; }

private:
  uint32_t image;
  uint32_t coordinate;
  uint32_t sample;
};

/// \brief Load instruction representation
class SpirvLoad : public SpirvInstruction {
public:
  SpirvLoad(QualType resultType, uint32_t resultId, SourceLocation loc,
            uint32_t pointerId, llvm::Optional<spv::MemoryAccessMask> mask);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Load;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvLoad)

  uint32_t getPointer() const { return pointer; }
  bool hasMemoryAccessSemantics() const { return memoryAccess.hasValue(); }
  spv::MemoryAccessMask getMemoryAccess() const {
    return memoryAccess.getValue();
  }

private:
  uint32_t pointer;
  llvm::Optional<spv::MemoryAccessMask> memoryAccess;
};

/// \brief OpSampledImage instruction
class SpirvSampledImage : public SpirvInstruction {
public:
  SpirvSampledImage(QualType resultType, uint32_t resultId, SourceLocation loc,
                    uint32_t imageId, uint32_t samplerId);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_SampledImage;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSampledImage)

  uint32_t getImage() const { return image; }
  uint32_t getSampler() const { return sampler; }

private:
  uint32_t image;
  uint32_t sampler;
};

/// \brief Select operation representation.
class SpirvSelect : public SpirvInstruction {
public:
  SpirvSelect(QualType resultType, uint32_t resultId, SourceLocation loc,
              uint32_t cond, uint32_t trueId, uint32_t falseId);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Select;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSelect)

  uint32_t getCondition() const { return condition; }
  uint32_t getTrueObject() const { return trueObject; }
  uint32_t getFalseObject() const { return falseObject; }

private:
  uint32_t condition;
  uint32_t trueObject;
  uint32_t falseObject;
};

/// \brief OpSpecConstantOp instruction where the operation is binary.
class SpirvSpecConstantBinaryOp : public SpirvInstruction {
public:
  SpirvSpecConstantBinaryOp(spv::Op specConstantOp, QualType resultType,
                            uint32_t resultId, SourceLocation loc,
                            uint32_t operand1, uint32_t operand2);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_SpecConstantBinaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSpecConstantBinaryOp)

  spv::Op getSpecConstantopcode() const { return specOp; }
  uint32_t getOperand1() const { return operand1; }
  uint32_t getOperand2() const { return operand2; }

private:
  spv::Op specOp;
  uint32_t operand1;
  uint32_t operand2;
};

/// \brief OpSpecConstantOp instruction where the operation is unary.
class SpirvSpecConstantUnaryOp : public SpirvInstruction {
public:
  SpirvSpecConstantUnaryOp(spv::Op specConstantOp, QualType resultType,
                           uint32_t resultId, SourceLocation loc,
                           uint32_t operand);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_SpecConstantUnaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSpecConstantUnaryOp)

  spv::Op getSpecConstantopcode() const { return specOp; }
  uint32_t getOperand() const { return operand; }

private:
  spv::Op specOp;
  uint32_t operand;
};

/// \brief Store instruction representation
class SpirvStore : public SpirvInstruction {
public:
  SpirvStore(SourceLocation loc, uint32_t pointerId, uint32_t objectId,
             llvm::Optional<spv::MemoryAccessMask> mask);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Store;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvStore)

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

/// \brief Represents SPIR-V unary operation instructions.
///
/// This class includes:
/// ----------------------------------------------------------------------------
/// opTranspose    // Matrix capability
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
  SpirvUnaryOp(spv::Op opcode, QualType resultType, uint32_t resultId,
               SourceLocation loc, uint32_t op);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_UnaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvUnaryOp)

  uint32_t getOperand() const { return operand; }

private:
  uint32_t operand;
};

/// \brief OpVectorShuffle instruction
class SpirvVectorShuffle : public SpirvInstruction {
public:
  SpirvVectorShuffle(QualType resultType, uint32_t resultId, SourceLocation loc,
                     uint32_t vec1Id, uint32_t vec2Id,
                     llvm::ArrayRef<uint32_t> componentsVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_VectorShuffle;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvVectorShuffle)

  uint32_t getVec1() const { return vec1; }
  uint32_t getVec2() const { return vec2; }
  llvm::ArrayRef<uint32_t> getComponents() const { return components; }

private:
  uint32_t vec1;
  uint32_t vec2;
  llvm::SmallVector<uint32_t, 4> components;
};

#undef DECLARE_INVOKE_VISITOR_FOR_CLASS

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVINSTRUCTION_H
