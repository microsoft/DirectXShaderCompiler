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

#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

class SpirvContext;

class RemoveBufferBlockVisitor : public Visitor {
public:
  RemoveBufferBlockVisitor(SpirvContext &spvCtx,
                           const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx) {}

  bool visit(SpirvModule *, Phase) override;

  using Visitor::visit;

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  bool visitInstruction(SpirvInstruction *instr) override;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_REMOVEBUFFERBLOCKVISITOR_H
