# Test runner infrastructure for HLSL headers. This configures the HLSL header test trees
# for use by Lit, and delegates to LLVM's lit test handlers.

if (CMAKE_CFG_INTDIR STREQUAL ".")
  set(LLVM_BUILD_MODE ".")
else ()
  set(LLVM_BUILD_MODE "%(build_mode)s")
endif ()

string(REPLACE ${CMAKE_CFG_INTDIR} ${LLVM_BUILD_MODE} CLANG_TOOLS_DIR ${LLVM_RUNTIME_OUTPUT_INTDIR})

set(CLANG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  )

# Explicitly set dependencies for HLSL
set(CLANG_TEST_DEPS dxc dxa dxopt dxl dxv dxr dxcompiler clang-tblgen llvm-config opt FileCheck count not ClangUnitTests)

# if (WIN32)
# list(APPEND CLANG_TEST_DEPS
#      dxc_batch ExecHLSLTests
#     )
# endif()

add_lit_testsuite(check-hlsl-headers "Running the HLSL headers regression tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  #LIT ${LLVM_LIT}
  PARAMS ${CLANG_TEST_PARAMS}
    skip_taef_exec=False
  DEPENDS ${CLANG_TEST_DEPS}
  ARGS ${CLANG_TEST_EXTRA_ARGS}
  )
set_target_properties(check-hlsl-headers PROPERTIES FOLDER "HLSL headers tests")
