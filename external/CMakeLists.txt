# Define root location for all external dependencies
set(DXC_EXTERNAL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    CACHE STRING "Root location of all external projects")

# Enabling SPIR-V codegen requires SPIRV-Headers for spirv.hpp and
# SPIRV-Tools for SPIR-V disassembling functionality.
if (${ENABLE_SPIRV_CODEGEN})
  set(DXC_SPIRV_HEADERS_DIR "${DXC_EXTERNAL_ROOT_DIR}/SPIRV-Headers"
      CACHE STRING "Location of SPIRV-Headers source")
  set(DXC_SPIRV_TOOLS_DIR "${DXC_EXTERNAL_ROOT_DIR}/SPIRV-Tools"
      CACHE STRING "Location of SPIRV-Tools source")

  if (NOT DEFINED SPIRV-Headers_SOURCE_DIR)
    if (IS_DIRECTORY ${DXC_SPIRV_HEADERS_DIR})
      add_subdirectory(${DXC_SPIRV_HEADERS_DIR} EXCLUDE_FROM_ALL)
    endif()
  endif()
  if (NOT DEFINED SPIRV-Headers_SOURCE_DIR)
    message(FATAL_ERROR "SPIRV-Headers was not found - required for SPIR-V codegen")
  else()
    set(SPIRV_HEADER_INCLUDE_DIR ${SPIRV-Headers_SOURCE_DIR}/include PARENT_SCOPE)
  endif()

  if (NOT TARGET SPIRV-Tools)
    if (IS_DIRECTORY ${DXC_SPIRV_TOOLS_DIR})
      # We only need the library from SPIRV-Tools.
      set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building SPIRV-Tools executables")
      if (NOT HLSL_ENABLE_DEBUG_ITERATORS)
        set(SPIRV_TOOLS_EXTRA_DEFINITIONS /D_ITERATOR_DEBUG_LEVEL=0)
      endif()
      add_subdirectory(${DXC_SPIRV_TOOLS_DIR} EXCLUDE_FROM_ALL)
    endif()
  endif()
  if (NOT TARGET SPIRV-Tools)
    message(FATAL_ERROR "SPIRV-Tools was not found - required for SPIR-V codegen")
  else()
    set(SPIRV_TOOLS_INCLUDE_DIR ${spirv-tools_SOURCE_DIR}/include PARENT_SCOPE)
  endif()

  set(SPIRV_DEP_TARGETS
    SPIRV-Tools-static
    SPIRV-Tools-opt
  )

  # Organize these targets better in Visual Studio
  foreach(target ${SPIRV_DEP_TARGETS})
    set_property(TARGET ${target} PROPERTY FOLDER "External dependencies")
  endforeach()

  # We need GoogleTest for unit and SPIR-V codegen testing.
  if (${SPIRV_BUILD_TESTS})
    set(DXC_GTEST_DIR "${DXC_EXTERNAL_ROOT_DIR}/googletest"
        CACHE STRING "Location of GoogleTest source")
    if (NOT TARGET gtest)
      if (IS_DIRECTORY ${DXC_GTEST_DIR})
        # Configure googletest
        include(GTestConfig.cmake)
      endif()
      if (NOT TARGET gtest)
        message(FATAL_ERROR "GoogleTest was not found - required for SPIR-V codegen")
      endif()
    endif()
  endif()

  # We need Effcee for SPIR-V codegen testing.
  if (${SPIRV_BUILD_TESTS})
    # re2 is needed since it's required by effcee.
    set(DXC_RE2_DIR "${DXC_EXTERNAL_ROOT_DIR}/re2" CACHE STRING "Location of RE2 source")
    set(DXC_EFFCEE_DIR "${DXC_EXTERNAL_ROOT_DIR}/effcee" CACHE STRING "Location of Effcee source")

    if (NOT TARGET re2)
      if (IS_DIRECTORY ${DXC_RE2_DIR})
        # Avoid exception handling warning from MSVC.
        # This add_compile_options() will only affect the current directory and its subdirectories.
        if (WIN32)
          add_compile_options(/EHs)
        else(WIN32)
          # Disable all warnings in subproject RE2
          add_compile_options(-w)
        endif(WIN32)
        # Don't build/run re2's tests.
        set(RE2_BUILD_TESTING OFF CACHE BOOL "Skip RE2 tests")
        add_subdirectory(${DXC_RE2_DIR} EXCLUDE_FROM_ALL)
      endif()
    endif()

    if (NOT TARGET effcee)
      if (IS_DIRECTORY ${DXC_EFFCEE_DIR})
        # Configure effcee
        set(EFFCEE_RE2_DIR "${DXC_RE2_DIR}" CACHE STRING "Location of RE2 for effcee")
        set(EFFCEE_ENABLE_SHARED_CRT ON CACHE BOOL "Enable using shared C Runtime")
        # Don't build/run effcee's tests.
        set(EFFCEE_BUILD_TESTING OFF CACHE BOOL "Skip effcee tests")
        set(EFFCEE_BUILD_SAMPLES OFF CACHE BOOL "Skip building effcee examples")
        add_subdirectory(${DXC_EFFCEE_DIR} EXCLUDE_FROM_ALL)
      endif()
    endif()

    # Organize these targets better in Visual Studio
    set(SPIRV_TEST_DEP_TARGETS
      effcee
      re2
    )
    foreach(target ${SPIRV_TEST_DEP_TARGETS})
      set_property(TARGET ${target} PROPERTY FOLDER "External dependencies")
    endforeach()

  endif()

endif()
