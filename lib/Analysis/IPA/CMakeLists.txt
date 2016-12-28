# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
add_llvm_library(LLVMipa
  CallGraph.cpp
  CallGraphSCCPass.cpp
  CallPrinter.cpp
  GlobalsModRef.cpp
  IPA.cpp
  InlineCost.cpp
  )

add_dependencies(LLVMipa intrinsics_gen)
