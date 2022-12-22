set(LLVM_LINK_COMPONENTS
  Support
  dxcSupport
  )

add_clang_unittest(BasicTests
  CharInfoTest.cpp
  DiagnosticTest.cpp
  FileManagerTest.cpp
  SourceManagerTest.cpp
  VirtualFileSystemTest.cpp
  WinAdapterTest.cpp
  )

target_link_libraries(BasicTests
  clangBasic
  clangLex
  )
