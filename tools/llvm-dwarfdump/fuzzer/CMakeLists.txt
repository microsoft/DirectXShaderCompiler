# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  DebugInfoDWARF
  Object
  Support
  )

add_llvm_executable(llvm-dwarfdump-fuzzer
  EXCLUDE_FROM_ALL
  llvm-dwarfdump-fuzzer.cpp
  )

target_link_libraries(llvm-dwarfdump-fuzzer
  LLVMFuzzer
  )
