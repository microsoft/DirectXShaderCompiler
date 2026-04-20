# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
# Builds dxreflector.exe

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  dxcsupport
  Option     # option library
  Support    # For Atomic increment/decrement
  )

add_clang_executable(dxreflector
  dxreflector.cpp
#  dxreflector.rc
  )

target_link_libraries(dxreflector
  dxclib
  dxcompiler
  )

set_target_properties(dxreflector PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})
# set_target_properties(dxreflector PROPERTIES ENABLE_EXPORTS 1)

include_directories(${LLVM_SOURCE_DIR}/tools/clang/tools)

add_dependencies(dxreflector dxclib dxcompiler)

if(UNIX)
  set(CLANGXX_LINK_OR_COPY create_symlink)
# Create a relative symlink
  set(dxreflector_binary "dxreflector${CMAKE_EXECUTABLE_SUFFIX}")
else()
  set(CLANGXX_LINK_OR_COPY copy)
  set(dxreflector_binary "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/dxreflector${CMAKE_EXECUTABLE_SUFFIX}")
endif()

install(TARGETS dxreflector
  RUNTIME DESTINATION bin)

if(MSVC)
  target_compile_options(dxreflector PRIVATE /W4 /WX)
else()
  target_compile_options(dxreflector PRIVATE -Wall -Wextra -Werror -Wno-pedantic)
endif()

