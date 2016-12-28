# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangIndex
  CommentToXML.cpp
  USRGeneration.cpp

  ADDITIONAL_HEADERS
  SimpleFormatContext.h

  LINK_LIBS
  clangAST
  clangBasic
  clangFormat
  clangRewrite
  clangToolingCore
  )
