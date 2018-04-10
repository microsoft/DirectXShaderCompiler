//===-- EmitSPIRVOptions.cpp - Options for SPIR-V CodeGen -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "clang/SPIRV/EmitSPIRVOptions.h"

namespace clang {

void EmitSPIRVOptions::Initialize() {
  if (useDxLayout) {
    cBufferLayoutRule = spirv::LayoutRule::FxcCTBuffer;
    tBufferLayoutRule = spirv::LayoutRule::FxcCTBuffer;
    sBufferLayoutRule = spirv::LayoutRule::FxcSBuffer;
  } else if (useGlLayout) {
    cBufferLayoutRule = spirv::LayoutRule::GLSLStd140;
    tBufferLayoutRule = spirv::LayoutRule::GLSLStd430;
    sBufferLayoutRule = spirv::LayoutRule::GLSLStd430;
  } else {
    cBufferLayoutRule = spirv::LayoutRule::RelaxedGLSLStd140;
    tBufferLayoutRule = spirv::LayoutRule::RelaxedGLSLStd430;
    sBufferLayoutRule = spirv::LayoutRule::RelaxedGLSLStd430;
  }
}

} // end namespace clang
