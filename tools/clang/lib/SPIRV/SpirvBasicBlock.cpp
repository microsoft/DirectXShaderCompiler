//===--- SpirvBasicBlock.cpp - SPIR-V Basic Block Implementation -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

SpirvBasicBlock::SpirvBasicBlock(llvm::StringRef name)
    : labelId(0), labelName(name), mergeTarget(nullptr),
      continueTarget(nullptr), debugScope(nullptr) {}

SpirvBasicBlock::~SpirvBasicBlock() {
  for (auto instructionNode : instructions)
    instructionNode.instruction->releaseMemory();
  if (debugScope)
    debugScope->releaseMemory();
}

bool SpirvBasicBlock::hasTerminator() const {
  return !instructions.empty() &&
         isa<SpirvTerminator>(instructions.back().instruction);
}

bool SpirvBasicBlock::invokeVisitor(Visitor *visitor,
                                    llvm::ArrayRef<SpirvVariable *> vars,
                                    bool reverseOrder) {
  if (!visitor->visit(this, Visitor::Phase::Init))
    return false;

  if (debugScope && !visitor->visit(debugScope))
    return false;

  if (reverseOrder) {
    for (auto iter = instructions.rbegin(); iter != instructions.rend();
         ++iter) {
      if (!iter->instruction->invokeVisitor(visitor))
        return false;
    }
    // If a basic block is the first basic block of a function, it should
    // include all the variables of the function.
    if (!vars.empty()) {
      for (auto var = vars.rbegin(); var != vars.rend(); ++var) {
        if (!(*var)->invokeVisitor(visitor))
          return false;
      }
    }
  } else {
    // If a basic block is the first basic block of a function, it should
    // include all the variables of the function.
    if (!vars.empty()) {
      for (auto *var : vars) {
        if (!var->invokeVisitor(visitor))
          return false;
      }
    }

    for (auto iter = instructions.begin(); iter != instructions.end(); ++iter) {
      if (!iter->instruction->invokeVisitor(visitor))
        return false;
    }
  }

  if (!visitor->visit(this, Visitor::Phase::Done))
    return false;

  return true;
}

void SpirvBasicBlock::addSuccessor(SpirvBasicBlock *bb) {
  assert(bb && "cannot add null basic block as successor");
  successors.push_back(bb);
}

} // end namespace spirv
} // end namespace clang
