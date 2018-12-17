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
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"

namespace clang {
namespace spirv {

class BoolType;
class FloatType;
class IntegerType;
class SpirvBasicBlock;
class SpirvFunction;
class SpirvType;
class SpirvVariable;
class SpirvString;
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
    IK_LineInfo,        // OpLine (debug)
    IK_Decoration,      // Op*Decorate
    IK_Type,            // OpType*
    IK_Variable,        // OpVariable

    // Different kind of constants. Order matters.
    IK_ConstantBoolean,
    IK_ConstantInteger,
    IK_ConstantFloat,
    IK_ConstantComposite,
    IK_ConstantNull,

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
    IK_CompositeInsert,  // OpCompositeInsert
    IK_ExtInst,          // OpExtInst
    IK_FunctionCall,     // OpFunctionCall

    IK_EndPrimitive, // OpEndPrimitive
    IK_EmitVertex,   // OpEmitVertex

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
    IK_ArrayLength,               // OpArrayLength
  };

  virtual ~SpirvInstruction() = default;

  // Invokes SPIR-V visitor on this instruction.
  virtual bool invokeVisitor(Visitor *) = 0;

  Kind getKind() const { return kind; }
  spv::Op getopcode() const { return opcode; }
  QualType getAstResultType() const { return astResultType; }
  void setAstResultType(QualType type) { astResultType = type; }
  bool hasAstResultType() const { return astResultType != QualType(); }

  uint32_t getResultTypeId() const { return resultTypeId; }
  void setResultTypeId(uint32_t id) { resultTypeId = id; }

  bool hasResultType() const { return resultType != nullptr; }
  const SpirvType *getResultType() const { return resultType; }
  void setResultType(const SpirvType *type) { resultType = type; }

  // TODO: The responsibility of assigning the result-id of an instruction
  // shouldn't be on the instruction itself.
  uint32_t getResultId() const { return resultId; }
  void setResultId(uint32_t id) { resultId = id; }

  clang::SourceLocation getSourceLocation() const { return srcLoc; }

  void setDebugName(llvm::StringRef name) { debugName = name; }
  llvm::StringRef getDebugName() const { return debugName; }

  SpirvLayoutRule getLayoutRule() const { return layoutRule; }
  void setLayoutRule(SpirvLayoutRule rule) { layoutRule = rule; }

  void setContainsAliasComponent(bool contains) { containsAlias = contains; }
  bool containsAliasComponent() const { return containsAlias; }

  void setStorageClass(spv::StorageClass sc) { storageClass = sc; }
  spv::StorageClass getStorageClass() const { return storageClass; }

  void setRValue(bool rvalue = true) { isRValue_ = rvalue; }
  bool isRValue() const { return isRValue_; }

  void setRelaxedPrecision() { isRelaxedPrecision_ = true; }
  bool isRelaxedPrecision() const { return isRelaxedPrecision_; }

  void setNonUniform(bool nu = true) { isNonUniform_ = nu; }
  bool isNonUniform() const { return isNonUniform_; }

protected:
  // Forbid creating SpirvInstruction directly
  SpirvInstruction(Kind kind, spv::Op opcode, QualType astResultType,
                   SourceLocation loc);

protected:
  const Kind kind;

  spv::Op opcode;
  QualType astResultType;
  uint32_t resultId;
  SourceLocation srcLoc;
  std::string debugName;
  const SpirvType *resultType;
  uint32_t resultTypeId;
  SpirvLayoutRule layoutRule;

  /// Indicates whether this evaluation result contains alias variables
  ///
  /// This field should only be true for stand-alone alias variables, which is
  /// of pointer-to-pointer type, or struct variables containing alias fields.
  /// After dereferencing the alias variable, this should be set to false to let
  /// CodeGen fall back to normal handling path.
  ///
  /// Note: legalization specific code
  bool containsAlias;

  spv::StorageClass storageClass;
  bool isRValue_;
  bool isRelaxedPrecision_;
  bool isNonUniform_;
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
  SpirvExtInstImport(SourceLocation loc,
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
                  SpirvFunction *entryPoint, llvm::StringRef nameStr,
                  llvm::ArrayRef<SpirvVariable *> iface);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_EntryPoint;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvEntryPoint)

  spv::ExecutionModel getExecModel() const { return execModel; }
  SpirvFunction *getEntryPoint() const { return entryPoint; }
  llvm::StringRef getEntryPointName() const { return name; }
  llvm::ArrayRef<SpirvVariable *> getInterface() const { return interfaceVec; }

