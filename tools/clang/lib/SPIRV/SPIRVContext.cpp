//===--- SPIRVContext.cpp - SPIR-V SPIRVContext implementation-------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <tuple>

#include "clang/SPIRV/SPIRVContext.h"
#include "llvm/llvm_assert/assert.h"

namespace clang {
namespace spirv {

const Decoration *SPIRVContext::registerDecoration(const Decoration &d) {
  // Insert function will only insert if it doesn't already exist in the set.
  DecorationSet::iterator it;
  std::tie(it, std::ignore) = existingDecorations.insert(d);
  return &*it;
}

} // end namespace spirv
} // end namespace clang
