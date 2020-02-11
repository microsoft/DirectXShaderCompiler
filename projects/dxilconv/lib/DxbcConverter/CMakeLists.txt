# Build DxbcConverter.lib.

find_package(D3D12 REQUIRED)

add_dxilconv_project_library(DxbcConverter
  DxbcConverter.cpp
  DxbcUtil.cpp
)

add_dependencies(DxbcConverter intrinsics_gen DxcRuntimeEtw)

include_directories(
    ${D3D12_INCLUDE_DIRS}
)