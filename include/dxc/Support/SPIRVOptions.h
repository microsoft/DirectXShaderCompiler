///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SPIRVOptions.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef LLVM_SPIRV_OPTIONS_H
#define LLVM_SPIRV_OPTIONS_H

#ifdef ENABLE_SPIRV_CODEGEN

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Option/ArgList.h"

enum class SpirvLayoutRule {
  Void,
  GLSLStd140,
  GLSLStd430,
  RelaxedGLSLStd140, // std140 with relaxed vector layout
  RelaxedGLSLStd430, // std430 with relaxed vector layout
  FxcCTBuffer,       // fxc.exe layout rule for cbuffer/tbuffer
  FxcSBuffer,        // fxc.exe layout rule for structured buffers
};

struct SpirvCodeGenOptions {
  /// Disable legalization and optimization and emit raw SPIR-V
  bool codeGenHighLevel;
  bool defaultRowMajor;
  bool disableValidation;
  bool invertY; // Additive inverse
  bool invertW; // Multiplicative inverse
  bool useGlLayout;
  bool useDxLayout;
  bool enable16BitTypes;
  bool enableReflect;
  bool debugInfoFile;
  bool debugInfoSource;
  bool debugInfoLine;
  bool debugInfoTool;
  bool noWarnIgnoredFeatures;
  llvm::StringRef stageIoOrder;
  llvm::SmallVector<int32_t, 4> bShift;
  llvm::SmallVector<int32_t, 4> tShift;
  llvm::SmallVector<int32_t, 4> sShift;
  llvm::SmallVector<int32_t, 4> uShift;
  std::vector<std::string> bindRegister;
  llvm::SmallVector<llvm::StringRef, 4> allowedExtensions;
  llvm::StringRef targetEnv;
  SpirvLayoutRule cBufferLayoutRule;
  SpirvLayoutRule tBufferLayoutRule;
  SpirvLayoutRule sBufferLayoutRule;
  llvm::SmallVector<llvm::StringRef, 4> optConfig;

  // String representation of all command line options.
  std::string clOptions;
};

#endif // ENABLE_SPIRV_CODEGEN
#endif // LLVM_SPIRV_OPTIONS_H
