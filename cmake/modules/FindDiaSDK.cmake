# Find the DIA SDK path.
get_filename_component(VS_PATH32 "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\14.0;InstallDir]" ABSOLUTE CACHE)
get_filename_component(VS_PATH64 "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\14.0;InstallDir]" ABSOLUTE CACHE)
# VS_PATH32 will be something like C:/Program Files (x86)/Microsoft Visual Studio 14.0/Common7/IDE

# Also look for in vs15 or vs16 install.
set(PROGRAMFILES_X86 "ProgramFiles(x86)")
set(VS_PATH32 "$ENV{${PROGRAMFILES_X86}}/Microsoft Visual Studio")
get_filename_component(VS16_C_PATH32 "${VS_PATH32}/2019/Community/Common7/IDE" ABSOLUTE CACHE)
get_filename_component(VS16_P_PATH32 "${VS_PATH32}/2019/Professional/Common7/IDE" ABSOLUTE CACHE)
get_filename_component(VS16_E_PATH32 "${VS_PATH32}/2019/Enterprise/Common7/IDE" ABSOLUTE CACHE)
get_filename_component(VS15_C_PATH32 "${VS_PATH32}/2017/Community/Common7/IDE" ABSOLUTE CACHE)
get_filename_component(VS15_P_PATH32 "${VS_PATH32}/2017/Professional/Common7/IDE" ABSOLUTE CACHE)
get_filename_component(VS15_E_PATH32 "${VS_PATH32}/2017/Enterprise/Common7/IDE" ABSOLUTE CACHE)

# Find the DIA SDK path, it will typically look something like this.
# C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\DIA SDK\include
# C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\DIA SDK\include
# C:\Program Files (x86)\Microsoft Visual Studio 14.0\DIA SDK\include
find_path(DIASDK_INCLUDE_DIR    # Set variable DIASDK_INCLUDE_DIR
          dia2.h                # Find a path with dia2.h
          HINTS "${VS16_C_PATH32}/../../DIA SDK/include" 
          HINTS "${VS16_P_PATH32}/../../DIA SDK/include"
          HINTS "${VS16_E_PATH32}/../../DIA SDK/include"
          HINTS "${VS15_C_PATH32}/../../DIA SDK/include" 
          HINTS "${VS15_P_PATH32}/../../DIA SDK/include"
          HINTS "${VS15_E_PATH32}/../../DIA SDK/include"
          HINTS "${VS_PATH64}/../../DIA SDK/include"
          HINTS "${VS_PATH32}/../../DIA SDK/include"
          DOC "path to DIA SDK header files"
          HINTS
          )

if (CMAKE_GENERATOR MATCHES "Visual Studio.*Win64" )
  find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib
               HINTS ${DIASDK_INCLUDE_DIR}/../lib/amd64 )
elseif (CMAKE_GENERATOR MATCHES "Visual Studio.*ARM" )
  find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib
               HINTS ${DIASDK_INCLUDE_DIR}/../lib/arm )
else (CMAKE_GENERATOR MATCHES "Visual Studio.*Win64" )
  find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib
               HINTS ${DIASDK_INCLUDE_DIR}/../lib )
endif (CMAKE_GENERATOR MATCHES "Visual Studio.*Win64" )

set(DIASDK_LIBRARIES ${DIASDK_GUIDS_LIBRARY})
set(DIASDK_INCLUDE_DIRS ${DIASDK_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set DIASDK_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(DIASDK  DEFAULT_MSG
                                  DIASDK_LIBRARIES DIASDK_INCLUDE_DIR)

mark_as_advanced(DIASDK_INCLUDE_DIRS DIASDK_LIBRARIES)