private:
  spv::ExecutionModel execModel;
  SpirvFunction *entryPoint;
  std::string name;
  llvm::SmallVector<SpirvVariable *, 8> interfaceVec;
};

/// \brief OpExecutionMode and OpExecutionModeId instructions
class SpirvExecutionMode : public SpirvInstruction {
public:
  SpirvExecutionMode(SourceLocation loc, SpirvFunction *entryPointFunction,
                     spv::ExecutionMode, llvm::ArrayRef<uint32_t> params,
                     bool usesIdParams);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ExecutionMode;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvExecutionMode)

  SpirvFunction *getEntryPoint() const { return entryPoint; }
  spv::ExecutionMode getExecutionMode() const { return execMode; }
  llvm::ArrayRef<uint32_t> getParams() const { return params; }

private:
  SpirvFunction *entryPoint;
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
              SpirvString *file, llvm::StringRef src);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Source;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSource)

  spv::SourceLanguage getSourceLanguage() const { return lang; }
  uint32_t getVersion() const { return version; }
  bool hasFile() const { return file != nullptr; }
  SpirvString *getFile() const { return file; }
  llvm::StringRef getSource() const { return source; }

private:
  spv::SourceLanguage lang;
  uint32_t version;
  SpirvString *file;
  std::string source;
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

/// \brief OpDecorate(Id) and OpMemberDecorate instructions
class SpirvDecoration : public SpirvInstruction {
public:
  SpirvDecoration(SourceLocation loc, SpirvInstruction *target,
                  spv::Decoration decor, llvm::ArrayRef<uint32_t> params = {},
                  llvm::Optional<uint32_t> index = llvm::None);
  SpirvDecoration(SourceLocation loc, SpirvInstruction *target,
                  spv::Decoration decor, llvm::StringRef stringParam,
                  llvm::Optional<uint32_t> index = llvm::None);

  // Used for creating OpDecorateId instructions
  SpirvDecoration(SourceLocation loc, SpirvInstruction *target,
                  spv::Decoration decor,
                  llvm::ArrayRef<SpirvInstruction *> params);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Decoration;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvDecoration)

  // Returns the instruction that is the target of the decoration.
  SpirvInstruction *getTarget() const { return target; }

  spv::Decoration getDecoration() const { return decoration; }
  llvm::ArrayRef<uint32_t> getParams() const { return params; }
  llvm::ArrayRef<SpirvInstruction *> getIdParams() const { return idParams; }
  bool isMemberDecoration() const { return index.hasValue(); }
  uint32_t getMemberIndex() const { return index.getValue(); }

private:
  spv::Op getDecorateOpcode(spv::Decoration,
                            const llvm::Optional<uint32_t> &memberIndex);

private:
  SpirvInstruction *target;
  spv::Decoration decoration;
  llvm::Optional<uint32_t> index;
  llvm::SmallVector<uint32_t, 4> params;
  llvm::SmallVector<SpirvInstruction *, 4> idParams;
};

/// \brief OpVariable instruction
class SpirvVariable : public SpirvInstruction {
public:
  /// \brief An enum class for representing what the DeclContext is used for
  enum class ContextUsageKind {
    CBuffer = 0,
    TBuffer = 1,
    PushConstant = 2,
    Globals = 3,
    None = 4
  };

  SpirvVariable(QualType resultType, SourceLocation loc, spv::StorageClass sc,
                SpirvInstruction *initializerId = 0);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Variable;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvVariable)

  bool hasInitializer() const { return initializer != nullptr; }
  SpirvInstruction *getInitializer() const { return initializer; }
  void setContextUsageKind(ContextUsageKind k) { contextUsageKind = k; }
  ContextUsageKind getContextUsageKind() const { return contextUsageKind; }

private:
  SpirvInstruction *initializer;
  ContextUsageKind contextUsageKind;
};

class SpirvFunctionParameter : public SpirvInstruction {
public:
  SpirvFunctionParameter(QualType resultType, SourceLocation loc);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_FunctionParameter;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvFunctionParameter)
};

/// \brief Merge instructions include OpLoopMerge and OpSelectionMerge
class SpirvMerge : public SpirvInstruction {
public:
  SpirvBasicBlock *getMergeBlock() const { return mergeBlock; }

protected:
  SpirvMerge(Kind kind, spv::Op opcode, SourceLocation loc,
             SpirvBasicBlock *mergeBlock);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_LoopMerge ||
           inst->getKind() == IK_SelectionMerge;
  }

