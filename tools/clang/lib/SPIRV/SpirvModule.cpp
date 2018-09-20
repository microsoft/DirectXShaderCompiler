//===--- SpirvModule.cpp - SPIR-V Module Implementation ----------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvModule.h"

namespace clang {
namespace spirv {

SpirvModule::SpirvModule()
    : bound(1), memoryModel(nullptr), debugSource(nullptr) {}

} // end namespace spirv
} // end namespace clang

