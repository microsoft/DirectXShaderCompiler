//===--- CapabilityVisitor.cpp - Capability Visitor --------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CapabilityVisitor.h"
#include "clang/SPIRV/SpirvBuilder.h"

namespace clang {
namespace spirv {

void CapabilityVisitor::addExtension(Extension ext, llvm::StringRef target,
                                     SourceLocation loc) {
  featureManager.requestExtension(ext, target, loc);
  // Do not emit OpExtension if the given extension is natively supported in
  // the target environment.
  if (featureManager.isExtensionRequiredForTargetEnv(ext))
    spvBuilder.requireExtension(featureManager.getExtensionName(ext), loc);
}

void CapabilityVisitor::addCapability(spv::Capability cap, SourceLocation loc) {
  if (cap != spv::Capability::Max) {
    spvBuilder.requireCapability(cap, loc);
  }
}

void CapabilityVisitor::addCapabilityForType(const SpirvType *type,
                                             SourceLocation loc,
                                             spv::StorageClass sc) {
  // Defent against instructions that do not have a return type.
  if (!type)
    return;

  // Integer-related capabilities
  if (const auto *intType = dyn_cast<IntegerType>(type)) {
    switch (intType->getBitwidth()) {
    case 16: {
      // Usage of a 16-bit integer type.
      addCapability(spv::Capability::Int16);

      // Usage of a 16-bit integer type as stage I/O.
      if (sc == spv::StorageClass::Input || sc == spv::StorageClass::Output) {
        addExtension(Extension::KHR_16bit_storage, "16-bit stage IO variables",
                     loc);
        addCapability(spv::Capability::StorageInputOutput16);
      }
      break;
    }
    case 64: {
      addCapability(spv::Capability::Int64);
      break;
    }
    default:
      break;
    }
  }
  // Float-related capabilities
  else if (const auto *floatType = dyn_cast<FloatType>(type)) {
    switch (floatType->getBitwidth()) {
    case 16: {
      // Usage of a 16-bit float type.
      addCapability(spv::Capability::Float16);

      // Usage of a 16-bit float type as stage I/O.
      if (sc == spv::StorageClass::Input || sc == spv::StorageClass::Output) {
        addExtension(Extension::KHR_16bit_storage, "16-bit stage IO variables",
                     loc);
        addCapability(spv::Capability::StorageInputOutput16);
      }
      break;
    }
    case 64: {
      addCapability(spv::Capability::Float64);
      break;
    }
    default:
      break;
    }
  }
  // Vectors
  else if (const auto *vecType = dyn_cast<VectorType>(type)) {
    addCapabilityForType(vecType->getElementType(), loc, sc);
  }
  // Matrices
  else if (const auto *matType = dyn_cast<MatrixType>(type)) {
    addCapabilityForType(matType->getElementType(), loc, sc);
  }
  // Arrays
  else if (const auto *arrType = dyn_cast<ArrayType>(type)) {
    addCapabilityForType(arrType->getElementType(), loc, sc);
  }
  // Runtime array of resources requires additional capability.
  else if (const auto *raType = dyn_cast<RuntimeArrayType>(type)) {
    if (SpirvType::isResourceType(raType->getElementType())) {
      // the elements inside the runtime array are resources
      addExtension(Extension::EXT_descriptor_indexing,
                   "runtime array of resources", loc);
      addCapability(spv::Capability::RuntimeDescriptorArrayEXT);
    }
    addCapabilityForType(raType->getElementType(), loc, sc);
  }
  // Image types
  else if (const auto *imageType = dyn_cast<ImageType>(type)) {
    switch (imageType->getDimension()) {
    case spv::Dim::Buffer: {
      addCapability(spv::Capability::SampledBuffer);
      if (imageType->withSampler() == ImageType::WithSampler::No) {
        addCapability(spv::Capability::ImageBuffer);
      }
      break;
    }
    case spv::Dim::Dim1D: {
      if (imageType->withSampler() == ImageType::WithSampler::No) {
        addCapability(spv::Capability::Image1D);
      } else {
        addCapability(spv::Capability::Sampled1D);
      }
      break;
    }
    case spv::Dim::SubpassData: {
      addCapability(spv::Capability::InputAttachment);
      break;
    }
    default:
      break;
    }

    switch (imageType->getImageFormat()) {
    case spv::ImageFormat::Rg32f:
    case spv::ImageFormat::Rg16f:
    case spv::ImageFormat::R11fG11fB10f:
    case spv::ImageFormat::R16f:
    case spv::ImageFormat::Rgba16:
    case spv::ImageFormat::Rgb10A2:
    case spv::ImageFormat::Rg16:
    case spv::ImageFormat::Rg8:
    case spv::ImageFormat::R16:
    case spv::ImageFormat::R8:
    case spv::ImageFormat::Rgba16Snorm:
    case spv::ImageFormat::Rg16Snorm:
    case spv::ImageFormat::Rg8Snorm:
    case spv::ImageFormat::R16Snorm:
    case spv::ImageFormat::R8Snorm:
    case spv::ImageFormat::Rg32i:
    case spv::ImageFormat::Rg16i:
    case spv::ImageFormat::Rg8i:
    case spv::ImageFormat::R16i:
    case spv::ImageFormat::R8i:
    case spv::ImageFormat::Rgb10a2ui:
    case spv::ImageFormat::Rg32ui:
    case spv::ImageFormat::Rg16ui:
    case spv::ImageFormat::Rg8ui:
    case spv::ImageFormat::R16ui:
    case spv::ImageFormat::R8ui:
      addCapability(spv::Capability::StorageImageExtendedFormats);
      break;
    default:
      // Only image formats requiring extended formats are relevant. The rest
      // just pass through.
      break;
    }

    if (imageType->isArrayedImage() && imageType->isMSImage())
      addCapability(spv::Capability::ImageMSArray);

    addCapabilityForType(imageType->getSampledType(), loc, sc);
  }
  // Sampled image type
  else if (const auto *sampledImageType = dyn_cast<SampledImageType>(type)) {
    addCapabilityForType(sampledImageType->getImageType(), loc, sc);
  }
  // Pointer type
  else if (const auto *ptrType = dyn_cast<SpirvPointerType>(type)) {
    addCapabilityForType(ptrType->getPointeeType(), loc, sc);
  }
  // Struct type
  else if (const auto *structType = dyn_cast<StructType>(type)) {
    if (SpirvType::isOrContainsType<NumericalType, 16>(structType)) {
      addExtension(Extension::KHR_16bit_storage, "16-bit types in resource",
                   loc);
      if (sc == spv::StorageClass::PushConstant) {
        addCapability(spv::Capability::StoragePushConstant16);
      } else if (structType->getInterfaceType() ==
                 StructInterfaceType::UniformBuffer) {
        addCapability(spv::Capability::StorageUniform16);
      } else if (structType->getInterfaceType() ==
                 StructInterfaceType::StorageBuffer) {
        addCapability(spv::Capability::StorageUniformBufferBlock16);
      }
    }
    for (auto field : structType->getFields())
      addCapabilityForType(field.type, loc, sc);
  }
}

bool CapabilityVisitor::visit(SpirvDecoration *decor) {
  const auto loc = decor->getSourceLocation();
  switch (decor->getDecoration()) {
  case spv::Decoration::Sample: {
    addCapability(spv::Capability::SampleRateShading, loc);
    break;
  }
  case spv::Decoration::NonUniformEXT: {
    addExtension(Extension::EXT_descriptor_indexing, "NonUniformEXT", loc);
    addCapability(spv::Capability::ShaderNonUniformEXT);

    break;
  }
  case spv::Decoration::HlslSemanticGOOGLE:
  case spv::Decoration::HlslCounterBufferGOOGLE: {
    addExtension(Extension::GOOGLE_hlsl_functionality1, "SPIR-V reflection",
                 loc);
    break;
  }
  // Capabilities needed for built-ins
  case spv::Decoration::BuiltIn: {
    assert(decor->getParams().size() == 1);
    const auto builtin = static_cast<spv::BuiltIn>(decor->getParams()[0]);
    switch (builtin) {
    case spv::BuiltIn::SampleId:
    case spv::BuiltIn::SamplePosition: {
      addCapability(spv::Capability::SampleRateShading, loc);
      break;
    }
    case spv::BuiltIn::SubgroupSize:
    case spv::BuiltIn::NumSubgroups:
    case spv::BuiltIn::SubgroupId:
    case spv::BuiltIn::SubgroupLocalInvocationId: {
      addCapability(spv::Capability::GroupNonUniform, loc);
      break;
    }
    case spv::BuiltIn::BaseVertex: {
      addExtension(Extension::KHR_shader_draw_parameters, "BaseVertex Builtin",
                   loc);
      addCapability(spv::Capability::DrawParameters);
      break;
    }
    case spv::BuiltIn::BaseInstance: {
      addExtension(Extension::KHR_shader_draw_parameters,
                   "BaseInstance Builtin", loc);
      addCapability(spv::Capability::DrawParameters);
      break;
    }
    case spv::BuiltIn::DrawIndex: {
      addExtension(Extension::KHR_shader_draw_parameters, "DrawIndex Builtin",
                   loc);
      addCapability(spv::Capability::DrawParameters);
      break;
    }
    case spv::BuiltIn::DeviceIndex: {
      addExtension(Extension::KHR_device_group, "DeviceIndex Builtin", loc);
      addCapability(spv::Capability::DeviceGroup);
      break;
    }
    case spv::BuiltIn::FragStencilRefEXT: {
      addExtension(Extension::EXT_shader_stencil_export, "SV_StencilRef", loc);
      addCapability(spv::Capability::StencilExportEXT);
      break;
    }
    case spv::BuiltIn::ViewIndex: {
      addExtension(Extension::KHR_multiview, "SV_ViewID", loc);
      addCapability(spv::Capability::MultiView);
      break;
    }
    case spv::BuiltIn::FullyCoveredEXT: {
      addExtension(Extension::EXT_fragment_fully_covered, "SV_InnerCoverage",
                   loc);
      addCapability(spv::Capability::FragmentFullyCoveredEXT);
      break;
    }
    case spv::BuiltIn::PrimitiveId: {
      // PrimitiveID can be used as PSIn or MSPOut.
      if (shaderModel == spv::ExecutionModel::Fragment ||
          shaderModel == spv::ExecutionModel::MeshNV)
        addCapability(spv::Capability::Geometry);
      break;
    }
    case spv::BuiltIn::Layer: {
      if (shaderModel == spv::ExecutionModel::Vertex ||
          shaderModel == spv::ExecutionModel::TessellationControl ||
          shaderModel == spv::ExecutionModel::TessellationEvaluation) {
        addExtension(Extension::EXT_shader_viewport_index_layer,
                     "SV_RenderTargetArrayIndex", loc);
        addCapability(spv::Capability::ShaderViewportIndexLayerEXT);
      } else if (shaderModel == spv::ExecutionModel::Fragment ||
                 shaderModel == spv::ExecutionModel::MeshNV) {
        // SV_RenderTargetArrayIndex can be used as PSIn or MSPOut.
        addCapability(spv::Capability::Geometry);
      }
      break;
    }
    case spv::BuiltIn::ViewportIndex: {
      if (shaderModel == spv::ExecutionModel::Vertex ||
          shaderModel == spv::ExecutionModel::TessellationControl ||
          shaderModel == spv::ExecutionModel::TessellationEvaluation) {
        addExtension(Extension::EXT_shader_viewport_index_layer,
                     "SV_ViewPortArrayIndex", loc);
        addCapability(spv::Capability::ShaderViewportIndexLayerEXT);
      } else if (shaderModel == spv::ExecutionModel::Fragment ||
                 shaderModel == spv::ExecutionModel::Geometry ||
                 shaderModel == spv::ExecutionModel::MeshNV) {
        // SV_ViewportArrayIndex can be used as PSIn or GSOut or MSPOut.
        addCapability(spv::Capability::MultiViewport);
      }
      break;
    }
    case spv::BuiltIn::ClipDistance: {
      addCapability(spv::Capability::ClipDistance);
      break;
    }
    case spv::BuiltIn::CullDistance: {
      addCapability(spv::Capability::CullDistance);
      break;
    }
    case spv::BuiltIn::BaryCoordNoPerspAMD:
    case spv::BuiltIn::BaryCoordNoPerspCentroidAMD:
    case spv::BuiltIn::BaryCoordNoPerspSampleAMD:
    case spv::BuiltIn::BaryCoordSmoothAMD:
    case spv::BuiltIn::BaryCoordSmoothCentroidAMD:
    case spv::BuiltIn::BaryCoordSmoothSampleAMD:
    case spv::BuiltIn::BaryCoordPullModelAMD: {
      addExtension(Extension::AMD_shader_explicit_vertex_parameter,
                   "SV_Barycentrics", loc);
      break;
    }
    case spv::BuiltIn::FragSizeEXT: {
      addExtension(Extension::EXT_fragment_invocation_density, "SV_ShadingRate",
                   loc);
      addCapability(spv::Capability::FragmentDensityEXT);
      break;
    }
    default:
      break;
    }

    break;
  }
  default:
    break;
  }

  return true;
}

spv::Capability
CapabilityVisitor::getNonUniformCapability(const SpirvType *type) {
  if (!type)
    return spv::Capability::Max;

  if (const auto *arrayType = dyn_cast<ArrayType>(type)) {
    return getNonUniformCapability(arrayType->getElementType());
  }
  if (SpirvType::isTexture(type) || SpirvType::isSampler(type)) {
    return spv::Capability::SampledImageArrayNonUniformIndexingEXT;
  }
  if (SpirvType::isRWTexture(type)) {
    return spv::Capability::StorageImageArrayNonUniformIndexingEXT;
  }
  if (SpirvType::isBuffer(type)) {
    return spv::Capability::UniformTexelBufferArrayNonUniformIndexingEXT;
  }
  if (SpirvType::isRWBuffer(type)) {
    return spv::Capability::StorageTexelBufferArrayNonUniformIndexingEXT;
  }
  if (SpirvType::isSubpassInput(type) || SpirvType::isSubpassInputMS(type)) {
    return spv::Capability::InputAttachmentArrayNonUniformIndexingEXT;
  }

  return spv::Capability::Max;
}

bool CapabilityVisitor::visit(SpirvImageQuery *instr) {
  addCapabilityForType(instr->getResultType(), instr->getSourceLocation(),
                       instr->getStorageClass());
  addCapability(spv::Capability::ImageQuery);
  return true;
}

bool CapabilityVisitor::visit(SpirvImageSparseTexelsResident *instr) {
  addCapabilityForType(instr->getResultType(), instr->getSourceLocation(),
                       instr->getStorageClass());
  addCapability(spv::Capability::ImageGatherExtended);
  addCapability(spv::Capability::SparseResidency);
  return true;
}

bool CapabilityVisitor::visit(SpirvImageOp *instr) {
  addCapabilityForType(instr->getResultType(), instr->getSourceLocation(),
                       instr->getStorageClass());
  if (instr->hasOffset() || instr->hasConstOffsets())
    addCapability(spv::Capability::ImageGatherExtended);
  if (instr->hasMinLod())
    addCapability(spv::Capability::MinLod);
  if (instr->isSparse())
    addCapability(spv::Capability::SparseResidency);

  return true;
}

bool CapabilityVisitor::visitInstruction(SpirvInstruction *instr) {
  const SpirvType *resultType = instr->getResultType();
  const auto opcode = instr->getopcode();
  const auto loc = instr->getSourceLocation();

  // Add result-type-specific capabilities
  addCapabilityForType(resultType, loc, instr->getStorageClass());

  // Add NonUniform capabilities if necessary
  if (instr->isNonUniform()) {
    addExtension(Extension::EXT_descriptor_indexing, "NonUniformEXT", loc);
    addCapability(spv::Capability::ShaderNonUniformEXT);
    addCapability(getNonUniformCapability(resultType));
  }

  // Add opcode-specific capabilities
  switch (opcode) {
  case spv::Op::OpDPdxCoarse:
  case spv::Op::OpDPdyCoarse:
  case spv::Op::OpFwidthCoarse:
  case spv::Op::OpDPdxFine:
  case spv::Op::OpDPdyFine:
  case spv::Op::OpFwidthFine:
    addCapability(spv::Capability::DerivativeControl);
    break;
  case spv::Op::OpGroupNonUniformElect:
    addCapability(spv::Capability::GroupNonUniform);
    break;
  case spv::Op::OpGroupNonUniformAny:
  case spv::Op::OpGroupNonUniformAll:
  case spv::Op::OpGroupNonUniformAllEqual:
    addCapability(spv::Capability::GroupNonUniformVote);
    break;
  case spv::Op::OpGroupNonUniformBallot:
  case spv::Op::OpGroupNonUniformInverseBallot:
  case spv::Op::OpGroupNonUniformBallotBitExtract:
  case spv::Op::OpGroupNonUniformBallotBitCount:
  case spv::Op::OpGroupNonUniformBallotFindLSB:
  case spv::Op::OpGroupNonUniformBallotFindMSB:
  case spv::Op::OpGroupNonUniformBroadcast:
  case spv::Op::OpGroupNonUniformBroadcastFirst:
    addCapability(spv::Capability::GroupNonUniformBallot);
    break;
  case spv::Op::OpGroupNonUniformIAdd:
  case spv::Op::OpGroupNonUniformFAdd:
  case spv::Op::OpGroupNonUniformIMul:
  case spv::Op::OpGroupNonUniformFMul:
  case spv::Op::OpGroupNonUniformSMax:
  case spv::Op::OpGroupNonUniformUMax:
  case spv::Op::OpGroupNonUniformFMax:
  case spv::Op::OpGroupNonUniformSMin:
  case spv::Op::OpGroupNonUniformUMin:
  case spv::Op::OpGroupNonUniformFMin:
  case spv::Op::OpGroupNonUniformBitwiseAnd:
  case spv::Op::OpGroupNonUniformBitwiseOr:
  case spv::Op::OpGroupNonUniformBitwiseXor:
  case spv::Op::OpGroupNonUniformLogicalAnd:
  case spv::Op::OpGroupNonUniformLogicalOr:
  case spv::Op::OpGroupNonUniformLogicalXor:
    addCapability(spv::Capability::GroupNonUniformArithmetic);
    break;
  case spv::Op::OpGroupNonUniformQuadBroadcast:
  case spv::Op::OpGroupNonUniformQuadSwap:
    addCapability(spv::Capability::GroupNonUniformQuad);
    break;
  case spv::Op::OpVariable: {
    if (spvOptions.enableReflect &&
        !cast<SpirvVariable>(instr)->getHlslUserType().empty()) {
      addExtension(Extension::GOOGLE_user_type, "HLSL User Type", loc);
      addExtension(Extension::GOOGLE_hlsl_functionality1, "HLSL User Type",
                   loc);
    }
    break;
  }
  default:
    break;
  }

  return true;
}

bool CapabilityVisitor::visit(SpirvEntryPoint *entryPoint) {
  shaderModel = entryPoint->getExecModel();
  switch (shaderModel) {
  case spv::ExecutionModel::Fragment:
  case spv::ExecutionModel::Vertex:
  case spv::ExecutionModel::GLCompute:
    addCapability(spv::Capability::Shader);
    break;
  case spv::ExecutionModel::Geometry:
    addCapability(spv::Capability::Geometry);
    break;
  case spv::ExecutionModel::TessellationControl:
  case spv::ExecutionModel::TessellationEvaluation:
    addCapability(spv::Capability::Tessellation);
    break;
  case spv::ExecutionModel::RayGenerationNV:
  case spv::ExecutionModel::IntersectionNV:
  case spv::ExecutionModel::ClosestHitNV:
  case spv::ExecutionModel::AnyHitNV:
  case spv::ExecutionModel::MissNV:
  case spv::ExecutionModel::CallableNV:
    if (featureManager.isExtensionEnabled("SPV_NV_ray_tracing")) {
      addCapability(spv::Capability::RayTracingNV);
      addExtension(Extension::NV_ray_tracing, "SPV_NV_ray_tracing", {});
    } else {
      addCapability(spv::Capability::RayTracingProvisionalKHR);
      addExtension(Extension::KHR_ray_tracing, "SPV_KHR_ray_tracing", {});
    }
    break;
  case spv::ExecutionModel::MeshNV:
  case spv::ExecutionModel::TaskNV:
    addCapability(spv::Capability::MeshShadingNV);
    addExtension(Extension::NV_mesh_shader, "SPV_NV_mesh_shader", {});
    break;
  default:
    llvm_unreachable("found unknown shader model");
    break;
  }
  return true;
}

bool CapabilityVisitor::visit(SpirvExecutionMode *execMode) {
  if (execMode->getExecutionMode() == spv::ExecutionMode::PostDepthCoverage) {
    addCapability(spv::Capability::SampleMaskPostDepthCoverage,
                  execMode->getEntryPoint()->getSourceLocation());
    addExtension(Extension::KHR_post_depth_coverage,
                 "[[vk::post_depth_coverage]]", execMode->getSourceLocation());
  }
  return true;
}

bool CapabilityVisitor::visit(SpirvExtInstImport *instr) {
  if (instr->getExtendedInstSetName() == "NonSemantic.DebugPrintf")
    addExtension(Extension::KHR_non_semantic_info, "DebugPrintf",
                 /*SourceLocation*/ {});
  return true;
}

bool CapabilityVisitor::visit(SpirvExtInst *instr) {
  // OpExtInst using the GLSL extended instruction allows only 32-bit types by
  // default for interpolation instructions. The AMD_gpu_shader_half_float
  // extension adds support for 16-bit floating-point component types for these
  // instructions:
  // InterpolateAtCentroid, InterpolateAtSample, InterpolateAtOffset
  if (SpirvType::isOrContainsType<FloatType, 16>(instr->getResultType()))
    switch (instr->getInstruction()) {
    case GLSLstd450::GLSLstd450InterpolateAtCentroid:
    case GLSLstd450::GLSLstd450InterpolateAtSample:
    case GLSLstd450::GLSLstd450InterpolateAtOffset:
      addExtension(Extension::AMD_gpu_shader_half_float, "16-bit float",
                   instr->getSourceLocation());
    default:
      break;
    }

  return visitInstruction(instr);
}

bool CapabilityVisitor::visit(SpirvDemoteToHelperInvocationEXT *inst) {
  addCapability(spv::Capability::DemoteToHelperInvocationEXT,
                inst->getSourceLocation());
  addExtension(Extension::EXT_demote_to_helper_invocation, "discard",
               inst->getSourceLocation());
  return true;
}

} // end namespace spirv
} // end namespace clang
