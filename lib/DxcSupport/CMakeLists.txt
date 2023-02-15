# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
add_llvm_library(LLVMDxcSupport
  dxcapi.use.cpp
  dxcmem.cpp
  FileIOHelper.cpp
  Global.cpp
  HLSLOptions.cpp
  Unicode.cpp
  WinAdapter.cpp
  WinFunctions.cpp
  )

#generate header with platform-specific library name
configure_file(
${LLVM_MAIN_SRC_DIR}/lib/DxcSupport/SharedLibAffix.inc
SharedLibAffix.h
)

include_directories( ${LLVM_INCLUDE_DIR}/DxcSupport) #needed to find the generated header

add_dependencies(LLVMDxcSupport TablegenHLSLOptions)
