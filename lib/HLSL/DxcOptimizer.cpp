///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcOptimizer.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides an IDxcOptimizer implementation.                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/microcom.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/DXIL/DxilModule.h"
#include "llvm/Analysis/ReducibilityAnalysis.h"
#include "dxc/HLSL/HLMatrixLowerPass.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/ComputeViewIdState.h"
#include "llvm/Analysis/DxilValueCache.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/Support/dxcapi.impl.h"

#include "llvm/Pass.h"
#include "llvm/PassInfo.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <algorithm>
#include <list>   // should change this for string_table
#include <vector>

#include "llvm/PassPrinters/PassPrinters.h"

using namespace llvm;
using namespace hlsl;

inline static bool wcseq(LPCWSTR a, LPCWSTR b) {
  return 0 == wcscmp(a, b);
}
inline static bool wcsstartswith(LPCWSTR value, LPCWSTR prefix) {
  while (*value && *prefix && *value == *prefix) {
    ++value;
    ++prefix;
  }
  return *prefix == L'\0';
}

namespace hlsl {

HRESULT SetupRegistryPassForHLSL() {
  try
  {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    /* <py::lines('INIT-PASSES')>hctdb_instrhelp.get_init_passes(set(["llvm", "dxil_gen"]))</py>*/
    // INIT-PASSES:BEGIN
    initializeADCEPass(Registry);
    initializeAddDiscriminatorsPass(Registry);
    initializeAlignmentFromAssumptionsPass(Registry);
    initializeAlwaysInlinerPass(Registry);
    initializeArgPromotionPass(Registry);
    initializeBDCEPass(Registry);
    initializeBarrierNoopPass(Registry);
    initializeBasicAliasAnalysisPass(Registry);
    initializeCFGSimplifyPassPass(Registry);
    initializeCFLAliasAnalysisPass(Registry);
    initializeCleanupDxBreakPass(Registry);
    initializeComputeViewIdStatePass(Registry);
    initializeConstantMergePass(Registry);
    initializeCorrelatedValuePropagationPass(Registry);
    initializeDAEPass(Registry);
    initializeDAHPass(Registry);
    initializeDCEPass(Registry);
    initializeDSEPass(Registry);
    initializeDeadInstEliminationPass(Registry);
    initializeDxilAllocateResourcesForLibPass(Registry);
    initializeDxilCleanupAddrSpaceCastPass(Registry);
    initializeDxilCondenseResourcesPass(Registry);
    initializeDxilConditionalMem2RegPass(Registry);
    initializeDxilConvergentClearPass(Registry);
    initializeDxilConvergentMarkPass(Registry);
    initializeDxilDeadFunctionEliminationPass(Registry);
    initializeDxilEliminateOutputDynamicIndexingPass(Registry);
    initializeDxilEliminateVectorPass(Registry);
    initializeDxilEmitMetadataPass(Registry);
    initializeDxilEraseDeadRegionPass(Registry);
    initializeDxilExpandTrigIntrinsicsPass(Registry);
    initializeDxilFinalizeModulePass(Registry);
    initializeDxilFinalizePreservesPass(Registry);
    initializeDxilFixConstArrayInitializerPass(Registry);
    initializeDxilGenerationPassPass(Registry);
    initializeDxilInsertPreservesPass(Registry);
    initializeDxilLegalizeEvalOperationsPass(Registry);
    initializeDxilLegalizeResourcesPass(Registry);
    initializeDxilLegalizeSampleOffsetPassPass(Registry);
    initializeDxilLoadMetadataPass(Registry);
    initializeDxilLoopDeletionPass(Registry);
    initializeDxilLoopUnrollPass(Registry);
    initializeDxilLowerCreateHandleForLibPass(Registry);
    initializeDxilNoOptLegalizePass(Registry);
    initializeDxilPrecisePropagatePassPass(Registry);
    initializeDxilPreserveAllOutputsPass(Registry);
    initializeDxilPreserveToSelectPass(Registry);
    initializeDxilPromoteLocalResourcesPass(Registry);
    initializeDxilPromoteStaticResourcesPass(Registry);
    initializeDxilRemoveDeadBlocksPass(Registry);
    initializeDxilRenameResourcesPass(Registry);
    initializeDxilRewriteOutputArgDebugInfoPass(Registry);
    initializeDxilSimpleGVNHoistPass(Registry);
    initializeDxilTranslateRawBufferPass(Registry);
    initializeDxilValidateWaveSensitivityPass(Registry);
    initializeDxilValueCachePass(Registry);
    initializeDynamicIndexingVectorToArrayPass(Registry);
    initializeEarlyCSELegacyPassPass(Registry);
    initializeEliminateAvailableExternallyPass(Registry);
    initializeFloat2IntPass(Registry);
    initializeFunctionAttrsPass(Registry);
    initializeGVNPass(Registry);
    initializeGlobalDCEPass(Registry);
    initializeGlobalOptPass(Registry);
    initializeHLDeadFunctionEliminationPass(Registry);
    initializeHLEmitMetadataPass(Registry);
    initializeHLEnsureMetadataPass(Registry);
    initializeHLExpandStoreIntrinsicsPass(Registry);
    initializeHLLegalizeParameterPass(Registry);
    initializeHLMatrixLowerPassPass(Registry);
    initializeHLPreprocessPass(Registry);
    initializeHoistConstantArrayPass(Registry);
    initializeIPSCCPPass(Registry);
    initializeIndVarSimplifyPass(Registry);
    initializeInstructionCombiningPassPass(Registry);
    initializeInvalidateUndefResourcesPass(Registry);
    initializeJumpThreadingPass(Registry);
    initializeLICMPass(Registry);
    initializeLoadCombinePass(Registry);
    initializeLoopDeletionPass(Registry);
    initializeLoopDistributePass(Registry);
    initializeLoopIdiomRecognizePass(Registry);
    initializeLoopInterchangePass(Registry);
    initializeLoopRerollPass(Registry);
    initializeLoopRotatePass(Registry);
    initializeLoopUnrollPass(Registry);
    initializeLoopUnswitchPass(Registry);
    initializeLowerBitSetsPass(Registry);
    initializeLowerExpectIntrinsicPass(Registry);
    initializeLowerStaticGlobalIntoAllocaPass(Registry);
    initializeMatrixBitcastLowerPassPass(Registry);
    initializeMergeFunctionsPass(Registry);
    initializeMergedLoadStoreMotionPass(Registry);
    initializeMultiDimArrayToOneDimArrayPass(Registry);
    initializeNoPausePassesPass(Registry);
    initializePausePassesPass(Registry);
    initializePromotePassPass(Registry);
    initializePruneEHPass(Registry);
    initializeReassociatePass(Registry);
    initializeReducibilityAnalysisPass(Registry);
    initializeRegToMemHlslPass(Registry);
    initializeResourceToHandlePass(Registry);
    initializeResumePassesPass(Registry);
    initializeRewriteSymbolsPass(Registry);
    initializeSCCPPass(Registry);
    initializeSROAPass(Registry);
    initializeSROA_DTPass(Registry);
    initializeSROA_Parameter_HLSLPass(Registry);
    initializeSROA_SSAUpPass(Registry);
    initializeSampleProfileLoaderPass(Registry);
    initializeScalarizerPass(Registry);
    initializeScopedNoAliasAAPass(Registry);
    initializeSimpleInlinerPass(Registry);
    initializeSimplifyInstPass(Registry);
    initializeStripDeadPrototypesPassPass(Registry);
    initializeTargetLibraryInfoWrapperPassPass(Registry);
    initializeTargetTransformInfoWrapperPassPass(Registry);
    initializeTypeBasedAliasAnalysisPass(Registry);
    initializeVerifierLegacyPassPass(Registry);
    // INIT-PASSES:END
    // Not schematized - exclusively for compiler authors.
    initializeCFGPrinterPasses(Registry);
  }
  CATCH_CPP_RETURN_HRESULT();
  return S_OK;
}

}

