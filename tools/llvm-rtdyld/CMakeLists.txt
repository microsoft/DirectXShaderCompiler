# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  DebugInfoDWARF
  ExecutionEngine
  MC
  Object
  RuntimeDyld
  Support
  )

add_llvm_tool(llvm-rtdyld
  llvm-rtdyld.cpp
  )
