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

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
/// Structs for controlling behaviors of SPIR-V codegen.
struct EmitSPIRVOptions {
  /// Disable legalization and optimization and emit raw SPIR-V
  bool codeGenHighLevel;
  bool defaultRowMajor;
  bool disableValidation;
  bool invertY;
  bool ignoreUnusedResources;
  bool enable16BitTypes;
  llvm::StringRef stageIoOrder;
  llvm::SmallVector<uint32_t, 4> bShift;
  llvm::SmallVector<uint32_t, 4> tShift;
  llvm::SmallVector<uint32_t, 4> sShift;
  llvm::SmallVector<uint32_t, 4> uShift;
};
} // end namespace clang

#endif
