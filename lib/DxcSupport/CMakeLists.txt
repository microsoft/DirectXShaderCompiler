# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
add_llvm_library(LLVMDxcSupport
  dxcapi.use.cpp
  FileIOHelper.cpp
  Global.cpp
  HLSLOptions.cpp
  Unicode.cpp
  )

add_dependencies(LLVMDxcSupport TablegenHLSLOptions)
