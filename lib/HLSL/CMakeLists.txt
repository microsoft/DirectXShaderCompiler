# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
add_llvm_library(LLVMHLSL
  DxilCBuffer.cpp
  DxilCompType.cpp
  DxilCondenseResources.cpp
  DxilContainer.cpp
  DxilContainerAssembler.cpp
  DxilContainerReflection.cpp
  DxilGenerationPass.cpp
  DxilInterpolationMode.cpp
  DxilMetadataHelper.cpp
  DxilModule.cpp
  DXILOperations.cpp
  DxilResource.cpp
  DxilResourceBase.cpp
  DxilRootSignature.cpp
  DxilSampler.cpp
  DxilSemantic.cpp
  DxilShaderModel.cpp
  DxilSignature.cpp
  DxilSignatureAllocator.cpp
  DxilSignatureElement.cpp
  DxilSigPoint.cpp
  DxilTargetLowering.cpp
  DxilTargetTransformInfo.cpp
  DxilTypeSystem.cpp
  DxilValidation.cpp
  DxilValidationRaceCondCheck.cpp
  DxcOptimizer.cpp
  HLMatrixLowerPass.cpp
  HLModule.cpp
  HLOperations.cpp
  HLOperationLower.cpp
  HLOperationLowerExtension.cpp
  HLResource.cpp
  ReducibilityAnalysis.cpp
  WaveSensitivityAnalysis.cpp
  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/IR
)

add_dependencies(LLVMHLSL intrinsics_gen)
