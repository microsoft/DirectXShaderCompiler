# The macro choose_msvc_crt() takes a list of possible
# C runtimes to choose from, in the form of compiler flags,
# to present to the user. (MTd for /MTd, etc)
#
# The macro is invoked at the end of the file.
#
# To let the user override the MSVC runtime library for each build type:
# 1. Detect legacy CRT flags and reflect them in LLVM_USE_CRT_*.
# 2. Validate explicitly selected LLVM_USE_CRT_* values.
# 3. Translate them to CMake's MSVC runtime library abstraction.

### Helper macros: ###
macro(make_crt_regex regex crts)
  set(${regex} "")
  foreach(crt ${${crts}})
    # Trying to match the beginning or end of the string with stuff
    # like [ ^]+ didn't work, so use a bunch of parentheses instead.
    set(${regex} "${${regex}}|(^| +)/${crt}($| +)")
  endforeach(crt)
  string(REGEX REPLACE "^\\|" "" ${regex} "${${regex}}")
endmacro(make_crt_regex)

macro(get_current_crt crt_current regex flagsvar)
  # Find the selected-by-CMake CRT for each build type, if any.
  # Strip off the leading slash and any whitespace.
  string(REGEX MATCH "${${regex}}" ${crt_current} "${${flagsvar}}")
  string(REPLACE "/" " " ${crt_current} "${${crt_current}}")
  string(STRIP "${${crt_current}}" ${crt_current})
endmacro(get_current_crt)

macro(choose_msvc_crt MSVC_CRT)
  if(LLVM_USE_CRT)
    message(FATAL_ERROR
      "LLVM_USE_CRT is deprecated. Use the CMAKE_BUILD_TYPE-specific
variables (LLVM_USE_CRT_DEBUG, etc) instead.")
  endif()

  make_crt_regex(MSVC_CRT_REGEX ${MSVC_CRT})

  set(llvm_crt_build_types ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
  list(REMOVE_DUPLICATES llvm_crt_build_types)

  foreach(build_type ${llvm_crt_build_types})
    string(TOUPPER "${build_type}" build)
    if (NOT LLVM_USE_CRT_${build})
      get_current_crt(LLVM_USE_CRT_${build}
        MSVC_CRT_REGEX
        CMAKE_CXX_FLAGS_${build})
      set(LLVM_USE_CRT_${build}
        "${LLVM_USE_CRT_${build}}"
        CACHE STRING "Specify VC++ CRT to use for ${build_type} configurations."
        FORCE)
      set_property(CACHE LLVM_USE_CRT_${build}
        PROPERTY STRINGS ;${${MSVC_CRT}})
    endif(NOT LLVM_USE_CRT_${build})
  endforeach(build_type)

  set(llvm_crt_override_requested FALSE)
  foreach(build_type ${llvm_crt_build_types})
    string(TOUPPER "${build_type}" build)
    if (NOT "${LLVM_USE_CRT_${build}}" STREQUAL "")
      set(llvm_crt_override_requested TRUE)
      list(FIND ${MSVC_CRT} ${LLVM_USE_CRT_${build}} idx)
      if (idx LESS 0)
        message(FATAL_ERROR
          "Invalid value for LLVM_USE_CRT_${build}: ${LLVM_USE_CRT_${build}}. Valid options are one of: ${${MSVC_CRT}}")
      endif (idx LESS 0)
      message(STATUS "Using ${build_type} VC++ CRT: ${LLVM_USE_CRT_${build}}")
    endif()
  endforeach(build_type)

  if (llvm_crt_override_requested)
    set(cmake_msvc_runtime_library "")
    foreach(build_type ${llvm_crt_build_types})
      string(TOUPPER "${build_type}" build)
      set(crt "${LLVM_USE_CRT_${build}}")
      if ("${crt}" STREQUAL "")
        if ("${build}" STREQUAL "DEBUG")
          set(crt "MDd")
        else()
          set(crt "MD")
        endif()
      endif()

      if ("${crt}" STREQUAL "MD")
        set(runtime_library "MultiThreadedDLL")
      elseif ("${crt}" STREQUAL "MDd")
        set(runtime_library "MultiThreadedDebugDLL")
      elseif ("${crt}" STREQUAL "MT")
        set(runtime_library "MultiThreaded")
      elseif ("${crt}" STREQUAL "MTd")
        set(runtime_library "MultiThreadedDebug")
      endif()
      string(APPEND cmake_msvc_runtime_library
        "$<$<CONFIG:${build_type}>:${runtime_library}>")
    endforeach(build_type)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "${cmake_msvc_runtime_library}")
  endif()
endmacro(choose_msvc_crt MSVC_CRT)


# List of valid CRTs for MSVC
set(MSVC_CRT
  MD
  MDd
  MT
  MTd)

choose_msvc_crt(MSVC_CRT)
