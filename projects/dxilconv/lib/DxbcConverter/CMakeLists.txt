# Build DxbcConverter.lib.

add_dxilconv_project_library(DxbcConverter
  DxbcConverter.cpp
  DxbcUtil.cpp
)

add_dependencies(DxbcConverter intrinsics_gen DxcRuntimeEtw)