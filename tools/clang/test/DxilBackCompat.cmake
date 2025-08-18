# HLSL: Tests for DXIL backward compatability
include(ExternalProject)
cmake_policy(SET CMP0135 NEW)

function(add_released_dxc name version)
    ExternalProject_Add(${name}
        URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/${version}/${name}.zip
        DOWNLOAD_DIR dxc_releases
        SOURCE_DIR dxc_releases/${version}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
    )
endfunction()

# https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.6.2112/dxc_2025_07_14.zip
# https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.6.2112/dxc_2025_05_24.zip
# https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2502/dxc_2025_02_20.zip
# https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2308/dxc_2023_08_14.zip
# https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.6.2112/dxc_2021_12_08.zip

add_released_dxc(dxc_2025_07_14 v1.8.2505.1)
add_released_dxc(dxc_2025_05_24 v1.8.2505)
add_released_dxc(dxc_2025_02_20 v1.8.2502)
add_released_dxc(dxc_2023_08_14 v1.7.2308)
add_released_dxc(dxc_2021_12_08 v1.6.2112)
