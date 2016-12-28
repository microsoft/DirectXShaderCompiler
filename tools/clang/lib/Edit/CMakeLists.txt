# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  Support
  )

set (HLSL_IGNORE_SOURCES
  RewriteObjCFoundationAPI.cpp
  )

add_clang_library(clangEdit
  Commit.cpp
  EditedSource.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex
  )
