//===--- BackendUtil.cpp - LLVM Backend Utilities -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/CodeGen/BackendUtil.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Frontend/CodeGenOptions.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/Utils.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/SchedulerRegistry.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetSubtargetInfo.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/ObjCARC.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/SymbolRewriter.h"
#include <memory>
#include <set>
#include <sstream>
#include <iomanip>
#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/HLSL/DxilGenerationPass.h" // HLSL Change
#include "dxc/HLSL/HLMatrixLowerPass.h"  // HLSL Change
#include "dxc/Support/WinIncludes.h"

using namespace clang;
using namespace llvm;

namespace {

class EmitAssemblyHelper {
  DiagnosticsEngine &Diags;
  const CodeGenOptions &CodeGenOpts;
  const clang::TargetOptions &TargetOpts;
  const LangOptions &LangOpts;
  Module *TheModule;
  // HLSL Changes Start
  std::string CodeGenPassesConfig;
  std::string PerModulePassesConfig;
  std::string PerFunctionPassesConfig;
  mutable llvm::raw_string_ostream CodeGenPassesConfigOS;
  mutable llvm::raw_string_ostream PerModulePassesConfigOS;
  mutable llvm::raw_string_ostream PerFunctionPassesConfigOS;
  // HLSL Changes End

  Timer CodeGenerationTime;

  mutable legacy::PassManager *CodeGenPasses;
  mutable legacy::PassManager *PerModulePasses;
  mutable legacy::FunctionPassManager *PerFunctionPasses;

private:
  TargetIRAnalysis getTargetIRAnalysis() const {
    if (TM)
      return TM->getTargetIRAnalysis();

    return TargetIRAnalysis();
  }

  legacy::PassManager *getCodeGenPasses() const {
    if (!CodeGenPasses) {
      CodeGenPasses = new legacy::PassManager();
      CodeGenPasses->TrackPassOS = &CodeGenPassesConfigOS;
      CodeGenPasses->add(
          createTargetTransformInfoWrapperPass(getTargetIRAnalysis()));
    }
    return CodeGenPasses;
  }

  legacy::PassManager *getPerModulePasses() const {
    if (!PerModulePasses) {
      PerModulePasses = new legacy::PassManager();
      PerModulePasses->TrackPassOS = &PerModulePassesConfigOS;
      PerModulePasses->add(
          createTargetTransformInfoWrapperPass(getTargetIRAnalysis()));
    }
    return PerModulePasses;
  }

  legacy::FunctionPassManager *getPerFunctionPasses() const {
    if (!PerFunctionPasses) {
      PerFunctionPasses = new legacy::FunctionPassManager(TheModule);
      PerFunctionPasses->TrackPassOS = &PerFunctionPassesConfigOS;
      PerFunctionPasses->add(
          createTargetTransformInfoWrapperPass(getTargetIRAnalysis()));
    }
    return PerFunctionPasses;
  }

  void CreatePasses();

  /// Generates the TargetMachine.
  /// Returns Null if it is unable to create the target machine.
  /// Some of our clang tests specify triples which are not built
  /// into clang. This is okay because these tests check the generated
  /// IR, and they require DataLayout which depends on the triple.
  /// In this case, we allow this method to fail and not report an error.
  /// When MustCreateTM is used, we print an error if we are unable to load
  /// the requested target.
  TargetMachine *CreateTargetMachine(bool MustCreateTM);

  /// Add passes necessary to emit assembly or LLVM IR.
  ///
  /// \return True on success.
  bool AddEmitPasses(BackendAction Action, raw_pwrite_stream &OS);

public:
  EmitAssemblyHelper(DiagnosticsEngine &_Diags,
                     const CodeGenOptions &CGOpts,
                     const clang::TargetOptions &TOpts,
                     const LangOptions &LOpts,
                     Module *M)
    : Diags(_Diags), CodeGenOpts(CGOpts), TargetOpts(TOpts), LangOpts(LOpts),
      TheModule(M),
      // HLSL Changes Start
      CodeGenPassesConfigOS(CodeGenPassesConfig),
      PerModulePassesConfigOS(PerModulePassesConfig),
      PerFunctionPassesConfigOS(PerFunctionPassesConfig),
      // HLSL Changes End
      CodeGenerationTime("Code Generation Time"),
      CodeGenPasses(nullptr), PerModulePasses(nullptr),
      PerFunctionPasses(nullptr) {}

  ~EmitAssemblyHelper() {
    delete CodeGenPasses;
    delete PerModulePasses;
    delete PerFunctionPasses;
    if (CodeGenOpts.DisableFree)
      BuryPointer(std::move(TM));
  }

  std::unique_ptr<TargetMachine> TM;

  void EmitAssembly(BackendAction Action, raw_pwrite_stream *OS);
};

// We need this wrapper to access LangOpts and CGOpts from extension functions
// that we add to the PassManagerBuilder.
class PassManagerBuilderWrapper : public PassManagerBuilder {
public:
  PassManagerBuilderWrapper(const CodeGenOptions &CGOpts,
                            const LangOptions &LangOpts)
      : PassManagerBuilder(), CGOpts(CGOpts), LangOpts(LangOpts) {}
  const CodeGenOptions &getCGOpts() const { return CGOpts; }
  const LangOptions &getLangOpts() const { return LangOpts; }
private:
  const CodeGenOptions &CGOpts;
  const LangOptions &LangOpts;
};

}

#ifdef MS_ENABLE_OBJCARC // HLSL Change

static void addObjCARCAPElimPass(const PassManagerBuilder &Builder, PassManagerBase &PM) {
  if (Builder.OptLevel > 0)
    PM.add(createObjCARCAPElimPass());
}

static void addObjCARCExpandPass(const PassManagerBuilder &Builder, PassManagerBase &PM) {
  if (Builder.OptLevel > 0)
    PM.add(createObjCARCExpandPass());
}

static void addObjCARCOptPass(const PassManagerBuilder &Builder, PassManagerBase &PM) {
  if (Builder.OptLevel > 0)
    PM.add(createObjCARCOptPass());
}

#endif // MS_ENABLE_OBJCARC - HLSL Change

static void addSampleProfileLoaderPass(const PassManagerBuilder &Builder,
                                       legacy::PassManagerBase &PM) {
  const PassManagerBuilderWrapper &BuilderWrapper =
      static_cast<const PassManagerBuilderWrapper &>(Builder);
  const CodeGenOptions &CGOpts = BuilderWrapper.getCGOpts();
  PM.add(createSampleProfileLoaderPass(CGOpts.SampleProfileFile));
}

static void addAddDiscriminatorsPass(const PassManagerBuilder &Builder,
                                     legacy::PassManagerBase &PM) {
  PM.add(createAddDiscriminatorsPass());
}

#ifdef MS_ENABLE_OBJCARC // HLSL Change
static void addBoundsCheckingPass(const PassManagerBuilder &Builder,
                                    legacy::PassManagerBase &PM) {
  PM.add(createBoundsCheckingPass());
}
#endif // MS_ENABLE_OBJCARC // HLSL Change

#ifdef MS_ENABLE_INSTR // HLSL Change

static void addSanitizerCoveragePass(const PassManagerBuilder &Builder,
                                     legacy::PassManagerBase &PM) {
  const PassManagerBuilderWrapper &BuilderWrapper =
      static_cast<const PassManagerBuilderWrapper&>(Builder);
  const CodeGenOptions &CGOpts = BuilderWrapper.getCGOpts();
  SanitizerCoverageOptions Opts;
  Opts.CoverageType =
      static_cast<SanitizerCoverageOptions::Type>(CGOpts.SanitizeCoverageType);
  Opts.IndirectCalls = CGOpts.SanitizeCoverageIndirectCalls;
  Opts.TraceBB = CGOpts.SanitizeCoverageTraceBB;
  Opts.TraceCmp = CGOpts.SanitizeCoverageTraceCmp;
  Opts.Use8bitCounters = CGOpts.SanitizeCoverage8bitCounters;
  PM.add(createSanitizerCoverageModulePass(Opts));
}

static void addAddressSanitizerPasses(const PassManagerBuilder &Builder,
                                      legacy::PassManagerBase &PM) {
  PM.add(createAddressSanitizerFunctionPass(/*CompileKernel*/false));
  PM.add(createAddressSanitizerModulePass(/*CompileKernel*/false));
}

static void addKernelAddressSanitizerPasses(const PassManagerBuilder &Builder,
                                            legacy::PassManagerBase &PM) {
  PM.add(createAddressSanitizerFunctionPass(/*CompileKernel*/true));
  PM.add(createAddressSanitizerModulePass(/*CompileKernel*/true));
}

static void addMemorySanitizerPass(const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
  const PassManagerBuilderWrapper &BuilderWrapper =
      static_cast<const PassManagerBuilderWrapper&>(Builder);
  const CodeGenOptions &CGOpts = BuilderWrapper.getCGOpts();
  PM.add(createMemorySanitizerPass(CGOpts.SanitizeMemoryTrackOrigins));

  // MemorySanitizer inserts complex instrumentation that mostly follows
  // the logic of the original code, but operates on "shadow" values.
  // It can benefit from re-running some general purpose optimization passes.
  if (Builder.OptLevel > 0) {
    PM.add(createEarlyCSEPass());
    PM.add(createReassociatePass());
    PM.add(createLICMPass());
    PM.add(createGVNPass());
    PM.add(createInstructionCombiningPass());
    PM.add(createDeadStoreEliminationPass());
  }
}

