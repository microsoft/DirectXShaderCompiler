# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_unittest(BasicTests
  CharInfoTest.cpp
  DiagnosticTest.cpp
  FileManagerTest.cpp
  SourceManagerTest.cpp
  VirtualFileSystemTest.cpp
  )

target_link_libraries(BasicTests
  clangBasic
  clangLex
  )
