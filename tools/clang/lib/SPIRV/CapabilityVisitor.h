//===--- CapabilityVisitor.h - Capability Visitor ----------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_CAPABILITYVISITOR_H
#define LLVM_CLANG_LIB_SPIRV_CAPABILITYVISITOR_H

#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

class SpirvBuilder;

class CapabilityVisitor : public Visitor {
public:
  CapabilityVisitor(SpirvContext &spvCtx, const SpirvCodeGenOptions &opts,
                    SpirvBuilder &builder)
      : Visitor(opts, spvCtx), spvBuilder(builder) {}

  bool visit(SpirvDecoration *decor);
  bool visit(SpirvEntryPoint *);
  bool visit(SpirvExecutionMode *);
  bool visit(SpirvImageQuery *);
  bool visit(SpirvImageOp *);
  bool visit(SpirvImageSparseTexelsResident *);

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  bool visitInstruction(SpirvInstruction *instr);

private:
  /// Adds necessary capabilities for using the given type.
  /// The called may also provide the storage class for variable types, because
  /// in the case of variable types, the storage class may affect the capability
  /// that is used.
  void addCapabilityForType(const SpirvType *, SourceLocation loc,
                            spv::StorageClass sc);

  /// Returns the capability required to non-uniformly index into the given
  /// type.
  spv::Capability getNonUniformCapability(const SpirvType *);

private:
  SpirvBuilder &spvBuilder;        /// SPIR-V builder
  spv::ExecutionModel shaderModel; /// Execution model
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_CAPABILITYVISITOR_H
