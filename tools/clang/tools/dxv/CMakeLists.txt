# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
# Builds dxv.exe

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  dxcsupport
  Option     # option library
  )

add_clang_executable(dxv
  dxv.cpp
  )

target_link_libraries(dxv
  dxcompiler
  )

set_target_properties(dxv PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})

add_dependencies(dxv dxcompiler)

if(UNIX)
  set(CLANGXX_LINK_OR_COPY create_symlink)
# Create a relative symlink
  set(dxv_binary "dxv${CMAKE_EXECUTABLE_SUFFIX}")
else()
  set(CLANGXX_LINK_OR_COPY copy)
  set(dxv_binary "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/dxv${CMAKE_EXECUTABLE_SUFFIX}")
endif()

install(TARGETS dxv
  RUNTIME DESTINATION bin)

