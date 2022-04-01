//===--- SpirvTypeVisitor.cpp - AST type to SPIR-V type impl -----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SpirvTypeVisitor.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvUtils.h"

namespace clang {
namespace spirv {

bool SpirvTypeVisitor::visitInstruction(SpirvInstruction *instr) {
  SpirvUtils utils(spvContext);
  utils.applyResultTypeTransformations(instr);
  return true;
}

} // namespace spirv
} // namespace clang
