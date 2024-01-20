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

// for memset dependency:
#include <cstring>
#include <vector>

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilNodeProps.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {
class Function;
class Constant;
} // namespace llvm

namespace hlsl {
struct DxilFunctionProps {
  DxilFunctionProps() {
    memset(&ShaderProps, 0, sizeof(ShaderProps));
    shaderKind = DXIL::ShaderKind::Invalid;
    NodeShaderID = {};
    NodeShaderSharedInput = {};
    memset(&Node, 0, sizeof(Node));
    Node.LaunchType = DXIL::NodeLaunchType::Invalid;
    Node.LocalRootArgumentsTableIndex = -1;
    waveMinSize = 0;
    waveMaxSize = 0;
    wavePreferredSize = 0;
  }
  union {
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
    // Mesh shader.
    struct {
      unsigned maxVertexCount;
      unsigned maxPrimitiveCount;
      DXIL::MeshOutputTopology outputTopology;
      unsigned payloadSizeInBytes;
    } MS;
    // Amplification shader.
    struct {
      unsigned payloadSizeInBytes;
    } AS;
  } ShaderProps;

  // numThreads shared between multiple shader types and node shaders.
  unsigned numThreads[3];

  struct NodeProps {
    DXIL::NodeLaunchType LaunchType = DXIL::NodeLaunchType::Invalid;
    bool IsProgramEntry;
    int LocalRootArgumentsTableIndex;
    unsigned DispatchGrid[3];
    unsigned MaxDispatchGrid[3];
    unsigned MaxRecursionDepth;
  } Node;

  DXIL::ShaderKind shaderKind;
  NodeID NodeShaderID;
  NodeID NodeShaderSharedInput;
  std::vector<NodeIOProperties> InputNodes;
  std::vector<NodeIOProperties> OutputNodes;

  // SM 6.6 allows WaveSize specification for only a single required size. SM
  // 6.8+ allows specification of WaveSize as a min, max and preferred value.
  unsigned waveMinSize;
  unsigned waveMaxSize;
  unsigned wavePreferredSize;
  // Save root signature for lib profile entry.
  std::vector<uint8_t> serializedRootSignature;
  void SetSerializedRootSignature(const uint8_t *pData, unsigned size) {
    serializedRootSignature.assign(pData, pData + size);
  }

  // TODO: Should we have an unmangled name here for ray tracing shaders?
  bool IsPS() const { return shaderKind == DXIL::ShaderKind::Pixel; }
  bool IsVS() const { return shaderKind == DXIL::ShaderKind::Vertex; }
  bool IsGS() const { return shaderKind == DXIL::ShaderKind::Geometry; }
  bool IsHS() const { return shaderKind == DXIL::ShaderKind::Hull; }
  bool IsDS() const { return shaderKind == DXIL::ShaderKind::Domain; }
  bool IsCS() const { return shaderKind == DXIL::ShaderKind::Compute; }
  bool IsGraphics() const {
    return (shaderKind >= DXIL::ShaderKind::Pixel &&
            shaderKind <= DXIL::ShaderKind::Domain) ||
           shaderKind == DXIL::ShaderKind::Mesh ||
           shaderKind == DXIL::ShaderKind::Amplification;
  }
  bool IsRayGeneration() const {
    return shaderKind == DXIL::ShaderKind::RayGeneration;
  }
  bool IsIntersection() const {
    return shaderKind == DXIL::ShaderKind::Intersection;
  }
  bool IsAnyHit() const { return shaderKind == DXIL::ShaderKind::AnyHit; }
  bool IsClosestHit() const {
    return shaderKind == DXIL::ShaderKind::ClosestHit;
  }
  bool IsMiss() const { return shaderKind == DXIL::ShaderKind::Miss; }
  bool IsCallable() const { return shaderKind == DXIL::ShaderKind::Callable; }
  bool IsRay() const {
    return (shaderKind >= DXIL::ShaderKind::RayGeneration &&
            shaderKind <= DXIL::ShaderKind::Callable);
  }
  bool IsMS() const { return shaderKind == DXIL::ShaderKind::Mesh; }
  bool IsAS() const { return shaderKind == DXIL::ShaderKind::Amplification; }
  bool IsNode() const {
    return shaderKind == DXIL::ShaderKind::Node ||
           Node.LaunchType != DXIL::NodeLaunchType::Invalid;
  };
};

} // namespace hlsl
