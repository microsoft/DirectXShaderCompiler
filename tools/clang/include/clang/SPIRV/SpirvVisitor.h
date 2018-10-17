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

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  virtual bool visitInstruction(SpirvInstruction *) {}

#define DEFINE_VISIT_METHOD(cls)                                               \
  virtual bool visit(cls *i) { visitInstruction(i); }

  DEFINE_VISIT_METHOD(SpirvCapability)
  DEFINE_VISIT_METHOD(SpirvExtension)
  DEFINE_VISIT_METHOD(SpirvExtInstImport)
  DEFINE_VISIT_METHOD(SpirvMemoryModel)
  DEFINE_VISIT_METHOD(SpirvEntryPoint)
  DEFINE_VISIT_METHOD(SpirvExecutionMode)
  DEFINE_VISIT_METHOD(SpirvString)
  DEFINE_VISIT_METHOD(SpirvSource)
  DEFINE_VISIT_METHOD(SpirvModuleProcessed)
  DEFINE_VISIT_METHOD(SpirvDecoration)
  DEFINE_VISIT_METHOD(SpirvVariable)

  DEFINE_VISIT_METHOD(SpirvFunctionParameter)
  DEFINE_VISIT_METHOD(SpirvLoopMerge)
  DEFINE_VISIT_METHOD(SpirvSelectionMerge)
  DEFINE_VISIT_METHOD(SpirvBranching)
  DEFINE_VISIT_METHOD(SpirvBranch)
  DEFINE_VISIT_METHOD(SpirvBranchConditional)
  DEFINE_VISIT_METHOD(SpirvKill)
  DEFINE_VISIT_METHOD(SpirvReturn)
  DEFINE_VISIT_METHOD(SpirvSwitch)
  DEFINE_VISIT_METHOD(SpirvUnreachable)

  DEFINE_VISIT_METHOD(SpirvAccessChain)
  DEFINE_VISIT_METHOD(SpirvAtomic)
  DEFINE_VISIT_METHOD(SpirvBarrier)
  DEFINE_VISIT_METHOD(SpirvBinaryOp)
  DEFINE_VISIT_METHOD(SpirvBitFieldExtract)
  DEFINE_VISIT_METHOD(SpirvBitFieldInsert)
  DEFINE_VISIT_METHOD(SpirvComposite)
  DEFINE_VISIT_METHOD(SpirvCompositeExtract)
  DEFINE_VISIT_METHOD(SpirvExtInst)
  DEFINE_VISIT_METHOD(SpirvFunctionCall)
  DEFINE_VISIT_METHOD(SpirvNonUniformBinaryOp)
  DEFINE_VISIT_METHOD(SpirvNonUniformElect)
  DEFINE_VISIT_METHOD(SpirvNonUniformUnaryOp)
  DEFINE_VISIT_METHOD(SpirvImageOp)
  DEFINE_VISIT_METHOD(SpirvImageQuery)
  DEFINE_VISIT_METHOD(SpirvImageSparseTexelsResident)
  DEFINE_VISIT_METHOD(SpirvImageTexelPointer)
  DEFINE_VISIT_METHOD(SpirvLoad)
  DEFINE_VISIT_METHOD(SpirvSampledImage)
  DEFINE_VISIT_METHOD(SpirvSelect)
  DEFINE_VISIT_METHOD(SpirvSpecConstantBinaryOp)
  DEFINE_VISIT_METHOD(SpirvSpecConstantUnaryOp)
  DEFINE_VISIT_METHOD(SpirvStore)
  DEFINE_VISIT_METHOD(SpirvUnaryOp)
  DEFINE_VISIT_METHOD(SpirvVectorShuffle)

#undef DEFINE_VISIT_METHOD

protected:
  Visitor() = default;
};

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVVISITOR_H
