# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
if(WIN32)
find_package(TAEF REQUIRED)
find_package(DiaSDK REQUIRED) # Used for constants and declarations.
find_package(D3D12 REQUIRED) # Used for ExecutionTest.cpp.
endif(WIN32)

set( LLVM_LINK_COMPONENTS
  support
  mssupport
  dxcsupport
  dxil
  dxilcontainer
  dxilrootsignature
  hlsl
  option
  bitreader
  bitwriter
  analysis
  ipa
  irreader
  transformutils  # for CloneModule
  )

if(WIN32)
set(HLSL_IGNORE_SOURCES
  TestMain.cpp
  HLSLTestOptions.cpp
)
add_clang_library(clang-hlsl-tests SHARED
  AllocatorTest.cpp
  CompilerTest.cpp
  DxilContainerTest.cpp
  DxilModuleTest.cpp
  DXIsenseTest.cpp
  ExecutionTest.cpp
  ExtensionTest.cpp
  FunctionTest.cpp
  LinkerTest.cpp
  MSFileSysTest.cpp
  Objects.cpp
  OptimizerTest.cpp
  OptionsTest.cpp
  PixTest.cpp
  RewriterTest.cpp
  ShaderOpTest.cpp
  SystemValueTest.cpp
  ValidationTest.cpp
  VerifierTest.cpp
  clang-hlsl-tests.rc
  )
else (WIN32)
set(HLSL_IGNORE_SOURCES
  ExecutionTest.cpp
  LinkerTest.cpp
  MSFileSysTest.cpp
  PixTest.cpp
  RewriterTest.cpp
  ShaderOpTest.cpp
  DxilContainerTest.cpp
  ValidationTest.cpp
  CompilerTest.cpp
  )

add_clang_unittest(clang-hlsl-tests
  AllocatorTest.cpp
  DxilModuleTest.cpp
  DXIsenseTest.cpp
  ExtensionTest.cpp
  FunctionTest.cpp
  HLSLTestOptions.cpp
  Objects.cpp
  OptimizerTest.cpp
  OptionsTest.cpp
  SystemValueTest.cpp
  TestMain.cpp
  VerifierTest.cpp
  )

endif(WIN32)

set_target_properties(clang-hlsl-tests PROPERTIES FOLDER "Clang tests")

if (WIN32)
target_link_libraries(clang-hlsl-tests PRIVATE
  dxcompiler
  HLSLTestLib
  LLVMDxilDia
  ${TAEF_LIBRARIES}
  ${DIASDK_LIBRARIES}
  ${D3D12_LIBRARIES}
  )
else(WIN32)
target_link_libraries(clang-hlsl-tests
  dxcompiler
  HLSLTestLib
  )
endif(WIN32)

if(WIN32)
# Add includes for platform helpers and dxc API.
include_directories(${TAEF_INCLUDE_DIRS})
include_directories(${DIASDK_INCLUDE_DIRS})
include_directories(${D3D12_INCLUDE_DIRS})

endif(WIN32)

# Add includes to directly reference intrinsic tables.
include_directories(../../lib/Sema)

add_dependencies(clang-hlsl-tests dxcompiler)

if(WIN32)
install(TARGETS clang-hlsl-tests
  RUNTIME DESTINATION bin)
else(WIN32)
set_output_directory(clang-hlsl-tests
  ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})
endif(WIN32)

if(WIN32)
# Add a .user file with settings for te.exe.
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" DOS_STYLE_SOURCE_DIR)
file(TO_NATIVE_PATH "${TAEF_BIN_DIR}" DOS_TAEF_BIN_DIR)
configure_file(clang-hlsl-tests.vcxproj.user.txt clang-hlsl-tests.vcxproj.user)
endif(WIN32)

add_test(NAME test-hlsl-codegen
  COMMAND clang-hlsl-tests --HlslDataDir
          ${PROJECT_SOURCE_DIR}/tools/clang/test/HLSL)
