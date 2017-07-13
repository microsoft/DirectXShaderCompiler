//===- unittests/SPIRV/SpirvTestOptions.cpp ----- Test Options Init -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines and initializes command line options that can be passed to
// SPIR-V gtests.
//
//===----------------------------------------------------------------------===//

#include "SPIRVTestOptions.h"

namespace clang {
namespace spirv {
namespace testOptions {

std::string inputDataDir = "";

} // namespace testOptions
} // namespace spirv
} // namespace clang
