# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
# Build HLSLTestEngine.exe as a test harness

set(SOURCES
  FenceWrapper.cpp
  Main.cpp
  MappedData.cpp
  SimpleTextFile.cpp
  )

include_directories(../../../UnitTests/HLSL)

add_clang_executable(HLSLTestEngine ${SOURCES})

target_compile_definitions(HLSLTestEngine PRIVATE UNICODE)

target_link_libraries(HLSLTestEngine d3d12)
target_link_libraries(HLSLTestEngine dxgi)
target_link_libraries(HLSLTestEngine runtimeobject)

set_target_properties(HLSLTestEngine
  PROPERTIES
  LINKER_LANGUAGE CXX)
