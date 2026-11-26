# Tests that link dxcompiler_static directly, with no dlopen/LoadLibrary of
# dxcompiler.

set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_unittest(DxcStaticTests
  DxcStaticTest.cpp
  )
target_link_libraries(DxcStaticTests
  dxcompiler_static
  )
