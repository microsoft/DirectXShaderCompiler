# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  dxcsupport
  Support
  dxil
  dxilcontainer
  )


add_clang_library(dxil2spvlib
  dxil2spv.cpp
  )

target_link_libraries(dxil2spvlib
  dxcompiler
  dxclib
  clangSPIRV
  SPIRV-Tools
  )

llvm_map_components_to_libnames(llvm_libs support core irreader dxil)

target_link_libraries(dxil2spvlib ${llvm_libs})
