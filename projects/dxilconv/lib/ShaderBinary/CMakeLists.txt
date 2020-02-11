# Build ShaderBinary.lib.

find_package(D3D12 REQUIRED)

add_dxilconv_project_library(ShaderBinary
  ShaderBinary.cpp
)

include_directories(
    ${D3D12_INCLUDE_DIRS}
)
