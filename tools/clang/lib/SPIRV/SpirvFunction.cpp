//===--- SpirvFunction.cpp - SPIR-V Function Implementation ------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

SpirvFunction::SpirvFunction(QualType type, uint32_t id,
                             spv::FunctionControlMask control,
                             SourceLocation loc, llvm::StringRef name)
    : functionType(type), functionId(id), functionControl(control),
      functionLoc(loc), functionName(name) {}

bool SpirvFunction::invokeVisitor(Visitor *visitor) {
  if (!visitor->visit(this, Visitor::Phase::Init))
    return false;

  for (auto *param : parameters)
    visitor->visit(param);

  for (auto *bb : basicBlocks)
    if (!bb->invokeVisitor(visitor))
      return false;

  if (!visitor->visit(this, Visitor::Phase::Done))
    return false;

  return true;
}

void SpirvFunction::addParameter(SpirvFunctionParameter *param) {
  assert(param && "cannot add null function parameter");
  parameters.push_back(param);
}

void SpirvFunction::addVariable(SpirvVariable *var) {
  assert(var && "cannot add null variable to function");
  variables.push_back(var);
}

void SpirvFunction::addBasicBlock(SpirvBasicBlock *bb) {
  assert(bb && "cannot add null basic block to function");
  basicBlocks.push_back(bb);
}

} // end namespace spirv
} // end namespace clang
