//===- Dxil2SpvTestOptions.cpp ----- Test Options Init---------------------===//
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

#include "Dxil2SpvTestOptions.h"

namespace clang {
namespace dxil2spv {
namespace testOptions {

std::string inputDataDir = "";

} // namespace testOptions
} // namespace dxil2spv
} // namespace clang
