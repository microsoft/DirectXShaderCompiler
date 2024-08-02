set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_unittest(DxilHashTests
  DxilHashTest.cpp
  )

include_directories(${LLVM_SOURCE_DIR}/tools/clang/tools/DxilHash)

target_link_libraries(DxilHashTests
  DxilHash
  )
