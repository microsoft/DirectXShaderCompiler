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

#include "spirv/1.0/spirv.hpp11"
#include "clang/SPIRV/InstBuilder.h"
#include "llvm/ADT/ArrayRef.h"

namespace clang {
namespace spirv {

/// Creates an InstBuilder which pushes all constructed SPIR-V instructions
/// into the given binary.
inline InstBuilder constructInstBuilder(std::vector<uint32_t> &binary) {
  return InstBuilder([&binary](std::vector<uint32_t> &&words) {
    binary.insert(binary.end(), words.begin(), words.end());
  });
}

/// Returns the words in SPIR-V module header with the given id bound.
inline std::vector<uint32_t> getModuleHeader(uint32_t bound) {
  return {spv::MagicNumber, spv::Version, 14u << 16, bound, 0};
}

/// Creates a SPIR-V instruction.
inline std::vector<uint32_t> constructInst(spv::Op opcode,
                                           llvm::ArrayRef<uint32_t> params) {
  std::vector<uint32_t> words;

  words.push_back(static_cast<uint32_t>(opcode));
  for (auto w : params) {
    words.push_back(w);
  }
  words.front() |= static_cast<uint32_t>(words.size()) << 16;

  return words;
}

/// A simple instruction builder for testing purpose.
class SimpleInstBuilder {
public:
  /// Constructs a simple instruction builder with no module header.
  SimpleInstBuilder() {}

  /// Constructs a simple instruction builder with module header having the
  /// given id bound.
  explicit SimpleInstBuilder(uint32_t bound) : words(getModuleHeader(bound)) {}

  /// Adds an instruction.
  void inst(spv::Op opcode, llvm::ArrayRef<uint32_t> params) {
    auto inst = constructInst(opcode, params);
    words.insert(words.end(), inst.begin(), inst.end());
  }

  const std::vector<uint32_t> &get() const { return words; }

private:
  std::vector<uint32_t> words;
};

/// Expects the given status is success.
inline void expectBuildSuccess(InstBuilder::Status status) {
  EXPECT_EQ(InstBuilder::Status::Success, status);
}

} // end namespace spirv
} // end namespace clang