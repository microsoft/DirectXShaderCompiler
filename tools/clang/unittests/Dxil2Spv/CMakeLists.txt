set(LLVM_LINK_COMPONENTS
  Support
  dxcsupport
  dxil
  dxilrootsignature
  hlsl
  )

add_clang_unittest(clang-dxil2spv-tests
  TestMain.cpp
  LitTest.cpp
  WholeFileTestFixture.cpp
  WholeFileTestFixture.h
  FileTestUtils.cpp
  FileTestUtils.h
  Dxil2SpvTestOptions.cpp
  Dxil2SpvTestOptions.h
  )

target_link_libraries(clang-dxil2spv-tests
  clangSPIRV
  dxcompiler
  dxil2spvlib
  effcee
  )

include_directories(${LLVM_SOURCE_DIR}/tools/clang/tools)

target_include_directories(clang-dxil2spv-tests
  PRIVATE ${SPIRV_TOOLS_INCLUDE_DIR} ${DXC_EFFCEE_DIR})

set_output_directory(clang-dxil2spv-tests
  ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})

add_test(NAME test-dxil2spv
  COMMAND clang-dxil2spv-tests --dxil2spv-test-root
          ${PROJECT_SOURCE_DIR}/tools/clang/test/Dxil2Spv)