static ArrayRef<LPCSTR> GetPassArgNames(LPCSTR passName) {
  /* <py::lines('GETPASSARGNAMES')>hctdb_instrhelp.get_pass_arg_names()</py>*/
  // GETPASSARGNAMES:BEGIN
  static const LPCSTR AddDiscriminatorsArgs[] = { "no-discriminators" };
  static const LPCSTR AlwaysInlinerArgs[] = { "InsertLifetime", "InlineThreshold" };
  static const LPCSTR ArgPromotionArgs[] = { "maxElements" };
  static const LPCSTR CFGSimplifyPassArgs[] = { "Threshold", "Ftor", "bonus-inst-threshold" };
  static const LPCSTR DxilAddPixelHitInstrumentationArgs[] = { "force-early-z", "add-pixel-cost", "rt-width", "sv-position-index", "num-pixels" };
  static const LPCSTR DxilConditionalMem2RegArgs[] = { "NoOpt" };
  static const LPCSTR DxilDebugInstrumentationArgs[] = { "UAVSize", "parameter0", "parameter1", "parameter2" };
  static const LPCSTR DxilGenerationPassArgs[] = { "NotOptimized" };
  static const LPCSTR DxilInsertPreservesArgs[] = { "AllowPreserves" };
  static const LPCSTR DxilLoopUnrollArgs[] = { "MaxIterationAttempt", "OnlyWarnOnFail" };
  static const LPCSTR DxilOutputColorBecomesConstantArgs[] = { "mod-mode", "constant-red", "constant-green", "constant-blue", "constant-alpha" };
  static const LPCSTR DxilPIXMeshShaderOutputInstrumentationArgs[] = { "UAVSize" };
  static const LPCSTR DxilRenameResourcesArgs[] = { "prefix", "from-binding", "keep-name" };
  static const LPCSTR DxilShaderAccessTrackingArgs[] = { "config", "checkForDynamicIndexing" };
  static const LPCSTR DynamicIndexingVectorToArrayArgs[] = { "ReplaceAllVectors" };
  static const LPCSTR Float2IntArgs[] = { "float2int-max-integer-bw" };
  static const LPCSTR GVNArgs[] = { "noloads", "enable-pre", "enable-load-pre", "max-recurse-depth" };
  static const LPCSTR JumpThreadingArgs[] = { "Threshold", "jump-threading-threshold" };
  static const LPCSTR LICMArgs[] = { "disable-licm-promotion" };
  static const LPCSTR LoopDistributeArgs[] = { "loop-distribute-verify", "loop-distribute-non-if-convertible" };
  static const LPCSTR LoopRerollArgs[] = { "max-reroll-increment", "reroll-num-tolerated-failed-matches" };
  static const LPCSTR LoopRotateArgs[] = { "MaxHeaderSize", "rotation-max-header-size" };
  static const LPCSTR LoopUnrollArgs[] = { "Threshold", "Count", "AllowPartial", "Runtime", "unroll-threshold", "unroll-percent-dynamic-cost-saved-threshold", "unroll-dynamic-cost-savings-discount", "unroll-max-iteration-count-to-analyze", "unroll-count", "unroll-allow-partial", "unroll-runtime", "pragma-unroll-threshold" };
  static const LPCSTR LoopUnswitchArgs[] = { "Os", "loop-unswitch-threshold" };
  static const LPCSTR LowerBitSetsArgs[] = { "lowerbitsets-avoid-reuse" };
  static const LPCSTR LowerExpectIntrinsicArgs[] = { "likely-branch-weight", "unlikely-branch-weight" };
  static const LPCSTR MergeFunctionsArgs[] = { "mergefunc-sanity" };
  static const LPCSTR RewriteSymbolsArgs[] = { "DL", "rewrite-map-file" };
  static const LPCSTR SROAArgs[] = { "RequiresDomTree", "SkipHLSLMat", "force-ssa-updater", "sroa-random-shuffle-slices", "sroa-strict-inbounds" };
  static const LPCSTR SROA_DTArgs[] = { "Threshold", "StructMemberThreshold", "ArrayElementThreshold", "ScalarLoadThreshold" };
  static const LPCSTR SROA_SSAUpArgs[] = { "Threshold", "StructMemberThreshold", "ArrayElementThreshold", "ScalarLoadThreshold" };
  static const LPCSTR SampleProfileLoaderArgs[] = { "sample-profile-file", "sample-profile-max-propagate-iterations" };
  static const LPCSTR ScopedNoAliasAAArgs[] = { "enable-scoped-noalias" };
  static const LPCSTR SimpleInlinerArgs[] = { "InsertLifetime", "InlineThreshold" };
  static const LPCSTR TargetLibraryInfoWrapperPassArgs[] = { "TLIImpl", "vector-library" };
  static const LPCSTR TargetTransformInfoWrapperPassArgs[] = { "TIRA" };
  static const LPCSTR TypeBasedAliasAnalysisArgs[] = { "enable-tbaa" };
  static const LPCSTR VerifierLegacyPassArgs[] = { "FatalErrors", "verify-debug-info" };
  if (strcmp(passName, "add-discriminators") == 0) return ArrayRef<LPCSTR>(AddDiscriminatorsArgs, _countof(AddDiscriminatorsArgs));
  if (strcmp(passName, "always-inline") == 0) return ArrayRef<LPCSTR>(AlwaysInlinerArgs, _countof(AlwaysInlinerArgs));
  if (strcmp(passName, "argpromotion") == 0) return ArrayRef<LPCSTR>(ArgPromotionArgs, _countof(ArgPromotionArgs));
  if (strcmp(passName, "simplifycfg") == 0) return ArrayRef<LPCSTR>(CFGSimplifyPassArgs, _countof(CFGSimplifyPassArgs));
  if (strcmp(passName, "hlsl-dxil-add-pixel-hit-instrmentation") == 0) return ArrayRef<LPCSTR>(DxilAddPixelHitInstrumentationArgs, _countof(DxilAddPixelHitInstrumentationArgs));
  if (strcmp(passName, "dxil-cond-mem2reg") == 0) return ArrayRef<LPCSTR>(DxilConditionalMem2RegArgs, _countof(DxilConditionalMem2RegArgs));
  if (strcmp(passName, "hlsl-dxil-debug-instrumentation") == 0) return ArrayRef<LPCSTR>(DxilDebugInstrumentationArgs, _countof(DxilDebugInstrumentationArgs));
  if (strcmp(passName, "dxilgen") == 0) return ArrayRef<LPCSTR>(DxilGenerationPassArgs, _countof(DxilGenerationPassArgs));
  if (strcmp(passName, "dxil-insert-preserves") == 0) return ArrayRef<LPCSTR>(DxilInsertPreservesArgs, _countof(DxilInsertPreservesArgs));
  if (strcmp(passName, "dxil-loop-unroll") == 0) return ArrayRef<LPCSTR>(DxilLoopUnrollArgs, _countof(DxilLoopUnrollArgs));
  if (strcmp(passName, "hlsl-dxil-constantColor") == 0) return ArrayRef<LPCSTR>(DxilOutputColorBecomesConstantArgs, _countof(DxilOutputColorBecomesConstantArgs));
  if (strcmp(passName, "hlsl-dxil-pix-meshshader-output-instrumentation") == 0) return ArrayRef<LPCSTR>(DxilPIXMeshShaderOutputInstrumentationArgs, _countof(DxilPIXMeshShaderOutputInstrumentationArgs));
  if (strcmp(passName, "dxil-rename-resources") == 0) return ArrayRef<LPCSTR>(DxilRenameResourcesArgs, _countof(DxilRenameResourcesArgs));
  if (strcmp(passName, "hlsl-dxil-pix-shader-access-instrumentation") == 0) return ArrayRef<LPCSTR>(DxilShaderAccessTrackingArgs, _countof(DxilShaderAccessTrackingArgs));
  if (strcmp(passName, "dynamic-vector-to-array") == 0) return ArrayRef<LPCSTR>(DynamicIndexingVectorToArrayArgs, _countof(DynamicIndexingVectorToArrayArgs));
  if (strcmp(passName, "float2int") == 0) return ArrayRef<LPCSTR>(Float2IntArgs, _countof(Float2IntArgs));
  if (strcmp(passName, "gvn") == 0) return ArrayRef<LPCSTR>(GVNArgs, _countof(GVNArgs));
  if (strcmp(passName, "jump-threading") == 0) return ArrayRef<LPCSTR>(JumpThreadingArgs, _countof(JumpThreadingArgs));
  if (strcmp(passName, "licm") == 0) return ArrayRef<LPCSTR>(LICMArgs, _countof(LICMArgs));
  if (strcmp(passName, "loop-distribute") == 0) return ArrayRef<LPCSTR>(LoopDistributeArgs, _countof(LoopDistributeArgs));
  if (strcmp(passName, "loop-reroll") == 0) return ArrayRef<LPCSTR>(LoopRerollArgs, _countof(LoopRerollArgs));
  if (strcmp(passName, "loop-rotate") == 0) return ArrayRef<LPCSTR>(LoopRotateArgs, _countof(LoopRotateArgs));
  if (strcmp(passName, "loop-unroll") == 0) return ArrayRef<LPCSTR>(LoopUnrollArgs, _countof(LoopUnrollArgs));
  if (strcmp(passName, "loop-unswitch") == 0) return ArrayRef<LPCSTR>(LoopUnswitchArgs, _countof(LoopUnswitchArgs));
  if (strcmp(passName, "lowerbitsets") == 0) return ArrayRef<LPCSTR>(LowerBitSetsArgs, _countof(LowerBitSetsArgs));
  if (strcmp(passName, "lower-expect") == 0) return ArrayRef<LPCSTR>(LowerExpectIntrinsicArgs, _countof(LowerExpectIntrinsicArgs));
  if (strcmp(passName, "mergefunc") == 0) return ArrayRef<LPCSTR>(MergeFunctionsArgs, _countof(MergeFunctionsArgs));
  if (strcmp(passName, "rewrite-symbols") == 0) return ArrayRef<LPCSTR>(RewriteSymbolsArgs, _countof(RewriteSymbolsArgs));
  if (strcmp(passName, "sroa") == 0) return ArrayRef<LPCSTR>(SROAArgs, _countof(SROAArgs));
  if (strcmp(passName, "scalarrepl") == 0) return ArrayRef<LPCSTR>(SROA_DTArgs, _countof(SROA_DTArgs));
  if (strcmp(passName, "scalarrepl-ssa") == 0) return ArrayRef<LPCSTR>(SROA_SSAUpArgs, _countof(SROA_SSAUpArgs));
  if (strcmp(passName, "sample-profile") == 0) return ArrayRef<LPCSTR>(SampleProfileLoaderArgs, _countof(SampleProfileLoaderArgs));
  if (strcmp(passName, "scoped-noalias") == 0) return ArrayRef<LPCSTR>(ScopedNoAliasAAArgs, _countof(ScopedNoAliasAAArgs));
  if (strcmp(passName, "inline") == 0) return ArrayRef<LPCSTR>(SimpleInlinerArgs, _countof(SimpleInlinerArgs));
  if (strcmp(passName, "targetlibinfo") == 0) return ArrayRef<LPCSTR>(TargetLibraryInfoWrapperPassArgs, _countof(TargetLibraryInfoWrapperPassArgs));
  if (strcmp(passName, "tti") == 0) return ArrayRef<LPCSTR>(TargetTransformInfoWrapperPassArgs, _countof(TargetTransformInfoWrapperPassArgs));
  if (strcmp(passName, "tbaa") == 0) return ArrayRef<LPCSTR>(TypeBasedAliasAnalysisArgs, _countof(TypeBasedAliasAnalysisArgs));
  if (strcmp(passName, "verify") == 0) return ArrayRef<LPCSTR>(VerifierLegacyPassArgs, _countof(VerifierLegacyPassArgs));
  // GETPASSARGNAMES:END
  return ArrayRef<LPCSTR>();
}

