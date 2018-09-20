//===--- SpirvFunction.cpp - SPIR-V Function Implementation ------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvFunction.h"

namespace clang {
namespace spirv {

SpirvFunction::SpirvFunction(QualType type, uint32_t id,
                             spv::FunctionControlMask control)
    : functionType(type), functionId(id), functionControl(control) {}

} // end namespace spirv
} // end namespace clang
