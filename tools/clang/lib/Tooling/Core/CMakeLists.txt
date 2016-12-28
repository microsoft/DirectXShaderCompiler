# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS support)

add_clang_library(clangToolingCore
  Replacement.cpp

  LINK_LIBS
  clangBasic
  clangLex
  clangRewrite
  )
