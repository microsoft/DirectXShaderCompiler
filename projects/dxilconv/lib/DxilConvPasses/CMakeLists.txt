# Build DxilConvPasses.lib.

add_dxilconv_project_library(DxilConvPasses
  NormalizeDxil.cpp
  ScopeNestedCFG.cpp
  InitializePasses.cpp
  ScopeNestInfo.cpp
  DxilCleanup.cpp
)

target_include_directories(DxilConvPasses
PUBLIC
  $<BUILD_INTERFACE:${DXILCONV_PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${DXILCONV_PROJECT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)


# Link against LLVMDXIL so that ScopeNestedCFG can use DxilUtil functions
target_link_libraries(DxilConvPasses
  PRIVATE LLVMDXIL
)

target_include_directories(DxilConvPasses
  PRIVATE
    ${DXC_SOURCE_DIR}/lib/DXIL  # adjust if DXIL is not in DXC_SOURCE_DIR/lib/DXIL
    ${DXC_SOURCE_DIR}/lib/IR  # adjust if DXIL is not in DXC_SOURCE_DIR/lib/DXIL
)
