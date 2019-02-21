# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
# Builds dxc.exe

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

add_clang_executable(dxc
  dxcmain.cpp
#  dxr.rc
  )

target_link_libraries(dxc
  dxclib
  dxcompiler
  )

if(ENABLE_SPIRV_CODEGEN)
  target_link_libraries(dxc SPIRV-Tools)
endif()

set_target_properties(dxc PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})
hlsl_update_product_ver("dxc")

if (WIN32)
  include_directories(AFTER ${DIASDK_INCLUDE_DIRS})
endif (WIN32)

add_dependencies(dxc dxclib dxcompiler)

if(UNIX)
  set(CLANGXX_LINK_OR_COPY create_symlink)
# Create a relative symlink
  set(dxc_binary "dxc${CMAKE_EXECUTABLE_SUFFIX}")
else()
  set(CLANGXX_LINK_OR_COPY copy)
  set(dxc_binary "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/dxc${CMAKE_EXECUTABLE_SUFFIX}")
endif()

install(TARGETS dxc
  RUNTIME DESTINATION bin)

add_subdirectory(dxclib)

