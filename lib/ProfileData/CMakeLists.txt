# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
add_llvm_library(LLVMProfileData
  InstrProf.cpp
  InstrProfReader.cpp
  InstrProfWriter.cpp
  CoverageMapping.cpp
  CoverageMappingWriter.cpp
  CoverageMappingReader.cpp
  SampleProf.cpp
  SampleProfReader.cpp
  SampleProfWriter.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/ProfileData

  DEPENDS
  intrinsics_gen
  )
