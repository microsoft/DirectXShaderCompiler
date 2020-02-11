# Build DxilConvPasses.lib.

add_dxilconv_project_library(DxilConvPasses
  NormalizeDxil.cpp
  ScopeNestedCFG.cpp
  InitializePasses.cpp
  ScopeNestInfo.cpp
  DxilCleanup.cpp
)
