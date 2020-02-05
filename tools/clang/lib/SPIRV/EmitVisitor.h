//===-- EmitVisitor.h - Emit Visitor ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_EMITVISITOR_H
#define LLVM_CLANG_SPIRV_EMITVISITOR_H

#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvVisitor.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"

#include <functional>

namespace clang {
namespace spirv {

class SpirvFunction;
class SpirvBasicBlock;
class SpirvType;

class EmitTypeHandler {
public:
  struct DecorationInfo {
    DecorationInfo(spv::Decoration decor, llvm::ArrayRef<uint32_t> params = {},
                   llvm::Optional<uint32_t> index = llvm::None)
        : decoration(decor), decorationParams(params.begin(), params.end()),
          memberIndex(index) {}

    bool operator==(const DecorationInfo &other) const {
      return decoration == other.decoration &&
             decorationParams == other.decorationParams &&
             memberIndex.hasValue() == other.memberIndex.hasValue() &&
             (!memberIndex.hasValue() ||
              memberIndex.getValue() == other.memberIndex.getValue());
    }

    spv::Decoration decoration;
    llvm::SmallVector<uint32_t, 4> decorationParams;
    llvm::Optional<uint32_t> memberIndex;
  };

public:
  EmitTypeHandler(ASTContext &astCtx, SpirvContext &spvContext,
                  std::vector<uint32_t> *debugVec,
                  std::vector<uint32_t> *decVec,
                  std::vector<uint32_t> *typesVec,
                  const std::function<uint32_t()> &takeNextIdFn)
      : astContext(astCtx), context(spvContext), debugVariableBinary(debugVec),
        annotationsBinary(decVec), typeConstantBinary(typesVec),
        takeNextIdFunction(takeNextIdFn), emittedConstantInts({}),
        emittedConstantFloats({}), emittedConstantComposites({}),
        emittedConstantNulls({}), emittedConstantBools() {
    assert(decVec);
    assert(typesVec);
  }

  // Disable copy constructor/assignment.
  EmitTypeHandler(const EmitTypeHandler &) = delete;
  EmitTypeHandler &operator=(const EmitTypeHandler &) = delete;

  // Emits the instruction for the given type into the typeConstantBinary and
  // returns the result-id for the type. If the type has already been emitted,
  // it only returns its result-id.
  //
  // If any names are associated with the type (or its members in case of
  // structs), the OpName/OpMemberNames will also be emitted.
  //
  // If any decorations apply to the type, it also emits the decoration
  // instructions into the annotationsBinary.
  uint32_t emitType(const SpirvType *);

  // Emits OpDecorate (or OpMemberDecorate if memberIndex is non-zero)
  // targetting the given type. Uses the given decoration kind and its
  // parameters.
  void emitDecoration(uint32_t typeResultId, spv::Decoration,
                      llvm::ArrayRef<uint32_t> decorationParams,
                      llvm::Optional<uint32_t> memberIndex = llvm::None);

  uint32_t getOrCreateConstant(SpirvConstant *);

  // Emits an OpConstant instruction and returns its result-id.
  // For non-specialization constants, if an identical constant has already been
  // emitted, returns the existing constant's result-id.
  //
  // Note1: This method modifies the curTypeInst. Do not call in the middle of
  // construction of another instruction.
  //
  // Note 2: Integer constants may need to be generated for cases where there is
  // no SpirvConstantInteger instruction in the module. For example, we need to
  // emit an integer in order to create an array type. Therefore,
  // 'getOrCreateConstantInt' has a different signature than others. If a
  // constant instruction is provided, and it already has a result-id assigned,
  // it will be used. Otherwise a new result-id will be allocated for the
  // instruction.
  uint32_t
  getOrCreateConstantInt(llvm::APInt value, const SpirvType *type,
                         bool isSpecConst,
                         SpirvInstruction *constantInstruction = nullptr);
  uint32_t getOrCreateConstantFloat(SpirvConstantFloat *);
  uint32_t getOrCreateConstantComposite(SpirvConstantComposite *);
  uint32_t getOrCreateConstantNull(SpirvConstantNull *);
  uint32_t getOrCreateConstantBool(SpirvConstantBoolean *);

private:
  void initTypeInstruction(spv::Op op);
  void finalizeTypeInstruction();

