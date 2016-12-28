# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
# Builds HLSLHost.exe

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  )

add_clang_executable(HLSLHost
  HLSLHost.cpp
  )

target_link_libraries(HLSLHost
  clang-hlsl-tests
  )

set_target_properties(HLSLHost PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})

include_directories(AFTER ${DIASDK_INCLUDE_DIRS})

add_dependencies(HLSLHost clang-hlsl-tests)

if(UNIX)
  set(CLANGXX_LINK_OR_COPY create_symlink)
# Create a relative symlink
  set(HLSLHost_binary "HLSLHost${CMAKE_EXECUTABLE_SUFFIX}")
else()
  set(CLANGXX_LINK_OR_COPY copy)
  set(HLSLHost_binary "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/HLSLHost${CMAKE_EXECUTABLE_SUFFIX}")
endif()

install(TARGETS HLSLHost
  RUNTIME DESTINATION bin)

