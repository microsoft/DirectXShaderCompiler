//===--- SpirvTypeVisitor.h - SPIR-V type visitor ----------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_SPIRVTYPEVISITOR_H
#define LLVM_CLANG_LIB_SPIRV_SPIRVTYPEVISITOR_H

#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

/// The class responsible for some legalization of SPIR-V types.
class SpirvTypeVisitor : public Visitor {
public:
  SpirvTypeVisitor(SpirvContext &spvCtx, const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx), spvContext(spvCtx) {}

  using Visitor::visit;

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  bool visitInstruction(SpirvInstruction *instr) override;

private:
  SpirvContext &spvContext; /// SPIR-V context
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_SPIRVTYPEVISITOR_H