static ArrayRef<LPCSTR> GetPassArgDescriptions(LPCSTR passName) {
  /* <py::lines('GETPASSARGDESCS')>hctdb_instrhelp.get_pass_arg_descs()</py>*/
  // GETPASSARGDESCS:BEGIN
  static const LPCSTR AddDiscriminatorsArgs[] = { "None" };
  static const LPCSTR AlwaysInlinerArgs[] = { "Insert @llvm.lifetime intrinsics", "Insert @llvm.lifetime intrinsics" };
  static const LPCSTR ArgPromotionArgs[] = { "None" };
  static const LPCSTR CFGSimplifyPassArgs[] = { "None", "None", "Control the number of bonus instructions (default = 1)" };
  static const LPCSTR DxilAddPixelHitInstrumentationArgs[] = { "None", "None", "None", "None", "None" };
  static const LPCSTR DxilConditionalMem2RegArgs[] = { "None" };
  static const LPCSTR DxilDebugInstrumentationArgs[] = { "None", "None", "None", "None" };
  static const LPCSTR DxilGenerationPassArgs[] = { "None" };
  static const LPCSTR DxilInsertPreservesArgs[] = { "None" };
  static const LPCSTR DxilLoopUnrollArgs[] = { "Maximum number of iterations to attempt when iteratively unrolling.", "Whether to just warn when unrolling fails." };
  static const LPCSTR DxilOutputColorBecomesConstantArgs[] = { "None", "None", "None", "None", "None" };
  static const LPCSTR DxilPIXMeshShaderOutputInstrumentationArgs[] = { "None" };
  static const LPCSTR DxilRenameResourcesArgs[] = { "Prefix to add to resource names", "Append binding to name when bound", "Keep name when appending binding" };
  static const LPCSTR DxilShaderAccessTrackingArgs[] = { "None", "None" };
  static const LPCSTR DynamicIndexingVectorToArrayArgs[] = { "None" };
  static const LPCSTR Float2IntArgs[] = { "Max integer bitwidth to consider in float2int" };
  static const LPCSTR GVNArgs[] = { "None", "None", "None", "Max recurse depth" };
  static const LPCSTR JumpThreadingArgs[] = { "None", "Max block size to duplicate for jump threading" };
  static const LPCSTR LICMArgs[] = { "Disable memory promotion in LICM pass" };
  static const LPCSTR LoopDistributeArgs[] = { "Turn on DominatorTree and LoopInfo verification after Loop Distribution", "Whether to distribute into a loop that may not be if-convertible by the loop vectorizer" };
  static const LPCSTR LoopRerollArgs[] = { "The maximum increment for loop rerolling", "The maximum number of failures to tolerate during fuzzy matching." };
  static const LPCSTR LoopRotateArgs[] = { "None", "The default maximum header size for automatic loop rotation" };
  static const LPCSTR LoopUnrollArgs[] = { "None", "None", "None", "None", "The baseline cost threshold for loop unrolling", "The percentage of estimated dynamic cost which must be saved by unrolling to allow unrolling up to the max threshold.", "This is the amount discounted from the total unroll cost when the unrolled form has a high dynamic cost savings (triggered by the '-unroll-perecent-dynamic-cost-saved-threshold' flag).", "Don't allow loop unrolling to simulate more than this number of iterations when checking full unroll profitability", "Use this unroll count for all loops including those with unroll_count pragma values, for testing purposes", "Allows loops to be partially unrolled until -unroll-threshold loop size is reached.", "Unroll loops with run-time trip counts", "Unrolled size limit for loops with an unroll(full) or unroll_count pragma." };
  static const LPCSTR LoopUnswitchArgs[] = { "Optimize for size", "Max loop size to unswitch" };
  static const LPCSTR LowerBitSetsArgs[] = { "Try to avoid reuse of byte array addresses using aliases" };
  static const LPCSTR LowerExpectIntrinsicArgs[] = { "Weight of the branch likely to be taken (default = 64)", "Weight of the branch unlikely to be taken (default = 4)" };
  static const LPCSTR MergeFunctionsArgs[] = { "How many functions in module could be used for MergeFunctions pass sanity check. '0' disables this check. Works only with '-debug' key." };
  static const LPCSTR RewriteSymbolsArgs[] = { "None", "None" };
  static const LPCSTR SROAArgs[] = { "None", "None", "Force the pass to not use DomTree and mem2reg, insteadforming SSA values through the SSAUpdater infrastructure.", "Enable randomly shuffling the slices to help uncover instability in their order.", "Experiment with completely strict handling of inbounds GEPs." };
  static const LPCSTR SROA_DTArgs[] = { "None", "None", "None", "None" };
  static const LPCSTR SROA_SSAUpArgs[] = { "None", "None", "None", "None" };
  static const LPCSTR SampleProfileLoaderArgs[] = { "None", "None" };
  static const LPCSTR ScopedNoAliasAAArgs[] = { "Use to disable scoped no-alias" };
  static const LPCSTR SimpleInlinerArgs[] = { "Insert @llvm.lifetime intrinsics", "Insert @llvm.lifetime intrinsics" };
  static const LPCSTR TargetLibraryInfoWrapperPassArgs[] = { "None", "None" };
  static const LPCSTR TargetTransformInfoWrapperPassArgs[] = { "None" };
  static const LPCSTR TypeBasedAliasAnalysisArgs[] = { "Use to disable TBAA functionality" };
  static const LPCSTR VerifierLegacyPassArgs[] = { "None", "None" };
  if (strcmp(passName, "add-discriminators") == 0) return ArrayRef<LPCSTR>(AddDiscriminatorsArgs, _countof(AddDiscriminatorsArgs));
  if (strcmp(passName, "always-inline") == 0) return ArrayRef<LPCSTR>(AlwaysInlinerArgs, _countof(AlwaysInlinerArgs));
  if (strcmp(passName, "argpromotion") == 0) return ArrayRef<LPCSTR>(ArgPromotionArgs, _countof(ArgPromotionArgs));
  if (strcmp(passName, "simplifycfg") == 0) return ArrayRef<LPCSTR>(CFGSimplifyPassArgs, _countof(CFGSimplifyPassArgs));
  if (strcmp(passName, "hlsl-dxil-add-pixel-hit-instrmentation") == 0) return ArrayRef<LPCSTR>(DxilAddPixelHitInstrumentationArgs, _countof(DxilAddPixelHitInstrumentationArgs));
  if (strcmp(passName, "dxil-cond-mem2reg") == 0) return ArrayRef<LPCSTR>(DxilConditionalMem2RegArgs, _countof(DxilConditionalMem2RegArgs));
  if (strcmp(passName, "hlsl-dxil-debug-instrumentation") == 0) return ArrayRef<LPCSTR>(DxilDebugInstrumentationArgs, _countof(DxilDebugInstrumentationArgs));
  if (strcmp(passName, "dxilgen") == 0) return ArrayRef<LPCSTR>(DxilGenerationPassArgs, _countof(DxilGenerationPassArgs));
  if (strcmp(passName, "dxil-insert-preserves") == 0) return ArrayRef<LPCSTR>(DxilInsertPreservesArgs, _countof(DxilInsertPreservesArgs));
  if (strcmp(passName, "dxil-loop-unroll") == 0) return ArrayRef<LPCSTR>(DxilLoopUnrollArgs, _countof(DxilLoopUnrollArgs));
  if (strcmp(passName, "hlsl-dxil-constantColor") == 0) return ArrayRef<LPCSTR>(DxilOutputColorBecomesConstantArgs, _countof(DxilOutputColorBecomesConstantArgs));
  if (strcmp(passName, "hlsl-dxil-pix-meshshader-output-instrumentation") == 0) return ArrayRef<LPCSTR>(DxilPIXMeshShaderOutputInstrumentationArgs, _countof(DxilPIXMeshShaderOutputInstrumentationArgs));
  if (strcmp(passName, "dxil-rename-resources") == 0) return ArrayRef<LPCSTR>(DxilRenameResourcesArgs, _countof(DxilRenameResourcesArgs));
  if (strcmp(passName, "hlsl-dxil-pix-shader-access-instrumentation") == 0) return ArrayRef<LPCSTR>(DxilShaderAccessTrackingArgs, _countof(DxilShaderAccessTrackingArgs));
  if (strcmp(passName, "dynamic-vector-to-array") == 0) return ArrayRef<LPCSTR>(DynamicIndexingVectorToArrayArgs, _countof(DynamicIndexingVectorToArrayArgs));
  if (strcmp(passName, "float2int") == 0) return ArrayRef<LPCSTR>(Float2IntArgs, _countof(Float2IntArgs));
  if (strcmp(passName, "gvn") == 0) return ArrayRef<LPCSTR>(GVNArgs, _countof(GVNArgs));
  if (strcmp(passName, "jump-threading") == 0) return ArrayRef<LPCSTR>(JumpThreadingArgs, _countof(JumpThreadingArgs));
  if (strcmp(passName, "licm") == 0) return ArrayRef<LPCSTR>(LICMArgs, _countof(LICMArgs));
  if (strcmp(passName, "loop-distribute") == 0) return ArrayRef<LPCSTR>(LoopDistributeArgs, _countof(LoopDistributeArgs));
  if (strcmp(passName, "loop-reroll") == 0) return ArrayRef<LPCSTR>(LoopRerollArgs, _countof(LoopRerollArgs));
  if (strcmp(passName, "loop-rotate") == 0) return ArrayRef<LPCSTR>(LoopRotateArgs, _countof(LoopRotateArgs));
  if (strcmp(passName, "loop-unroll") == 0) return ArrayRef<LPCSTR>(LoopUnrollArgs, _countof(LoopUnrollArgs));
  if (strcmp(passName, "loop-unswitch") == 0) return ArrayRef<LPCSTR>(LoopUnswitchArgs, _countof(LoopUnswitchArgs));
  if (strcmp(passName, "lowerbitsets") == 0) return ArrayRef<LPCSTR>(LowerBitSetsArgs, _countof(LowerBitSetsArgs));
  if (strcmp(passName, "lower-expect") == 0) return ArrayRef<LPCSTR>(LowerExpectIntrinsicArgs, _countof(LowerExpectIntrinsicArgs));
  if (strcmp(passName, "mergefunc") == 0) return ArrayRef<LPCSTR>(MergeFunctionsArgs, _countof(MergeFunctionsArgs));
  if (strcmp(passName, "rewrite-symbols") == 0) return ArrayRef<LPCSTR>(RewriteSymbolsArgs, _countof(RewriteSymbolsArgs));
  if (strcmp(passName, "sroa") == 0) return ArrayRef<LPCSTR>(SROAArgs, _countof(SROAArgs));
  if (strcmp(passName, "scalarrepl") == 0) return ArrayRef<LPCSTR>(SROA_DTArgs, _countof(SROA_DTArgs));
  if (strcmp(passName, "scalarrepl-ssa") == 0) return ArrayRef<LPCSTR>(SROA_SSAUpArgs, _countof(SROA_SSAUpArgs));
  if (strcmp(passName, "sample-profile") == 0) return ArrayRef<LPCSTR>(SampleProfileLoaderArgs, _countof(SampleProfileLoaderArgs));
  if (strcmp(passName, "scoped-noalias") == 0) return ArrayRef<LPCSTR>(ScopedNoAliasAAArgs, _countof(ScopedNoAliasAAArgs));
  if (strcmp(passName, "inline") == 0) return ArrayRef<LPCSTR>(SimpleInlinerArgs, _countof(SimpleInlinerArgs));
  if (strcmp(passName, "targetlibinfo") == 0) return ArrayRef<LPCSTR>(TargetLibraryInfoWrapperPassArgs, _countof(TargetLibraryInfoWrapperPassArgs));
  if (strcmp(passName, "tti") == 0) return ArrayRef<LPCSTR>(TargetTransformInfoWrapperPassArgs, _countof(TargetTransformInfoWrapperPassArgs));
  if (strcmp(passName, "tbaa") == 0) return ArrayRef<LPCSTR>(TypeBasedAliasAnalysisArgs, _countof(TypeBasedAliasAnalysisArgs));
  if (strcmp(passName, "verify") == 0) return ArrayRef<LPCSTR>(VerifierLegacyPassArgs, _countof(VerifierLegacyPassArgs));
  // GETPASSARGDESCS:END
  return ArrayRef<LPCSTR>();
}

