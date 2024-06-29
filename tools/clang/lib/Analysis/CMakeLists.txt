set(LLVM_LINK_COMPONENTS
  Support
  )

# HLSL Change - add ignored sources
set(HLSL_IGNORE_SOURCES
  )

add_clang_library(clangAnalysis

  LINK_LIBS
  clangAST
  clangBasic
  clangLex
  )
