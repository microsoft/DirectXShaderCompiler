# Define root location for all external dependencies
set(DXC_EXTERNAL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    CACHE STRING "Root location of all external projects")

# We need to match this setting across everything we link together
if (NOT HLSL_ENABLE_DEBUG_ITERATORS)
  add_definitions(/D_ITERATOR_DEBUG_LEVEL=0)
endif (NOT HLSL_ENABLE_DEBUG_ITERATORS)

# Need DirectX-Headers module if not on windows
if (NOT DIRECTX_HEADER_INCLUDE_DIR)
  if (NOT WIN32)
    if (IS_DIRECTORY "${DXC_EXTERNAL_ROOT_DIR}/DirectX-Headers")
      set(DIRECTX_HEADER_INCLUDE_DIR ${DXC_EXTERNAL_ROOT_DIR}/DirectX-Headers/include PARENT_SCOPE)
    else()
      message(FATAL_ERROR "DirectX-Headers was not found - required for reflection support on *nix see https://github.com/microsoft/DirectX-Headers")
    endif()
  endif (NOT WIN32)
endif(NOT DIRECTX_HEADER_INCLUDE_DIR)

# Enabling SPIR-V codegen requires SPIRV-Headers for spirv.hpp and
# SPIRV-Tools for SPIR-V disassembling functionality.
if (${ENABLE_SPIRV_CODEGEN})
  set(DXC_SPIRV_HEADERS_DIR "${DXC_EXTERNAL_ROOT_DIR}/SPIRV-Headers"
      CACHE STRING "Location of SPIRV-Headers source")
  set(DXC_SPIRV_TOOLS_DIR "${DXC_EXTERNAL_ROOT_DIR}/SPIRV-Tools"
      CACHE STRING "Location of SPIRV-Tools source")

  if (NOT DEFINED SPIRV-Headers_SOURCE_DIR)
    if (IS_DIRECTORY ${DXC_SPIRV_HEADERS_DIR})
      add_subdirectory(${DXC_SPIRV_HEADERS_DIR}
          "${CMAKE_BINARY_DIR}/external/SPIRV-Headers"
          EXCLUDE_FROM_ALL)
    endif()
  endif()
  if (NOT DEFINED SPIRV-Headers_SOURCE_DIR)
    message(FATAL_ERROR "SPIRV-Headers was not found - required for SPIR-V codegen")
  else()
    set(SPIRV_HEADER_INCLUDE_DIR ${SPIRV-Headers_SOURCE_DIR}/include PARENT_SCOPE)
  endif()

  if (NOT TARGET SPIRV-Tools)
    if (IS_DIRECTORY ${DXC_SPIRV_TOOLS_DIR})
      # Avoid implicit fallthrough warning from clang
      # This add_compile_options() will only affect the current directory and its subdirectories.
      if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-implicit-fallthrough)
      endif()
      # We only need the library from SPIRV-Tools.
      set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building SPIRV-Tools executables")
      if (NOT HLSL_ENABLE_DEBUG_ITERATORS)
        set(SPIRV_TOOLS_EXTRA_DEFINITIONS /D_ITERATOR_DEBUG_LEVEL=0)
      endif()
      add_subdirectory(${DXC_SPIRV_TOOLS_DIR}
          "${CMAKE_BINARY_DIR}/external/SPIRV-Tools"
          EXCLUDE_FROM_ALL)
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

endif()
