# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
# Builds dxil2spv.exe

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  dxcsupport
  Support    # just for assert and raw streams
  dxil
  dxilcontainer
  )

add_clang_executable(dxil2spv
  dxil2spvmain.cpp
  )

target_link_libraries(dxil2spv
  dxcompiler
  dxclib
  clangSPIRV
  SPIRV-Tools
  )

llvm_map_components_to_libnames(llvm_libs support core irreader dxil)

target_link_libraries(dxil2spv ${llvm_libs})

set_target_properties(dxil2spv
  PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})

install(TARGETS dxil2spv
  RUNTIME DESTINATION bin)
