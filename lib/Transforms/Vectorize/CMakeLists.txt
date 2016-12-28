# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
# HLSL Change - add ignored sources
set(HLSL_IGNORE_SOURCES BBVectorize.cpp LoopVectorize.cpp SLPVectorizer.cpp)

add_llvm_library(LLVMVectorize
  Vectorize.cpp
#  BBVectorize.cpp
#  LoopVectorize.cpp
#  SLPVectorizer.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
  )

add_dependencies(LLVMVectorize intrinsics_gen)
