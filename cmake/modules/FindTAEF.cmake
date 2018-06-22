# Find the TAEF path that supports x86 and x64.
get_filename_component(WINDOWS_KIT_10_PATH "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE CACHE)
get_filename_component(WINDOWS_KIT_81_PATH "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot81]" ABSOLUTE CACHE)

# Find the TAEF path, it will typically look something like this.
# "C:\Program Files (x86)\Windows Kits\8.1\Testing\Development\inc"
set(pfx86 "programfiles(x86)")  # Work around behavior for environment names allows chars.
find_path(TAEF_INCLUDE_DIR      # Set variable TAEF_INCLUDE_DIR
          Wex.Common.h          # Find a path with Wex.Common.h
          HINTS "${CMAKE_SOURCE_DIR}/external/taef/build/Include"
          HINTS "${WINDOWS_KIT_10_PATH}/Testing/Development/inc"
          HINTS "${WINDOWS_KIT_81_PATH}/Testing/Development/inc"
          DOC "path to TAEF header files"
          HINTS
          )

if ("${DXC_BUILD_ARCH}" STREQUAL "x64" )
  find_library(TAEF_COMMON_LIBRARY NAMES Te.Common.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/x64
               HINTS ${TAEF_INCLUDE_DIR}/../lib/x64 )
  find_library(TAEF_WEX_COMMON_LIBRARY NAMES Wex.Common.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/x64
               HINTS ${TAEF_INCLUDE_DIR}/../lib/x64 )
  find_library(TAEF_WEX_LOGGER_LIBRARY NAMES Wex.Logger.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/x64
               HINTS ${TAEF_INCLUDE_DIR}/../lib/x64 )
elseif (CMAKE_GENERATOR MATCHES "Visual Studio.*ARM" OR "${DXC_BUILD_ARCH}" STREQUAL "ARM")
  find_library(TAEF_COMMON_LIBRARY NAMES Te.Common.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/arm
               HINTS ${TAEF_INCLUDE_DIR}/../lib/arm )
  find_library(TAEF_WEX_COMMON_LIBRARY NAMES Wex.Common.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/arm
               HINTS ${TAEF_INCLUDE_DIR}/../lib/arm )
  find_library(TAEF_WEX_LOGGER_LIBRARY NAMES Wex.Logger.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/arm
               HINTS ${TAEF_INCLUDE_DIR}/../lib/arm )
elseif (CMAKE_GENERATOR MATCHES "Visual Studio.*ARM64" OR "${DXC_BUILD_ARCH}" STREQUAL "ARM64")
  find_library(TAEF_COMMON_LIBRARY NAMES Te.Common.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/arm64
               HINTS ${TAEF_INCLUDE_DIR}/../lib/arm64 )
  find_library(TAEF_WEX_COMMON_LIBRARY NAMES Wex.Common.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/arm64
               HINTS ${TAEF_INCLUDE_DIR}/../lib/arm64 )
  find_library(TAEF_WEX_LOGGER_LIBRARY NAMES Wex.Logger.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/arm64
               HINTS ${TAEF_INCLUDE_DIR}/../lib/arm64 )
elseif ("${DXC_BUILD_ARCH}" STREQUAL "Win32" )
  find_library(TAEF_COMMON_LIBRARY NAMES Te.Common.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/x86
               HINTS ${TAEF_INCLUDE_DIR}/../lib/x86 )
  find_library(TAEF_WEX_COMMON_LIBRARY NAMES Wex.Common.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/x86
               HINTS ${TAEF_INCLUDE_DIR}/../lib/x86 )
  find_library(TAEF_WEX_LOGGER_LIBRARY NAMES Wex.Logger.lib
               HINTS ${TAEF_INCLUDE_DIR}/../Library/x86
               HINTS ${TAEF_INCLUDE_DIR}/../lib/x86 )
endif ("${DXC_BUILD_ARCH}" STREQUAL "x64" )

set(TAEF_LIBRARIES ${TAEF_COMMON_LIBRARY} ${TAEF_WEX_COMMON_LIBRARY} ${TAEF_WEX_LOGGER_LIBRARY})
set(TAEF_INCLUDE_DIRS ${TAEF_INCLUDE_DIR})

# Prefer the version that supports both x86 and x64, else prefer latest.
if(EXISTS "${CMAKE_SOURCE_DIR}/external/taef/build/Binaries/amd64/te.exe")
  set(TAEF_BIN_DIR "${CMAKE_SOURCE_DIR}/external/taef/build/Binaries")
elseif(EXISTS "${WINDOWS_KIT_10_PATH}/Testing/Runtimes/TAEF/x86/te.exe"
   AND EXISTS "${WINDOWS_KIT_10_PATH}/Testing/Runtimes/TAEF/x64/te.exe")
  set(TAEF_BIN_DIR "${WINDOWS_KIT_10_PATH}/Testing/Runtimes/TAEF")
elseif(EXISTS "${WINDOWS_KIT_81_PATH}/Testing/Runtimes/TAEF/x86/te.exe"
    AND EXISTS "${WINDOWS_KIT_81_PATH}/Testing/Runtimes/TAEF/x64/te.exe")
  set(TAEF_BIN_DIR "${WINDOWS_KIT_81_PATH}/Testing/Runtimes/TAEF")
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

