//===- Dxil2SpvTestOptions.h ----- Command Line Options--------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the command line options that can be passed to dxil2spv
// gtests.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_UNITTESTS_DXIL2SPV_TEST_OPTIONS_H
#define LLVM_CLANG_UNITTESTS_DXIL2SPV_TEST_OPTIONS_H

#include <string>

namespace clang {
namespace dxil2spv {

/// \brief Includes any command line options that may be passed to gtest for
/// running the dxil2spv tests.
namespace testOptions {

/// \brief Command line option that specifies the path to the directory that
/// contains files that have the DXIL source code and expected SPIR-V code.
extern std::string inputDataDir;

} // namespace testOptions
} // namespace dxil2spv
} // namespace clang

#endif
