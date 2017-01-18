# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
find_package(TAEF REQUIRED)
find_package(DiaSDK REQUIRED) # Used for constants and declarations.
find_package(D3D12 REQUIRED) # Used for ExecutionTest.cpp.

set( LLVM_LINK_COMPONENTS
  support
  mssupport
  dxcsupport
  hlsl
  option
  )

add_clang_library(clang-hlsl-tests SHARED
  AllocatorTest.cpp
  CompilationResult.h
  CompilerTest.cpp
  DiscardStmt.cpp
  DxilContainerTest.cpp
  DXIsenseTest.cpp
  ExecutionTest.cpp
  ExtensionTest.cpp
  FileCheckerTest.cpp
  FileCheckForTest.cpp
  FunctionTest.cpp
  HLSLTestData.h
  HlslTestUtils.h
  MSFileSysTest.cpp
  Objects.cpp
  OptionsTest.cpp
  RewriterTest.cpp
  ShaderOpTest.cpp
  SystemValueTest.cpp
  ValidationTest.cpp
  VerifierTest.cpp
  )
set_target_properties(clang-hlsl-tests PROPERTIES FOLDER "Clang tests")

target_link_libraries(clang-hlsl-tests PRIVATE
  dxcompiler
  ${TAEF_LIBRARIES}
  ${DIASDK_LIBRARIES}
  ${D3D12_LIBRARIES}
  )

# Add includes for platform helpers and dxc API.
include_directories(${TAEF_INCLUDE_DIRS})
include_directories(${DIASDK_INCLUDE_DIRS})
include_directories(${D3D12_INCLUDE_DIRS})

# Add includes to directly reference intrinsic tables.
include_directories(../../lib/Sema)

add_dependencies(clang-hlsl-tests dxcompiler)

install(TARGETS clang-hlsl-tests
  RUNTIME DESTINATION bin)

# Add a .user file with settings for te.exe.
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" DOS_STYLE_SOURCE_DIR)
file(TO_NATIVE_PATH "${TAEF_BIN_DIR}" DOS_TAEF_BIN_DIR)
configure_file(clang-hlsl-tests.vcxproj.user.txt clang-hlsl-tests.vcxproj.user)
