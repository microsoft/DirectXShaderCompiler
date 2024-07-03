if (WIN32)
add_clang_library(DxilHash
  DxilHash.cpp)
else()
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${WINTOOLS_SOURCE_ROOT}/include
  ${DXC_PROJECTS_SOURCE_DIR}/include
  ${DXC_PROJECTS_BINARY_DIR}/include
  ${DXILDLL_PROJECT_SOURCE_DIR}/include
  ${DXILDLL_PROJECT_BINARY_DIR}/include
)
add_clang_library(DxilHash
  DxilHash.cpp)
endif()