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
      spvBuilder.requireCapability(spv::Capability::Int16);

      // Usage of a 16-bit integer type as stage I/O.
      if (sc == spv::StorageClass::Input || sc == spv::StorageClass::Output) {
        spvBuilder.addExtension(Extension::KHR_16bit_storage,
                                "16-bit stage IO variables", loc);
        spvBuilder.requireCapability(spv::Capability::StorageInputOutput16);
      }
      break;
    }
    case 64: {
      spvBuilder.requireCapability(spv::Capability::Int64);
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
      // It looks like the validator does not approve of Float16
      // capability even though we do use the necessary extension.
      // TODO: Re-enable adding Float16 capability below.
      // spvBuilder.requireCapability(spv::Capability::Float16);
      spvBuilder.addExtension(Extension::AMD_gpu_shader_half_float,
                              "16-bit float", {});

      // Usage of a 16-bit float type as stage I/O.
      if (sc == spv::StorageClass::Input || sc == spv::StorageClass::Output) {
        spvBuilder.addExtension(Extension::KHR_16bit_storage,
                                "16-bit stage IO variables", loc);
        spvBuilder.requireCapability(spv::Capability::StorageInputOutput16);
      }
      break;
    }
    case 64: {
      spvBuilder.requireCapability(spv::Capability::Float64);
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
      spvBuilder.addExtension(Extension::EXT_descriptor_indexing,
                              "runtime array of resources", {});
      spvBuilder.requireCapability(spv::Capability::RuntimeDescriptorArrayEXT);
    }
    addCapabilityForType(raType->getElementType(), loc, sc);
  }
  // Image types
  else if (const auto *imageType = dyn_cast<ImageType>(type)) {
    switch (imageType->getDimension()) {
    case spv::Dim::Buffer: {
      spvBuilder.requireCapability(spv::Capability::SampledBuffer);
      if (imageType->withSampler() == ImageType::WithSampler::No) {
        spvBuilder.requireCapability(spv::Capability::ImageBuffer);
      }
      break;
    }
    case spv::Dim::Dim1D: {
      if (imageType->withSampler() == ImageType::WithSampler::No) {
        spvBuilder.requireCapability(spv::Capability::Image1D);
      } else {
        spvBuilder.requireCapability(spv::Capability::Sampled1D);
      }
      break;
    }
    case spv::Dim::SubpassData: {
      spvBuilder.requireCapability(spv::Capability::InputAttachment);
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
      spvBuilder.requireCapability(
          spv::Capability::StorageImageExtendedFormats);
      break;
    default:
      // Only image formats requiring extended formats are relevant. The rest
      // just pass through.
      break;
    }

    if (imageType->isArrayedImage() && imageType->isMSImage())
      spvBuilder.requireCapability(spv::Capability::ImageMSArray);

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
    if (SpirvType::isOrContains16BitType(structType)) {
      spvBuilder.addExtension(Extension::KHR_16bit_storage,
                              "16-bit types in resource", loc);
      if (sc == spv::StorageClass::PushConstant) {
        spvBuilder.requireCapability(spv::Capability::StoragePushConstant16);
      } else if (structType->getInterfaceType() ==
                 StructInterfaceType::UniformBuffer) {
        spvBuilder.requireCapability(spv::Capability::StorageUniform16);
      } else if (structType->getInterfaceType() ==
                 StructInterfaceType::StorageBuffer) {
        spvBuilder.requireCapability(
            spv::Capability::StorageUniformBufferBlock16);
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
    spvBuilder.requireCapability(spv::Capability::SampleRateShading, loc);
    break;
  }
  case spv::Decoration::NonUniformEXT: {
    spvBuilder.addExtension(Extension::EXT_descriptor_indexing, "NonUniformEXT",
                            loc);
    spvBuilder.requireCapability(spv::Capability::ShaderNonUniformEXT);

    break;
  }
  // Capabilities needed for built-ins
  case spv::Decoration::BuiltIn: {
    assert(decor->getParams().size() == 1);
    const auto builtin = static_cast<spv::BuiltIn>(decor->getParams()[0]);
    switch (builtin) {
    case spv::BuiltIn::SampleId:
    case spv::BuiltIn::SamplePosition: {
      spvBuilder.requireCapability(spv::Capability::SampleRateShading, loc);
      break;
    }
    case spv::BuiltIn::SubgroupSize:
    case spv::BuiltIn::NumSubgroups:
    case spv::BuiltIn::SubgroupId:
    case spv::BuiltIn::SubgroupLocalInvocationId: {
      spvBuilder.requireCapability(spv::Capability::GroupNonUniform, loc);
      break;
    }
    case spv::BuiltIn::BaseVertex: {
      spvBuilder.addExtension(Extension::KHR_shader_draw_parameters,
                              "BaseVertex Builtin", loc);
      spvBuilder.requireCapability(spv::Capability::DrawParameters);
      break;
    }
    case spv::BuiltIn::BaseInstance: {
      spvBuilder.addExtension(Extension::KHR_shader_draw_parameters,
                              "BaseInstance Builtin", loc);
      spvBuilder.requireCapability(spv::Capability::DrawParameters);
      break;
    }
    case spv::BuiltIn::DrawIndex: {
      spvBuilder.addExtension(Extension::KHR_shader_draw_parameters,
                              "DrawIndex Builtin", loc);
      spvBuilder.requireCapability(spv::Capability::DrawParameters);
      break;
    }
    case spv::BuiltIn::DeviceIndex: {
      spvBuilder.addExtension(Extension::KHR_device_group,
                              "DeviceIndex Builtin", loc);
      spvBuilder.requireCapability(spv::Capability::DeviceGroup);
      break;
    }
    case spv::BuiltIn::FragStencilRefEXT: {
      spvBuilder.addExtension(Extension::EXT_shader_stencil_export,
                              "SV_StencilRef", loc);
      spvBuilder.requireCapability(spv::Capability::StencilExportEXT);
      break;
    }
    case spv::BuiltIn::ViewIndex: {
      spvBuilder.addExtension(Extension::KHR_multiview, "SV_ViewID", loc);
      spvBuilder.requireCapability(spv::Capability::MultiView);
      break;
    }
    case spv::BuiltIn::FullyCoveredEXT: {
      spvBuilder.addExtension(Extension::EXT_fragment_fully_covered,
                              "SV_InnerCoverage", loc);
      spvBuilder.requireCapability(spv::Capability::FragmentFullyCoveredEXT);
      break;
    }
    case spv::BuiltIn::PrimitiveId: {
      // PrimitiveID can be used as PSIn
      if (shaderModel == spv::ExecutionModel::Fragment)
        spvBuilder.requireCapability(spv::Capability::Geometry);
      break;
    }
    case spv::BuiltIn::Layer: {
      if (shaderModel == spv::ExecutionModel::Vertex ||
          shaderModel == spv::ExecutionModel::TessellationControl ||
          shaderModel == spv::ExecutionModel::TessellationEvaluation) {
        spvBuilder.addExtension(Extension::EXT_shader_viewport_index_layer,
                                "SV_RenderTargetArrayIndex", loc);
        spvBuilder.requireCapability(
            spv::Capability::ShaderViewportIndexLayerEXT);
      } else if (shaderModel == spv::ExecutionModel::Fragment) {
        // SV_RenderTargetArrayIndex can be used as PSIn.
        spvBuilder.requireCapability(spv::Capability::Geometry);
      }
      break;
    }
    case spv::BuiltIn::ViewportIndex: {
      if (shaderModel == spv::ExecutionModel::Vertex ||
          shaderModel == spv::ExecutionModel::TessellationControl ||
          shaderModel == spv::ExecutionModel::TessellationEvaluation) {
        spvBuilder.addExtension(Extension::EXT_shader_viewport_index_layer,
                                "SV_ViewPortArrayIndex", loc);
        spvBuilder.requireCapability(
            spv::Capability::ShaderViewportIndexLayerEXT);
      } else if (shaderModel == spv::ExecutionModel::Fragment ||
                 shaderModel == spv::ExecutionModel::Geometry) {
        // SV_ViewportArrayIndex can be used as PSIn.
        spvBuilder.requireCapability(spv::Capability::MultiViewport);
      }
      break;
    }
    case spv::BuiltIn::ClipDistance: {
      spvBuilder.requireCapability(spv::Capability::ClipDistance);
      break;
    }
    case spv::BuiltIn::CullDistance: {
      spvBuilder.requireCapability(spv::Capability::CullDistance);
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
  spvBuilder.requireCapability(spv::Capability::ImageQuery);
  return true;
}

bool CapabilityVisitor::visit(SpirvImageSparseTexelsResident *instr) {
  addCapabilityForType(instr->getResultType(), instr->getSourceLocation(),
                       instr->getStorageClass());
  spvBuilder.requireCapability(spv::Capability::ImageGatherExtended);
  return true;
}

bool CapabilityVisitor::visit(SpirvImageOp *instr) {
  addCapabilityForType(instr->getResultType(), instr->getSourceLocation(),
                       instr->getStorageClass());
  if (instr->hasOffset() || instr->hasConstOffsets())
    spvBuilder.requireCapability(spv::Capability::ImageGatherExtended);
  if (instr->hasMinLod())
    spvBuilder.requireCapability(spv::Capability::MinLod);
  if (instr->isSparse())
    spvBuilder.requireCapability(spv::Capability::SparseResidency);

  return true;
}

bool CapabilityVisitor::visitInstruction(SpirvInstruction *instr) {
  const SpirvType *resultType = instr->getResultType();
  const auto opcode = instr->getopcode();

  // Add result-type-specific capabilities
  addCapabilityForType(resultType, instr->getSourceLocation(),
                       instr->getStorageClass());

  // Add NonUniform capabilities if necessary
  if (instr->isNonUniform()) {
    spvBuilder.requireCapability(getNonUniformCapability(resultType));
  }

  // Add opcode-specific capabilities
  switch (opcode) {
  case spv::Op::OpDPdxCoarse:
  case spv::Op::OpDPdyCoarse:
  case spv::Op::OpFwidthCoarse:
  case spv::Op::OpDPdxFine:
  case spv::Op::OpDPdyFine:
  case spv::Op::OpFwidthFine:
    spvBuilder.requireCapability(spv::Capability::DerivativeControl);
    break;
  case spv::Op::OpGroupNonUniformElect:
    spvBuilder.requireCapability(spv::Capability::GroupNonUniform);
    break;
  case spv::Op::OpGroupNonUniformAny:
  case spv::Op::OpGroupNonUniformAll:
  case spv::Op::OpGroupNonUniformAllEqual:
    spvBuilder.requireCapability(spv::Capability::GroupNonUniformVote);
    break;
  case spv::Op::OpGroupNonUniformBallot:
  case spv::Op::OpGroupNonUniformInverseBallot:
  case spv::Op::OpGroupNonUniformBallotBitExtract:
  case spv::Op::OpGroupNonUniformBallotBitCount:
  case spv::Op::OpGroupNonUniformBallotFindLSB:
  case spv::Op::OpGroupNonUniformBallotFindMSB:
  case spv::Op::OpGroupNonUniformBroadcast:
  case spv::Op::OpGroupNonUniformBroadcastFirst:
    spvBuilder.requireCapability(spv::Capability::GroupNonUniformBallot);
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
    spvBuilder.requireCapability(spv::Capability::GroupNonUniformArithmetic);
    break;
  case spv::Op::OpGroupNonUniformQuadBroadcast:
  case spv::Op::OpGroupNonUniformQuadSwap:
    spvBuilder.requireCapability(spv::Capability::GroupNonUniformQuad);
    break;
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
    spvBuilder.requireCapability(spv::Capability::Shader);
    break;
  case spv::ExecutionModel::Geometry:
    spvBuilder.requireCapability(spv::Capability::Geometry);
    break;
  case spv::ExecutionModel::TessellationControl:
  case spv::ExecutionModel::TessellationEvaluation:
    spvBuilder.requireCapability(spv::Capability::Tessellation);
    break;
  default:
    llvm_unreachable("found unknown shader model");
    break;
  }
  return true;
}

bool CapabilityVisitor::visit(SpirvExecutionMode *execMode) {
  if (execMode->getExecutionMode() == spv::ExecutionMode::PostDepthCoverage) {
    spvBuilder.requireCapability(
        spv::Capability::SampleMaskPostDepthCoverage,
        execMode->getEntryPoint()->getSourceLocation());
  }
  return true;
}

} // end namespace spirv
} // end namespace clang