private:
  SpirvBasicBlock *mergeBlock;
};

class SpirvLoopMerge : public SpirvMerge {
public:
  SpirvLoopMerge(SourceLocation loc, SpirvBasicBlock *mergeBlock,
                 SpirvBasicBlock *contTarget, spv::LoopControlMask mask);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_LoopMerge;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvLoopMerge)

  SpirvBasicBlock *getContinueTarget() const { return continueTarget; }
  spv::LoopControlMask getLoopControlMask() const { return loopControlMask; }

private:
  SpirvBasicBlock *continueTarget;
  spv::LoopControlMask loopControlMask;
};

class SpirvSelectionMerge : public SpirvMerge {
public:
  SpirvSelectionMerge(SourceLocation loc, SpirvBasicBlock *mergeBlock,
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

  virtual llvm::ArrayRef<SpirvBasicBlock *> getTargetBranches() const = 0;

protected:
  SpirvBranching(Kind kind, spv::Op opcode, SourceLocation loc);
};

/// \brief OpBranch instruction
class SpirvBranch : public SpirvBranching {
public:
  SpirvBranch(SourceLocation loc, SpirvBasicBlock *target);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Branch;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBranch)

  SpirvBasicBlock *getTargetLabel() const { return targetLabel; }

  // Returns all possible basic blocks that could be taken by the branching
  // instruction.
  llvm::ArrayRef<SpirvBasicBlock *> getTargetBranches() const override {
    return {targetLabel};
  }

private:
  SpirvBasicBlock *targetLabel;
};

/// \brief OpBranchConditional instruction
class SpirvBranchConditional : public SpirvBranching {
public:
  SpirvBranchConditional(SourceLocation loc, SpirvInstruction *cond,
                         SpirvBasicBlock *trueLabel,
                         SpirvBasicBlock *falseLabel);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_BranchConditional;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBranchConditional)

  llvm::ArrayRef<SpirvBasicBlock *> getTargetBranches() const override {
    return {trueLabel, falseLabel};
  }

  SpirvInstruction *getCondition() const { return condition; }
  SpirvBasicBlock *getTrueLabel() const { return trueLabel; }
  SpirvBasicBlock *getFalseLabel() const { return falseLabel; }

private:
  SpirvInstruction *condition;
  SpirvBasicBlock *trueLabel;
  SpirvBasicBlock *falseLabel;
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
  SpirvReturn(SourceLocation loc, SpirvInstruction *retVal = 0);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Return;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvReturn)

  bool hasReturnValue() const { return returnValue != 0; }
  SpirvInstruction *getReturnValue() const { return returnValue; }

private:
  SpirvInstruction *returnValue;
};

/// \brief Switch instruction
class SpirvSwitch : public SpirvBranching {
public:
  SpirvSwitch(
      SourceLocation loc, SpirvInstruction *selector,
      SpirvBasicBlock *defaultLabel,
      llvm::ArrayRef<std::pair<uint32_t, SpirvBasicBlock *>> &targetsVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Switch;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSwitch)

  SpirvInstruction *getSelector() const { return selector; }
  SpirvBasicBlock *getDefaultLabel() const { return defaultLabel; }
  llvm::ArrayRef<std::pair<uint32_t, SpirvBasicBlock *>> getTargets() const {
    return targets;
  }
  // Returns the branch label that will be taken for the given literal.
  SpirvBasicBlock *getTargetLabelForLiteral(uint32_t) const;
  // Returns all possible branches that could be taken by the switch statement.
  llvm::ArrayRef<SpirvBasicBlock *> getTargetBranches() const override;

private:
  SpirvInstruction *selector;
  SpirvBasicBlock *defaultLabel;
  llvm::SmallVector<std::pair<uint32_t, SpirvBasicBlock *>, 4> targets;
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
  SpirvAccessChain(QualType resultType, SourceLocation loc,
                   SpirvInstruction *base,
                   llvm::ArrayRef<SpirvInstruction *> indexVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_AccessChain;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvAccessChain)