  // Returns the result-id for the given type and decorations. If a type with
  // the same decorations have already been used, it returns the existing
  // result-id. If not, creates a new result-id for such type and returns it.
  uint32_t getResultIdForType(const SpirvType *, bool *alreadyExists);

  // Emits an OpName (if memberIndex is not provided) or OpMemberName (if
  // memberIndex is provided) for the given target result-id.
  void emitNameForType(llvm::StringRef name, uint32_t targetTypeId,
                       llvm::Optional<uint32_t> memberIndex = llvm::None);

  // There is no guarantee that an instruction or a function or a basic block
  // has been assigned result-id. This method returns the result-id for the
  // given object. If a result-id has not been assigned yet, it'll assign
  // one and return it.
  template <class T> uint32_t getOrAssignResultId(T *obj) {
    if (!obj->getResultId()) {
      obj->setResultId(takeNextIdFunction());
    }
    return obj->getResultId();
  }

private:
  /// Emits error to the diagnostic engine associated with this visitor.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              SourceLocation loc = {}) {
    const auto diagId = astContext.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, message);
    return astContext.getDiagnostics().Report(loc, diagId);
  }

private:
  ASTContext &astContext;
  SpirvContext &context;
  std::vector<uint32_t> curTypeInst;
  std::vector<uint32_t> curDecorationInst;
  std::vector<uint32_t> *debugVariableBinary;
  std::vector<uint32_t> *annotationsBinary;
  std::vector<uint32_t> *typeConstantBinary;
  std::function<uint32_t()> takeNextIdFunction;

  // The array type requires the result-id of an OpConstant for its length. In
  // order to avoid duplicate OpConstant instructions, we keep a map of constant
  // uint value to the result-id of the OpConstant for that value.
  llvm::DenseMap<std::pair<uint64_t, const SpirvType *>, uint32_t>
      emittedConstantInts;
  llvm::DenseMap<std::pair<uint64_t, const SpirvType *>, uint32_t>
      emittedConstantFloats;
  llvm::SmallVector<SpirvConstantComposite *, 8> emittedConstantComposites;
  llvm::SmallVector<SpirvConstantNull *, 8> emittedConstantNulls;
  SpirvConstantBoolean *emittedConstantBools[2];

  // emittedTypes is a map that caches the result-id of types in order to avoid
  // emitting an identical type multiple times.
  llvm::DenseMap<const SpirvType *, uint32_t> emittedTypes;
};

/// \breif The visitor class that emits the SPIR-V words from the in-memory
/// representation.
class EmitVisitor : public Visitor {
public:
  /// \brief The struct representing a SPIR-V module header.
  struct Header {
    /// \brief Default constructs a SPIR-V module header with id bound 0.
    Header(uint32_t bound, uint32_t version);

    /// \brief Feeds the consumer with all the SPIR-V words for this header.
    std::vector<uint32_t> takeBinary();

    const uint32_t magicNumber;
    uint32_t version;
    const uint32_t generator;
    uint32_t bound;
    const uint32_t reserved;
  };

public:
  EmitVisitor(ASTContext &astCtx, SpirvContext &spvCtx,
              const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx), astContext(astCtx), id(0),
        typeHandler(astCtx, spvCtx, &debugVariableBinary, &annotationsBinary,
                    &typeConstantBinary,
                    [this]() -> uint32_t { return takeNextId(); }),
        debugMainFileId(0), debugLine(0), debugColumn(0),
        lastOpWasMergeInst(false) {}