static void addThreadSanitizerPass(const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
  PM.add(createThreadSanitizerPass());
}

static void addDataFlowSanitizerPass(const PassManagerBuilder &Builder,
                                     legacy::PassManagerBase &PM) {
  const PassManagerBuilderWrapper &BuilderWrapper =
      static_cast<const PassManagerBuilderWrapper&>(Builder);
  const LangOptions &LangOpts = BuilderWrapper.getLangOpts();
  PM.add(createDataFlowSanitizerPass(LangOpts.SanitizerBlacklistFiles));
}

#endif // MS_ENABLE_INSTR - HLSL Change

static TargetLibraryInfoImpl *createTLII(llvm::Triple &TargetTriple,
                                         const CodeGenOptions &CodeGenOpts) {
  TargetLibraryInfoImpl *TLII = new TargetLibraryInfoImpl(TargetTriple);
  if (!CodeGenOpts.SimplifyLibCalls)
    TLII->disableAllFunctions();

  switch (CodeGenOpts.getVecLib()) {
  case CodeGenOptions::Accelerate:
    TLII->addVectorizableFunctionsFromVecLib(TargetLibraryInfoImpl::Accelerate);
    break;
  default:
    break;
  }
  return TLII;
}

static void addSymbolRewriterPass(const CodeGenOptions &Opts,
                                  legacy::PassManager *MPM) {
  llvm::SymbolRewriter::RewriteDescriptorList DL;

  llvm::SymbolRewriter::RewriteMapParser MapParser;
  for (const auto &MapFile : Opts.RewriteMapFiles)
    MapParser.parse(MapFile, &DL);

  MPM->add(createRewriteSymbolsPass(DL));
}

void EmitAssemblyHelper::CreatePasses() {
  unsigned OptLevel = CodeGenOpts.OptimizationLevel;
  CodeGenOptions::InliningMethod Inlining = CodeGenOpts.getInlining();

  // Handle disabling of LLVM optimization, where we want to preserve the
  // internal module before any optimization.
  if (CodeGenOpts.DisableLLVMOpts || CodeGenOpts.HLSLHighLevel) { // HLSL Change
    OptLevel = 0;
    // HLSL Change Begins.
    // HLSL always inline.
    if (!LangOpts.HLSL || CodeGenOpts.HLSLHighLevel)
    Inlining = CodeGenOpts.NoInlining;
    // HLSL Change Ends.
  }

  PassManagerBuilderWrapper PMBuilder(CodeGenOpts, LangOpts);
  PMBuilder.OptLevel = OptLevel;
  PMBuilder.SizeLevel = CodeGenOpts.OptimizeSize;
  PMBuilder.BBVectorize = CodeGenOpts.VectorizeBB;
  PMBuilder.SLPVectorize = CodeGenOpts.VectorizeSLP;
  PMBuilder.LoopVectorize = CodeGenOpts.VectorizeLoop;
  PMBuilder.HLSLHighLevel = CodeGenOpts.HLSLHighLevel; // HLSL Change
  PMBuilder.HLSLExtensionsCodeGen = CodeGenOpts.HLSLExtensionsCodegen.get(); // HLSL Change
  PMBuilder.HLSLResMayAlias = CodeGenOpts.HLSLResMayAlias; // HLSL Change

  PMBuilder.DisableUnitAtATime = !CodeGenOpts.UnitAtATime;
  PMBuilder.DisableUnrollLoops = !CodeGenOpts.UnrollLoops;
  PMBuilder.MergeFunctions = CodeGenOpts.MergeFunctions;
  PMBuilder.PrepareForLTO = CodeGenOpts.PrepareForLTO;
  PMBuilder.RerollLoops = CodeGenOpts.RerollLoops;

  PMBuilder.addExtension(PassManagerBuilder::EP_EarlyAsPossible,
                         addAddDiscriminatorsPass);

  if (!CodeGenOpts.SampleProfileFile.empty())
    PMBuilder.addExtension(PassManagerBuilder::EP_EarlyAsPossible,
                           addSampleProfileLoaderPass);

  // In ObjC ARC mode, add the main ARC optimization passes.
#ifdef MS_ENABLE_OBJCARC // HLSL Change
  if (LangOpts.ObjCAutoRefCount) {
    PMBuilder.addExtension(PassManagerBuilder::EP_EarlyAsPossible,
                           addObjCARCExpandPass);
    PMBuilder.addExtension(PassManagerBuilder::EP_ModuleOptimizerEarly,
                           addObjCARCAPElimPass);
    PMBuilder.addExtension(PassManagerBuilder::EP_ScalarOptimizerLate,
                           addObjCARCOptPass);
  }

  if (LangOpts.Sanitize.has(SanitizerKind::LocalBounds)) {
    PMBuilder.addExtension(PassManagerBuilder::EP_ScalarOptimizerLate,
                           addBoundsCheckingPass);
    PMBuilder.addExtension(PassManagerBuilder::EP_EnabledOnOptLevel0,
                           addBoundsCheckingPass);
  }
#endif // MS_ENABLE_OBJCARC - HLSL Change

#ifdef MS_ENABLE_INSTR // HLSL Change

  if (CodeGenOpts.SanitizeCoverageType ||
      CodeGenOpts.SanitizeCoverageIndirectCalls ||
      CodeGenOpts.SanitizeCoverageTraceCmp) {
    PMBuilder.addExtension(PassManagerBuilder::EP_OptimizerLast,
                           addSanitizerCoveragePass);
    PMBuilder.addExtension(PassManagerBuilder::EP_EnabledOnOptLevel0,
                           addSanitizerCoveragePass);
  }

  if (LangOpts.Sanitize.has(SanitizerKind::Address)) {
    PMBuilder.addExtension(PassManagerBuilder::EP_OptimizerLast,
                           addAddressSanitizerPasses);
    PMBuilder.addExtension(PassManagerBuilder::EP_EnabledOnOptLevel0,
                           addAddressSanitizerPasses);
  }

  if (LangOpts.Sanitize.has(SanitizerKind::KernelAddress)) {
    PMBuilder.addExtension(PassManagerBuilder::EP_OptimizerLast,
                           addKernelAddressSanitizerPasses);
    PMBuilder.addExtension(PassManagerBuilder::EP_EnabledOnOptLevel0,
                           addKernelAddressSanitizerPasses);
  }

  if (LangOpts.Sanitize.has(SanitizerKind::Memory)) {
    PMBuilder.addExtension(PassManagerBuilder::EP_OptimizerLast,
                           addMemorySanitizerPass);
    PMBuilder.addExtension(PassManagerBuilder::EP_EnabledOnOptLevel0,
                           addMemorySanitizerPass);
  }

  if (LangOpts.Sanitize.has(SanitizerKind::Thread)) {
    PMBuilder.addExtension(PassManagerBuilder::EP_OptimizerLast,
                           addThreadSanitizerPass);
    PMBuilder.addExtension(PassManagerBuilder::EP_EnabledOnOptLevel0,
                           addThreadSanitizerPass);
  }

  if (LangOpts.Sanitize.has(SanitizerKind::DataFlow)) {
    PMBuilder.addExtension(PassManagerBuilder::EP_OptimizerLast,
                           addDataFlowSanitizerPass);
    PMBuilder.addExtension(PassManagerBuilder::EP_EnabledOnOptLevel0,
                           addDataFlowSanitizerPass);
  }

#endif // MS_ENABLE_INSTR - HLSL Change

  // Figure out TargetLibraryInfo.
  Triple TargetTriple(TheModule->getTargetTriple());
  PMBuilder.LibraryInfo = createTLII(TargetTriple, CodeGenOpts);

  switch (Inlining) {
  case CodeGenOptions::NoInlining: break;
  case CodeGenOptions::NormalInlining: {
    PMBuilder.Inliner =
        createFunctionInliningPass(OptLevel, CodeGenOpts.OptimizeSize);
    break;
  }
  case CodeGenOptions::OnlyAlwaysInlining:
    // Respect always_inline.
    if (OptLevel == 0)
      // Do not insert lifetime intrinsics at -O0.
      PMBuilder.Inliner = createAlwaysInlinerPass(false);
    else
      PMBuilder.Inliner = createAlwaysInlinerPass();
    break;
  }

  // Set up the per-function pass manager.
  legacy::FunctionPassManager *FPM = getPerFunctionPasses();
  if (CodeGenOpts.VerifyModule)
    FPM->add(createVerifierPass());
  PMBuilder.populateFunctionPassManager(*FPM);

  // Set up the per-module pass manager.
  legacy::PassManager *MPM = getPerModulePasses();
  if (!CodeGenOpts.RewriteMapFiles.empty())
    addSymbolRewriterPass(CodeGenOpts, MPM);

#ifdef MS_ENABLE_INSTR // HLSL Change
  if (!CodeGenOpts.DisableGCov &&
      (CodeGenOpts.EmitGcovArcs || CodeGenOpts.EmitGcovNotes)) {
    // Not using 'GCOVOptions::getDefault' allows us to avoid exiting if
    // LLVM's -default-gcov-version flag is set to something invalid.
    GCOVOptions Options;
    Options.EmitNotes = CodeGenOpts.EmitGcovNotes;
    Options.EmitData = CodeGenOpts.EmitGcovArcs;
    memcpy(Options.Version, CodeGenOpts.CoverageVersion, 4);
    Options.UseCfgChecksum = CodeGenOpts.CoverageExtraChecksum;
    Options.NoRedZone = CodeGenOpts.DisableRedZone;
    Options.FunctionNamesInData =
        !CodeGenOpts.CoverageNoFunctionNamesInData;
    Options.ExitBlockBeforeBody = CodeGenOpts.CoverageExitBlockBeforeBody;
    MPM->add(createGCOVProfilerPass(Options));
    if (CodeGenOpts.getDebugInfo() == CodeGenOptions::NoDebugInfo)
      MPM->add(createStripSymbolsPass(true));
  }

  if (CodeGenOpts.ProfileInstrGenerate) {
    InstrProfOptions Options;
    Options.NoRedZone = CodeGenOpts.DisableRedZone;
    Options.InstrProfileOutput = CodeGenOpts.InstrProfileOutput;
    MPM->add(createInstrProfilingPass(Options));
  }
#endif

  PMBuilder.populateModulePassManager(*MPM);
}

