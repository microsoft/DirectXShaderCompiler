# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_unittest(SemaTests
  ExternalSemaSourceTest.cpp
  )

target_link_libraries(SemaTests
  clangAST
  clangBasic
  clangFrontend
  clangParse
  clangSema
  clangTooling
  )
