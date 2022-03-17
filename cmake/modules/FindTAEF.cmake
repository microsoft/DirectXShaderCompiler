# Find the TAEF path that supports x86 and x64.
get_filename_component(WINDOWS_KIT_10_PATH "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE CACHE)
get_filename_component(WINDOWS_KIT_81_PATH "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot81]" ABSOLUTE CACHE)

# Find the TAEF path, it will typically look something like this.
# "C:\Program Files (x86)\Windows Kits\8.1\Testing\Development\inc"
set(pfx86 "programfiles(x86)")  # Work around behavior for environment names allows chars.
find_path(TAEF_INCLUDE_DIR      # Set variable TAEF_INCLUDE_DIR
          Wex.Common.h          # Find a path with Wex.Common.h
          HINTS "$ENV{TAEF_PATH}/../../../Include"
          HINTS "$ENV{TAEF_PATH}/../../../Development/inc"
          HINTS "${CMAKE_SOURCE_DIR}/external/taef/build/Include"
          HINTS "${WINDOWS_KIT_10_PATH}/Testing/Development/inc"
          HINTS "${WINDOWS_KIT_81_PATH}/Testing/Development/inc"
          DOC "path to TAEF header files"
          HINTS
          )

macro(find_taef_libraries targetplatform)
  set(TAEF_LIBRARIES)
  foreach(L Te.Common.lib Wex.Common.lib Wex.Logger.lib)
    find_library(TAEF_LIB_${L} NAMES ${L}
                HINTS ${TAEF_INCLUDE_DIR}/../Library/${targetplatform}
                HINTS ${TAEF_INCLUDE_DIR}/../lib/${targetplatform})
    set(TAEF_LIBRARIES ${TAEF_LIBRARIES} ${TAEF_LIB_${L}})
  endforeach()
  set(TAEF_COMMON_LIBRARY ${TAEF_LIB_Te.Common.lib})
endmacro(find_taef_libraries)

if ("${DXC_BUILD_ARCH}" STREQUAL "x64" )
  find_taef_libraries(x64)
elseif (CMAKE_GENERATOR MATCHES "Visual Studio.*ARM" OR "${DXC_BUILD_ARCH}" STREQUAL "ARM")
  find_taef_libraries(arm)
elseif (CMAKE_GENERATOR MATCHES "Visual Studio.*ARM64" OR "${DXC_BUILD_ARCH}" MATCHES "ARM64.*")
  find_taef_libraries(arm64)
elseif ("${DXC_BUILD_ARCH}" STREQUAL "Win32" )
  find_taef_libraries(x86)
endif ("${DXC_BUILD_ARCH}" STREQUAL "x64" )

set(TAEF_INCLUDE_DIRS ${TAEF_INCLUDE_DIR})

# Get TAEF binaries path from the header location
set(TAEF_NUGET_BIN ${TAEF_INCLUDE_DIR}/../Binaries/Release)
set(TAEF_SDK_BIN ${TAEF_INCLUDE_DIR}/../../Runtimes/TAEF)

if(EXISTS "$ENV{HLSL_TAEF_DIR}/x64/te.exe" OR EXISTS "$ENV{HLSL_TAEF_DIR}/x86/te.exe")
  # Use HLSL_TAEF_DIR for debug executable setting if set.
  # we don't actually support multiple architectures in the same project.
  set(TAEF_BIN_DIR "$ENV{HLSL_TAEF_DIR}")
elseif(EXISTS "${TAEF_NUGET_BIN}/x64/te.exe" AND EXISTS "${TAEF_NUGET_BIN}/x86/te.exe")
  set(TAEF_BIN_DIR "${TAEF_NUGET_BIN}")
elseif(EXISTS "${TAEF_SDK_BIN}/x64/te.exe" AND EXISTS "${TAEF_SDK_BIN}/x86/te.exe")
  set(TAEF_BIN_DIR "${TAEF_SDK_BIN}")
elseif(EXISTS "${WINDOWS_KIT_10_PATH}")
  message(ERROR "Unable to find TAEF binaries under Windows 10 SDK.")
elseif(EXISTS "${WINDOWS_KIT_81_PATH}")
  message(ERROR "Unable to find TAEF binaries under Windows 8.1 or 10 SDK.")
else()
  message(ERROR "Unable to find TAEF binaries or Windows 8.1 or 10 SDK.")
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set TAEF_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(TAEF  DEFAULT_MSG
                                  TAEF_COMMON_LIBRARY TAEF_INCLUDE_DIR)

mark_as_advanced(TAEF_INCLUDE_DIR TAEF_LIBRARY)
