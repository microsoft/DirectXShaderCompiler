# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
# Builds dxa.exe

if (WIN32)
  find_package(DiaSDK REQUIRED) # Used for constants and declarations.
endif (WIN32)

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  DXIL
  HLSL
  dxcsupport
  Option     # option library
  )

add_clang_executable(dxa
  dxa.cpp
  )

target_link_libraries(dxa
  dxcompiler
  )

set_target_properties(dxa PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})

if (WIN32)
  include_directories(AFTER ${DIASDK_INCLUDE_DIRS})
endif (WIN32)

add_dependencies(dxa dxcompiler)

if(UNIX)
  set(CLANGXX_LINK_OR_COPY create_symlink)
# Create a relative symlink
  set(dxa_binary "dxa${CMAKE_EXECUTABLE_SUFFIX}")
else()
  set(CLANGXX_LINK_OR_COPY copy)
  set(dxa_binary "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/dxa${CMAKE_EXECUTABLE_SUFFIX}")
endif()

install(TARGETS dxa
  RUNTIME DESTINATION bin)

