//===--- SpirvBuilder.cpp - SPIR-V Builder Implementation --------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvBuilder.h"

namespace clang {
namespace spirv {

SpirvBuilder::SpirvBuilder(SpirvContext &ctx)
    : context(ctx), module(nullptr), function(nullptr) {
  module = new (context) SpirvModule;
}

} // end namespace spirv
} // end namespace clang