TargetMachine *EmitAssemblyHelper::CreateTargetMachine(bool MustCreateTM) {
  // Create the TargetMachine for generating code.
  std::string Error;
  std::string Triple = TheModule->getTargetTriple();
  const llvm::Target *TheTarget = TargetRegistry::lookupTarget(Triple, Error);
  if (!TheTarget) {
    if (MustCreateTM)
      Diags.Report(diag::err_fe_unable_to_create_target) << Error;
    return nullptr;
  }

  unsigned CodeModel =
    llvm::StringSwitch<unsigned>(CodeGenOpts.CodeModel)
      .Case("small", llvm::CodeModel::Small)
      .Case("kernel", llvm::CodeModel::Kernel)
      .Case("medium", llvm::CodeModel::Medium)
      .Case("large", llvm::CodeModel::Large)
      .Case("default", llvm::CodeModel::Default)
      .Default(~0u);
  assert(CodeModel != ~0u && "invalid code model!");
  llvm::CodeModel::Model CM = static_cast<llvm::CodeModel::Model>(CodeModel);

#if 0 // HLSL Change - no global state mutation on per-instance work
  SmallVector<const char *, 16> BackendArgs;
  BackendArgs.push_back("clang"); // Fake program name.
  if (!CodeGenOpts.DebugPass.empty()) {
    BackendArgs.push_back("-debug-pass");
    BackendArgs.push_back(CodeGenOpts.DebugPass.c_str());
  }
  if (!CodeGenOpts.LimitFloatPrecision.empty()) {
    BackendArgs.push_back("-limit-float-precision");
    BackendArgs.push_back(CodeGenOpts.LimitFloatPrecision.c_str());
  }
  for (unsigned i = 0, e = CodeGenOpts.BackendOptions.size(); i != e; ++i)
    BackendArgs.push_back(CodeGenOpts.BackendOptions[i].c_str());
  BackendArgs.push_back(nullptr);
  llvm::cl::ParseCommandLineOptions(BackendArgs.size() - 1,
                                    BackendArgs.data());
#endif // HLSL Change

  std::string FeaturesStr;
  if (!TargetOpts.Features.empty()) {
#if 0 // HLSL Change
    SubtargetFeatures Features;
    for (const std::string &Feature : TargetOpts.Features)
      Features.AddFeature(Feature);
    FeaturesStr = Features.getString();
#endif // HLSL Change
  }

  llvm::Reloc::Model RM = llvm::Reloc::Default;
  if (CodeGenOpts.RelocationModel == "static") {
    RM = llvm::Reloc::Static;
  } else if (CodeGenOpts.RelocationModel == "pic") {
    RM = llvm::Reloc::PIC_;
  } else {
    assert(CodeGenOpts.RelocationModel == "dynamic-no-pic" &&
           "Invalid PIC model!");
    RM = llvm::Reloc::DynamicNoPIC;
  }

  CodeGenOpt::Level OptLevel = CodeGenOpt::Default;
  switch (CodeGenOpts.OptimizationLevel) {
  default: break;
  case 0: OptLevel = CodeGenOpt::None; break;
  case 3: OptLevel = CodeGenOpt::Aggressive; break;
  }

  llvm::TargetOptions Options;

  if (!TargetOpts.Reciprocals.empty())
    Options.Reciprocals = TargetRecip(TargetOpts.Reciprocals);

  Options.ThreadModel =
    llvm::StringSwitch<llvm::ThreadModel::Model>(CodeGenOpts.ThreadModel)
      .Case("posix", llvm::ThreadModel::POSIX)
      .Case("single", llvm::ThreadModel::Single);

  if (CodeGenOpts.DisableIntegratedAS)
    Options.DisableIntegratedAS = true;

  if (CodeGenOpts.CompressDebugSections)
    Options.CompressDebugSections = true;

  if (CodeGenOpts.UseInitArray)
    Options.UseInitArray = true;

  // Set float ABI type.
  if (CodeGenOpts.FloatABI == "soft" || CodeGenOpts.FloatABI == "softfp")
    Options.FloatABIType = llvm::FloatABI::Soft;
  else if (CodeGenOpts.FloatABI == "hard")
    Options.FloatABIType = llvm::FloatABI::Hard;
  else {
    assert(CodeGenOpts.FloatABI.empty() && "Invalid float abi!");
    Options.FloatABIType = llvm::FloatABI::Default;
  }

  // Set FP fusion mode.
  switch (CodeGenOpts.getFPContractMode()) {
  case CodeGenOptions::FPC_Off:
    Options.AllowFPOpFusion = llvm::FPOpFusion::Strict;
    break;
  case CodeGenOptions::FPC_On:
    Options.AllowFPOpFusion = llvm::FPOpFusion::Standard;
    break;
  case CodeGenOptions::FPC_Fast:
    Options.AllowFPOpFusion = llvm::FPOpFusion::Fast;
    break;
  }

  Options.LessPreciseFPMADOption = CodeGenOpts.LessPreciseFPMAD;
  Options.NoInfsFPMath = CodeGenOpts.NoInfsFPMath;
  Options.NoNaNsFPMath = CodeGenOpts.NoNaNsFPMath;
  Options.NoZerosInBSS = CodeGenOpts.NoZeroInitializedInBSS;
  Options.UnsafeFPMath = CodeGenOpts.UnsafeFPMath;
  Options.StackAlignmentOverride = CodeGenOpts.StackAlignment;
  Options.PositionIndependentExecutable = LangOpts.PIELevel != 0;
  Options.FunctionSections = CodeGenOpts.FunctionSections;
  Options.DataSections = CodeGenOpts.DataSections;
  Options.UniqueSectionNames = CodeGenOpts.UniqueSectionNames;

  Options.MCOptions.MCRelaxAll = CodeGenOpts.RelaxAll;
  Options.MCOptions.MCSaveTempLabels = CodeGenOpts.SaveTempLabels;
  Options.MCOptions.MCUseDwarfDirectory = !CodeGenOpts.NoDwarfDirectoryAsm;
  Options.MCOptions.MCNoExecStack = CodeGenOpts.NoExecStack;
  Options.MCOptions.MCFatalWarnings = CodeGenOpts.FatalWarnings;
  Options.MCOptions.AsmVerbose = CodeGenOpts.AsmVerbose;
  Options.MCOptions.ABIName = TargetOpts.ABI;

  TargetMachine *TM = TheTarget->createTargetMachine(Triple, TargetOpts.CPU,
                                                     FeaturesStr, Options,
                                                     RM, CM, OptLevel);

  return TM;
}

bool EmitAssemblyHelper::AddEmitPasses(BackendAction Action,
                                       raw_pwrite_stream &OS) {

  // Create the code generator passes.
  legacy::PassManager *PM = getCodeGenPasses();

  // Add LibraryInfo.
  llvm::Triple TargetTriple(TheModule->getTargetTriple());
  std::unique_ptr<TargetLibraryInfoImpl> TLII(
      createTLII(TargetTriple, CodeGenOpts));
  PM->add(new TargetLibraryInfoWrapperPass(*TLII));

  // Normal mode, emit a .s or .o file by running the code generator. Note,
  // this also adds codegenerator level optimization passes.
  TargetMachine::CodeGenFileType CGFT = TargetMachine::CGFT_AssemblyFile;
  if (Action == Backend_EmitObj)
    CGFT = TargetMachine::CGFT_ObjectFile;
  else if (Action == Backend_EmitMCNull)
    CGFT = TargetMachine::CGFT_Null;
  else
    assert(Action == Backend_EmitAssembly && "Invalid action!");

  // Add ObjC ARC final-cleanup optimizations. This is done as part of the
  // "codegen" passes so that it isn't run multiple times when there is
  // inlining happening.
#ifdef MS_ENABLE_OBJCARC // HLSL Change
  if (CodeGenOpts.OptimizationLevel > 0)
    PM->add(createObjCARCContractPass());
#endif // HLSL Change

  if (TM->addPassesToEmitFile(*PM, OS, CGFT,
                              /*DisableVerify=*/!CodeGenOpts.VerifyModule)) {
    Diags.Report(diag::err_fe_unable_to_interface_with_target);
    return false;
  }

  return true;
}

