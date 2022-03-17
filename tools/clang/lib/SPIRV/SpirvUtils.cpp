//===------- SpirvUtils.cpp - SPIR-V Utils ----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvUtils.h"
#include "llvm/Support/ErrorHandling.h"

namespace clang {
namespace spirv {

SpirvUtils::SpirvUtils(SpirvContext &context) : spvContext(context) {}

spv::ExecutionModel
SpirvUtils::getSpirvShaderStage(hlsl::ShaderModel::Kind smk) {
  switch (smk) {
  case hlsl::ShaderModel::Kind::Vertex:
    return spv::ExecutionModel::Vertex;
  case hlsl::ShaderModel::Kind::Hull:
    return spv::ExecutionModel::TessellationControl;
  case hlsl::ShaderModel::Kind::Domain:
    return spv::ExecutionModel::TessellationEvaluation;
  case hlsl::ShaderModel::Kind::Geometry:
    return spv::ExecutionModel::Geometry;
  case hlsl::ShaderModel::Kind::Pixel:
    return spv::ExecutionModel::Fragment;
  case hlsl::ShaderModel::Kind::Compute:
    return spv::ExecutionModel::GLCompute;
  case hlsl::ShaderModel::Kind::RayGeneration:
    return spv::ExecutionModel::RayGenerationNV;
  case hlsl::ShaderModel::Kind::Intersection:
    return spv::ExecutionModel::IntersectionNV;
  case hlsl::ShaderModel::Kind::AnyHit:
    return spv::ExecutionModel::AnyHitNV;
  case hlsl::ShaderModel::Kind::ClosestHit:
    return spv::ExecutionModel::ClosestHitNV;
  case hlsl::ShaderModel::Kind::Miss:
    return spv::ExecutionModel::MissNV;
  case hlsl::ShaderModel::Kind::Callable:
    return spv::ExecutionModel::CallableNV;
  case hlsl::ShaderModel::Kind::Mesh:
    return spv::ExecutionModel::MeshNV;
  case hlsl::ShaderModel::Kind::Amplification:
    return spv::ExecutionModel::TaskNV;
  default:
    llvm_unreachable("invalid shader model kind");
    break;
  }
}

void SpirvUtils::applyResultTypeTransformations(SpirvInstruction *instr) {
  const auto *resultType = instr->getResultType();
  switch (instr->getopcode()) {
  // Variables and function parameters must have a pointer type.
  case spv::Op::OpFunctionParameter:
  case spv::Op::OpVariable: {
    if (auto *var = dyn_cast<SpirvVariable>(instr)) {
      auto vkImgFeatures = spvContext.getVkImageFeaturesForSpirvVariable(var);
      if (vkImgFeatures.format != spv::ImageFormat::Unknown) {
        if (const auto *imageType = dyn_cast<ImageType>(resultType)) {
          resultType = spvContext.getImageType(imageType, vkImgFeatures.format);
          instr->setResultType(resultType);
        }
      }
    }
    const SpirvType *pointerType =
        spvContext.getPointerType(resultType, instr->getStorageClass());
    instr->setResultType(pointerType);
    break;
  }
  // Access chains must have a pointer type. The storage class for the pointer
  // is the same as the storage class of the access base.
  case spv::Op::OpAccessChain: {
    const auto *pointerType = spvContext.getPointerType(
        resultType,
        cast<SpirvAccessChain>(instr)->getBase()->getStorageClass());
    instr->setResultType(pointerType);
    break;
  }
  default:
    break;
  }
}

} // namespace spirv
} // namespace clang