  SpirvInstruction *getBase() const { return base; }
  llvm::ArrayRef<SpirvInstruction *> getIndexes() const { return indices; }

private:
  SpirvInstruction *base;
  llvm::SmallVector<SpirvInstruction *, 4> indices;
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
  SpirvAtomic(spv::Op opcode, QualType resultType, SourceLocation loc,
              SpirvInstruction *pointer, spv::Scope, spv::MemorySemanticsMask,
              SpirvInstruction *value = nullptr);
  SpirvAtomic(spv::Op opcode, QualType resultType, SourceLocation loc,
              SpirvInstruction *pointer, spv::Scope,
              spv::MemorySemanticsMask semanticsEqual,
              spv::MemorySemanticsMask semanticsUnequal,
              SpirvInstruction *value, SpirvInstruction *comparator);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Atomic;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvAtomic)

  SpirvInstruction *getPointer() const { return pointer; }
  spv::Scope getScope() const { return scope; }
  spv::MemorySemanticsMask getMemorySemantics() const { return memorySemantic; }
  bool hasValue() const { return value != nullptr; }
  SpirvInstruction *getValue() const { return value; }
  bool hasComparator() const { return comparator != nullptr; }
  SpirvInstruction *getComparator() const { return comparator; }
  spv::MemorySemanticsMask getMemorySemanticsEqual() const {
    return memorySemantic;
  }
  spv::MemorySemanticsMask getMemorySemanticsUnequal() const {
    return memorySemanticUnequal;
  }

private:
  SpirvInstruction *pointer;
  spv::Scope scope;
  spv::MemorySemanticsMask memorySemantic;
  spv::MemorySemanticsMask memorySemanticUnequal;
  SpirvInstruction *value;
  SpirvInstruction *comparator;
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
  SpirvBinaryOp(spv::Op opcode, QualType resultType, SourceLocation loc,
                SpirvInstruction *op1, SpirvInstruction *op2);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_BinaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBinaryOp)

  SpirvInstruction *getOperand1() const { return operand1; }
  SpirvInstruction *getOperand2() const { return operand2; }
  bool isSpecConstantOp() const {
    return getopcode() == spv::Op::OpSpecConstantOp;
  }

private:
  SpirvInstruction *operand1;
  SpirvInstruction *operand2;
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

  virtual SpirvInstruction *getBase() const { return base; }
  virtual SpirvInstruction *getOffset() const { return offset; }
  virtual SpirvInstruction *getCount() const { return count; }

protected:
  SpirvBitField(Kind kind, spv::Op opcode, QualType resultType,
                SourceLocation loc, SpirvInstruction *base,
                SpirvInstruction *offset, SpirvInstruction *count);

private:
  SpirvInstruction *base;
  SpirvInstruction *offset;
  SpirvInstruction *count;
};

class SpirvBitFieldExtract : public SpirvBitField {
public:
  SpirvBitFieldExtract(QualType resultType, SourceLocation loc,
                       SpirvInstruction *base, SpirvInstruction *offset,
                       SpirvInstruction *count, bool isSigned);

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
  SpirvBitFieldInsert(QualType resultType, SourceLocation loc,
                      SpirvInstruction *base, SpirvInstruction *insert,
                      SpirvInstruction *offset, SpirvInstruction *count);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_BitFieldInsert;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvBitFieldInsert)

  SpirvInstruction *getInsert() const { return insert; }

private:
  SpirvInstruction *insert;
};

class SpirvConstant : public SpirvInstruction {
public:
  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() >= IK_ConstantBoolean &&
           inst->getKind() <= IK_ConstantNull;
  }

  bool isSpecConstant() const;

protected:
  SpirvConstant(Kind, spv::Op, const SpirvType *);
  SpirvConstant(Kind, spv::Op, QualType);
};

class SpirvConstantBoolean : public SpirvConstant {
public:
  SpirvConstantBoolean(const BoolType *type, bool value,
                       bool isSpecConst = false);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ConstantBoolean;
  }

  bool operator==(const SpirvConstantBoolean &that) const;

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantBoolean)

  bool getValue() const { return value; }

private:
  bool value;
};

/// \brief Represent OpConstant for integer values.
class SpirvConstantInteger : public SpirvConstant {
public:
  SpirvConstantInteger(QualType type, llvm::APInt value,
                       bool isSpecConst = false);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ConstantInteger;
  }

  bool operator==(const SpirvConstantInteger &that) const;

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantInteger)

  llvm::APInt getValue() const { return value; }

private:
  llvm::APInt value;
};

class SpirvConstantFloat : public SpirvConstant {
public:
  SpirvConstantFloat(QualType type, llvm::APFloat value,
                     bool isSpecConst = false);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ConstantFloat;
  }

  bool operator==(const SpirvConstantFloat &that) const;

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantFloat)

  llvm::APFloat getValue() const { return value; }

private:
  llvm::APFloat value;
};

