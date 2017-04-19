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

uint32_t SPIRVContext::getResultIdForType(const Type *t) {
  assert(t != nullptr);
  uint32_t result_id = 0;

  auto iter = typeResultIdMap.find(t);
  if (iter == typeResultIdMap.end()) {
    // The Type has not been defined yet. Reserve an ID for it.
    result_id = takeNextId();
    typeResultIdMap[t] = result_id;
  } else {
    result_id = iter->second;
  }

  assert(result_id != 0);
  return result_id;
}

const Type *SPIRVContext::registerType(const Type &t) {
  // Insert function will only insert if it doesn't already exist in the set.
  TypeSet::iterator it;
  std::tie(it, std::ignore) = existingTypes.insert(t);
  return &*it;
}

const Decoration *SPIRVContext::registerDecoration(const Decoration &d) {
  // Insert function will only insert if it doesn't already exist in the set.
  DecorationSet::iterator it;
  std::tie(it, std::ignore) = existingDecorations.insert(d);
  return &*it;
}

} // end namespace spirv
} // end namespace clang
