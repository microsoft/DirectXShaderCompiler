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
};

} // namespace hlsl
