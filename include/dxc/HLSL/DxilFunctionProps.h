///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilFunctionProps.h                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Function properties for a dxil shader function.                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/HLSL/DxilConstants.h"

namespace llvm {
class Function;
class Constant;
}

namespace hlsl {
struct DxilFunctionProps {
  union {
    // Compute shader.
    struct {
      unsigned numThreads[3];
    } CS;
    // Geometry shader.
    struct {
      DXIL::InputPrimitive inputPrimitive;
      unsigned maxVertexCount;
      unsigned instanceCount;
      DXIL::PrimitiveTopology
          streamPrimitiveTopologies[DXIL::kNumOutputStreams];
    } GS;
    // Hull shader.
    struct {
      llvm::Function *patchConstantFunc;
      DXIL::TessellatorDomain domain;
      DXIL::TessellatorPartitioning partition;
      DXIL::TessellatorOutputPrimitive outputPrimitive;
      unsigned inputControlPoints;
      unsigned outputControlPoints;
      float maxTessFactor;
    } HS;
    // Domain shader.
    struct {
      DXIL::TessellatorDomain domain;
      unsigned inputControlPoints;
    } DS;
    // Vertex shader.
    struct {
      llvm::Constant *clipPlanes[DXIL::kNumClipPlanes];
    } VS;
    // Pixel shader.
    struct {
      bool EarlyDepthStencil;
    } PS;
  } ShaderProps;
  DXIL::ShaderKind shaderKind;
  bool IsPS() const     { return shaderKind == DXIL::ShaderKind::Pixel; }
  bool IsVS() const     { return shaderKind == DXIL::ShaderKind::Vertex; }
  bool IsGS() const     { return shaderKind == DXIL::ShaderKind::Geometry; }
  bool IsHS() const     { return shaderKind == DXIL::ShaderKind::Hull; }
  bool IsDS() const     { return shaderKind == DXIL::ShaderKind::Domain; }
  bool IsCS() const     { return shaderKind == DXIL::ShaderKind::Compute; }
  bool IsGraphics() const {
    switch (shaderKind) {
    case DXIL::ShaderKind::Compute:
    case DXIL::ShaderKind::Library:
    case DXIL::ShaderKind::Invalid:
      return false;
    default:
      return true;
    }
  }
};

} // namespace hlsl
