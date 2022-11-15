# Test runner infrastructure for dxilconv. This configures the dxilconv test trees
# for use by Lit, and delegates to LLVM's lit test handlers.

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/taef/lit.site.cfg.in
  ${CMAKE_CURRENT_BINARY_DIR}/taef/lit.site.cfg
  )


# HLSL Change Begin - Add taef tests
add_lit_target("check-dxilconv" "Running lit suite dxilconv"
          ${CMAKE_CURRENT_SOURCE_DIR}/taef
          PARAMS
                 dxilconv_site_config=${CMAKE_CURRENT_BINARY_DIR}/taef/lit.site.cfg
                 taef_filter="/select:\"@Architecture='${DXC_BUILD_ARCH}'\""
          DEPENDS dxilconv-tests
          ARGS ${CLANG_TEST_EXTRA_ARGS}
        )
# HLSL Change End
