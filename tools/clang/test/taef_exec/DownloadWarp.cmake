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

execute_process(
    COMMAND ${NUGET_EXE} install -ForceEnglishOutput Microsoft.Direct3D.WARP -OutputDirectory ${CMAKE_BINARY_DIR}/nuget ${NUGET_WARP_EXTRA_ARGS}
    COMMAND_ERROR_IS_FATAL ANY
    RESULT_VARIABLE result
    OUTPUT_VARIABLE nuget_output
    ERROR_VARIABLE nuget_output)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "nuget install Microsoft.Direct3D.WARP failed with exit code ${result}.")
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





# # Download the latest nuget package for the given ID. Can pass in a custom source, defaults to nuget public feed.
# function(GetNuGetPackageLatestVersion)
#     set(params NAME ID SOURCE OUTPUT_DIR OUTPUT_VARIABLE PREVIEW)
#     cmake_parse_arguments(PARSE_ARGV 0 ARG "" "${params}" "")

#     if(NOT ARG_OUTPUT_DIR)
#         set(ARG_OUTPUT_DIR )
#     endif()

#     set(nuget_exe_path "${ARG_OUTPUT_DIR}\\nuget.exe install")
#     EnsureNugetExists(${nuget_exe_path})

#     if (${ARG_ID}_LATEST_VERSION)
#         set(${ARG_OUTPUT_VARIABLE} ${${ARG_ID}_LATEST_VERSION} PARENT_SCOPE)
#     else()
#         if(NOT ARG_SOURCE)
#             set(ARG_SOURCE https://api.nuget.org/v3/index.json)
#         endif()

#         if(NOT ARG_PREVIEW)
#             set(ARG_PREVIEW OFF)
#         endif()

#         if(ARG_PREVIEW)
#             # Note that '-Prerelease' options will only return a prerelease package if that is also the latest.
#             # If you want a prerelease package with an older version number than the latest release package then you
#             # need to pass a specific version number.
#             message("Will add '-Prelease' to nuget list command")
#             set(prerelease "-Prerelease")
#         endif()

#         execute_process(
#             COMMAND ${nuget_exe_path} 
#             list ${ARG_ID}
#             -Source ${ARG_SOURCE}
#             ${prerelease}
#             RESULT_VARIABLE result
#             OUTPUT_VARIABLE nuget_list_output
#             OUTPUT_STRIP_TRAILING_WHITESPACE
#         )

#         if(NOT ${result} STREQUAL "0")
#             message(FATAL_ERROR "NuGet failed to find latest version of package ${ARG_ID} with exit code ${result}.")
#         endif()

#         # Get last line of running nuget.exe list <ID>.
#         string(REPLACE "\n" ";" nuget_list_output ${nuget_list_output})
#         list(POP_BACK nuget_list_output nuget_list_last_line)
#         if(nuget_list_last_line STREQUAL "No packages found.")
#             message(FATAL_ERROR "NuGet failed to find latest version of package ${ARG_ID}.")
#         endif()

#         # The last line should have the format <ID> <VERSION>
#         string(REPLACE " " ";" nuget_list_last_line ${nuget_list_last_line})
#         list(POP_BACK nuget_list_last_line nuget_version)

#         if(NOT nuget_version)
#             message(FATAL_ERROR "NuGet failed to find latest version of package ${ARG_ID}.")
#         endif()

#         message("Nuget found version:${nuget_version} for ${ARG_ID}")

#         # Save output variable and cache the result so subsequent calls to the version-unspecified package 
#         # are faster.
#         set(${ARG_OUTPUT_VARIABLE} ${nuget_version} PARENT_SCOPE)
#         set(${ARG_ID}_LATEST_VERSION ${nuget_version} CACHE INTERNAL "")
#     endif()
# endfunction()

# # Installs a NuGet package under OUTPUT_DIR.
# #
# # FetchNuGetPackage(
# #     ID Microsoft.Direct3D.WARP
# #     VERSION 1.0.13
# #     SOURCE https://api.nuget.org/v3/index.json
# # )
# #
# # This function sets a variable <name>_SOURCE_DIR (e.g. Microsoft.Direct3D.WARP_SOURCE_DIR in above example) to the 
# # extract NuGet package contents.
# function(FetchNuGetPackage)
#     set(params NAME ID VERSION SOURCE OUTPUT_DIR RELEASE_TYPE)
#     cmake_parse_arguments(PARSE_ARGV 0 ARG "" "${params}" "")

#     # The NAME parameter is optional: if it's not set then the package ID is used as the name. The 
#     # reason for having a separate NAME is to allow a consistent identifier for packages whose ID
#     # changes with each release (e.g. GDK).
#     if(NOT ARG_NAME)
#         set(ARG_NAME ${ARG_ID})
#     endif()

#     if(NOT ARG_OUTPUT_DIR)
#         set(ARG_OUTPUT_DIR ${BINARY_DIR}/temp)
#     endif()
    
#     set(nuget_exe_path ${ARG_OUTPUT_DIR}/nuget.exe)

#     if(NOT ARG_SOURCE)
#         set(ARG_SOURCE https://api.nuget.org/v3/index.json)
#     endif()

