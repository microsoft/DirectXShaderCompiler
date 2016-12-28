# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
add_llvm_library(LLVMMCDisassembler
  Disassembler.cpp
  MCRelocationInfo.cpp
  MCExternalSymbolizer.cpp
  MCDisassembler.cpp
  )
