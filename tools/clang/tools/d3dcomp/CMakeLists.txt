# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
# Build a d3dcompiler_dxc_bridge.dll component that bridges to dxcompiler.dll

set(SOURCES
  d3dcomp.cpp
  d3dcomp.def
  )

add_clang_library(d3dcompiler_dxc_bridge SHARED ${SOURCES})
target_link_libraries(d3dcompiler_dxc_bridge PRIVATE dxcompiler)
add_dependencies(d3dcompiler_dxc_bridge dxcompiler)

set_target_properties(d3dcompiler_dxc_bridge
  PROPERTIES
  OUTPUT_NAME "d3dcompiler_dxc_bridge"
  VERSION ${LIBCLANG_LIBRARY_VERSION})
