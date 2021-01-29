# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
# Builds dxclib

if (WIN32)
  find_package(DiaSDK REQUIRED) # Used for constants and declarations.
endif (WIN32)

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  dxcsupport
  DXIL
  DxilContainer
  HLSL
  Option     # option library
  Support    # just for assert and raw streams
  )

add_clang_library(dxclib
  dxc.cpp
  )

if(ENABLE_SPIRV_CODEGEN)
  target_link_libraries(dxclib PRIVATE SPIRV-Tools)
endif()

if (WIN32)
  include_directories(AFTER ${DIASDK_INCLUDE_DIRS})
endif (WIN32)

target_compile_definitions(dxclib
    PRIVATE VERSION_STRING_SUFFIX=" for ${CMAKE_SYSTEM_NAME}")
    
add_dependencies(dxclib TablegenHLSLOptions dxcompiler)