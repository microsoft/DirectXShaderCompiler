set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangSPIRV
  AlignmentSizeCalculator.cpp
  AstTypeProbe.cpp
  BlockReadableOrder.cpp
  CapabilityVisitor.cpp
  DeclResultIdMapper.cpp
  EmitSPIRVAction.cpp
  EmitVisitor.cpp
  FeatureManager.cpp
  GlPerVertex.cpp
  InitListHandler.cpp
  InstBuilderAuto.cpp
  InstBuilderManual.cpp
  LiteralTypeVisitor.cpp
  LowerTypeVisitor.cpp
  SPIRVContext.cpp
  SPIRVEmitter.cpp
  SpirvBasicBlock.cpp
  SpirvBuilder.cpp
  SpirvInstruction.cpp
  SpirvFunction.cpp
  SpirvModule.cpp
  SpirvType.cpp
  String.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangFrontend
  clangLex
  SPIRV-Tools-opt
  )

target_include_directories(clangSPIRV PUBLIC ${SPIRV_HEADER_INCLUDE_DIR})
target_include_directories(clangSPIRV PRIVATE ${SPIRV_TOOLS_INCLUDE_DIR})
