cmake_minimum_required(VERSION 3.30)
project(DXCSmoke
    DESCRIPTION "DevSH Units Tests"
    LANGUAGES NONE
)

include(CTest)
enable_testing()

find_program(DXC_EXE dxc HINTS 
    "${CMAKE_CURRENT_SOURCE_DIR}/install/bin" 
    "$ENV{DXC_PATH}" 
    "${DXC_PATH}"
REQUIRED)

file(GLOB DXC_SMOKE_HLSL "${CMAKE_CURRENT_SOURCE_DIR}/in/*.hlsl")
set(DXC_SMOKE_SPIRV_DIR "${CMAKE_CURRENT_BINARY_DIR}/spirv")
file(MAKE_DIRECTORY "${DXC_SMOKE_SPIRV_DIR}")

set(OPTS
    -HV 202x
    -Wno-c++14-extensions
    -Wno-gnu-static-float-init
    -Wno-c++1z-extensions
    -Wno-c++11-extensions
    -fvk-use-scalar-layout
    -enable-16bit-types
    -Zpr
    -spirv
    -fspv-target-env=vulkan1.3
    -Wno-local-type-template-args
    -O3
    -T lib_6_8
)

foreach(in IN LISTS DXC_SMOKE_HLSL)
    get_filename_component(name "${in}" NAME_WE)
    set(out "${DXC_SMOKE_SPIRV_DIR}/${name}.spv")
    add_test(NAME dxc_smoke_${name}
        COMMAND "${DXC_EXE}" -Fc "${out}" ${OPTS} "${in}"
    )
endforeach()