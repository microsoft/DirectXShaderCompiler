# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
add_llvm_library(LLVMDXIL
  DxilCBuffer.cpp
  DxilCompType.cpp
  DxilInterpolationMode.cpp
  DxilMetadataHelper.cpp
  DxilModule.cpp
  DxilOperations.cpp
  DxilResource.cpp
  DxilResourceBase.cpp
  DxilSampler.cpp
  DxilSemantic.cpp
  DxilShaderFlags.cpp
  DxilShaderModel.cpp
  DxilSignature.cpp
  DxilSignatureElement.cpp
  DxilSubobject.cpp
  DxilTypeSystem.cpp
  DxilUtil.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/IR
)

add_dependencies(LLVMDXIL intrinsics_gen)
