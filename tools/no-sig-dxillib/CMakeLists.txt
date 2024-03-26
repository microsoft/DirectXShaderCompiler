# Builds a testing version of dxil.dll

set(SHARED_LIBRARY TRUE)

set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  BitWriter
  DxcSupport
  DxilContainer
  DxilRootSignature
  HLSL
  MSSupport
  Support
  TransformUtils
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LLVM_BINARY_DIR}/projects/include
)

set(SOURCES
  NoSigDxilLib.cpp
  NoSigDxilLib.def
  NoSigDxilLibValidator.cpp
)

add_llvm_library(testdxil SHARED ${SOURCES})

if(WIN32)
  add_dependencies(testdxil DxcRuntimeEtw)
endif(WIN32)

