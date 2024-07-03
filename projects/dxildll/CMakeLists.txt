set(DXILDLL_PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(DXILDLL_PROJECT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

macro(add_dxildll_project_library name)
  add_llvm_library(${name} ${ARGN})
  set_output_directory(${name} ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})  
  set_target_properties(${name} PROPERTIES FOLDER "Dxildll libraries")
endmacro(add_dxildll_project_library)

macro(add_dxildll_project_executable name)
  add_llvm_library(${name} SHARED ${ARGN})
  set_target_properties(${name} PROPERTIES FOLDER "Dxildll executables")
endmacro(add_dxildll_project_executable)

macro(add_dxildll_project_test_library name)
  add_dxildll_project_library(${name} ${ARGN})
  set_target_properties(${name} PROPERTIES FOLDER "Dxildll tests")
endmacro(add_dxildll_project_test_library)


#if(WIN32)
  add_subdirectory(lib)
  add_subdirectory(tools)
  #if (HLSL_INCLUDE_TESTS)
  #  add_subdirectory(unittests)
  #  add_subdirectory(test)
  #endif (HLSL_INCLUDE_TESTS)
#endif()