class SpirvConstantComposite : public SpirvConstant {
public:
  SpirvConstantComposite(const SpirvType *type,
                         llvm::ArrayRef<SpirvConstant *> constituents,
                         bool isSpecConst = false);
  SpirvConstantComposite(QualType type,
                         llvm::ArrayRef<SpirvConstant *> constituents,
                         bool isSpecConst = false);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ConstantComposite;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantComposite)

  llvm::ArrayRef<SpirvConstant *> getConstituents() const {
    return constituents;
  }

private:
  llvm::SmallVector<SpirvConstant *, 4> constituents;
};

class SpirvConstantNull : public SpirvConstant {
public:
  SpirvConstantNull(const SpirvType *type);
  SpirvConstantNull(QualType type);

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvConstantNull)

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ConstantNull;
  }

  bool operator==(const SpirvConstantNull &that) const;
};

/// \brief Composition instructions
///
/// This class includes OpConstantComposite, OpSpecConstantComposite,
/// and OpCompositeConstruct.
class SpirvComposite : public SpirvInstruction {
public:
  SpirvComposite(QualType resultType, SourceLocation loc,
                 llvm::ArrayRef<SpirvInstruction *> constituentsVec,
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
  llvm::ArrayRef<SpirvInstruction *> getConstituents() const {
    return consituents;
  }

private:
  llvm::SmallVector<SpirvInstruction *, 4> consituents;
};

/// \brief Extraction instruction (OpCompositeExtract)
class SpirvCompositeExtract : public SpirvInstruction {
public:
  SpirvCompositeExtract(QualType resultType, SourceLocation loc,
                        SpirvInstruction *composite,
                        llvm::ArrayRef<uint32_t> indices);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_CompositeExtract;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvCompositeExtract)

  SpirvInstruction *getComposite() const { return composite; }
  llvm::ArrayRef<uint32_t> getIndexes() const { return indices; }

private:
  SpirvInstruction *composite;
  llvm::SmallVector<uint32_t, 4> indices;
};

/// \brief Composite insertion instruction (OpCompositeInsert)
class SpirvCompositeInsert : public SpirvInstruction {
public:
  SpirvCompositeInsert(QualType resultType, SourceLocation loc,
                       SpirvInstruction *composite, SpirvInstruction *object,
                       llvm::ArrayRef<uint32_t> indices);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_CompositeInsert;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvCompositeInsert)

  SpirvInstruction *getComposite() const { return composite; }
  SpirvInstruction *getObject() const { return object; }
  llvm::ArrayRef<uint32_t> getIndexes() const { return indices; }

private:
  SpirvInstruction *composite;
  SpirvInstruction *object;
  llvm::SmallVector<uint32_t, 4> indices;
};

/// \brief EmitVertex instruction
class SpirvEmitVertex : public SpirvInstruction {
public:
  SpirvEmitVertex(SourceLocation loc);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_EmitVertex;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvEmitVertex)
};

/// \brief EndPrimitive instruction
class SpirvEndPrimitive : public SpirvInstruction {
public:
  SpirvEndPrimitive(SourceLocation loc);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_EndPrimitive;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvEndPrimitive)
};

/// \brief ExtInst instruction
class SpirvExtInst : public SpirvInstruction {
public:
  SpirvExtInst(QualType resultType, SourceLocation loc, SpirvExtInstImport *set,
               GLSLstd450 inst, llvm::ArrayRef<SpirvInstruction *> operandsVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ExtInst;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvExtInst)

  SpirvExtInstImport *getInstructionSet() const { return instructionSet; }
  GLSLstd450 getInstruction() const { return instruction; }
  llvm::ArrayRef<SpirvInstruction *> getOperands() const { return operands; }

private:
  SpirvExtInstImport *instructionSet;
  GLSLstd450 instruction;
  llvm::SmallVector<SpirvInstruction *, 4> operands;
};

/// \brief OpFunctionCall instruction
class SpirvFunctionCall : public SpirvInstruction {
public:
  SpirvFunctionCall(QualType resultType, SourceLocation loc,
                    SpirvFunction *function,
                    llvm::ArrayRef<SpirvInstruction *> argsVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_FunctionCall;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvFunctionCall)

  SpirvFunction *getFunction() const { return function; }
  llvm::ArrayRef<SpirvInstruction *> getArgs() const { return args; }

private:
  SpirvFunction *function;
  llvm::SmallVector<SpirvInstruction *, 4> args;
};

