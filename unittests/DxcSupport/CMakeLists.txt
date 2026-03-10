set(LLVM_LINK_COMPONENTS
  Support
  dxcSupport
  )

add_clang_unittest(DxcSupportTests
  ExitRemovalTest.cpp
  WinAdapterTest.cpp
  )
