//===--- SpirvBasicBlock.cpp - SPIR-V Basic Block Implementation -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvBasicBlock.h"

namespace clang {
namespace spirv {

SpirvBasicBlock::SpirvBasicBlock(uint32_t id, llvm::StringRef name)
    : labelId(id), labelName(name), mergeTarget(nullptr),
      continueTarget(nullptr) {}

bool SpirvBasicBlock::hasTerminator() const {
  return !instructions.empty() && instructions.back()->isTerminator();
}

} // end namespace spirv
} // end namespace clang
