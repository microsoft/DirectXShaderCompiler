# This script is run at configure time to install the latest WARP.
#
# NOTE: !! describe the environment variable that can be used to override which
# WARP is used during testing. !!

include_guard(GLOBAL)

find_program(NUGET_EXE nuget.exe HINTS ${CMAKE_BINARY_DIR}/nuget)
if(NOT NUGET_EXE)
    message(STATUS "nuget.exe not found in, download nuget.exe to ${CMAKE_BINARY_DIR}/nuget/nuget.exe...")
    file(DOWNLOAD 
        https://dist.nuget.org/win-x86-commandline/latest/nuget.exe
        ${CMAKE_BINARY_DIR}/nuget/nuget.exe
    )
    find_program(NUGET_EXE nuget.exe HINTS ${CMAKE_BINARY_DIR}/nuget)
    if(NOT NUGET_EXE)
        message(FATAL_ERROR "nuget.exe not found in ${CMAKE_BINARY_DIR}/nuget/nuget.exe")
    endif()
endif()

# Install the WARP nuget package.  The NUGET_WARP_EXTRA_ARGS cmake variable can
# be use to specify additional arguments to the nuget.exe command line.  For
# example, to install a specific version of WARP, set NUGET_WARP_EXTRA_ARGS to
# "-Version 1.0.13".
set(NUGET_WARP_EXTRA_ARGS "" CACHE STRING 
    "Extra arguments to pass to nuget.exe when installing Microsoft.Direct3D.WARP.")

# NUGET_WARP_EXTRA_ARGS gets passed as a single command-line argument. In cmake,
# lists are items separated by semicolons, so these will become separate
# arguments.
string(REPLACE " " ";" NUGET_WARP_EXTRA_ARGS_LIST ${NUGET_WARP_EXTRA_ARGS})

execute_process(
    COMMAND ${NUGET_EXE} install -ForceEnglishOutput Microsoft.Direct3D.WARP -OutputDirectory ${CMAKE_BINARY_DIR}/nuget ${NUGET_WARP_EXTRA_ARGS_LIST}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE nuget_output
    ERROR_VARIABLE nuget_output)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "nuget install Microsoft.Direct3D.WARP failed with exit code ${result}.\n${nuget_output}")

endif()

string(REGEX MATCH "Package \"(Microsoft.Direct3D.WARP..+)\" is already installed" IGNORED_OUTPUT ${nuget_output})
if(CMAKE_MATCH_1)
    set(WARP_PACKAGE ${CMAKE_MATCH_1})
else()
    string(REGEX MATCH "Added package '(Microsoft.Direct3D.WARP..+)' to folder" IGNORED_OUTPUT ${nuget_output})
    if (CMAKE_MATCH_1)
        set(WARP_PACKAGE ${CMAKE_MATCH_1})
    else()
        message(FATAL_ERROR "Failed to find package install named in nuget output:\n ${nuget_output}")
    endif()
endif()

set(WARP_DIR ${CMAKE_BINARY_DIR}/nuget/${WARP_PACKAGE})

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(WARP_ARCH "x64")
endif()
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "X86")
    set(WARP_ARCH "win32")
endif()
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "ARM64")
    set(WARP_ARCH "arm64")
endif()

set(WARP_DLL ${WARP_DIR}/build/native/bin/${WARP_ARCH}/d3d10warp.dll)

