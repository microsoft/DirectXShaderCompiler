set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangSPIRV
  BlockReadableOrder.cpp
  Constant.cpp
  DeclResultIdMapper.cpp
  Decoration.cpp
  EmitSPIRVAction.cpp
  FeatureManager.cpp
  GlPerVertex.cpp
  InitListHandler.cpp
  InstBuilderAuto.cpp
  InstBuilderManual.cpp
  ModuleBuilder.cpp
  SPIRVContext.cpp
  SPIRVEmitter.cpp
  SpirvInstruction.cpp
  String.cpp
  Structure.cpp
  Type.cpp
  TypeTranslator.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangFrontend
  clangLex
  SPIRV-Tools-opt
  )

target_include_directories(clangSPIRV PUBLIC ${SPIRV_HEADER_INCLUDE_DIR})
target_include_directories(clangSPIRV PRIVATE ${SPIRV_TOOLS_INCLUDE_DIR})