static bool IsPassOptionName(StringRef S) {
  /* <py::lines('ISPASSOPTIONNAME')>hctdb_instrhelp.get_is_pass_option_name()</py>*/
  // ISPASSOPTIONNAME:BEGIN
  return S.equals("AllowPartial")
    ||  S.equals("AllowPreserves")
    ||  S.equals("ArrayElementThreshold")
    ||  S.equals("Count")
    ||  S.equals("DL")
    ||  S.equals("FatalErrors")
    ||  S.equals("Ftor")
    ||  S.equals("InlineThreshold")
    ||  S.equals("InsertLifetime")
    ||  S.equals("MaxHeaderSize")
    ||  S.equals("MaxIterationAttempt")
    ||  S.equals("NoOpt")
    ||  S.equals("NotOptimized")
    ||  S.equals("OnlyWarnOnFail")
    ||  S.equals("Os")
    ||  S.equals("ReplaceAllVectors")
    ||  S.equals("RequiresDomTree")
    ||  S.equals("Runtime")
    ||  S.equals("ScalarLoadThreshold")
    ||  S.equals("SkipHLSLMat")
    ||  S.equals("StructMemberThreshold")
    ||  S.equals("TIRA")
    ||  S.equals("TLIImpl")
    ||  S.equals("Threshold")
    ||  S.equals("UAVSize")
    ||  S.equals("add-pixel-cost")
    ||  S.equals("bonus-inst-threshold")
    ||  S.equals("checkForDynamicIndexing")
    ||  S.equals("config")
    ||  S.equals("constant-alpha")
    ||  S.equals("constant-blue")
    ||  S.equals("constant-green")
    ||  S.equals("constant-red")
    ||  S.equals("disable-licm-promotion")
    ||  S.equals("enable-load-pre")
    ||  S.equals("enable-pre")
    ||  S.equals("enable-scoped-noalias")
    ||  S.equals("enable-tbaa")
    ||  S.equals("float2int-max-integer-bw")
    ||  S.equals("force-early-z")
    ||  S.equals("force-ssa-updater")
    ||  S.equals("from-binding")
    ||  S.equals("jump-threading-threshold")
    ||  S.equals("keep-name")
    ||  S.equals("likely-branch-weight")
    ||  S.equals("loop-distribute-non-if-convertible")
    ||  S.equals("loop-distribute-verify")
    ||  S.equals("loop-unswitch-threshold")
    ||  S.equals("lowerbitsets-avoid-reuse")
    ||  S.equals("max-recurse-depth")
    ||  S.equals("max-reroll-increment")
    ||  S.equals("maxElements")
    ||  S.equals("mergefunc-sanity")
    ||  S.equals("mod-mode")
    ||  S.equals("no-discriminators")
    ||  S.equals("noloads")
    ||  S.equals("num-pixels")
    ||  S.equals("parameter0")
    ||  S.equals("parameter1")
    ||  S.equals("parameter2")
    ||  S.equals("pragma-unroll-threshold")
    ||  S.equals("prefix")
    ||  S.equals("reroll-num-tolerated-failed-matches")
    ||  S.equals("rewrite-map-file")
    ||  S.equals("rotation-max-header-size")
    ||  S.equals("rt-width")
    ||  S.equals("sample-profile-file")
    ||  S.equals("sample-profile-max-propagate-iterations")
    ||  S.equals("sroa-random-shuffle-slices")
    ||  S.equals("sroa-strict-inbounds")
    ||  S.equals("sv-position-index")
    ||  S.equals("unlikely-branch-weight")
    ||  S.equals("unroll-allow-partial")
    ||  S.equals("unroll-count")
    ||  S.equals("unroll-dynamic-cost-savings-discount")
    ||  S.equals("unroll-max-iteration-count-to-analyze")
    ||  S.equals("unroll-percent-dynamic-cost-saved-threshold")
    ||  S.equals("unroll-runtime")
    ||  S.equals("unroll-threshold")
    ||  S.equals("vector-library")
    ||  S.equals("verify-debug-info");
  // ISPASSOPTIONNAME:END
}

