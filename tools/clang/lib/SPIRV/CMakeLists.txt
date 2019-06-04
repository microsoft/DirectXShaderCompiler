set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangSPIRV
  AlignmentSizeCalculator.cpp
  AstTypeProbe.cpp
  BlockReadableOrder.cpp
  CapabilityVisitor.cpp
  DeclResultIdMapper.cpp
  EmitSpirvAction.cpp
  EmitVisitor.cpp
  FeatureManager.cpp
  GlPerVertex.cpp
  InitListHandler.cpp
  LiteralTypeVisitor.cpp
  LowerTypeVisitor.cpp
  PreciseVisitor.cpp
  RawBufferMethods.cpp
  RelaxedPrecisionVisitor.cpp
  SpirvBasicBlock.cpp
  SpirvBuilder.cpp
  SpirvContext.cpp
  SpirvEmitter.cpp
  SpirvFunction.cpp
  SpirvInstruction.cpp
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
