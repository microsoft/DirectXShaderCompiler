# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
add_llvm_library(LLVMDebugInfoDWARF
  DWARFAbbreviationDeclaration.cpp
  DWARFAcceleratorTable.cpp
  DWARFCompileUnit.cpp
  DWARFContext.cpp
  DWARFDebugAbbrev.cpp
  DWARFDebugArangeSet.cpp
  DWARFDebugAranges.cpp
  DWARFDebugFrame.cpp
  DWARFDebugInfoEntry.cpp
  DWARFDebugLine.cpp
  DWARFDebugLoc.cpp
  DWARFDebugRangeList.cpp
  DWARFFormValue.cpp
  DWARFTypeUnit.cpp
  DWARFUnit.cpp
  SyntaxHighlighting.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/DebugInfo/DWARF
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/DebugInfo
  )