static void FatalErrorHandlerStreamWrite(void *user_data, const std::string& reason, bool gen_crash_diag) {
  raw_ostream *OS = (raw_ostream *)user_data;
  *OS << reason;
  throw std::exception();
}

static HRESULT Utf8ToUtf16CoTaskMalloc(LPCSTR pValue, LPWSTR *ppResult) {
  if (ppResult == nullptr)
    return E_POINTER;
  int count = MultiByteToWideChar(CP_UTF8, 0, pValue, -1, nullptr, 0);
  *ppResult = (wchar_t*)CoTaskMemAlloc(sizeof(wchar_t) * count);
  if (*ppResult == nullptr)
    return E_OUTOFMEMORY;
  MultiByteToWideChar(CP_UTF8, 0, pValue, -1, *ppResult, count);
  return S_OK;
}

class DxcOptimizerPass : public IDxcOptimizerPass {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  LPCSTR m_pOptionName;
  LPCSTR m_pDescription;
  ArrayRef<LPCSTR> m_pArgNames;
  ArrayRef<LPCSTR> m_pArgDescriptions;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcOptimizerPass)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcOptimizerPass>(this, iid, ppvObject);
  }

  HRESULT Initialize(LPCSTR pOptionName, LPCSTR pDescription, ArrayRef<LPCSTR> pArgNames, ArrayRef<LPCSTR> pArgDescriptions) {
    DXASSERT(pArgNames.size() == pArgDescriptions.size(), "else lookup tables are out of alignment");
    m_pOptionName = pOptionName;
    m_pDescription = pDescription;
    m_pArgNames = pArgNames;
    m_pArgDescriptions = pArgDescriptions;
    return S_OK;
  }
  static HRESULT Create(IMalloc *pMalloc, LPCSTR pOptionName, LPCSTR pDescription, ArrayRef<LPCSTR> pArgNames, ArrayRef<LPCSTR> pArgDescriptions, IDxcOptimizerPass **ppResult) {
    CComPtr<DxcOptimizerPass> result;
    *ppResult = nullptr;
    result = DxcOptimizerPass::Alloc(pMalloc);
    IFROOM(result);
    IFR(result->Initialize(pOptionName, pDescription, pArgNames, pArgDescriptions));
    *ppResult = result.Detach();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetOptionName(_COM_Outptr_ LPWSTR *ppResult) override {
    return Utf8ToUtf16CoTaskMalloc(m_pOptionName, ppResult);
  }
  HRESULT STDMETHODCALLTYPE GetDescription(_COM_Outptr_ LPWSTR *ppResult) override {
    return Utf8ToUtf16CoTaskMalloc(m_pDescription, ppResult);
  }

  HRESULT STDMETHODCALLTYPE GetOptionArgCount(_Out_ UINT32 *pCount) override {
    if (!pCount) return E_INVALIDARG;
    *pCount = m_pArgDescriptions.size();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetOptionArgName(UINT32 argIndex, LPWSTR *ppResult) override {
    if (!ppResult) return E_INVALIDARG;
    if (argIndex >= m_pArgNames.size()) return E_INVALIDARG;
    return Utf8ToUtf16CoTaskMalloc(m_pArgNames[argIndex], ppResult);
  }
  HRESULT STDMETHODCALLTYPE GetOptionArgDescription(UINT32 argIndex, LPWSTR *ppResult) override {
    if (!ppResult) return E_INVALIDARG;
    if (argIndex >= m_pArgDescriptions.size()) return E_INVALIDARG;
    return Utf8ToUtf16CoTaskMalloc(m_pArgDescriptions[argIndex], ppResult);
  }
};

class DxcOptimizer : public IDxcOptimizer {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  PassRegistry *m_registry;
  std::vector<const PassInfo *> m_passes;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcOptimizer)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcOptimizer>(this, iid, ppvObject);
  }

  HRESULT Initialize();
  const PassInfo *getPassByID(llvm::AnalysisID PassID);
  const PassInfo *getPassByName(const char *pName);
  HRESULT STDMETHODCALLTYPE GetAvailablePassCount(_Out_ UINT32 *pCount) override {
    return AssignToOut<UINT32>(m_passes.size(), pCount);
  }
  HRESULT STDMETHODCALLTYPE GetAvailablePass(UINT32 index, _COM_Outptr_ IDxcOptimizerPass** ppResult) override;
  HRESULT STDMETHODCALLTYPE RunOptimizer(IDxcBlob *pBlob,
    _In_count_(optionCount) LPCWSTR *ppOptions, UINT32 optionCount,
    _COM_Outptr_ IDxcBlob **ppOutputModule,
    _COM_Outptr_opt_ IDxcBlobEncoding **ppOutputText) override;
};

