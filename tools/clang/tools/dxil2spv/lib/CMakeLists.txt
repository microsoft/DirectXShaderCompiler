# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.

set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  core
  dxcsupport
  dxil
  dxilcontainer
  irreader
  support
  )

add_clang_library(dxil2spvlib
  dxil2spv.cpp
  )

target_link_libraries(dxil2spvlib PRIVATE
  clangBasic
  clangCodeGen
  clangSPIRV
  dxclib
  dxcompiler
  LLVMHLSL
  LLVMipo
  SPIRV-Tools
  )