set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangSPIRV
  EmitSPIRVAction.cpp
  SPIRVBuilder.cpp
  String.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangFrontend
  clangLex
  )