class CapturePassManager : public llvm::legacy::PassManagerBase {
private:
  SmallVector<Pass *, 64> Passes;
public:
  ~CapturePassManager() {
    for (auto P : Passes) delete P;
  }

  void add(Pass *P) override {
    Passes.push_back(P);
  }

  size_t size() const { return Passes.size(); }
  const char *getPassNameAt(size_t index) const {
    return Passes[index]->getPassName();
  }
  llvm::AnalysisID getPassIDAt(size_t index) const {
    return Passes[index]->getPassID();
  }
};

HRESULT DxcOptimizer::Initialize() {
  try {
    m_registry = PassRegistry::getPassRegistry();

    struct PRL : public PassRegistrationListener {
      std::vector<const PassInfo *> *Passes;
      void passEnumerate(const PassInfo * PI) override {
        DXASSERT(nullptr != PI->getNormalCtor(), "else cannot construct");
        Passes->push_back(PI);
      }
    };
    PRL prl;
    prl.Passes = &this->m_passes;
    m_registry->enumerateWith(&prl);
  }
  CATCH_CPP_RETURN_HRESULT();
  return S_OK;
}

const PassInfo *DxcOptimizer::getPassByID(llvm::AnalysisID PassID) {
  return m_registry->getPassInfo(PassID);
}

