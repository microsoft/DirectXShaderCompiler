//===--- SpirvFunction.cpp - SPIR-V Function Implementation ------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvFunction.h"
#include "BlockReadableOrder.h"
#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

SpirvFunction::SpirvFunction(QualType returnType, SourceLocation loc,
                             llvm::StringRef name, bool isPrecise)
    : functionId(0), astReturnType(returnType), returnType(nullptr),
      fnType(nullptr), relaxedPrecision(false), precise(isPrecise),
      containsAlias(false), rvalue(false), functionLoc(loc),
      functionName(name) {}

SpirvFunction::~SpirvFunction() {
  for (auto *param : parameters)
    param->releaseMemory();
  for (auto *var : variables)
    var->releaseMemory();
  for (auto *bb : basicBlocks)
    bb->~SpirvBasicBlock();
}

bool SpirvFunction::invokeVisitor(Visitor *visitor, bool reverseOrder) {
  if (!visitor->visit(this, Visitor::Phase::Init))
    return false;

  for (auto *param : parameters)
    visitor->visit(param);

  // Collect basic blocks in a human-readable order that satisfies SPIR-V
  // validation rules.
  std::vector<SpirvBasicBlock *> orderedBlocks;
  if (!basicBlocks.empty()) {
    BlockReadableOrderVisitor([&orderedBlocks](SpirvBasicBlock *block) {
      orderedBlocks.push_back(block);
    }).visit(basicBlocks.front());
  }

  SpirvBasicBlock *firstBB = orderedBlocks.empty() ? nullptr : orderedBlocks[0];

  if (reverseOrder)
    std::reverse(orderedBlocks.begin(), orderedBlocks.end());

  for (auto *bb : orderedBlocks) {
    // The first basic block of the function should first visit the function
    // variables.
    if (bb == firstBB) {
      if (!bb->invokeVisitor(visitor, variables, reverseOrder))
        return false;
    }
    // The rest of the basic blocks in the function do not need to visit
    // function variables.
    else {
      if (!bb->invokeVisitor(visitor, {}, reverseOrder))
        return false;
    }
  }

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
