//===--- RelaxedPrecisionVisitor.h - RelaxedPrecision Visitor ----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_RELAXEDPRECISIONVISITOR_H
#define LLVM_CLANG_LIB_SPIRV_RELAXEDPRECISIONVISITOR_H

#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

class RelaxedPrecisionVisitor : public Visitor {
public:
  RelaxedPrecisionVisitor(SpirvContext &spvCtx, const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx) {}

  bool visit(SpirvFunction *, Phase);

  bool visit(SpirvVariable *);
  bool visit(SpirvFunctionParameter *);
  bool visit(SpirvAccessChain *);
  bool visit(SpirvAtomic *);
  bool visit(SpirvBitFieldExtract *);
  bool visit(SpirvBitFieldInsert *);
  bool visit(SpirvConstantBoolean *);
  bool visit(SpirvConstantInteger *);
  bool visit(SpirvConstantFloat *);
  bool visit(SpirvConstantComposite *);
  bool visit(SpirvCompositeConstruct *);
  bool visit(SpirvCompositeExtract *);
  bool visit(SpirvCompositeInsert *);
  bool visit(SpirvExtInst *);
  bool visit(SpirvFunctionCall *);
  bool visit(SpirvLoad *);
  bool visit(SpirvSelect *);
  bool visit(SpirvStore *);
  bool visit(SpirvSpecConstantBinaryOp *);
  bool visit(SpirvSpecConstantUnaryOp *);
  bool visit(SpirvBinaryOp *);
  bool visit(SpirvUnaryOp *);
  bool visit(SpirvVectorShuffle *);
  bool visit(SpirvImageOp *);

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  bool visitInstruction(SpirvInstruction *instr) { return true; }
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_RELAXEDPRECISIONVISITOR_H
