//===--- PreciseVisitor.h ---- Precise Visitor -------------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_PRECISEVISITOR_H
#define LLVM_CLANG_LIB_SPIRV_PRECISEVISITOR_H

#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

class PreciseVisitor : public Visitor {
public:
  PreciseVisitor(SpirvContext &spvCtx, const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx) {}

  bool visit(SpirvFunction *, Phase);

  bool visit(SpirvVariable *);
  bool visit(SpirvReturn *);
  bool visit(SpirvSelect *);
  bool visit(SpirvVectorShuffle *);
  bool visit(SpirvBitFieldExtract *);
  bool visit(SpirvBitFieldInsert *);
  bool visit(SpirvAtomic *);
  bool visit(SpirvCompositeConstruct *);
  bool visit(SpirvCompositeExtract *);
  bool visit(SpirvCompositeInsert *);
  bool visit(SpirvLoad *);
  bool visit(SpirvStore *);
  bool visit(SpirvBinaryOp *);
  bool visit(SpirvUnaryOp *);
  bool visit(SpirvNonUniformBinaryOp *);
  bool visit(SpirvNonUniformUnaryOp *);
  bool visit(SpirvExtInst *);

  // TODO: Support propagation of 'precise' through OpSpecConstantOp and image
  // operations if necessary. Related instruction classes are:
  // SpirvSpecConstantBinaryOp, SpirvSpecConstantUnaryOp
  // SpirvImageOp, SpirvImageQuery, SpirvImageTexelPointer, SpirvSampledImage

private:
  bool curFnRetValPrecise; ///< Whether current function is 'precise'
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_PRECISEVISITOR_H