static bool usesUavResource(CallInst* call) {
  CallInst* createHandle = dyn_cast<CallInst>(call->getArgOperand(1));
  assert(createHandle);
  assert(createHandle->getCalledFunction()->getName().startswith("dx.op.createHandle"));
  ConstantInt* resourceClass = dyn_cast<ConstantInt>(createHandle->getArgOperand(1));
  if( (nullptr != resourceClass) && (resourceClass->getZExtValue() == 1) )
      return true;

  return false;
}

// Rematerialization logic of future compilation should imply basic to moderate optimizations wrt live values.
static bool canRemat(Instruction* inst) {
  if( isa<GetElementPtrInst>( inst ) )
      return true;
  else if( isa<AllocaInst>( inst ) )
      return true;
  else if( isa<LoadInst>( inst ) )
      return true;

  if( CallInst* call = dyn_cast<CallInst>(inst) ) {
      if( call->getCalledFunction()->getName().startswith( "dx.op.bufferLoad" ) && !usesUavResource( call ) )
          return true;
      else if (call->getCalledFunction()->getName().startswith("dx.op.textureLoad") && !usesUavResource(call))
          return true;
      else if (call->getCalledFunction()->getName().startswith("dx.op.rawBufferLoad") && !usesUavResource(call))
          return true;
      else if (call->getCalledFunction()->getName().startswith("dx.op.createHandleForLib"))
          return true;
  }

  return false;
}

// Full path locations can be complicated and some projects have strange behavior. Customize accordingly.
static std::string formatSourceLocation( const std::string& RootPath, const std::string& Location, int lineNumber, bool isUE4Path )
{
  std::string fullPath = "";

  // Only add the root path if one does not already exist for Location.
  if( Location.find( ':' ) == StringRef::npos )
      fullPath = RootPath;

  fullPath += Location + "(" + std::to_string(lineNumber) + ")"; 

  std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

  // For some reason UE4 removes the "Shaders" folder from paths but this is required for source linking; inject it back in if necessary.
  if( isUE4Path )
  {
      int pos = fullPath.find( "/Engine" );
      if( pos == StringRef::npos )
      {
          // This should not happen, but UE4 will include files with no folder and cannot be traced back to a source file on disk.
          // Only solution for now is to return the Location, which is often just the filename.
          return Location;
      }
      fullPath.insert( pos + 7, "/Shaders" );
  }

  return fullPath;
}

template<class T, class C>
static std::vector<T> sortInstructions(C& c)
{
  std::vector<T> sorted;

  for (T inst : c)
      sorted.push_back(inst);

  if (sorted.empty())
      return sorted;

  DominatorTree DT;
  DT.recalculate(*sorted[0]->getParent()->getParent());

  std::sort(sorted.begin(), sorted.end(), [&] (T a, T b) -> bool {
      if (DT.dominates(a->getParent(), b->getParent()))
          return true;
      else if (DT.dominates(b->getParent(), a->getParent()))
          return false;

      Function::iterator itA( a->getParent() );
      Function::iterator itB( b->getParent() );
      size_t ad = std::distance( a->getParent()->getParent()->begin(), itA );
      size_t bd = std::distance( a->getParent()->getParent()->begin(), itB );

      return ad < bd;
  });

  return sorted;
}

