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
  ${DXILDLL_PROJECT_SOURCE_DIR}/include
  ${DXILDLL_PROJECT_BINARY_DIR}/include
)

set(sources
  dxildll.cpp
  dxildll.def
  DxcSigningContainerBuilder.cpp
  dxcvalidatorp.cpp
)

if (WIN32)
add_dxildll_project_executable(dxildll ${sources})
list(APPEND DXCLibs 
  kernel32.lib
  atls.lib
  advapi32.lib
  ole32.lib)
else()
add_llvm_library(dxildll SHARED ${sources})
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
#wintools_set_resource_file(dxildll dxildll.rc)
else()
set_target_properties(dxildll PROPERTIES
    LINK_FLAGS "-z defs")
endif()

set_target_properties(dxildll
  PROPERTIES
  OUTPUT_NAME "dxil"
)
