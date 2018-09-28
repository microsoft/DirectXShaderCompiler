//===-- SpirvVisitor.h - SPIR-V Visitor -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVVISITOR_H
#define LLVM_CLANG_SPIRV_SPIRVVISITOR_H

#include "clang/SPIRV/SpirvInstruction.h"

namespace clang {
namespace spirv {

class SpirvModule;
class SpirvFunction;
class SpirvBasicBlock;

/// \brief The base class for different SPIR-V visitor classes.
/// Each Visitor class serves a specific purpose and should override the
/// suitable visit methods accordingly in order to achieve its purpose.
class Visitor {
public:
  enum Phase {
    Init, //< Before starting the visit of the given construct
    Done, //< After finishing the visit of the given construct
  };

  // Virtual destructor
  virtual ~Visitor() = default;

  // Forbid copy construction and assignment
  Visitor(const Visitor &) = delete;
  Visitor &operator=(const Visitor &) = delete;

  // Forbid move construction and assignment
  Visitor(Visitor &&) = delete;
  Visitor &operator=(Visitor &&) = delete;

  // Visiting different SPIR-V constructs.
  virtual bool visit(SpirvModule *, Phase) {}
  virtual bool visit(SpirvFunction *, Phase) {}
  virtual bool visit(SpirvBasicBlock *, Phase) {}
  virtual bool visit(SpirvInstruction *) {}
  virtual bool visit(SpirvCapability *) {}
  virtual bool visit(SpirvExtension *) {}
  virtual bool visit(SpirvExtInstImport *) {}
  virtual bool visit(SpirvMemoryModel *) {}
  virtual bool visit(SpirvEntryPoint *) {}
  virtual bool visit(SpirvExecutionMode *) {}
  virtual bool visit(SpirvString *) {}
  virtual bool visit(SpirvSource *) {}
  virtual bool visit(SpirvName *) {}
  virtual bool visit(SpirvModuleProcessed *) {}
  virtual bool visit(SpirvDecoration *) {}
  virtual bool visit(SpirvVariable *) {}
  virtual bool visit(SpirvFunctionParameter *) {}
  virtual bool visit(SpirvLoopMerge *) {}
  virtual bool visit(SpirvSelectionMerge *) {}
  virtual bool visit(SpirvBranching *) {}
  virtual bool visit(SpirvBranch *) {}
  virtual bool visit(SpirvBranchConditional *) {}
  virtual bool visit(SpirvKill *) {}
  virtual bool visit(SpirvReturn *) {}
  virtual bool visit(SpirvSwitch *) {}
  virtual bool visit(SpirvUnreachable *) {}
  virtual bool visit(SpirvAccessChain *) {}
  virtual bool visit(SpirvAtomic *) {}
  virtual bool visit(SpirvBarrier *) {}
  virtual bool visit(SpirvBinaryOp *) {}
  virtual bool visit(SpirvBitFieldExtract *) {}
  virtual bool visit(SpirvBitFieldInsert *) {}
  virtual bool visit(SpirvComposite *) {}
  virtual bool visit(SpirvCompositeExtract *) {}
  virtual bool visit(SpirvExtInst *) {}
  virtual bool visit(SpirvFunctionCall *) {}
  virtual bool visit(SpirvNonUniformBinaryOp *) {}
  virtual bool visit(SpirvNonUniformElect *) {}
  virtual bool visit(SpirvNonUniformUnaryOp *) {}
  virtual bool visit(SpirvImageOp *) {}
  virtual bool visit(SpirvImageQuery *) {}
  virtual bool visit(SpirvImageSparseTexelsResident *) {}
  virtual bool visit(SpirvImageTexelPointer *) {}
  virtual bool visit(SpirvLoad *) {}
  virtual bool visit(SpirvSampledImage *) {}
  virtual bool visit(SpirvSelect *) {}
  virtual bool visit(SpirvSpecConstantBinaryOp *) {}
  virtual bool visit(SpirvSpecConstantUnaryOp *) {}
  virtual bool visit(SpirvStore *) {}
  virtual bool visit(SpirvUnaryOp *) {}
  virtual bool visit(SpirvVectorShuffle *) {}

protected:
  Visitor() = default;
};

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVVISITOR_H