const PassInfo *DxcOptimizer::getPassByName(const char *pName) {
  return m_registry->getPassInfo(StringRef(pName));
}

HRESULT STDMETHODCALLTYPE DxcOptimizer::GetAvailablePass(
    UINT32 index, _COM_Outptr_ IDxcOptimizerPass **ppResult) {
  IFR(AssignToOut(nullptr, ppResult));
  if (index >= m_passes.size())
    return E_INVALIDARG;
  return DxcOptimizerPass::Create(
      m_pMalloc, m_passes[index]->getPassArgument(),
      m_passes[index]->getPassName(),
      GetPassArgNames(m_passes[index]->getPassArgument()),
      GetPassArgDescriptions(m_passes[index]->getPassArgument()), ppResult);
}

HRESULT STDMETHODCALLTYPE DxcOptimizer::RunOptimizer(
    IDxcBlob *pBlob, _In_count_(optionCount) LPCWSTR *ppOptions,
    UINT32 optionCount, _COM_Outptr_ IDxcBlob **ppOutputModule,
    _COM_Outptr_opt_ IDxcBlobEncoding **ppOutputText) {
  AssignToOutOpt(nullptr, ppOutputModule);
  AssignToOutOpt(nullptr, ppOutputText);
  if (pBlob == nullptr)
    return E_POINTER;
  if (optionCount > 0 && ppOptions == nullptr)
    return E_POINTER;

  DxcThreadMalloc TM(m_pMalloc);

  // Setup input buffer.
  //
  // The ir parsing requires the buffer to be null terminated. We deal with
  // both source and bitcode input, so the input buffer may not be null
  // terminated; we create a new membuf that copies and appends for this.
  //
  // If we have the beginning of a DXIL program header, skip to the bitcode.
  //
  LLVMContext Context;
  SMDiagnostic Err;
  std::unique_ptr<MemoryBuffer> memBuf;
  std::unique_ptr<Module> M;
  const char * pBlobContent = reinterpret_cast<const char *>(pBlob->GetBufferPointer());
  unsigned blobSize = pBlob->GetBufferSize();
  const DxilProgramHeader *pProgramHeader =
    reinterpret_cast<const DxilProgramHeader *>(pBlobContent);
  if (IsValidDxilProgramHeader(pProgramHeader, blobSize)) {
    std::string DiagStr;
    GetDxilProgramBitcode(pProgramHeader, &pBlobContent, &blobSize);
    M = hlsl::dxilutil::LoadModuleFromBitcode(
      llvm::StringRef(pBlobContent, blobSize), Context, DiagStr);
  }
  else {
    StringRef bufStrRef(pBlobContent, blobSize);
    memBuf = MemoryBuffer::getMemBufferCopy(bufStrRef);
    M = parseIR(memBuf->getMemBufferRef(), Err, Context);
  }

  if (M == nullptr) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  legacy::PassManager ModulePasses;
  legacy::FunctionPassManager FunctionPasses(M.get());
  legacy::PassManagerBase *pPassManager = &ModulePasses;

  try {
    CComPtr<AbstractMemoryStream> pOutputStream;
    CComPtr<IDxcBlob> pOutputBlob;

    IFT(CreateMemoryStream(m_pMalloc, &pOutputStream));
    IFT(pOutputStream.QueryInterface(&pOutputBlob));

    raw_stream_ostream outStream(pOutputStream.p);

    //
    // Consider some differences from opt.exe:
    //
    // Create a new optimization pass for each one specified on the command line
    // as in StandardLinkOpts, OptLevelO1, etc.
    // No target machine, and so no passes get their target machine ctor called.
    // No print-after-each-pass option.
    // No printing of the pass options.
    // No StripDebug support.
    // No verifyModule before starting.
    // Use of PassPipeline for new manager.
    // No TargetInfo.
    // No DataLayout.
    //
    bool OutputAssembly = false;
    bool AnalyzeOnly = false;

    // First gather flags, wherever they may be.
    SmallVector<UINT32, 2> handled;
    for (UINT32 i = 0; i < optionCount; ++i) {
      if (wcseq(L"-S", ppOptions[i])) {
        OutputAssembly = true;
        handled.push_back(i);
        continue;
      }
      if (wcseq(L"-analyze", ppOptions[i])) {
        AnalyzeOnly = true;
        handled.push_back(i);
        continue;
      }
    }

    // TODO: should really use string_table for this once that's available
    std::list<std::string> optionsAnsi;
    SmallVector<PassOption, 2> options;
    for (UINT32 i = 0; i < optionCount; ++i) {
      if (std::find(handled.begin(), handled.end(), i) != handled.end()) {
        continue;
      }

      // Handle some special cases where we can inject a redirected output stream.
      if (wcsstartswith(ppOptions[i], L"-print-module")) {
        LPCWSTR pName = ppOptions[i] + _countof(L"-print-module") - 1;
        std::string Banner;
        if (*pName) {
          IFTARG(*pName != L':' || *pName != L'=');
          ++pName;
          CW2A name8(pName);
          Banner = "MODULE-PRINT ";
          Banner += name8.m_psz;
          Banner += "\n";
        }
        if (pPassManager == &ModulePasses)
          pPassManager->add(llvm::createPrintModulePass(outStream, Banner));
        continue;
      }

      // Handle special switches to toggle per-function prepasses vs. module passes.
      if (wcseq(ppOptions[i], L"-opt-fn-passes")) {
        pPassManager = &FunctionPasses;
        continue;
      }
      if (wcseq(ppOptions[i], L"-opt-mod-passes")) {
        pPassManager = &ModulePasses;
        continue;
      }

      CW2A optName(ppOptions[i], CP_UTF8);
      // The option syntax is
      const char ArgDelim = ',';
      // '-' OPTION_NAME (',' ARG_NAME ('=' ARG_VALUE)?)*
      char *pCursor = optName.m_psz;
      const char *pEnd = optName.m_psz + strlen(optName.m_psz);
      if (*pCursor != '-' && *pCursor != '/') {
        return E_INVALIDARG;
      }
      ++pCursor;
      const char *pOptionNameStart = pCursor;
      while (*pCursor && *pCursor != ArgDelim) {
        ++pCursor;
      }
      *pCursor = '\0';
      const llvm::PassInfo *PassInf = getPassByName(pOptionNameStart);
      if (!PassInf) {
        return E_INVALIDARG;
      }
      while (pCursor < pEnd) {
        // *pCursor is '\0' when we overwrite ',' to get a null-terminated string
        if (*pCursor && *pCursor != ArgDelim) {
          return E_INVALIDARG;
        }
        ++pCursor;
        const char *pArgStart = pCursor;
        while (*pCursor && *pCursor != ArgDelim) {
          ++pCursor;
        }
        StringRef argString = StringRef(pArgStart, pCursor - pArgStart);
        std::pair<StringRef, StringRef> nameValue = argString.split('=');
        if (!IsPassOptionName(nameValue.first)) {
          return E_INVALIDARG;
        }

        PassOption *OptionPos = std::lower_bound(options.begin(), options.end(), nameValue, PassOptionsCompare());
        // If empty, remove if available; otherwise upsert.
        if (nameValue.second.empty()) {
          if (OptionPos != options.end() && OptionPos->first == nameValue.first) {
            options.erase(OptionPos);
          }
        }
        else {
          if (OptionPos != options.end() && OptionPos->first == nameValue.first) {
            OptionPos->second = nameValue.second;
          }
          else {
            options.insert(OptionPos, nameValue);
          }
        }
      }

      DXASSERT(PassInf->getNormalCtor(), "else pass with no default .ctor was added");
      Pass *pass = PassInf->getNormalCtor()();
      pass->setOSOverride(&outStream);
      pass->applyOptions(options);
      options.clear();
      pPassManager->add(pass);
      if (AnalyzeOnly) {
        const bool Quiet = false;
        PassKind Kind = pass->getPassKind();
        switch (Kind) {
        case PT_BasicBlock:
          pPassManager->add(createBasicBlockPassPrinter(PassInf, outStream, Quiet));
          break;
        case PT_Region:
          pPassManager->add(createRegionPassPrinter(PassInf, outStream, Quiet));
          break;
        case PT_Loop:
          pPassManager->add(createLoopPassPrinter(PassInf, outStream, Quiet));
          break;
        case PT_Function:
          pPassManager->add(createFunctionPassPrinter(PassInf, outStream, Quiet));
          break;
        case PT_CallGraphSCC:
          pPassManager->add(createCallGraphPassPrinter(PassInf, outStream, Quiet));
          break;
        default:
          pPassManager->add(createModulePassPrinter(PassInf, outStream, Quiet));
          break;
        }
      }
    }

    ModulePasses.add(createVerifierPass());

    if (OutputAssembly) {
      ModulePasses.add(llvm::createPrintModulePass(outStream));
    }

    // Now that we have all of the passes ready, run them.
    {
      raw_ostream *err_ostream = &outStream;
      ScopedFatalErrorHandler errHandler(FatalErrorHandlerStreamWrite, err_ostream);

      FunctionPasses.doInitialization();
      for (Function &F : *M.get())
        if (!F.isDeclaration())
          FunctionPasses.run(F);
      FunctionPasses.doFinalization();
      ModulePasses.run(*M.get());
    }

    outStream.flush();
    if (ppOutputText != nullptr) {
      IFT(DxcCreateBlobWithEncodingSet(pOutputBlob, CP_UTF8, ppOutputText));
    }
    if (ppOutputModule != nullptr) {
      CComPtr<AbstractMemoryStream> pProgramStream;
      IFT(CreateMemoryStream(m_pMalloc, &pProgramStream));
      {
        raw_stream_ostream outStream(pProgramStream.p);
        WriteBitcodeToFile(M.get(), outStream, true);
      }
      IFT(pProgramStream.QueryInterface(ppOutputModule));
    }
  }
  CATCH_CPP_RETURN_HRESULT();

  return S_OK;
}

HRESULT CreateDxcOptimizer(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  CComPtr<DxcOptimizer> result = DxcOptimizer::Alloc(DxcGetThreadMallocNoRef());
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }
  IFR(result->Initialize());

  return result.p->QueryInterface(riid, ppv);
}
