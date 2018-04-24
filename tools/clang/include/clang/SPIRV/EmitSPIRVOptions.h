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
namespace spirv {
/// Memory layout rules
enum class LayoutRule {
  Void,
  GLSLStd140,
  GLSLStd430,
  RelaxedGLSLStd140, // std140 with relaxed vector layout
  RelaxedGLSLStd430, // std430 with relaxed vector layout
  FxcCTBuffer,       // fxc.exe layout rule for cbuffer/tbuffer
  FxcSBuffer,        // fxc.exe layout rule for structured buffers
};
} // namespace spirv

/// Structs for controlling behaviors of SPIR-V codegen.
struct EmitSPIRVOptions {
  /// Disable legalization and optimization and emit raw SPIR-V
  bool codeGenHighLevel;
  bool defaultRowMajor;
  bool disableValidation;
  bool invertY;
  bool useGlLayout;
  bool useDxLayout;
  bool enable16BitTypes;
  bool enableReflect;
  bool enableDebugInfo;
  llvm::StringRef stageIoOrder;
  llvm::SmallVector<int32_t, 4> bShift;
  llvm::SmallVector<int32_t, 4> tShift;
  llvm::SmallVector<int32_t, 4> sShift;
  llvm::SmallVector<int32_t, 4> uShift;
  llvm::SmallVector<llvm::StringRef, 4> allowedExtensions;
  llvm::StringRef targetEnv;
  spirv::LayoutRule cBufferLayoutRule;
  spirv::LayoutRule tBufferLayoutRule;
  spirv::LayoutRule sBufferLayoutRule;

  // Initializes dependent fields appropriately
  void Initialize();
};
} // end namespace clang

#endif