/// \brief Base for OpGroupNonUniform* instructions
class SpirvGroupNonUniformOp : public SpirvInstruction {
public:
  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() >= IK_GroupNonUniformBinaryOp &&
           inst->getKind() <= IK_GroupNonUniformUnaryOp;
  }

  spv::Scope getExecutionScope() const { return execScope; }

protected:
  SpirvGroupNonUniformOp(Kind kind, spv::Op opcode, QualType resultType,
                         SourceLocation loc, spv::Scope scope);

private:
  spv::Scope execScope;
};

/// \brief OpGroupNonUniform* binary instructions.
class SpirvNonUniformBinaryOp : public SpirvGroupNonUniformOp {
public:
  SpirvNonUniformBinaryOp(spv::Op opcode, QualType resultType,
                          SourceLocation loc, spv::Scope scope,
                          SpirvInstruction *arg1, SpirvInstruction *arg2);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_GroupNonUniformBinaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvNonUniformBinaryOp)

  SpirvInstruction *getArg1() const { return arg1; }
  SpirvInstruction *getArg2() const { return arg2; }

private:
  SpirvInstruction *arg1;
  SpirvInstruction *arg2;
};

/// \brief OpGroupNonUniformElect instruction. This is currently the only
/// non-uniform instruction that takes no other arguments.
class SpirvNonUniformElect : public SpirvGroupNonUniformOp {
public:
  SpirvNonUniformElect(QualType resultType, SourceLocation loc,
                       spv::Scope scope);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_GroupNonUniformElect;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvNonUniformElect)
};

/// \brief OpGroupNonUniform* unary instructions.
class SpirvNonUniformUnaryOp : public SpirvGroupNonUniformOp {
public:
  SpirvNonUniformUnaryOp(spv::Op opcode, QualType resultType,
                         SourceLocation loc, spv::Scope scope,
                         llvm::Optional<spv::GroupOperation> group,
                         SpirvInstruction *arg);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_GroupNonUniformUnaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvNonUniformUnaryOp)

  SpirvInstruction *getArg() const { return arg; }
  bool hasGroupOp() const { return groupOp.hasValue(); }
  spv::GroupOperation getGroupOp() const { return groupOp.getValue(); }

private:
  SpirvInstruction *arg;
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
  SpirvImageOp(spv::Op opcode, QualType resultType, SourceLocation loc,
               SpirvInstruction *image, SpirvInstruction *coordinate,
               spv::ImageOperandsMask mask, SpirvInstruction *dref = nullptr,
               SpirvInstruction *bias = nullptr,
               SpirvInstruction *lod = nullptr,
               SpirvInstruction *gradDx = nullptr,
               SpirvInstruction *gradDy = nullptr,
               SpirvInstruction *constOffset = nullptr,
               SpirvInstruction *offset = nullptr,
               SpirvInstruction *constOffsets = nullptr,
               SpirvInstruction *sample = nullptr,
               SpirvInstruction *minLod = nullptr,
               SpirvInstruction *component = nullptr,
               SpirvInstruction *texelToWrite = nullptr);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ImageOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvImageOp)

  SpirvInstruction *getImage() const { return image; }
  SpirvInstruction *getCoordinate() const { return coordinate; }
  spv::ImageOperandsMask getImageOperandsMask() const { return operandsMask; }

  bool isSparse() const;
  bool hasDref() const { return dref != nullptr; }
  bool hasBias() const { return bias != nullptr; }
  bool hasLod() const { return lod != nullptr; }
  bool hasGrad() const { return gradDx != nullptr && gradDy != nullptr; }
  bool hasConstOffset() const { return constOffset != nullptr; }
  bool hasOffset() const { return offset != nullptr; }
  bool hasConstOffsets() const { return constOffsets != nullptr; }
  bool hasSample() const { return sample != nullptr; }
  bool hasMinLod() const { return minLod != nullptr; }
  bool hasComponent() const { return component != nullptr; }
  bool isImageWrite() const { return texelToWrite != nullptr; }

  SpirvInstruction *getDref() const { return dref; }
  SpirvInstruction *getBias() const { return bias; }
  SpirvInstruction *getLod() const { return lod; }
  SpirvInstruction *getGradDx() const { return gradDx; }
  SpirvInstruction *getGradDy() const { return gradDy; }
  std::pair<SpirvInstruction *, SpirvInstruction *> getGrad() const {
    return std::make_pair(gradDx, gradDy);
  }
  SpirvInstruction *getConstOffset() const { return constOffset; }
  SpirvInstruction *getOffset() const { return offset; }
  SpirvInstruction *getConstOffsets() const { return constOffsets; }
  SpirvInstruction *getSample() const { return sample; }
  SpirvInstruction *getMinLod() const { return minLod; }
  SpirvInstruction *getComponent() const { return component; }
  SpirvInstruction *getTexelToWrite() const { return texelToWrite; }

