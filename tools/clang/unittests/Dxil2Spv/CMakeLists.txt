set(LLVM_LINK_COMPONENTS
  dxcsupport
  dxil
  dxilrootsignature
  hlsl
  Support
  )

add_clang_unittest(clang-dxil2spv-tests
  Dxil2SpvTestOptions.cpp
  Dxil2SpvTestOptions.h
  FileTestUtils.cpp
  FileTestUtils.h
  LitTest.cpp
  TestMain.cpp
  WholeFileTestFixture.cpp
  WholeFileTestFixture.h
  )

target_link_libraries(clang-dxil2spv-tests
  clangSPIRV
  dxcompiler
  dxil2spvlib
  HLSLTestLib
  )

include_directories(${LLVM_SOURCE_DIR}/tools/clang/tools)

target_include_directories(clang-dxil2spv-tests
  PRIVATE ${SPIRV_TOOLS_INCLUDE_DIR} ${DXC_EFFCEE_DIR})

set_output_directory(clang-dxil2spv-tests
  ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})

add_test(NAME test-dxil2spv
  COMMAND clang-dxil2spv-tests --dxil2spv-test-root
          ${PROJECT_SOURCE_DIR}/tools/clang/test/Dxil2Spv)
