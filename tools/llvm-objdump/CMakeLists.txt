# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  CodeGen
  DebugInfoDWARF
  MC
  MCDisassembler
  Object
  Support
  )

add_llvm_tool(llvm-objdump
  llvm-objdump.cpp
  COFFDump.cpp
  ELFDump.cpp
  MachODump.cpp
  )