private:
  SpirvInstruction *image;
  SpirvInstruction *coordinate;
  SpirvInstruction *dref;
  SpirvInstruction *bias;
  SpirvInstruction *lod;
  SpirvInstruction *gradDx;
  SpirvInstruction *gradDy;
  SpirvInstruction *constOffset;
  SpirvInstruction *offset;
  SpirvInstruction *constOffsets;
  SpirvInstruction *sample;
  SpirvInstruction *minLod;
  SpirvInstruction *component;
  SpirvInstruction *texelToWrite;
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
  SpirvImageQuery(spv::Op opcode, QualType resultType, SourceLocation loc,
                  SpirvInstruction *img, SpirvInstruction *lod = nullptr,
                  SpirvInstruction *coord = nullptr);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ImageQuery;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvImageQuery)

  SpirvInstruction *getImage() const { return image; }
  bool hasLod() const { return lod != nullptr; }
  SpirvInstruction *getLod() const { return lod; }
  bool hasCoordinate() const { return coordinate != nullptr; }
  SpirvInstruction *getCoordinate() const { return coordinate; }

private:
  SpirvInstruction *image;
  SpirvInstruction *lod;
  SpirvInstruction *coordinate;
};

/// \brief OpImageSparseTexelsResident instruction
class SpirvImageSparseTexelsResident : public SpirvInstruction {
public:
  SpirvImageSparseTexelsResident(QualType resultType, SourceLocation loc,
                                 SpirvInstruction *resCode);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ImageSparseTexelsResident;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvImageSparseTexelsResident)

  SpirvInstruction *getResidentCode() const { return residentCode; }

private:
  SpirvInstruction *residentCode;
};

/// \brief OpImageTexelPointer instruction
/// Note: The resultType stored in objects of this class are the underlying
/// type. The real result type of OpImageTexelPointer must always be an
/// OpTypePointer whose Storage Class operand is Image.
class SpirvImageTexelPointer : public SpirvInstruction {
public:
  SpirvImageTexelPointer(QualType resultType, SourceLocation loc,
                         SpirvInstruction *image, SpirvInstruction *coordinate,
                         SpirvInstruction *sample);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ImageTexelPointer;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvImageTexelPointer)

  SpirvInstruction *getImage() const { return image; }
  SpirvInstruction *getCoordinate() const { return coordinate; }
  SpirvInstruction *getSample() const { return sample; }

private:
  SpirvInstruction *image;
  SpirvInstruction *coordinate;
  SpirvInstruction *sample;
};

/// \brief Load instruction representation
class SpirvLoad : public SpirvInstruction {
public:
  SpirvLoad(QualType resultType, SourceLocation loc, SpirvInstruction *pointer,
            llvm::Optional<spv::MemoryAccessMask> mask = llvm::None);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Load;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvLoad)

  SpirvInstruction *getPointer() const { return pointer; }
  bool hasMemoryAccessSemantics() const { return memoryAccess.hasValue(); }
  spv::MemoryAccessMask getMemoryAccess() const {
    return memoryAccess.getValue();
  }

private:
  SpirvInstruction *pointer;
  llvm::Optional<spv::MemoryAccessMask> memoryAccess;
};

/// \brief OpSampledImage instruction
/// Result Type must be the OpTypeSampledImage type whose Image Type operand is
/// the type of Image. We store the QualType for the underlying image as result
/// type.
class SpirvSampledImage : public SpirvInstruction {
public:
  SpirvSampledImage(QualType resultType, SourceLocation loc,
                    SpirvInstruction *image, SpirvInstruction *sampler);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_SampledImage;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSampledImage)

  SpirvInstruction *getImage() const { return image; }
  SpirvInstruction *getSampler() const { return sampler; }

private:
  SpirvInstruction *image;
  SpirvInstruction *sampler;
};

/// \brief Select operation representation.
class SpirvSelect : public SpirvInstruction {
public:
  SpirvSelect(QualType resultType, SourceLocation loc, SpirvInstruction *cond,
              SpirvInstruction *trueId, SpirvInstruction *falseId);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Select;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSelect)

  SpirvInstruction *getCondition() const { return condition; }
  SpirvInstruction *getTrueObject() const { return trueObject; }
  SpirvInstruction *getFalseObject() const { return falseObject; }

