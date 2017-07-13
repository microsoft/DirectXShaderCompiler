set(LLVM_LINK_COMPONENTS
  Support
  dxcsupport
  hlsl
  )

add_clang_unittest(clang-spirv-tests
  CodeGenSPIRVTest.cpp
  DecorationTest.cpp
  InstBuilderTest.cpp
  ModuleBuilderTest.cpp
  SPIRVContextTest.cpp
  SPIRVTestOptions.cpp
  StructureTest.cpp
  TestMain.cpp
  StringTest.cpp
  TypeTest.cpp
  WholeFileCheck.cpp
  )

target_link_libraries(clang-spirv-tests
  clangCodeGen
  clangFrontend
  clangSPIRV
  SPIRV-Tools
  )

target_include_directories(clang-spirv-tests PRIVATE ${SPIRV_TOOLS_INCLUDE_DIR})

set_output_directory(clang-spirv-tests ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})
