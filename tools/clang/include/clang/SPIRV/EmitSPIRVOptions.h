//===-- EmitSPIRVOptions.h - Options for SPIR-V CodeGen ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_EMITSPIRVOPTIONS_H
#define LLVM_CLANG_SPIRV_EMITSPIRVOPTIONS_H

#include "llvm/ADT/StringRef.h"

namespace clang {
/// Structs for controlling behaviors of SPIR-V codegen.
struct EmitSPIRVOptions {
  llvm::StringRef stageIoOrder;
};
} // end namespace clang

#endif
