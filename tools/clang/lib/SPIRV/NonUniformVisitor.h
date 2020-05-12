//===--- NonUniformVisitor.h - NonUniform Visitor ----------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_NONUNIFORMVISITOR_H
#define LLVM_CLANG_LIB_SPIRV_NONUNIFORMVISITOR_H

#include "clang/SPIRV/FeatureManager.h"
#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

class SpirvBuilder;

class NonUniformVisitor : public Visitor {
public:
  NonUniformVisitor(SpirvContext &spvCtx, const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx) {}

  bool visit(SpirvLoad *);
  bool visit(SpirvAccessChain *);
  bool visit(SpirvUnaryOp *);
  bool visit(SpirvBinaryOp *);
  bool visit(SpirvSampledImage *);
  bool visit(SpirvImageTexelPointer *);
  bool visit(SpirvAtomic *);

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  bool visitInstruction(SpirvInstruction *instr) { return true; }

private:
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_NONUNIFORMVISITOR_H
