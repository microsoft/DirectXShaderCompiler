//===--- RemoveBufferBlockVisitor.h - RemoveBufferBlock Visitor --*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_REMOVEBUFFERBLOCKVISITOR_H
#define LLVM_CLANG_LIB_SPIRV_REMOVEBUFFERBLOCKVISITOR_H

#include "clang/AST/ASTContext.h"
#include "clang/SPIRV/FeatureManager.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

class SpirvContext;

class RemoveBufferBlockVisitor : public Visitor {
public:
  RemoveBufferBlockVisitor(ASTContext &astCtx, SpirvContext &spvCtx,
                           const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx), featureManager(astCtx.getDiagnostics(), opts) {}

  bool visit(SpirvModule *, Phase) override;

  using Visitor::visit;

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  bool visitInstruction(SpirvInstruction *instr) override;

private:
  /// Returns true if |type| is a SPIR-V type whose interface type is
  /// StorageBuffer.
  bool hasStorageBufferInterfaceType(const SpirvType *type);

  ///  Returns true if the BufferBlock decoration is deprecated (Vulkan 1.2 or
  ///  above).
  bool isBufferBlockDecorationDeprecated();

  FeatureManager featureManager;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_REMOVEBUFFERBLOCKVISITOR_H