// Determine live values that cross TraceRay calls and cannot be assumed to be rematerializable.
static void printLiveValues(Module* program, Function* main, raw_ostream &PrettyStr, raw_ostream &VSStr, std::string RootPath, bool isUE4Path) {
  std::map<CallInst *, std::vector<Instruction *>> spillsPerTraceCall;

  // Find TraceRay call sites.
  std::set<CallInst *> callSites;

  for (Function &F : *program) {

    if (!F.getName().startswith("dx.op.traceRay"))
      continue;

    for (User *user : F.users()) {
      CallInst *call = dyn_cast<CallInst>(user);
      if (call && call->getParent()->getParent() == main)
        callSites.insert(call);
    }
  }

  // Determine live values across each call.
  std::set<BasicBlock *> phiSelectorSpills;

  // Prepare Debug Info.
  DebugInfoFinder DIF;
  DIF.processModule(*program);

  for (CallInst *callSite : callSites) {
    // Determine CFG edges that can lead to this call site.
    std::set<std::pair<Instruction *, Instruction *>> predCfgEdges;
    {
      BasicBlock *BB = callSite->getParent();
      std::vector<BasicBlock *> worklist;

      for (;;) {
        for (auto i = llvm::pred_begin(BB); i != pred_end(BB); ++i) {
          BasicBlock *pred = *i;
          std::pair<Instruction *, Instruction *> edge =
              std::make_pair(pred->getTerminator(), BB->begin());

          if (!predCfgEdges.count(edge)) {
            predCfgEdges.insert(edge);
            worklist.push_back(pred);
          }
        }

        if (worklist.empty())
          break;

        BB = worklist.back();
        worklist.pop_back();
      }
    }

    // Determine known branch conditions based on CFG edges.
    // Prunes pred CFG edges based on invariants iteratively.
    std::set<Value *> conditionsKnownTrue;
    std::set<Value *> conditionsKnownFalse;

    for (;;) {
      bool change = false;
      for (std::pair<Instruction *, Instruction *> edge :
           std::set<std::pair<Instruction *, Instruction *>>(predCfgEdges)) {
        BranchInst *branch = dyn_cast<BranchInst>(edge.first);

        if (!branch || !branch->isConditional())
          continue;

        assert(branch->getNumSuccessors() == 2);

        Instruction *taken = branch->getSuccessor(0)->begin();
        Instruction *notTaken = branch->getSuccessor(1)->begin();

        std::pair<Instruction *, Instruction *> takenEdge =
            std::make_pair(edge.first, taken);
        std::pair<Instruction *, Instruction *> notTakenEdge =
            std::make_pair(edge.first, notTaken);

        int count =
            predCfgEdges.count(takenEdge) + predCfgEdges.count(notTakenEdge);

        if (!count)
          continue;

        if (count == 2) {
          if (conditionsKnownTrue.count(branch->getCondition())) {
            predCfgEdges.erase(notTakenEdge);
            change = true;
          }
          if (conditionsKnownFalse.count(branch->getCondition())) {
            predCfgEdges.erase(takenEdge);
            change = true;
          }
          continue;
        }

        if (predCfgEdges.count(takenEdge)) {
          if (!conditionsKnownTrue.count(branch->getCondition())) {
            conditionsKnownTrue.insert(branch->getCondition());
            change = true;
          }
        } else {
          if (!conditionsKnownFalse.count(branch->getCondition())) {
            conditionsKnownFalse.insert(branch->getCondition());
            change = true;
          }
        }
      }

      std::vector<PHINode *> phiConditions;

      for (Value *condition : conditionsKnownTrue) {
        PHINode *phi = dyn_cast<PHINode>(condition);
        if (phi)
          phiConditions.push_back(phi);
      }

      for (Value *condition : conditionsKnownFalse) {
        PHINode *phi = dyn_cast<PHINode>(condition);
        if (phi)
          phiConditions.push_back(phi);
      }

      // Propagate PHI conditions to incoming if only a single incoming.
      for (PHINode *phi : phiConditions) {
        unsigned numIncoming = 0;
        Value *incomingValue = nullptr;

        for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
          std::pair<Instruction *, Instruction *> edge =
              std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
                             phi->getParent()->begin());

          // This was defined before the trace call.
          // So we can check predCfgEdges for valid incoming values.
          if (predCfgEdges.count(edge)) {
            ++numIncoming;
            incomingValue = phi->getIncomingValue(i);
          }
        }

        assert(numIncoming);

        if (numIncoming == 1) {
          if (conditionsKnownFalse.count(incomingValue) ==
                  conditionsKnownFalse.count(phi) &&
              conditionsKnownTrue.count(incomingValue) ==
                  conditionsKnownTrue.count(phi))
            continue;

          change = true;

          if (conditionsKnownTrue.count(phi))
            conditionsKnownTrue.insert(incomingValue);
          else
            conditionsKnownFalse.insert(incomingValue);
        }
      }
#if !defined(NDEBUG)
      for (Value *condition : conditionsKnownTrue)
        assert(!conditionsKnownFalse.count(condition));

      for (Value *condition : conditionsKnownFalse)
        assert(!conditionsKnownTrue.count(condition));
#endif

      if (!change)
        break;
    }

    // Traverse each control flow edge separately and gather up seen defs along
    // the way. By keeping track of the from-block, we can select the right
    // incoming value at PHIs.
    std::set<Instruction *> liveValues;
    {
      Instruction *inst = callSite->getNextNode();

      BasicBlock *fromBlock = nullptr;
      std::set<Instruction *> defsSeen;
      std::set<std::pair<Instruction *, Instruction *>> visitedEdges;
      std::vector<
          std::tuple<BasicBlock *, Instruction *, std::set<Instruction *>>>
          worklist;

      defsSeen.insert(callSite);

      for (;;) {
        if (PHINode *phi = dyn_cast<PHINode>(inst)) {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            if (phi->getIncomingBlock(i) != fromBlock)
              continue;

            Instruction *use_inst =
                dyn_cast<Instruction>(phi->getIncomingValue(i));
            if (!use_inst)
              continue;

            if (!defsSeen.count(use_inst)) {
              // Use without a preceding def must be a live value.
              liveValues.insert(use_inst);
            }
          }
        } else {
          for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
            Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
            if (!use_inst)
              continue;

            if (!defsSeen.count(use_inst)) {
              // Use without a preceding def must be a live value.
              liveValues.insert(use_inst);
            }
          }
        }

        defsSeen.insert(inst);

        // Don't count instructions past another call site.
        // They will initiate spilling as needed independently.
        if (isa<CallInst>(inst) && callSites.count(cast<CallInst>(inst))) {
          if (worklist.empty())
            break;

          fromBlock = std::get<0>(worklist.back());
          inst = std::get<1>(worklist.back());
          defsSeen = std::move(std::get<2>(worklist.back()));
          worklist.pop_back();
          continue;
        }

        if (TerminatorInst *terminator = dyn_cast<TerminatorInst>(inst)) {
          BranchInst *branch = dyn_cast<BranchInst>(terminator);

          if (branch && branch->isConditional()) {
            // Conditional branch. Check against known conditions.
            assert(branch->getNumSuccessors() == 2);

            Instruction *taken = branch->getSuccessor(0)->begin();
            Instruction *notTaken = branch->getSuccessor(1)->begin();

            std::pair<Instruction *, Instruction *> takenEdge =
                std::make_pair(branch, taken);
            std::pair<Instruction *, Instruction *> notTakenEdge =
                std::make_pair(branch, notTaken);

            if (!conditionsKnownFalse.count(branch->getCondition())) {
              if (!visitedEdges.count(takenEdge)) {
                visitedEdges.insert(takenEdge);
                worklist.push_back(
                    std::make_tuple(inst->getParent(), taken, defsSeen));
              }
            }

            if (!conditionsKnownTrue.count(branch->getCondition())) {
              if (!visitedEdges.count(notTakenEdge)) {
                visitedEdges.insert(notTakenEdge);
                worklist.push_back(
                    std::make_tuple(inst->getParent(), notTaken, defsSeen));
              }
            }
          } else {
            for (unsigned i = 0, e = terminator->getNumSuccessors(); i < e;
                 ++i) {
              Instruction *successor = terminator->getSuccessor(i)->begin();
              std::pair<Instruction *, Instruction *> edge =
                  std::make_pair(inst, successor);

              if (!visitedEdges.count(edge)) {
                visitedEdges.insert(edge);
                worklist.push_back(
                    std::make_tuple(inst->getParent(), successor, defsSeen));
              }
            }
          }

          if (worklist.empty())
            break;

          fromBlock = std::get<0>(worklist.back());
          inst = std::get<1>(worklist.back());
          defsSeen = std::move(std::get<2>(worklist.back()));
          worklist.pop_back();
          continue;
        }

        inst = inst->getNextNode();
      }
    }

    // Determine initial sets of remat/spill.
    std::set<Instruction *> rematSet;
    std::set<Instruction *> spillSet;
    {
      std::vector<Instruction *> worklist;
      std::set<Instruction *> visited;

      for (Instruction *inst : liveValues) {
        visited.insert(inst);
        worklist.push_back(inst);
      }

      while (!worklist.empty()) {
        Instruction *inst = worklist.back();
        worklist.pop_back();

        if (!canRemat(inst)) {
          spillSet.insert(inst);
          continue;
        }

        rematSet.insert(inst);

        if (PHINode *phi = dyn_cast<PHINode>(inst)) {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            std::pair<Instruction *, Instruction *> edge =
                std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
                               phi->getParent()->begin());

            // This is a live value, i.e., something that was defined BEFORE the
            // trace call. So we can check predCfgEdges for valid incoming
            // values.
            if (predCfgEdges.count(edge)) {
              Instruction *use_inst =
                  dyn_cast<Instruction>(phi->getIncomingValue(i));
              if (use_inst && !visited.count(use_inst)) {
                visited.insert(use_inst);
                worklist.push_back(use_inst);
              }

              BranchInst *branch = dyn_cast<BranchInst>(
                  phi->getIncomingBlock(i)->getTerminator());
              if (branch && branch->isConditional() &&
                  isa<Instruction>(branch->getCondition()) &&
                  !visited.count(cast<Instruction>(branch->getCondition()))) {
                visited.insert(cast<Instruction>(branch->getCondition()));
                worklist.push_back(cast<Instruction>(branch->getCondition()));
              }
            }
          }
        } else {
          for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
            Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
            if (!use_inst)
              continue;

            if (!visited.count(use_inst)) {
              visited.insert(use_inst);
              worklist.push_back(use_inst);
            }
          }
        }
      }
    }

    // Save off initial spill set.
    std::set<Instruction *> initialSpillSet = spillSet;

    // Move looped PHIs without intervening spill to spill set.
    std::vector<PHINode *> phis;

    for (Instruction *inst : rematSet) {
      PHINode *phi = dyn_cast<PHINode>(inst);
      if (!phi)
        continue;
      phis.push_back(phi);
    }

    // Prefer to spill user-named variables, so start with them.
    std::vector<PHINode *> sortedPhis = sortInstructions<PHINode *>(phis);

    std::stable_sort(
        sortedPhis.begin(), sortedPhis.end(), [](PHINode *a, PHINode *b) {
          StringRef aName = a->getName();
          StringRef bName = b->getName();

          bool aIsUserName =
              !aName.empty() && ((aName[0] >= 'a' && aName[0] <= 'z') ||
                                 (aName[0] >= 'A' && aName[0] <= 'Z'));
          bool bIsUserName =
              !bName.empty() && ((bName[0] >= 'a' && bName[0] <= 'z') ||
                                 (bName[0] >= 'A' && bName[0] <= 'Z'));

          if (aIsUserName == bIsUserName)
            return false;

          return aIsUserName;
        });

    // We're sorting the phis. Otherwise this step isn't deterministic.
    for (PHINode *phi : sortedPhis) {
      bool loopWithoutSpill = false;

      std::vector<Instruction *> worklist;
      std::set<Instruction *> visited;

      bool first = true;
      worklist.push_back(phi);

      while (!worklist.empty()) {
        Instruction *inst = worklist.back();
        worklist.pop_back();

        if (first) {
          first = false;
        } else {
          if (inst == phi) {
            loopWithoutSpill = true;
            continue;
          }
        }

        if (PHINode *phi = dyn_cast<PHINode>(inst)) {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            BranchInst *branch =
                dyn_cast<BranchInst>(phi->getIncomingBlock(i)->getTerminator());
            if (!branch || !branch->isConditional())
              continue;

            Instruction *condition =
                dyn_cast<Instruction>(branch->getCondition());

            if (condition && !visited.count(condition)) {
              visited.insert(condition);

              if (rematSet.count(condition))
                worklist.push_back(condition);
            }
          }
        }

        for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
          Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
          if (!use_inst)
            continue;

          if (!spillSet.count(use_inst) && !rematSet.count(use_inst))
            continue;

          if (!visited.count(use_inst)) {
            visited.insert(use_inst);

            if (rematSet.count(use_inst))
              worklist.push_back(use_inst);
          }
        }
      }

      if (loopWithoutSpill) {
        rematSet.erase(phi);
        spillSet.insert(phi);
      }
    }

    // Backtrack live values to non-remat instructions.
    std::set<Instruction *> nonRematLiveValues;
    {
      std::vector<Instruction *> worklist;
      std::set<Instruction *> visited;

      for (Instruction *inst : liveValues) {
        visited.insert(inst);
        worklist.push_back(inst);
      }

      while (!worklist.empty()) {
        Instruction *inst = worklist.back();
        worklist.pop_back();

        assert(rematSet.count(inst) + spillSet.count(inst) == 1);

        if (spillSet.count(inst)) {
          nonRematLiveValues.insert(inst);
          continue;
        }

        if (PHINode *phi = dyn_cast<PHINode>(inst)) {
          // We can remat PHI if we can reconstruct which BB we came from.
          // This is a live value, i.e., something that was defined BEFORE the
          // trace call. So we can check predCfgEdges.
          unsigned numValidEdges = 0;
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            std::pair<Instruction *, Instruction *> edge =
                std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
                               phi->getParent()->begin());

            if (predCfgEdges.count(edge)) {
              ++numValidEdges;
            }
          }

          assert(numValidEdges);

          if (numValidEdges == 1) {
            // Only a single valid edge. The PHI must have one value.
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
              std::pair<Instruction *, Instruction *> edge =
                  std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
                                 phi->getParent()->begin());

              if (predCfgEdges.count(edge)) {
                Instruction *use_inst =
                    dyn_cast<Instruction>(phi->getIncomingValue(i));
                if (!use_inst)
                  continue;

                if (!visited.count(use_inst)) {
                  visited.insert(use_inst);
                  worklist.push_back(use_inst);
                }
              }
            }
            continue;
          }

          // Spill selector for block.
          phiSelectorSpills.insert(phi->getParent());

          // Add dependencies to work list.
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            std::pair<Instruction *, Instruction *> edge =
                std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
                               phi->getParent()->begin());

            if (predCfgEdges.count(edge)) {
              Instruction *use_inst =
                  dyn_cast<Instruction>(phi->getIncomingValue(i));
              if (!use_inst)
                continue;

              if (visited.count(use_inst))
                continue;

              visited.insert(use_inst);
              worklist.push_back(use_inst);
            }
          }
        } else {
          for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
            Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
            if (!use_inst)
              continue;

            if (visited.count(use_inst))
              continue;

            visited.insert(use_inst);
            worklist.push_back(use_inst);
          }
        }
      }
    }

    std::vector<Instruction *> &spills = spillsPerTraceCall[callSite];

    for (Instruction *inst : nonRematLiveValues)
      spills.push_back(inst);
  }
  
  // Begin output
  for (CallInst *callSite : sortInstructions<CallInst *>(callSites)) {
    std::ostringstream Spacer;
    {
      if (const llvm::DebugLoc &debugInfo = callSite->getDebugLoc()) {
        std::string filePath = debugInfo->getFilename();
        int line = debugInfo->getLine();
        std::string fullPath =
            formatSourceLocation(RootPath, filePath, line, isUE4Path);
        PrettyStr << "Live State for TRACE call at " << fullPath << "\n";
        VSStr << "Live State for TRACE call at " << fullPath << "\n";
        Spacer << std::setfill('-') << std::setw(fullPath.length() + 30)
               << std::right << "\n";
        PrettyStr << Spacer.str();
        VSStr << Spacer.str();
      } else {
        break;
      }
    }
    size_t regs = 0;
    for (Instruction *I : spillsPerTraceCall[callSite]) {
      // Only count the values for which metadata exists.
      // TODO: Report count of values for those which are not used metadata?
      // this can lead to missing live values in some cases.
      int extractElement = 0;
      if (isa<ExtractValueInst>(I)) {
        extractElement = dyn_cast<ExtractValueInst>(I)->getIndices()[0];
        I = dyn_cast<Instruction>(I->getOperand(0));
      }
      if (I->isUsedByMetadata()) {
        regs++;
      }
    }
    if (const llvm::DebugLoc &debugInfo = callSite->getDebugLoc()) {
      PrettyStr << "Total 32-bit registers: " << (int)regs << "\n";
      VSStr << "Total 32-bit registers: " << (int)regs << "\n";
    }

    std::ostringstream TmpStr;
    TmpStr << "--LIVE VALUES" << std::setfill('-')
           << std::setw(Spacer.str().length() - 13) << std::right << "\n";
    PrettyStr << TmpStr.str();
    VSStr << TmpStr.str();
    for (Instruction *I :
         sortInstructions<Instruction *>(spillsPerTraceCall[callSite])) {
      TmpStr.str("");
      int extractElement = 0;
      {
        if (isa<ExtractValueInst>(I)) {
          extractElement = dyn_cast<ExtractValueInst>(I)->getIndices()[0];
          I = dyn_cast<Instruction>(I->getOperand(0));
        }

        if (I->isUsedByMetadata()) {
          unsigned int totalLineWidth = 0;

          if (auto *L = LocalAsMetadata::getIfExists(I)) {
            if (auto *MDV = MetadataAsValue::getIfExists(I->getContext(), L)) {
              for (User *U : MDV->users()) {
                if (DbgValueInst *DVI = dyn_cast<DbgValueInst>(U)) {
                  DILocalVariable *Var = DVI->getVariable();
                  DILocation *Loc = DVI->getDebugLoc();
                  Metadata *VarTypeMD = Var->getRawType();
                  std::string VarName = Var->getName();
                  std::string OrigName = I->getName();

                  TmpStr << VarName;

                  DIType *VarType = dyn_cast_or_null<DIDerivedType>(VarTypeMD);
                  bool isStruct = false;
                  if (isa<DIDerivedType>(VarTypeMD)) {
                    VarType = dyn_cast_or_null<DIType>(
                        dyn_cast<DIDerivedType>(VarType)->getRawBaseType());
                    while (llvm::dwarf::DW_TAG_reference_type ==
                           VarType->getTag()) {
                      VarType = dyn_cast_or_null<DIType>(
                          dyn_cast<DIDerivedType>(VarType)->getRawBaseType());
                    }
                  } else if (isa<DICompositeType>(VarTypeMD)) {
                    VarType = dyn_cast<DICompositeType>(VarTypeMD);
                    isStruct =
                        llvm::dwarf::DW_TAG_structure_type == VarType->getTag();
                  } else if (isa<DIBasicType>(VarTypeMD))
                    VarType = dyn_cast<DIBasicType>(VarTypeMD);

                  assert(VarType);

                  // Inspect the llvm variable name for possible element.
                  // TODO Find a better/safer solution? One problem is variable
                  // names differ between Debug and Release compiles with Release
                  // names usually adopting "i.xxxxx" so only the first digit
                  // after ".i" can be assumed to correspond to an element index.
                  int pos = 0;
                  int memberIdx = -1;
                  int elementIdx = -1;
                  if (isStruct) {
                    pos = VarName.find('.', 0);
                    // VarName may be the base type and not the original
                    // source instruction leading to the live value, so check
                    // OrigName as well.
                    if (pos < 0) {
                      pos = OrigName.find(".i", 0);
                      memberIdx = atoi(
                          OrigName.substr(pos + 1, OrigName.find(".i", pos + 2))
                              .c_str());
                    } else {
                      memberIdx = atoi(
                          VarName.substr(pos + 1, VarName.find('.', pos + 1))
                              .c_str());
                    }
                  }
                  pos = VarName.find(".i", 0);
                  if (pos >= 0) {
                    while ('.' == VarName[pos + 2] && pos > 0) {
                      pos = VarName.find(".i", pos + 2);
                    }
                    elementIdx =
                        atoi(VarName.substr(pos + 2, 1).c_str());
                  }
                  // VarName may be the base type and not the original source
                  // instruction leading to the live value, so check OrigName
                  // as well.
                  else if (pos < 0 && OrigName.length() > 0) {
                    pos = OrigName.find(".i", 0);
                    if (pos >= 0)
                      elementIdx =
                          atoi(OrigName.substr(pos + 2, 1).c_str());
                  }

                  // Type size of 0 indicates a reference.
                  if (memberIdx >= 0) {
                    DICompositeType *CompType =
                        dyn_cast<DICompositeType>(VarTypeMD);
                    DIDerivedType *MemberData = dyn_cast<DIDerivedType>(
                        CompType->getElements()[memberIdx]);
                    TmpStr << "." << MemberData->getName().str();
                  }

                  // If there is no elementIdx found, but this is an extract
                  // element instruction we can use that element instead.
                  if (elementIdx < 0 && extractElement >= 0)
                    elementIdx = extractElement;

                  if (elementIdx >= 0) {
                    DIDerivedType *MemberData = nullptr;
                    DIType *ElementData = nullptr;
                    DICompositeType *CompType =
                        dyn_cast_or_null<DICompositeType>(VarTypeMD);
                    if (CompType) {
                      MemberData = dyn_cast<DIDerivedType>(
                          CompType->getElements()[memberIdx]);
                    } else
                      MemberData = dyn_cast_or_null<DIDerivedType>(VarTypeMD);

                    if (MemberData) {
                      Metadata *MD = MemberData->getRawBaseType();
                      ElementData = dyn_cast_or_null<DIDerivedType>(MD);

                      if (isa<DIDerivedType>(MD) &&
                          isa<DIDerivedType>(
                              dyn_cast<DIDerivedType>(ElementData)
                                  ->getRawBaseType())) {
                        DIDerivedType *DT = dyn_cast_or_null<DIDerivedType>(
                            dyn_cast<DIDerivedType>(ElementData)
                                ->getRawBaseType());
                        while (llvm::dwarf::DW_TAG_reference_type ==
                                   DT->getTag() ||
                               llvm::dwarf::DW_TAG_restrict_type ==
                                   DT->getTag()) {
                          DT = dyn_cast<DIDerivedType>(DT->getRawBaseType());
                        }
                        if (llvm::dwarf::DW_TAG_structure_type == DT->getTag())
                          TmpStr << "." << DT->getName().str();
                        DICompositeType *CompType =
                            dyn_cast_or_null<DICompositeType>(
                                DT->getRawBaseType());
                        if (CompType->getElements()) {
                          ElementData = dyn_cast<DIType>(
                              CompType->getElements()[elementIdx]);
                        } else {
                          ElementData = DT;
                        }
                      } else if (isa<DIDerivedType>(MD) &&
                                 isa<DICompositeType>(
                                     dyn_cast<DIDerivedType>(ElementData)
                                         ->getRawBaseType())) {
                        ElementData = dyn_cast<DIType>(
                            dyn_cast<DICompositeType>(
                                dyn_cast<DIDerivedType>(ElementData)
                                    ->getRawBaseType())
                                ->getElements()[elementIdx]);
                        MD = dyn_cast<DIDerivedType>(ElementData)
                                 ->getRawBaseType();

                        if (MD) {
                          while (isa<DICompositeType>(MD)) {
                            TmpStr << "." << ElementData->getName().str();
                            ElementData = dyn_cast<DIType>(
                                dyn_cast<DICompositeType>(MD)
                                    ->getElements()[elementIdx]);
                            MD = dyn_cast<DIDerivedType>(ElementData)
                                     ->getRawBaseType();
                          }
                        }
                      } else if (isa<DICompositeType>(MD) &&
                                 isa<DIType>(dyn_cast<DICompositeType>(MD)
                                                 ->getElements()[elementIdx])) {
                        ElementData =
                            dyn_cast<DIType>(dyn_cast<DICompositeType>(MD)
                                                 ->getElements()[elementIdx]);

                        MD = dyn_cast<DIDerivedType>(ElementData)
                                 ->getRawBaseType();
                        if (MD) {
                          while (isa<DICompositeType>(MD)) {
                            TmpStr << "." << ElementData->getName().str();
                            ElementData = dyn_cast<DIType>(
                                dyn_cast<DICompositeType>(MD)
                                    ->getElements()[elementIdx]);
                            MD = dyn_cast<DIDerivedType>(ElementData)
                                     ->getRawBaseType();
                          }
                        }
                      }
                    } else {
                      if (isa<DIBasicType>(VarTypeMD))
                        ElementData = nullptr;
                    }
                    if (ElementData)
                      TmpStr << "." << ElementData->getName().str();
                  }

                  TmpStr << " (" << VarType->getName().str();
                  TmpStr << "): "
                         << formatSourceLocation(RootPath,
                                                 Loc->getFilename().str(),
                                                 Loc->getLine(), isUE4Path)
                         << "\n";

                  // Output USE locations
                  std::vector<PHINode *> Phis;
                  std::set<PHINode *> VisitedPhis;
                  std::set<DILocation *> UseLocations;
                  while (1) {
                    for (User *U : I->users()) {
                      if (auto Inst = dyn_cast<Instruction>(U)) {
                        if (DILocation *Loc = Inst->getDebugLoc()) {
                          // Manually search for duplication location;
                          // DILocations can be distinct but their source
                          // location is not
                          bool bFound = false;
                          DIScope *scope = Loc->getScope();
                          for (auto L : UseLocations) {
                            if (L->getScope() == scope) {
                              if (L->getLine() == Loc->getLine() &&
                                  L->getColumn() == Loc->getColumn()) {
                                bFound = true;
                                break;
                              }
                            }
                          }

                          if (!bFound) {
                            UseLocations.insert(Loc);

                            // Traverse each control flow edge separately and
                            // gather up seen defs along the way. By keeping
                            // track of the from-block, we can select the
                            // right incoming value at PHIs.
                            {
                              Instruction *inst = callSite->getNextNode();

                              std::set<Instruction *> defsSeen;
                              defsSeen.insert(inst);

                              BasicBlock *fromBlock = nullptr;
                              std::set<std::pair<Instruction *, Instruction *>>
                                  visitedEdges;
                              std::set<Value *> conditionsKnownTrue;
                              std::set<Value *> conditionsKnownFalse;
                              std::vector<
                                  std::tuple<BasicBlock *, Instruction *,
                                             std::set<Instruction *>>>
                                  worklist;

                              for (;;) {

                                if (isa<DbgValueInst>(inst)) {
                                  inst = inst->getNextNode();
                                  continue;
                                }

                                if (inst == Inst) {
                                  // Found the use location.
                                  break;
                                }

                                if (TerminatorInst *terminator =
                                        dyn_cast<TerminatorInst>(inst)) {
                                  BranchInst *branch =
                                      dyn_cast<BranchInst>(terminator);

                                  if (branch && branch->isConditional()) {
                                    // Conditional branch. Check against known
                                    // conditions.
                                    assert(branch->getNumSuccessors() == 2);

                                    Instruction *taken =
                                        branch->getSuccessor(0)->begin();
                                    Instruction *notTaken =
                                        branch->getSuccessor(1)->begin();

                                    std::pair<Instruction *, Instruction *>
                                        takenEdge =
                                            std::make_pair(branch, taken);
                                    std::pair<Instruction *, Instruction *>
                                        notTakenEdge =
                                            std::make_pair(branch, notTaken);

                                    if (!conditionsKnownFalse.count(
                                            branch->getCondition())) {
                                      if (!visitedEdges.count(takenEdge)) {
                                        visitedEdges.insert(takenEdge);
                                        worklist.push_back(
                                            std::make_tuple(inst->getParent(),
                                                            taken, defsSeen));
                                      }
                                    }

                                    if (!conditionsKnownTrue.count(
                                            branch->getCondition())) {
                                      if (!visitedEdges.count(notTakenEdge)) {
                                        visitedEdges.insert(notTakenEdge);
                                        worklist.push_back(std::make_tuple(
                                            inst->getParent(), notTaken,
                                            defsSeen));
                                      }
                                    }
                                  } else {
                                    for (unsigned
                                             i = 0,
                                             e = terminator->getNumSuccessors();
                                         i < e; ++i) {
                                      Instruction *successor =
                                          terminator->getSuccessor(i)->begin();
                                      std::pair<Instruction *, Instruction *>
                                          edge =
                                              std::make_pair(inst, successor);

                                      if (!visitedEdges.count(edge)) {
                                        visitedEdges.insert(edge);
                                        worklist.push_back(std::make_tuple(
                                            inst->getParent(), successor,
                                            defsSeen));
                                      }
                                    }
                                  }

                                  if (worklist.empty())
                                    break;

                                  fromBlock = std::get<0>(worklist.back());
                                  inst = std::get<1>(worklist.back());
                                  defsSeen =
                                      std::move(std::get<2>(worklist.back()));
                                  worklist.pop_back();
                                  continue;
                                }

                                inst = inst->getNextNode();
                              }
                            }
                          }
                        } else {
                          if (isa<PHINode>(U)) {
                            PHINode *NewPhi = dyn_cast<PHINode>(U);
                            if (VisitedPhis.insert(NewPhi).second) {
                              Phis.push_back(NewPhi);
                            }
                          }
                        }
                      }
                    }

                    if (Phis.empty()) {
                      break;
                    } else {
                      I = Phis.back();
                      Phis.pop_back();
                    }
                  }
                  TmpStr << "Use Locations:\n";
                  PrettyStr << TmpStr.str();
                  VSStr << TmpStr.str();
                  // Find max path length
                  unsigned int maxPathLength = 0;
                  unsigned int maxFuncLength = 0;
                  for (auto Loc : UseLocations) {
                    std::string Path =
                        formatSourceLocation(RootPath, Loc->getFilename().str(),
                                             Loc->getLine(), isUE4Path);
                    if (Path.length() > maxPathLength)
                      maxPathLength = Path.length();
                    if (Loc->getScope()
                            ->getSubprogram()
                            ->getName()
                            .str()
                            .length() > maxFuncLength)
                      maxFuncLength = Loc->getScope()
                                          ->getSubprogram()
                                          ->getName()
                                          .str()
                                          .length();
                    if (DILocation *InLoc = Loc->getInlinedAt()) {
                      std::string Path = formatSourceLocation(
                          RootPath, InLoc->getFilename().str(),
                          InLoc->getLine(), isUE4Path);
                      if (Path.length() > maxPathLength)
                        maxPathLength = Path.length();
                      if (InLoc->getScope()
                              ->getSubprogram()
                              ->getName()
                              .str()
                              .length() > maxFuncLength)
                        maxFuncLength = InLoc->getScope()
                                            ->getSubprogram()
                                            ->getName()
                                            .str()
                                            .length();
                      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
                        InLoc = NestedInLoc;
                        std::string Path = formatSourceLocation(
                            RootPath, InLoc->getFilename().str(),
                            InLoc->getLine(), isUE4Path);
                        if (Path.length() > maxPathLength)
                          maxPathLength = Path.length();
                        if (InLoc->getScope()
                                ->getSubprogram()
                                ->getName()
                                .str()
                                .length() > maxFuncLength)
                          maxFuncLength = InLoc->getScope()
                                              ->getSubprogram()
                                              ->getName()
                                              .str()
                                              .length();
                      }
                    }
                  }
                  // Pad with spaces for inline text, etc
                  maxFuncLength += 2;
                  maxPathLength += 19;
                  totalLineWidth = maxFuncLength + maxPathLength;
                  // Output use locations
                  std::ostringstream LocationStr;
                  std::string FileName;
                  std::string FuncName;

                  // Output PrettyPrint version
                  for (auto Loc : UseLocations) {
                    FileName =
                        formatSourceLocation(RootPath, Loc->getFilename().str(),
                                             Loc->getLine(), isUE4Path);
                    FuncName =
                        "(" +
                        Loc->getScope()->getSubprogram()->getName().str() + ")";
                    LocationStr << std::setfill('_') << std::setw(maxPathLength)
                                << std::left << FileName
                                << std::setw(maxFuncLength) << std::right
                                << FuncName << "\n";
                    if (DILocation *InLoc = Loc->getInlinedAt()) {
                      FileName = "  -->inlined at " +
                                 formatSourceLocation(
                                     RootPath, InLoc->getFilename().str(),
                                     InLoc->getLine(), isUE4Path);
                      FuncName =
                          "(" +
                          InLoc->getScope()->getSubprogram()->getName().str() +
                          ")";
                      LocationStr << std::setfill('_')
                                  << std::setw(maxPathLength) << std::left
                                  << FileName << std::setw(maxFuncLength)
                                  << std::right << FuncName << "\n";
                      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
                        FileName =
                            "  -->inlined at " +
                            formatSourceLocation(
                                RootPath, NestedInLoc->getFilename().str(),
                                NestedInLoc->getLine(), isUE4Path);
                        FuncName = "(" +
                                   NestedInLoc->getScope()
                                       ->getSubprogram()
                                       ->getName()
                                       .str() +
                                   ")";
                        LocationStr << std::setfill('_')
                                    << std::setw(maxPathLength) << std::left
                                    << FileName << std::setw(maxFuncLength)
                                    << std::right << FuncName << "\n";
                        InLoc = NestedInLoc;
                      }
                    }
                  }
                  PrettyStr << LocationStr.str();
                  LocationStr.str("");
                  // Output VS source linking version
                  for (auto Loc : UseLocations) {
                    FileName =
                        formatSourceLocation(RootPath, Loc->getFilename().str(),
                                             Loc->getLine(), isUE4Path);
                    LocationStr << FileName
                                << ": (" +
                                       Loc->getScope()
                                           ->getSubprogram()
                                           ->getName()
                                           .str() +
                                       ")\n";
                    if (DILocation *InLoc = Loc->getInlinedAt()) {
                      LocationStr << "inlined at:\n";
                      FileName = formatSourceLocation(
                          RootPath, InLoc->getFilename().str(),
                          InLoc->getLine(), isUE4Path);
                      LocationStr << ">" + FileName
                                  << ": (" +
                                         InLoc->getScope()
                                             ->getSubprogram()
                                             ->getName()
                                             .str() +
                                         ")\n";
                      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
                        FileName = formatSourceLocation(
                            RootPath, NestedInLoc->getFilename().str(),
                            NestedInLoc->getLine(), isUE4Path);
                        LocationStr << ">" + FileName
                                    << ": (" +
                                           InLoc->getScope()
                                               ->getSubprogram()
                                               ->getName()
                                               .str() +
                                           ")\n";
                        InLoc = NestedInLoc;
                      }
                    }
                  }
                  VSStr << LocationStr.str();
                }
                // Found the debug info for the live value, now exit the loop.
                break;
              }
              std::ostringstream Spacer;
              Spacer << std::setfill('-') << std::setw(totalLineWidth + 1)
                     << std::right << "\n";
              PrettyStr << Spacer.str();
              VSStr << Spacer.str();
            }
          }
        }
      }
    }
  }
}

