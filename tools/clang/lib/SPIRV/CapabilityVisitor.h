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

#include "clang/SPIRV/FeatureManager.h"
#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

class SpirvBuilder;

class CapabilityVisitor : public Visitor {
public:
  CapabilityVisitor(ASTContext &astCtx, SpirvContext &spvCtx,
                    const SpirvCodeGenOptions &opts, SpirvBuilder &builder)
      : Visitor(opts, spvCtx), spvBuilder(builder),
        featureManager(astCtx.getDiagnostics(), opts) {}

  bool visit(SpirvDecoration *decor) override;
  bool visit(SpirvEntryPoint *) override;
  bool visit(SpirvExecutionMode *) override;
  bool visit(SpirvImageQuery *) override;
  bool visit(SpirvImageOp *) override;
  bool visit(SpirvImageSparseTexelsResident *) override;
  bool visit(SpirvExtInstImport *) override;
  bool visit(SpirvExtInst *) override;
  bool visit(SpirvDemoteToHelperInvocationEXT *) override;

  using Visitor::visit;

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  bool visitInstruction(SpirvInstruction *instr) override;

private:
  /// Adds necessary capabilities for using the given type.
  /// The called may also provide the storage class for variable types, because
  /// in the case of variable types, the storage class may affect the capability
  /// that is used.
  void addCapabilityForType(const SpirvType *, SourceLocation loc,
                            spv::StorageClass sc);

  /// Checks that the given extension is a valid extension for the target
  /// environment (e.g. Vulkan 1.0). And if so, utilizes the SpirvBuilder to add
  /// the given extension to the SPIR-V module in memory.
  void addExtension(Extension ext, llvm::StringRef target, SourceLocation loc);

  /// Checks that the given capability is a valid capability. And if so,
  /// utilizes the SpirvBuilder to add the given capability to the SPIR-V module
  /// in memory.
  void addCapability(spv::Capability, SourceLocation loc = {});

  /// Returns the capability required to non-uniformly index into the given
  /// type.
  spv::Capability getNonUniformCapability(const SpirvType *);

private:
  SpirvBuilder &spvBuilder;        ///< SPIR-V builder
  spv::ExecutionModel shaderModel; ///< Execution model
  FeatureManager featureManager;   ///< SPIR-V version/extension manager.
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_CAPABILITYVISITOR_H
