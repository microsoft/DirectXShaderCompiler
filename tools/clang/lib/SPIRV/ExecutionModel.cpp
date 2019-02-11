//===----- ExecutionModel.cpp - get/set SPV Execution Model -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file implements a execution model class that defines a mapping between
//  hlsl::ShaderModel::Kind and spv::ExecutionModel
//
//===----------------------------------------------------------------------===//

#include "ExecutionModel.h"

namespace clang {
namespace spirv {

bool ExecutionModel::IsValid() const {
  assert(IsPS() || IsVS() || IsGS() || IsHS() || IsDS() || IsCS() || IsRay() ||
         execModel == ExecModel::Max);
  return execModel != ExecModel::Max;
}

const ExecutionModel *
ExecutionModel::GetByStageName(const StringRef &stageName) {
  const ExecutionModel *em;
  switch (stageName[0]) {
  case 'c':
    switch (stageName[1]) {
    case 'o':
      em = GetByShaderKind(ShaderKind::Compute);
      break;
    case 'l':
      em = GetByShaderKind(ShaderKind::ClosestHit);
      break;
    case 'a':
      em = GetByShaderKind(ShaderKind::Callable);
      break;
    default:
      em = GetByShaderKind(ShaderKind::Invalid);
      break;
    }
    break;
  case 'v':
    em = GetByShaderKind(ShaderKind::Vertex);
    break;
  case 'h':
    em = GetByShaderKind(ShaderKind::Hull);
    break;
  case 'd':
    em = GetByShaderKind(ShaderKind::Domain);
    break;
  case 'g':
    em = GetByShaderKind(ShaderKind::Geometry);
    break;
  case 'p':
    em = GetByShaderKind(ShaderKind::Pixel);
    break;
  case 'r':
    em = GetByShaderKind(ShaderKind::RayGeneration);
    break;
  case 'i':
    em = GetByShaderKind(ShaderKind::Intersection);
    break;
  case 'a':
    em = GetByShaderKind(ShaderKind::AnyHit);
    break;
  case 'm':
    em = GetByShaderKind(ShaderKind::Miss);
    break;
  default:
    em = GetByShaderKind(ShaderKind::Invalid);
    break;
  }
  if (!em->IsValid()) {
    llvm_unreachable("Unknown stage name");
  }
  return em;
}

const ExecutionModel *ExecutionModel::GetByShaderKind(ShaderKind sk) {
  int idx = (int)sk;
  assert(idx < numExecutionModels);
  return &executionModels[idx];
}

typedef ExecutionModel EM;
const ExecutionModel
    ExecutionModel::executionModels[ExecutionModel::numExecutionModels] = {
        // Note: below sequence should match DXIL::ShaderKind
        // DXIL::ShaderKind <--> spv::ExecutionModel
        EM(ShaderKind::Pixel, ExecModel::Fragment),
        EM(ShaderKind::Vertex, ExecModel::Vertex),
        EM(ShaderKind::Geometry, ExecModel::Geometry),
        EM(ShaderKind::Hull, ExecModel::TessellationControl),
        EM(ShaderKind::Domain, ExecModel::TessellationEvaluation),
        EM(ShaderKind::Compute, ExecModel::GLCompute),
        EM(ShaderKind::Library, ExecModel::Max),
        EM(ShaderKind::RayGeneration, ExecModel::RayGenerationNV),
        EM(ShaderKind::Intersection, ExecModel::IntersectionNV),
        EM(ShaderKind::AnyHit, ExecModel::AnyHitNV),
        EM(ShaderKind::ClosestHit, ExecModel::ClosestHitNV),
        EM(ShaderKind::Miss, ExecModel::MissNV),
        EM(ShaderKind::Callable, ExecModel::CallableNV),
        EM(ShaderKind::Invalid, ExecModel::Max)};

} // end namespace spirv
} // end namespace clang
