# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  LTO
  MC
  Support
  Target
  )

add_llvm_tool(llvm-lto
  llvm-lto.cpp
  )

