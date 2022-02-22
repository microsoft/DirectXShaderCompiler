//===-- Predefines.h - Predefines for SPIR-V ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===--------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_PREDEFINES_H
#define LLVM_CLANG_SPIRV_PREDEFINES_H

#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace spirv {

void BuildPredefinesForSPIRV(llvm::raw_ostream &Output, bool isHlsl2021OrAbove);

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_PREDEFINES_H
