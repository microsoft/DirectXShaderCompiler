include(HCT)

add_hlsl_hctgen(HLSLIntrinsicOp OUTPUT HlslIntrinsicOp.h)

set(HLSL_TEST_DATA_DIR ${LLVM_SOURCE_DIR}/tools/clang/test/HLSL/)

configure_file(
  ${LLVM_MAIN_INCLUDE_DIR}/dxc/Test/TestConfig.h.in
  ${LLVM_INCLUDE_DIR}/dxc/Test/TestConfig.h
  )

add_subdirectory(DXIL)
add_subdirectory(DxilContainer)
add_subdirectory(HLSL)
add_subdirectory(Support)
add_subdirectory(Tracing)