private:
  SpirvInstruction *condition;
  SpirvInstruction *trueObject;
  SpirvInstruction *falseObject;
};

/// \brief OpSpecConstantOp instruction where the operation is binary.
class SpirvSpecConstantBinaryOp : public SpirvInstruction {
public:
  SpirvSpecConstantBinaryOp(spv::Op specConstantOp, QualType resultType,
                            SourceLocation loc, SpirvInstruction *operand1,
                            SpirvInstruction *operand2);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_SpecConstantBinaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSpecConstantBinaryOp)

  spv::Op getSpecConstantopcode() const { return specOp; }
  SpirvInstruction *getOperand1() const { return operand1; }
  SpirvInstruction *getOperand2() const { return operand2; }

private:
  spv::Op specOp;
  SpirvInstruction *operand1;
  SpirvInstruction *operand2;
};

/// \brief OpSpecConstantOp instruction where the operation is unary.
class SpirvSpecConstantUnaryOp : public SpirvInstruction {
public:
  SpirvSpecConstantUnaryOp(spv::Op specConstantOp, QualType resultType,
                           SourceLocation loc, SpirvInstruction *operand);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_SpecConstantUnaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvSpecConstantUnaryOp)

  spv::Op getSpecConstantopcode() const { return specOp; }
  SpirvInstruction *getOperand() const { return operand; }

private:
  spv::Op specOp;
  SpirvInstruction *operand;
};

/// \brief Store instruction representation
class SpirvStore : public SpirvInstruction {
public:
  SpirvStore(SourceLocation loc, SpirvInstruction *pointer,
             SpirvInstruction *object,
             llvm::Optional<spv::MemoryAccessMask> mask = llvm::None);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_Store;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvStore)

  SpirvInstruction *getPointer() const { return pointer; }
  SpirvInstruction *getObject() const { return object; }
  bool hasMemoryAccessSemantics() const { return memoryAccess.hasValue(); }
  spv::MemoryAccessMask getMemoryAccess() const {
    return memoryAccess.getValue();
  }

private:
  SpirvInstruction *pointer;
  SpirvInstruction *object;
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
  SpirvUnaryOp(spv::Op opcode, QualType resultType, SourceLocation loc,
               SpirvInstruction *op);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_UnaryOp;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvUnaryOp)

  SpirvInstruction *getOperand() const { return operand; }
  bool isConversionOp() const;

private:
  SpirvInstruction *operand;
};

/// \brief OpVectorShuffle instruction
class SpirvVectorShuffle : public SpirvInstruction {
public:
  SpirvVectorShuffle(QualType resultType, SourceLocation loc,
                     SpirvInstruction *vec1, SpirvInstruction *vec2,
                     llvm::ArrayRef<uint32_t> componentsVec);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_VectorShuffle;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvVectorShuffle)

  SpirvInstruction *getVec1() const { return vec1; }
  SpirvInstruction *getVec2() const { return vec2; }
  llvm::ArrayRef<uint32_t> getComponents() const { return components; }

private:
  SpirvInstruction *vec1;
  SpirvInstruction *vec2;
  llvm::SmallVector<uint32_t, 4> components;
};

class SpirvArrayLength : public SpirvInstruction {
public:
  SpirvArrayLength(QualType resultType, SourceLocation loc,
                   SpirvInstruction *structure, uint32_t arrayMember);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_ArrayLength;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvArrayLength)

  SpirvInstruction *getStructure() const { return structure; }
  uint32_t getArrayMember() const { return arrayMember; }

private:
  SpirvInstruction *structure;
  uint32_t arrayMember;
};

class SpirvLineInfo : public SpirvInstruction {
public:
  SpirvLineInfo(SpirvString *srcFile, uint32_t srcLine, uint32_t srcCol);

  // For LLVM-style RTTI
  static bool classof(const SpirvInstruction *inst) {
    return inst->getKind() == IK_LineInfo;
  }

  DECLARE_INVOKE_VISITOR_FOR_CLASS(SpirvLineInfo)

  SpirvString *getSourceFile() { return file; }
  uint32_t getSourceLine() { return line; }
  uint32_t getSourceColumn() { return column; }

private:
  SpirvString *file;
  uint32_t line;
  uint32_t column;
};

#undef DECLARE_INVOKE_VISITOR_FOR_CLASS

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVINSTRUCTION_H
