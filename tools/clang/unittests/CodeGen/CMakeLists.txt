# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  Core
  Support
  )

add_clang_unittest(ClangCodeGenTests
  BufferSourceTest.cpp
  )

target_link_libraries(ClangCodeGenTests
  clangBasic
  clangCodeGen
  clangFrontend
  clangParse
  )