#     if(NOT ARG_RELEASE_TYPE)
#         set(ARG_RELEASE_TYPE "LATEST_RELEASE")
#     endif()

#     set(PREVIEW OFF)
    
#     if(${ARG_RELEASE_TYPE} STREQUAL "LATEST_PREVIEW")
#         set(PREVIEW ON)
#     endif()

#     # Default to latest version
#     if(NOT ARG_VERSION)
#         GetNuGetPackageLatestVersion(
#             ID ${ARG_ID} 
#             SOURCE ${ARG_SOURCE} 
#             PREVIEW ${PREVIEW}
#             OUTPUT_DIR ${ARG_OUTPUT_DIR} 
#             OUTPUT_VARIABLE ARG_VERSION
#         )
#     endif()

#     set(nupkg_path ${ARG_OUTPUT_DIR}/${ARG_ID}.${ARG_VERSION}/${ARG_ID}.${ARG_VERSION}.nupkg)

#     if(NOT EXISTS ${nupkg_path})
#         message(STATUS "NuGet: adding package ${ARG_ID}.${ARG_VERSION}")

#         EnsureNugetExists(${nuget_exe_path})

#         set(retry_count 0)
#         set(max_retries 10)
#         set(retry_delay 10)
#         set(result 1)

#         # Run NuGet CLI to download the package.
#         while(NOT ${result} STREQUAL "0" AND ${retry_count} LESS ${max_retries})
#             message(STATUS "'${nuget_exe_path}' install '${ARG_ID}' -Version '${ARG_VERSION}' -Source '${ARG_SOURCE}' -OutputDirectory '${ARG_OUTPUT_DIR}' -DependencyVersion Ignore -Verbosity quiet")
#             execute_process(
#                 COMMAND 
#                 ${nuget_exe_path} 
#                 install ${ARG_ID}
#                 -Version ${ARG_VERSION}
#                 -Source ${ARG_SOURCE}
#                 -OutputDirectory ${ARG_OUTPUT_DIR}
#                 -DependencyVersion Ignore
#                 -Verbosity quiet
#                 RESULT_VARIABLE result
#             )
#             if(NOT ${result} STREQUAL "0")
#                 math(EXPR retry_count "${retry_count} + 1")

#                 message(STATUS "Nuget failed: '${result}'. Retrying in ${retry_delay} seconds...")
#                 execute_process(
#                     COMMAND 
#                     ${CMAKE_COMMAND} -E sleep ${retry_delay}
#                 )
#             endif()
#         endwhile()

#         if(NOT ${result} STREQUAL "0")
#             message(FATAL_ERROR "NuGet failed: '${result}' Package  '${ARG_NAME}' (${ARG_ID}.${ARG_VERSION})")
#         endif()
#     endif()

#     # Set output variable. The NAME parameter is optional: if it's not set then the package ID is used as the
#     # name. The reason for having a separate NAME is for packages whose IDs change (e.g. GDK) so that callers
#     # can use a fixed name in dependents. Example, targets can reference gdk_SOURCE_DIR with the snippet below
#     # instead of having to reference Microsoft.GDK.PC.230300_SOURCE_DIR.
#     #
#     # FetchNuGetPackage(
#     #     NAME gdk
#     #     ID Microsoft.GDK.PC.220300
#     #     VERSION 10.0.22621.3049
#     # )
#     set(${ARG_NAME}_SOURCE_DIR ${ARG_OUTPUT_DIR}/${ARG_ID}.${ARG_VERSION} PARENT_SCOPE)
# endfunction()

# # Begin the 'main' logic of this file. Previous code is all defintions.
# message("USE_WARP_FROM_NUGET: ${USE_WARP_FROM_NUGET}")
# if(${USE_WARP_FROM_NUGET} STREQUAL "LATEST_RELEASE" OR ${USE_WARP_FROM_NUGET} STREQUAL "LATEST_PREVIEW")

#   message("Fetching warp from nuget")

#   FetchNuGetPackage(ID Microsoft.Direct3D.WARP OUTPUT_DIR ${BINARY_DIR}/temp RELEASE_TYPE ${USE_WARP_FROM_NUGET})

#   if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
#     set(ARCH "x64")
#   endif()
#   if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "X86")
#     set(ARCH "win32")
#   endif()
#   if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "ARM64")
#     set(ARCH "arm64")
#   endif()

#   set(WARP_SOURCE_PATH "${Microsoft.Direct3D.WARP_SOURCE_DIR}/build/native/bin/${ARCH}")
#   set(WARP_DEST_PATH "${BINARY_DIR}/${BUILD_TYPE}/bin/")
#   message("Copying d3d10warp.dll and d3d10warp.pdb \n"
#            "  from:  ${WARP_SOURCE_PATH}\n"
#            "  to: ${WARP_DEST_PATH}")
#   file(COPY "${WARP_SOURCE_PATH}/d3d10warp.dll" 
#        DESTINATION "${WARP_DEST_PATH}")
#   file(COPY "${WARP_SOURCE_PATH}/d3d10warp.pdb" 
#        DESTINATION "${WARP_DEST_PATH}")
# endif()
