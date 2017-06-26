set(LLVM_LINK_COMPONENTS
  Support
  dxcsupport
  hlsl
  )

add_clang_unittest(clang-spirv-tests
  DecorationTest.cpp
  InstBuilderTest.cpp
  ModuleBuilderTest.cpp
  SPIRVContextTest.cpp
  StructureTest.cpp
  TestMain.cpp
  StringTest.cpp
  TypeTest.cpp
  )

target_link_libraries(clang-spirv-tests
  clangCodeGen
  clangFrontend
  clangSPIRV
  )

set_output_directory(clang-spirv-tests ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})
