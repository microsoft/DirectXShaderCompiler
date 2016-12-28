# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  AsmParser
  Core
  Support
  )

add_llvm_unittest(AsmParserTests
  AsmParserTest.cpp
  )
