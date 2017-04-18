//===-- Utils.h - SPIR-V Utils ----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_UTILS_H
#define LLVM_CLANG_SPIRV_UTILS_H

#include <string>
#include <vector>

namespace clang {
namespace spirv {
namespace utils {

/// \brief Reinterprets a given string as sequence of words. It follows the
/// SPIR-V string encoding requirements.
std::vector<uint32_t> encodeSPIRVString(std::string s);

/// \brief Reinterprets the given vector of 32-bit words as a string.
/// Expectes that the words represent a NULL-terminated string.
/// It follows the SPIR-V string encoding requirements.
std::string decodeSPIRVString(std::vector<uint32_t> &vec);

} // end namespace utils
} // end namespace spirv
} // end namespace clang

#endif