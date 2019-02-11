//===------- ExecutionModel.h - get/set SPV Execution Model -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file defines a execution model class that defines a mapping between
//  hlsl::ShaderModel::Kind and spv::ExecutionModel
//
//===----------------------------------------------------------------------===//

#ifndef EXECUTION_MODEL_H
#define EXECUTION_MODEL_H

#include "dxc/DXIL/DxilShaderModel.h"
#include "spirv/unified1/GLSL.std.450.h"
#include "clang/SPIRV/SpirvType.h"

namespace clang {
namespace spirv {

/// Execution Model class.
class ExecutionModel {
public:
  using ShaderKind = hlsl::ShaderModel::Kind;
  using ExecModel = spv::ExecutionModel;

  bool IsPS() const { return execModel == ExecModel::Fragment; }
  bool IsVS() const { return execModel == ExecModel::Vertex; }
  bool IsGS() const { return execModel == ExecModel::Geometry; }
  bool IsHS() const { return execModel == ExecModel::TessellationControl; }
  bool IsDS() const { return execModel == ExecModel::TessellationEvaluation; }
  bool IsCS() const { return execModel == ExecModel::GLCompute; }
  bool IsRay() const {
    return execModel >= ExecModel::RayGenerationNV &&
           execModel <= ExecModel::CallableNV;
  }
  bool IsValid() const;

  ShaderKind GetShaderKind() const { return shaderKind; }
  ExecModel GetExecutionModel() const { return execModel; }

  static const ExecutionModel *GetByStageName(const StringRef &stageName);
  static const ExecutionModel *GetByShaderKind(ShaderKind sk);

private:
  ShaderKind shaderKind;
  ExecModel execModel;

  ExecutionModel() = delete;
  ExecutionModel(ShaderKind sk, ExecModel em) : shaderKind(sk), execModel(em) {}

  static const unsigned numExecutionModels = 14;
  static const ExecutionModel executionModels[numExecutionModels];
};

} // end namespace spirv
} // end namespace clang

#endif // EXECUTION_MODEL_H
