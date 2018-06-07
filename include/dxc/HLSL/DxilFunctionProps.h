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
  DxilFunctionProps() {
    memset(this, 0, sizeof(DxilFunctionProps));
  }
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
    // Ray Tracing shaders
    struct {
      union {
        unsigned payloadSizeInBytes;
        unsigned paramSizeInBytes;
      };
      unsigned attributeSizeInBytes;
    } Ray;
  } ShaderProps;
  DXIL::ShaderKind shaderKind;
  // TODO: Should we have an unmangled name here for ray tracing shaders?
  bool IsPS() const     { return shaderKind == DXIL::ShaderKind::Pixel; }
  bool IsVS() const     { return shaderKind == DXIL::ShaderKind::Vertex; }
  bool IsGS() const     { return shaderKind == DXIL::ShaderKind::Geometry; }
  bool IsHS() const     { return shaderKind == DXIL::ShaderKind::Hull; }
  bool IsDS() const     { return shaderKind == DXIL::ShaderKind::Domain; }
  bool IsCS() const     { return shaderKind == DXIL::ShaderKind::Compute; }
  bool IsGraphics() const {
    return (shaderKind >= DXIL::ShaderKind::Pixel && shaderKind <= DXIL::ShaderKind::Domain);
  }
  bool IsRayGeneration() const { return shaderKind == DXIL::ShaderKind::RayGeneration; }
  bool IsIntersection() const { return shaderKind == DXIL::ShaderKind::Intersection; }
  bool IsAnyHit() const { return shaderKind == DXIL::ShaderKind::AnyHit; }
  bool IsClosestHit() const { return shaderKind == DXIL::ShaderKind::ClosestHit; }
  bool IsMiss() const { return shaderKind == DXIL::ShaderKind::Miss; }
  bool IsCallable() const { return shaderKind == DXIL::ShaderKind::Callable; }
  bool IsRay() const {
    return (shaderKind >= DXIL::ShaderKind::RayGeneration && shaderKind <= DXIL::ShaderKind::Callable);
  }
};

} // namespace hlsl
