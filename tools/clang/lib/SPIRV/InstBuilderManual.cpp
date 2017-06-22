//===-- InstBuilder.cpp - SPIR-V instruction builder ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/InstBuilder.h"
#include "clang/SPIRV/String.h"

#include "llvm/llvm_assert/assert.h"

namespace clang {
namespace spirv {

void InstBuilder::encodeString(std::string value) {
  const auto &words = string::encodeSPIRVString(value);
  TheInst.insert(TheInst.end(), words.begin(), words.end());
}

} // end namespace spirv
} // end namespace clang
