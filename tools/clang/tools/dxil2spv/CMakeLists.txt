# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
# Builds dxil2spv.exe

add_clang_executable(dxil2spv
  dxil2spvmain.cpp
  )

set_target_properties(dxil2spv
  PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})

install(TARGETS dxil2spv
  RUNTIME DESTINATION bin)
