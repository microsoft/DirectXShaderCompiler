//===-- EmitVisitor.h - Emit Visitor ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_EMITVISITOR_H
#define LLVM_CLANG_SPIRV_EMITVISITOR_H

#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

class SpirvModule;
class SpirvFunction;
class SpirvBasicBlock;

/// \breif The visitor class that emits the SPIR-V words from the in-memory
/// representation.
class EmitVisitor : public Visitor {
public:
  EmitVisitor() = default;

  // Visit different SPIR-V constructs for emitting.
  bool visit(SpirvModule *, Phase phase);
  bool visit(SpirvFunction *, Phase phase);
  bool visit(SpirvBasicBlock *, Phase phase);

  bool visit(SpirvCapability *);
  bool visit(SpirvExtension *);
  bool visit(SpirvExtInstImport *);
  bool visit(SpirvMemoryModel *);
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
  bool visit(SpirvBranching *);
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
  bool visit(SpirvComposite *);
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

private:
  // Initiates the creation of a new instruction with the given Opcode.
  void initInstruction(spv::Op);

  // Finalizes the current instruction by encoding the instruction size into the
  // first word, and then appends the current instruction to the SPIR-V binary.
  void finalizeInstruction();

  // Encodes the given string into the current instruction that is being built.
  void encodeString(llvm::StringRef value);

  // Provides the next available <result-id>
  uint32_t getNextId() { return ++id; }

  // Emits an OpName instruction into the debugBinary for the given target.
  void emitDebugNameForInstruction(uint32_t resultId, llvm::StringRef name);

  // TODO: Add a method for adding OpMemberName instructions for struct members
  // using the type information.

private:
  uint32_t id;
  // Current instruction being built
  SmallVector<uint32_t, 16> curInst;
  // All preamble instructions in the following order:
  // OpCapability, OpExtension, OpExtInstImport, OpMemoryModel, OpEntryPoint,
  // OpExecutionMode(Id)
  std::vector<uint32_t> preambleBinary;
  // All debug instructions *except* OpLine. Includes:
  // OpString, OpSourceExtension, OpSource, OpSourceContinued, OpName,
  // OpMemberName, OpModuleProcessed
  std::vector<uint32_t> debugBinary;
  // All annotation instructions: OpDecorate, OpMemberDecorate, OpGroupDecorate,
  // OpGroupMemberDecorate, and OpDecorationGroup.
  std::vector<uint32_t> annotationsBinary;
  // All OpLine instructions
  std::vector<uint32_t> typeConstantBinary;
  // All other instructions
  std::vector<uint32_t> mainBinary;
};

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_EMITVISITOR_H