  // Visit different SPIR-V constructs for emitting.
  bool visit(SpirvModule *, Phase phase);
  bool visit(SpirvFunction *, Phase phase);
  bool visit(SpirvBasicBlock *, Phase phase);

  bool visit(SpirvCapability *);
  bool visit(SpirvExtension *);
  bool visit(SpirvExtInstImport *);
  bool visit(SpirvMemoryModel *);
  bool visit(SpirvEmitVertex *);
  bool visit(SpirvEndPrimitive *);
  bool visit(SpirvEntryPoint *);
  bool visit(SpirvExecutionMode *);
  bool visit(SpirvString *);
  bool visit(SpirvSource *);
  bool visit(SpirvModuleProcessed *);
  bool visit(SpirvDecoration *);
  bool visit(SpirvVariable *);
  bool visit(SpirvFunctionParameter *);
  bool visit(SpirvLoopMerge *);
  bool visit(SpirvSelectionMerge *);
  bool visit(SpirvBranch *);
  bool visit(SpirvBranchConditional *);
  bool visit(SpirvKill *);
  bool visit(SpirvReturn *);
  bool visit(SpirvSwitch *);
  bool visit(SpirvUnreachable *);
  bool visit(SpirvAccessChain *);
  bool visit(SpirvAtomic *);
  bool visit(SpirvBarrier *);
  bool visit(SpirvBinaryOp *);
  bool visit(SpirvBitFieldExtract *);
  bool visit(SpirvBitFieldInsert *);
  bool visit(SpirvConstantBoolean *);
  bool visit(SpirvConstantInteger *);
  bool visit(SpirvConstantFloat *);
  bool visit(SpirvConstantComposite *);
  bool visit(SpirvConstantNull *);
  bool visit(SpirvCompositeConstruct *);
  bool visit(SpirvCompositeExtract *);
  bool visit(SpirvCompositeInsert *);
  bool visit(SpirvExtInst *);
  bool visit(SpirvFunctionCall *);
  bool visit(SpirvNonUniformBinaryOp *);
  bool visit(SpirvNonUniformElect *);
  bool visit(SpirvNonUniformUnaryOp *);
  bool visit(SpirvImageOp *);
  bool visit(SpirvImageQuery *);
  bool visit(SpirvImageSparseTexelsResident *);
  bool visit(SpirvImageTexelPointer *);
  bool visit(SpirvLoad *);
  bool visit(SpirvSampledImage *);
  bool visit(SpirvSelect *);
  bool visit(SpirvSpecConstantBinaryOp *);
  bool visit(SpirvSpecConstantUnaryOp *);
  bool visit(SpirvStore *);
  bool visit(SpirvUnaryOp *);
  bool visit(SpirvVectorShuffle *);
  bool visit(SpirvArrayLength *);
  bool visit(SpirvRayTracingOpNV *);
  bool visit(SpirvDebugInfoNone *);
  bool visit(SpirvDebugSource *);
  bool visit(SpirvDebugCompilationUnit *);
  bool visit(SpirvDebugLexicalBlock *);
  bool visit(SpirvDebugScope *);
  bool visit(SpirvDebugFunction *);
  bool visit(SpirvDebugLocalVariable *);
  bool visit(SpirvDebugDeclare *);
  bool visit(SpirvDebugGlobalVariable *);
  bool visit(SpirvDebugExpression *);
  bool visit(SpirvDebugTypeBasic *);
  bool visit(SpirvDebugTypeVector *);
  bool visit(SpirvDebugTypeArray *);
  bool visit(SpirvDebugTypeFunction *);

  // Returns the assembled binary built up in this visitor.
  std::vector<uint32_t> takeBinary();

private:
  // Returns the next available result-id.
  uint32_t takeNextId() { return ++id; }

  // There is no guarantee that an instruction or a function or a basic block
  // has been assigned result-id. This method returns the result-id for the
  // given object. If a result-id has not been assigned yet, it'll assign
  // one and return it.
  template <class T> uint32_t getOrAssignResultId(T *obj) {
    if (!obj->getResultId()) {
      obj->setResultId(takeNextId());
    }
    return obj->getResultId();
  }

