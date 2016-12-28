# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_unittest(FormatTests
  FormatTest.cpp
  FormatTestJava.cpp
  FormatTestJS.cpp
  FormatTestProto.cpp
  FormatTestSelective.cpp
  )

target_link_libraries(FormatTests
  clangFormat
  clangToolingCore
  )
