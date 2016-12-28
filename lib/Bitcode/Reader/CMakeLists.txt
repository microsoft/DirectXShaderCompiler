# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
# HLSL Change - add ignored sources
set(HLSL_IGNORE_SOURCES BitReader.cpp)

add_llvm_library(LLVMBitReader
  # BitReader.cpp # HLSL Change - this is just the C wrapper
  BitcodeReader.cpp
  BitstreamReader.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Bitcode
  )

add_dependencies(LLVMBitReader intrinsics_gen)
