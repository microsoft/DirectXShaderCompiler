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
                             spv::FunctionControlMask control)
    : functionType(type), functionId(id), functionControl(control) {}

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

} // end namespace spirv
} // end namespace clang
