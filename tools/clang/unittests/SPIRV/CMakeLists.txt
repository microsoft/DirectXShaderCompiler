set(LLVM_LINK_COMPONENTS
  Support
  dxcsupport
  dxil
  dxilrootsignature
  hlsl
  )

add_clang_unittest(ClangSPIRVTests
  CodeGenSpirvTest.cpp
  FileTestFixture.cpp
  FileTestUtils.cpp
  SpirvBasicBlockTest.cpp
  SpirvContextTest.cpp
  SpirvTestOptions.cpp
  SpirvTypeTest.cpp
  SpirvDebugInstructionTest.cpp
  SpirvConstantTest.cpp
  StringTest.cpp
  TestMain.cpp
  )

target_link_libraries(ClangSPIRVTests
  clangAST
  clangBasic
  clangCodeGen
  clangFrontend
  clangSPIRV
  clangTooling
  dxcompiler
  effcee
  SPIRV-Tools
  )

# This is necessary so that the linked dxcompiler is loaded into memory space
# first, and in case dxcompiler.so is loaded via 'dlopen', it resolves to the
# same memory space, and prevents getting two copies of global static variables.
add_dependencies(ClangSPIRVTests dxcompiler)

target_include_directories(ClangSPIRVTests
  PRIVATE ${SPIRV_TOOLS_INCLUDE_DIR} ${DXC_EFFCEE_DIR})

if (NOT CLANG_INCLUDE_TESTS)
  set_output_directory(ClangSPIRVTests
    ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})
endif()

set(SPIRV_TEST_DATA_DIR ${LLVM_SOURCE_DIR}/tools/clang/test/CodeGenSPIRV/)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SpirvTestOptions.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/SpirvTestOptions.h
    )

if(WIN32)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj") # otherwise will hit fatal error C1128 on windows x64
endif(WIN32)

if (NOT CLANG_INCLUDE_TESTS)
  add_test(NAME test-spirv-codegen COMMAND $<TARGET_FILE:ClangSPIRVTests>)
endif()
