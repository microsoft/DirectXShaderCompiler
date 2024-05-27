# HLSL Change

add_hlsl_hctgen(DxcOptimizer OUTPUT DxcOptimizer.inc BUILD_DIR)
add_hlsl_hctgen(DxilValidation OUTPUT DxilValidationImpl.inc BUILD_DIR)

add_llvm_library(LLVMScalarOpts

  # We ended up merging LLVMHLSL, LLVMipo and LLVMScalarOpts because they access eachother's functions, which isn't great on some platforms.

  ../../HLSL/ComputeViewIdState.cpp
  ../../HLSL/ComputeViewIdStateBuilder.cpp
  ../../HLSL/ControlDependence.cpp
  ../../HLSL/DxilCondenseResources.cpp
  ../../HLSL/DxilContainerReflection.cpp
  ../../HLSL/DxilConvergent.cpp
  ../../HLSL/DxilEliminateOutputDynamicIndexing.cpp
  ../../HLSL/DxilExpandTrigIntrinsics.cpp
  ../../HLSL/DxilGenerationPass.cpp
  ../../HLSL/DxilLegalizeEvalOperations.cpp
  ../../HLSL/DxilLegalizeSampleOffsetPass.cpp
  ../../HLSL/DxilLinker.cpp
  ../../HLSL/DxilLoopDeletion.cpp
  ../../HLSL/DxilPrecisePropagatePass.cpp
  ../../HLSL/DxilPreparePasses.cpp
  ../../HLSL/DxilPromoteResourcePasses.cpp
  ../../HLSL/DxilPackSignatureElement.cpp
  ../../HLSL/DxilPatchShaderRecordBindings.cpp
  ../../HLSL/DxilNoops.cpp
  ../../HLSL/DxilPreserveAllOutputs.cpp
  ../../HLSL/DxilRenameResourcesPass.cpp
  ../../HLSL/DxilSimpleGVNHoist.cpp
  ../../HLSL/DxilSignatureValidation.cpp
  ../../HLSL/DxilTargetLowering.cpp
  ../../HLSL/DxilTargetTransformInfo.cpp
  ../../HLSL/DxilTranslateRawBuffer.cpp
  ../../HLSL/DxilExportMap.cpp
  ../../HLSL/DxilValidation.cpp
  ../../HLSL/DxcOptimizer.cpp
  ../../HLSL/HLDeadFunctionElimination.cpp
  ../../HLSL/HLExpandStoreIntrinsics.cpp
  ../../HLSL/HLLegalizeParameter.cpp
  ../../HLSL/HLLowerUDT.cpp
  ../../HLSL/HLMatrixBitcastLowerPass.cpp
  ../../HLSL/HLMatrixLowerPass.cpp
  ../../HLSL/HLMatrixSubscriptUseReplacer.cpp
  ../../HLSL/HLMatrixType.cpp
  ../../HLSL/HLMetadataPasses.cpp
  ../../HLSL/HLModule.cpp
  ../../HLSL/HLOperations.cpp
  ../../HLSL/HLOperationLower.cpp
  ../../HLSL/HLOperationLowerExtension.cpp
  ../../HLSL/HLPreprocess.cpp
  ../../HLSL/HLResource.cpp
  ../../HLSL/HLSignatureLower.cpp
  ../../HLSL/HLUtil.cpp
  ../../HLSL/PauseResumePasses.cpp
  ../../HLSL/WaveSensitivityAnalysis.cpp
  ../../HLSL/DxilNoOptLegalize.cpp
  ../../HLSL/DxilPoisonValues.cpp
  ../../HLSL/DxilDeleteRedundantDebugValues.cpp

  # LLVMipo

  ../IPO/BarrierNoopPass.cpp
  ../IPO/ConstantMerge.cpp
  ../IPO/DeadArgumentElimination.cpp
  ../IPO/ElimAvailExtern.cpp
  ../IPO/ExtractGV.cpp
  ../IPO/FunctionAttrs.cpp
  ../IPO/GlobalDCE.cpp
  ../IPO/GlobalOpt.cpp
  ../IPO/IPConstantPropagation.cpp
  ../IPO/IPO.cpp
  ../IPO/InlineAlways.cpp
  ../IPO/InlineSimple.cpp
  ../IPO/Inliner.cpp
  ../IPO/Internalize.cpp
  ../IPO/LoopExtractor.cpp
  ../IPO/LowerBitSets.cpp
  ../IPO/MergeFunctions.cpp
  ../IPO/PartialInlining.cpp
  ../IPO/PassManagerBuilder.cpp
  ../IPO/PruneEH.cpp
  ../IPO/StripDeadPrototypes.cpp
  ../IPO/StripSymbols.cpp

  # Old LLVMScalarOpts stuff

  ADCE.cpp
  AlignmentFromAssumptions.cpp
  BDCE.cpp
  ConstantHoisting.cpp
  ConstantProp.cpp
  CorrelatedValuePropagation.cpp
  DCE.cpp
  DeadStoreElimination.cpp
  EarlyCSE.cpp
  FlattenCFGPass.cpp
  Float2Int.cpp
  GVN.cpp
  HoistConstantArray.cpp
  InductiveRangeCheckElimination.cpp
  IndVarSimplify.cpp
  JumpThreading.cpp
  LICM.cpp
  LoadCombine.cpp
  LoopDeletion.cpp
  LoopDistribute.cpp
  LoopIdiomRecognize.cpp
  LoopInstSimplify.cpp
  LoopInterchange.cpp
  LoopRerollPass.cpp
  LoopRotation.cpp
  LoopStrengthReduce.cpp
  LoopUnrollPass.cpp
  LoopUnswitch.cpp
  LowerAtomic.cpp
  LowerExpectIntrinsic.cpp
  LowerTypePasses.cpp
  MemCpyOptimizer.cpp
  MergedLoadStoreMotion.cpp
  NaryReassociate.cpp
  PartiallyInlineLibCalls.cpp
  PlaceSafepoints.cpp
  Reassociate.cpp
  Reg2Mem.cpp
  Reg2MemHLSL.cpp
  RewriteStatepointsForGC.cpp
  SCCP.cpp
  SROA.cpp
  SampleProfile.cpp
  Scalar.cpp
  ScalarReplAggregates.cpp
  ScalarReplAggregatesHLSL.cpp  # HLSL Change
  DxilLoopUnroll.cpp # HLSL Change
  DxilRemoveDeadBlocks.cpp # HLSL Change
  DxilEraseDeadRegion.cpp # HLSL Change
  DxilFixConstArrayInitializer.cpp # HLSL Change
  DxilEliminateVector.cpp # HLSL Change
  DxilConditionalMem2Reg.cpp # HLSL Change
  DxilRemoveUnstructuredLoopExits.cpp # HLSL Change
  Scalarizer.cpp
  SeparateConstOffsetFromGEP.cpp
  SimplifyCFGPass.cpp
  Sink.cpp
  SpeculativeExecution.cpp
  StraightLineStrengthReduce.cpp
  StructurizeCFG.cpp
  TailRecursionElimination.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/IR
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Scalar
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/IPO
  )

add_dependencies(LLVMScalarOpts intrinsics_gen)

target_link_libraries(LLVMScalarOpts PUBLIC LLVMDXIL LLVMPassPrinters) # HLSL Change
