# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  Core
  IRReader
  Support
  )

add_llvm_tool(llvm-diff
  llvm-diff.cpp
  DiffConsumer.cpp
  DiffLog.cpp
  DifferenceEngine.cpp
  )