  /// If we already created OpString for str, just return the created one.
  /// Otherwise, create it, keep it in debugFileIdMap, and return it.
  uint32_t getOrCreateOpString(llvm::StringRef str);

  // Emits an OpLine instruction for the given operation into the given binary
  // section.
  void emitDebugLine(spv::Op op, const SourceLocation &loc,
                     std::vector<uint32_t> *section);

  // Initiates the creation of a new instruction with the given Opcode.
  void initInstruction(spv::Op, const SourceLocation &);
  // Initiates the creation of the given SPIR-V instruction.
  // If the given instruction has a return type, it will also trigger emitting
  // the necessary type (and its associated decorations) and uses its result-id
  // in the instruction.
  void initInstruction(SpirvInstruction *);

  // Finalizes the current instruction by encoding the instruction size into the
  // first word, and then appends the current instruction to the given SPIR-V
  // binary section.
  void finalizeInstruction(std::vector<uint32_t> *section);

  // Encodes the given string into the current instruction that is being built.
  void encodeString(llvm::StringRef value);

  // Emits an OpName instruction into the debugBinary for the given target.
  void emitDebugNameForInstruction(uint32_t resultId, llvm::StringRef name);

  // TODO: Add a method for adding OpMemberName instructions for struct members
  // using the type information.

private:
  /// Emits error to the diagnostic engine associated with this visitor.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              SourceLocation loc = {}) {
    const auto diagId = astContext.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, message);
    return astContext.getDiagnostics().Report(loc, diagId);
  }

private:
  // Object that holds Clang AST nodes.
  ASTContext &astContext;
  // The last result-id that's been used so far.
  uint32_t id;
  // Handler for emitting types and their related instructions.
  EmitTypeHandler typeHandler;
  // Current instruction being built
  SmallVector<uint32_t, 16> curInst;
  // All preamble instructions in the following order:
  // OpCapability, OpExtension, OpExtInstImport, OpMemoryModel, OpEntryPoint,
  // OpExecutionMode(Id)
  std::vector<uint32_t> preambleBinary;
  // Debug instructions related to file. Includes:
  // OpString, OpSourceExtension, OpSource, OpSourceContinued
  std::vector<uint32_t> debugFileBinary;
  // All debug instructions related to variable name. Includes:
  // OpName, OpMemberName, OpModuleProcessed
  std::vector<uint32_t> debugVariableBinary;
  // All annotation instructions: OpDecorate, OpMemberDecorate, OpGroupDecorate,
  // OpGroupMemberDecorate, and OpDecorationGroup.
  std::vector<uint32_t> annotationsBinary;
  // All type and constant instructions
  std::vector<uint32_t> typeConstantBinary;
  // All global variable declarations (all OpVariable instructions whose Storage
  // Class is not Function)
  std::vector<uint32_t> globalVarsBinary;
  // All Rich Debug Info instructions
  std::vector<uint32_t> richDebugInfo;
  // All other instructions
  std::vector<uint32_t> mainBinary;
  // File information for debugging that will be used by OpLine.
  llvm::StringMap<uint32_t> debugFileIdMap;
  // Main file information for debugging that will be used by OpLine.
  uint32_t debugMainFileId;
  // One HLSL source line may result in several SPIR-V instructions. In order to
  // avoid emitting many OpLine instructions with identical line and column
  // numbers, we record the last line and column number that was used by OpLine,
  // and only emit a new OpLine when a new line/column in the source is
  // discovered. The last debug line number information emitted by OpLine.
  uint32_t debugLine;
  // The last debug column number information emitted by OpLine.
  uint32_t debugColumn;
  // True if the last emitted instruction was OpSelectionMerge or OpLoopMerge.
  bool lastOpWasMergeInst;
};

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_EMITVISITOR_H