void EmitAssemblyHelper::EmitAssembly(BackendAction Action,
                                      raw_pwrite_stream *OS) {
  TimeRegion Region(llvm::TimePassesIsEnabled ? &CodeGenerationTime : nullptr);

  bool UsesCodeGen = (Action != Backend_EmitNothing &&
                      Action != Backend_EmitBC &&
                      Action != Backend_EmitPasses &&
                      Action != Backend_EmitLL);
  if (!TM)
    TM.reset(CreateTargetMachine(UsesCodeGen));

  if (UsesCodeGen && !TM)
    return;
  if (TM)
    TheModule->setDataLayout(*TM->getDataLayout());
  CreatePasses();

  switch (Action) {
  case Backend_EmitNothing:
  case Backend_EmitPasses: // HLSL Change
    break;

  case Backend_EmitBC:
    getPerModulePasses()->add(
        createBitcodeWriterPass(*OS, CodeGenOpts.EmitLLVMUseLists));
    break;

  case Backend_EmitLL:
    getPerModulePasses()->add(
        createPrintModulePass(*OS, "", CodeGenOpts.EmitLLVMUseLists));
    break;

  default:
    if (!AddEmitPasses(Action, *OS))
      return;
  }

  // Before executing passes, print the final values of the LLVM options.
  cl::PrintOptionValues();

  // HLSL Change Starts
  if (Action == Backend_EmitPasses) {
    if (PerFunctionPasses) {
      *OS << "# Per-function passes\n"
             "-opt-fn-passes\n";
      *OS << PerFunctionPassesConfigOS.str();
    }
    if (PerModulePasses) {
      *OS << "# Per-module passes\n"
             "-opt-mod-passes\n";
      *OS << PerModulePassesConfigOS.str();
    }
    if (CodeGenPasses) {
      *OS << "# Code generation passes\n"
             "-opt-mod-passes\n";
      *OS << CodeGenPassesConfigOS.str();
    }
    return;
  }
  // HLSL Change Ends

  // Run passes. For now we do all passes at once, but eventually we
  // would like to have the option of streaming code generation.

  if (PerFunctionPasses) {
    PrettyStackTraceString CrashInfo("Per-function optimization");

    PerFunctionPasses->doInitialization();
    for (Function &F : *TheModule)
      if (!F.isDeclaration())
        PerFunctionPasses->run(F);
    PerFunctionPasses->doFinalization();
  }

  if (PerModulePasses) {
    PrettyStackTraceString CrashInfo("Per-module optimization passes");
    PerModulePasses->run(*TheModule);
  }

  if (CodeGenPasses) {
    PrettyStackTraceString CrashInfo("Code generation");
    CodeGenPasses->run(*TheModule);
  }

  // Find any define for LIVE_VALUE_REPORT_ROOT in order to build absolute path
  // locations in the LVA report.
  std::string RootPath = "";
  bool isUE4Path = false;
  // Look for the debuginfo compile unit and then process Live Value Report if
  // it exists
  NamedMDNode *NMD = TheModule->getNamedMetadata("llvm.dbg.cu");
  if (NMD) {
    llvm::NamedMDNode *Defines = TheModule->getNamedMetadata(
        hlsl::DxilMDHelper::kDxilSourceDefinesMDName);
    if (!Defines) {
      Defines = TheModule->getNamedMetadata("llvm.dbg.defines");
    }
    if (Defines) {
      for (unsigned i = 0, e = Defines->getNumOperands(); i != e; ++i) {
        MDNode *node = Defines->getOperand(i);

        if ((node->getNumOperands() > 0) &&
            node->getOperand(0)->getMetadataID() == Metadata::MDStringKind) {
          MDString *str = dyn_cast<MDString>(node->getOperand(0));
          if (str->getString().find("LIVE_VALUE_REPORT_ROOT=") !=
              StringRef::npos) {
            RootPath =
                str->getString().substr(23, str->getString().size() - 23);
          } else if (str->getString().find("LIVE_VALUE_REPORT_UE4_ROOT") !=
                     StringRef::npos) {
            RootPath =
                str->getString().substr(27, str->getString().size() - 27);
            isUE4Path = true;
          }
          // Standarize backslashes
          std::replace(RootPath.begin(), RootPath.end(), '\\', '/');
        }
      }
    }

    // Create two report streams, one pretty version for reading and one Visual
    // Studio version for source linking.
    std::string PrettyReport;
    std::string VSReport;
    raw_string_ostream PrettyStr(PrettyReport);
    raw_string_ostream VSStr(VSReport);
    PrettyStr << "\n";
    PrettyStr << "OPTIMIZED DXIL LIVE VALUE ANALYSIS\n";
    PrettyStr << "----------------------------------\n";
    for (Function &F : *TheModule) {
      printLiveValues(TheModule, &F, PrettyStr, VSStr, RootPath, isUE4Path);
    }
    // Echo Pretty string to VS debugger and console, cut into chunks as it
    // won't output a massive string.
    size_t outputSize = PrettyStr.str().length();
    const size_t blockSize = 1024;
    for (int i = 0; i < outputSize;) {
      OutputDebugString(PrettyStr.str().substr(i, blockSize).c_str());
      i += blockSize;
    }

    // Temporary spacer in the output.
    OutputDebugString("========================================================"
                      "============\n");
    // Echo VS string to VS debugger and console, cut into chunks as it won't
    // output a massive string.
    outputSize = VSStr.str().length();
    for (int i = 0; i < outputSize;) {
      OutputDebugString(VSStr.str().substr(i, blockSize).c_str());
      i += blockSize;
    }

    PrettyStr.close();
    VSStr.close();
  }
}

void clang::EmitBackendOutput(DiagnosticsEngine &Diags,
                              const CodeGenOptions &CGOpts,
                              const clang::TargetOptions &TOpts,
                              const LangOptions &LOpts, StringRef TDesc,
                              Module *M, BackendAction Action,
                              raw_pwrite_stream *OS) {
  EmitAssemblyHelper AsmHelper(Diags, CGOpts, TOpts, LOpts, M);

  AsmHelper.EmitAssembly(Action, OS);

  // If an optional clang TargetInfo description string was passed in, use it to
  // verify the LLVM TargetMachine's DataLayout.
  if (AsmHelper.TM && !TDesc.empty()) {
    std::string DLDesc =
        AsmHelper.TM->getDataLayout()->getStringRepresentation();
    if (DLDesc != TDesc) {
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error, "backend data layout '%0' does not match "
                                    "expected target description '%1'");
      Diags.Report(DiagID) << DLDesc << TDesc;
    }
  }
}
