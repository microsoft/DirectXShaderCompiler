# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS support)

add_clang_executable(clang-format-fuzzer
  EXCLUDE_FROM_ALL
  ClangFormatFuzzer.cpp
  )

target_link_libraries(clang-format-fuzzer
  ${CLANG_FORMAT_LIB_DEPS}
  LLVMFuzzer
  )
