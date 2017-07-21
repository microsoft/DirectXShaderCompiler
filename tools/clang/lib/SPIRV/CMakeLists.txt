set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangSPIRV
  Constant.cpp
  DeclResultIdMapper.cpp
  Decoration.cpp
  EmitSPIRVAction.cpp
  InstBuilderAuto.cpp
  InstBuilderManual.cpp
  ModuleBuilder.cpp
  SPIRVContext.cpp
  String.cpp
  Structure.cpp
  Type.cpp
  TypeTranslator.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangFrontend
  clangLex
  )

target_include_directories(clangSPIRV PUBLIC ${SPIRV_HEADER_INCLUDE_DIR})
