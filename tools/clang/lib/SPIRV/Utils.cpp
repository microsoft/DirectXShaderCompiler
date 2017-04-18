//===-- Utils.cpp - SPIR-V Utils --------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/Utils.h"
#include "llvm/llvm_assert/assert.h"

namespace clang {
namespace spirv {
namespace utils {

/// \brief Reinterprets a given string as sequence of words.
std::vector<uint32_t> encodeSPIRVString(std::string s) {
  // Initialize all words to 0.
  size_t numChars = s.size();
  std::vector<uint32_t> result(numChars / 4 + 1, 0);

  // From the SPIR-V spec, literal string is
  //
  // A nul-terminated stream of characters consuming an integral number of
  // words. The character set is Unicode in the UTF-8 encoding scheme. The UTF-8
  // octets (8-bit bytes) are packed four per word, following the little-endian
  // convention (i.e., the first octet is in the lowest-order 8 bits of the
  // word). The final word contains the string's nul-termination character (0),
  // and all contents past the end of the string in the final word are padded
  // with 0.
  //
  // So the following works on little endian machines.
  char *strDest = reinterpret_cast<char *>(result.data());
  strncpy(strDest, s.c_str(), numChars);
  return result;
}

/// \brief Reinterprets the given vector of 32-bit words as a string.
/// Expectes that the words represent a NULL-terminated string.
/// Assumes Little Endian architecture.
std::string decodeSPIRVString(std::vector<uint32_t>& vec) {
  std::string result;
  if (!vec.empty()) {
    result = std::string(reinterpret_cast<const char*>(vec.data()));
  }
  return result;
}

} // end namespace utils
} // end namespace spirv
} // end namespace clang
