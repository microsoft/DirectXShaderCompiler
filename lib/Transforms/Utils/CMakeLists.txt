# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
add_llvm_library(LLVMTransformUtils
  ASanStackFrameLayout.cpp
  AddDiscriminators.cpp
  BasicBlockUtils.cpp
  BreakCriticalEdges.cpp
  BuildLibCalls.cpp
  BypassSlowDivision.cpp
  CloneFunction.cpp
  CloneModule.cpp
  CmpInstAnalysis.cpp
  CodeExtractor.cpp
  CtorUtils.cpp
  DemoteRegToStack.cpp
  FlattenCFG.cpp
  GlobalStatus.cpp
  InlineFunction.cpp
  InstructionNamer.cpp
  IntegerDivision.cpp
  LCSSA.cpp
  Local.cpp
  LoopSimplify.cpp
  LoopUnroll.cpp
  LoopUnrollRuntime.cpp
  LoopUtils.cpp
  LoopVersioning.cpp
  LowerInvoke.cpp
  LowerSwitch.cpp
  Mem2Reg.cpp
  MetaRenamer.cpp
  ModuleUtils.cpp
  PromoteMemoryToRegister.cpp
  SSAUpdater.cpp
  SimplifyCFG.cpp
  SimplifyIndVar.cpp
  SimplifyInstructions.cpp
  SimplifyLibCalls.cpp
  SymbolRewriter.cpp
  UnifyFunctionExitNodes.cpp
  Utils.cpp
  ValueMapper.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Utils
  )

add_dependencies(LLVMTransformUtils intrinsics_gen)
