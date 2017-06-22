//===- unittests/SPIRV/SPIRVTestUtils.h - Common utilities for tests ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines utility functions for writing SPIR-V tests. They are
// not supposed to be included in source files other than tests.
//
//===----------------------------------------------------------------------===//

#include <initializer_list>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "clang/SPIRV/InstBuilder.h"
#include "spirv/1.0/spirv.hpp11"

namespace clang {
namespace spirv {

/// Creates an InstBuilder which pushes all constructed SPIR-V instructions
/// into the given binary.
inline InstBuilder constructInstBuilder(std::vector<uint32_t> &binary) {
  return InstBuilder([&binary](std::vector<uint32_t> &&words) {
    binary.insert(binary.end(), words.begin(), words.end());
  });
}

/// Creates a SPIR-V instruction.
inline std::vector<uint32_t>
constructInst(spv::Op opcode, std::initializer_list<uint32_t> params) {
  std::vector<uint32_t> words;
  words.push_back(static_cast<uint32_t>(opcode));
  for (auto w : params) {
    words.push_back(w);
  }
  words.front() |= static_cast<uint32_t>(words.size()) << 16;
  return words;
}

/// Expects the given status is success.
inline void expectBuildSuccess(InstBuilder::Status status) {
  EXPECT_EQ(InstBuilder::Status::Success, status);
}

/// Appends the part vector to the end of the all vector.
inline void appendVector(std::vector<uint32_t> *all,
                         const std::vector<uint32_t> &part) {
  all->insert(all->end(), part.begin(), part.end());
}

/// Returns the words in SPIR-V module header with the given id bound.
inline std::vector<uint32_t> getModuleHeader(uint32_t bound) {
  return {spv::MagicNumber, spv::Version, 14u << 16, bound, 0};
}

} // end namespace spirv
} // end namespace clang