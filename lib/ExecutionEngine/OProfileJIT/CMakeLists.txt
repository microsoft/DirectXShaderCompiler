# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.

include_directories( ${LLVM_OPROFILE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. )

add_llvm_library(LLVMOProfileJIT
  OProfileJITEventListener.cpp
  OProfileWrapper.cpp
  )
