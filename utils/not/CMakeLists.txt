# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
add_llvm_utility(not
  not.cpp
  )

target_link_libraries(not LLVMSupport LLVMMSSupport) # HLSL Change
