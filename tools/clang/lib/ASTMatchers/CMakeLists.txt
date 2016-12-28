# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
# add_subdirectory(Dynamic) # HLSL Change - no need to build this

set(LLVM_LINK_COMPONENTS support)

add_clang_library(clangASTMatchers
  ASTMatchFinder.cpp
  ASTMatchersInternal.cpp

  LINK_LIBS
  clangAST
  clangBasic
  )
