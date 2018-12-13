set(LLVM_LINK_COMPONENTS
  Support
  dxcsupport
  dxil
  dxilrootsignature
  hlsl
  )

add_clang_unittest(clang-spirv-tests
  CodeGenSPIRVTest.cpp
  FileTestFixture.cpp
  FileTestUtils.cpp
  SPIRVContextTest.cpp
  SPIRVTestOptions.cpp
  TestMain.cpp
  SpirvConstantTest.cpp
  StringTest.cpp
  WholeFileTestFixture.cpp
  )

target_link_libraries(clang-spirv-tests
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
add_dependencies(clang-spirv-tests dxcompiler)

target_include_directories(clang-spirv-tests
  PRIVATE ${SPIRV_TOOLS_INCLUDE_DIR} ${DXC_EFFCEE_DIR})

set_output_directory(clang-spirv-tests
  ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})

add_test(NAME test-spirv-codegen
  COMMAND clang-spirv-tests --spirv-test-root
          ${PROJECT_SOURCE_DIR}/tools/clang/test/CodeGenSPIRV)
