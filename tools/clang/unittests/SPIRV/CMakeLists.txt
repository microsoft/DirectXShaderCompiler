set(LLVM_LINK_COMPONENTS
  Support
  dxcsupport
  hlsl
  )

add_clang_unittest(clang-spirv-tests
  CodeGenSPIRVTest.cpp
  ConstantTest.cpp
  DecorationTest.cpp
  FileTestFixture.cpp
  FileTestUtils.cpp
  InstBuilderTest.cpp
  ModuleBuilderTest.cpp
  SPIRVContextTest.cpp
  SPIRVTestOptions.cpp
  StructureTest.cpp
  TestMain.cpp
  StringTest.cpp
  TypeTest.cpp
  WholeFileTestFixture.cpp
  )

target_compile_definitions(clang-spirv-tests PRIVATE
  # GoogleTest uses std::tr1, which is deprecated in VS2017.
  # The following is an escape-hatch macro to silence the deprecation warnings.
  -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)

target_link_libraries(clang-spirv-tests
  clangCodeGen
  clangFrontend
  clangSPIRV
  effcee
  SPIRV-Tools
  )

target_include_directories(clang-spirv-tests
  PRIVATE ${SPIRV_TOOLS_INCLUDE_DIR} ${DXC_EFFCEE_DIR})

set_output_directory(clang-spirv-tests
  ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})
