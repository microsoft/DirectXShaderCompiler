# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
# Builds dxil2spv.exe

add_clang_executable(dxil2spv
  dxil2spvmain.cpp
  )

add_subdirectory(lib)

target_link_libraries(dxil2spv
  clangSPIRV
  dxclib
  dxcompiler
  dxil2spvlib
  SPIRV-Tools
  )

set_target_properties(dxil2spv
  PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})

install(TARGETS dxil2spv
  RUNTIME DESTINATION bin)
