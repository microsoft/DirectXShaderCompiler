# Builds dxil.dll for Windows

set(SHARED_LIBRARY TRUE)

set(DXCComponents
  LLVMBitWriter
  LLVMDxcSupport
  LLVMDxilContainer
  LLVMDxilRootSignature
  LLVMHLSL
  LLVMMSSupport
  LLVMSupport
  LLVMTransformUtils
  LLVMAnalysis
  LLVMDxcBindingTable
  LLVMipa
)

foreach(C ${DXCComponents})
if (WIN32)
   list(APPEND DXCLibs ${C})
   list(APPEND DXCDependencies ${C})
else()
   list(APPEND DXCLibs ${C})
   list(APPEND DXCDependencies ${C})
endif()
endforeach(C ${DXCComponents})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${WINTOOLS_SOURCE_ROOT}/include
  ${DXC_PROJECTS_SOURCE_DIR}/include
  ${DXC_PROJECTS_BINARY_DIR}/include
  ${LLVM_BINARY_DIR}/projects/include
  ${LLVM_SOURCE_DIR}/tools/clang/tools/DxilHash
)

set(sources
  dxildll.cpp
  dxildll.def
  DxcHashingContainerBuilder.cpp
  dxcvalidatorp.cpp
)

if (WIN32)
add_clang_library(dxildll SHARED ${sources})
list(APPEND DXCLibs 
  kernel32.lib
  atls.lib
  advapi32.lib
  ole32.lib)
else()
add_clang_library(dxildll SHARED ${sources})
endif()

target_link_libraries(dxildll PRIVATE
  ${DXCLibs}
  DxilHash
)

if (WIN32)
add_dependencies(dxildll
    ${DXCDependencies}
    DxilHash
    DxcEtw
    DxcRuntimeEtw
)
else()
add_dependencies(dxildll
    ${DXCDependencies}
    DxilHash
)
endif()

if (WIN32)
    get_target_property(sources dxildll SOURCES)

    list(APPEND sources dxildll.rc)
    set_target_properties(${target_name} PROPERTIES SOURCES "${sources}")

    set_property(SOURCE dxildll.rc PROPERTY COMPILE_DEFINITIONS "INCLUDE_HLSL_VERSION_FILE=1")
    set_property(SOURCE dxildll.rc PROPERTY COMPILE_OPTIONS "/I" "${HLSL_VERSION_LOCATION}" "/I" "${LLVM_MAIN_SRC_DIR}")

else()
set_target_properties(dxildll PROPERTIES
    LINK_FLAGS "-z defs")
endif()

set_target_properties(dxildll
  PROPERTIES
  OUTPUT_NAME "dxil"
)
