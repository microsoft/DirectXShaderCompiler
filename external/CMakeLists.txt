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

  if (IS_DIRECTORY ${DXC_SPIRV_HEADERS_DIR})
    add_subdirectory(${DXC_SPIRV_HEADERS_DIR})
  endif()
  if (NOT DEFINED SPIRV-Headers_SOURCE_DIR)
    message(FATAL_ERROR "SPIRV-Headers was not found - required for SPIR-V codegen")
  else()
    set(SPIRV_HEADER_INCLUDE_DIR ${SPIRV-Headers_SOURCE_DIR}/include PARENT_SCOPE)
  endif()

  if (IS_DIRECTORY ${DXC_SPIRV_TOOLS_DIR})
    # We only need the library from SPIRV-Tools.
    set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building SPIRV-Tools executables")
    add_subdirectory(${DXC_SPIRV_TOOLS_DIR})
  endif()
  if (NOT TARGET SPIRV-Tools)
    message(FATAL_ERROR "SPIRV-Tools was not found - required for SPIR-V codegen")
  endif()

  set(SPIRV_DEP_TARGETS
    SPIRV-Tools
    SPIRV-Tools-opt
    spirv-tools-build-version
    spirv-tools-spv-amd-gcn-shader
    spirv-tools-vimsyntax
    install-headers
    SPIRV-Headers-example
    SPIRV-Headers-example-1.1
  )

  # Organize these targets better in Visual Studio
  foreach(target ${SPIRV_DEP_TARGETS})
    set_property(TARGET ${target} PROPERTY FOLDER "External dependencies")
  endforeach()
endif()
