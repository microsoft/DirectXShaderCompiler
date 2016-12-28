# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  AsmParser
  BitReader
  BitWriter
  Core
  IRReader
  Support
  )

add_llvm_tool(verify-uselistorder
  verify-uselistorder.cpp
  )
