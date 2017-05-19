///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilGenerationPass.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// DxilGenerationPass implementation.                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilSigPoint.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/HLOperationLower.h"

#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include <memory>
#include <unordered_set>

using namespace llvm;
using namespace hlsl;

// TODO: use hlsl namespace for the most of this file.

namespace {

// Collect unused phi of resources and remove them.
class ResourceRemover : public LoadAndStorePromoter {
  AllocaInst *AI;
  mutable std::unordered_set<PHINode *> unusedPhis;

public:
  ResourceRemover(ArrayRef<Instruction *> Insts, SSAUpdater &S)
      : LoadAndStorePromoter(Insts, S), AI(nullptr) {}

  void run(AllocaInst *AI, const SmallVectorImpl<Instruction *> &Insts) {
    // Remember which alloca we're promoting (for isInstInList).
    this->AI = AI;
    LoadAndStorePromoter::run(Insts);
    for (PHINode *P : unusedPhis) {
      P->eraseFromParent();
    }
  }
  bool
  isInstInList(Instruction *I,
               const SmallVectorImpl<Instruction *> &Insts) const override {
    if (LoadInst *LI = dyn_cast<LoadInst>(I))
      return LI->getOperand(0) == AI;
    return cast<StoreInst>(I)->getPointerOperand() == AI;
  }

  void replaceLoadWithValue(LoadInst *LI, Value *V) const override {
    if (PHINode *PHI = dyn_cast<PHINode>(V)) {
      if (PHI->user_empty())
        unusedPhis.insert(PHI);
    }
    LI->replaceAllUsesWith(UndefValue::get(LI->getType()));
  }
};

void InitResourceBase(const DxilResourceBase *pSource, DxilResourceBase *pDest) {
  DXASSERT_NOMSG(pSource->GetClass() == pDest->GetClass());
  pDest->SetKind(pSource->GetKind());
  pDest->SetID(pSource->GetID());
  pDest->SetSpaceID(pSource->GetSpaceID());
  pDest->SetLowerBound(pSource->GetLowerBound());
  pDest->SetRangeSize(pSource->GetRangeSize());
  pDest->SetGlobalSymbol(pSource->GetGlobalSymbol());
  pDest->SetGlobalName(pSource->GetGlobalName());
  pDest->SetHandle(pSource->GetHandle());
}

void InitResource(const DxilResource *pSource, DxilResource *pDest) {
  pDest->SetCompType(pSource->GetCompType());
  pDest->SetSampleCount(pSource->GetSampleCount());
  pDest->SetElementStride(pSource->GetElementStride());
  pDest->SetGloballyCoherent(pSource->IsGloballyCoherent());
  pDest->SetHasCounter(pSource->HasCounter());
  pDest->SetRW(pSource->IsRW());
  pDest->SetROV(pSource->IsROV());
  InitResourceBase(pSource, pDest);
}

void InitDxilModuleFromHLModule(HLModule &H, DxilModule &M, bool HasDebugInfo) {
  // Subsystems.
  unsigned ValMajor, ValMinor;
  H.GetValidatorVersion(ValMajor, ValMinor);
  M.SetValidatorVersion(ValMajor, ValMinor);
  M.SetShaderModel(H.GetShaderModel());

  // Entry function.
  Function *EntryFn = H.GetEntryFunction();
  DxilFunctionProps *FnProps = H.HasDxilFunctionProps(EntryFn) ? &H.GetDxilFunctionProps(EntryFn) : nullptr;
  M.SetEntryFunction(EntryFn);
  M.SetEntryFunctionName(H.GetEntryFunctionName());
  
  std::vector<GlobalVariable* > &LLVMUsed = M.GetLLVMUsed();

  // Resources
  for (auto && C : H.GetCBuffers()) {
    auto b = make_unique<DxilCBuffer>();
    InitResourceBase(C.get(), b.get());
    b->SetSize(C->GetSize());
    if (HasDebugInfo)
      LLVMUsed.emplace_back(cast<GlobalVariable>(b->GetGlobalSymbol()));

    b->SetGlobalSymbol(UndefValue::get(b->GetGlobalSymbol()->getType()));
    M.AddCBuffer(std::move(b));
  }
  for (auto && C : H.GetUAVs()) {
    auto b = make_unique<DxilResource>();
    InitResource(C.get(), b.get());
    if (HasDebugInfo)
      LLVMUsed.emplace_back(cast<GlobalVariable>(b->GetGlobalSymbol()));

    b->SetGlobalSymbol(UndefValue::get(b->GetGlobalSymbol()->getType()));
    M.AddUAV(std::move(b));
  }
  for (auto && C : H.GetSRVs()) {
    auto b = make_unique<DxilResource>();
    InitResource(C.get(), b.get());
    if (HasDebugInfo)
      LLVMUsed.emplace_back(cast<GlobalVariable>(b->GetGlobalSymbol()));

    b->SetGlobalSymbol(UndefValue::get(b->GetGlobalSymbol()->getType()));
    M.AddSRV(std::move(b));
  }
  for (auto && C : H.GetSamplers()) {
    auto b = make_unique<DxilSampler>();
    InitResourceBase(C.get(), b.get());
    b->SetSamplerKind(C->GetSamplerKind());
    if (HasDebugInfo)
      LLVMUsed.emplace_back(cast<GlobalVariable>(b->GetGlobalSymbol()));

    b->SetGlobalSymbol(UndefValue::get(b->GetGlobalSymbol()->getType()));
    M.AddSampler(std::move(b));
  }

  // Signatures.
  M.ResetInputSignature(H.ReleaseInputSignature());
  M.ResetOutputSignature(H.ReleaseOutputSignature());
  M.ResetPatchConstantSignature(H.ReleasePatchConstantSignature());
  M.ResetRootSignature(H.ReleaseRootSignature());

  // Shader properties.
  //bool m_bDisableOptimizations;
  M.m_ShaderFlags.SetDisableOptimizations(H.GetHLOptions().bDisableOptimizations);
  //bool m_bDisableMathRefactoring;
  //bool m_bEnableDoublePrecision;
  //bool m_bEnableDoubleExtensions;
  //bool m_bEnableMinPrecision;
  //M.CollectShaderFlags();

  //bool m_bForceEarlyDepthStencil;
  //bool m_bEnableRawAndStructuredBuffers;
  //bool m_bEnableMSAD;
  //M.m_ShaderFlags.SetAllResourcesBound(H.GetHLOptions().bAllResourcesBound);

  // Compute shader.
  if (FnProps != nullptr && FnProps->shaderKind == DXIL::ShaderKind::Compute) {
    auto &CS = FnProps->ShaderProps.CS;
    for (size_t i = 0; i < _countof(M.m_NumThreads); ++i)
      M.m_NumThreads[i] = CS.numThreads[i];
  }

  // Geometry shader.
  if (FnProps != nullptr && FnProps->shaderKind == DXIL::ShaderKind::Geometry) {
    auto &GS = FnProps->ShaderProps.GS;
    M.SetInputPrimitive(GS.inputPrimitive);
    M.SetMaxVertexCount(GS.maxVertexCount);
    for (size_t i = 0; i < _countof(GS.streamPrimitiveTopologies); ++i) {
      if (GS.streamPrimitiveTopologies[i] != DXIL::PrimitiveTopology::Undefined) {
        M.SetStreamActive(i, true);
        DXASSERT_NOMSG(M.GetStreamPrimitiveTopology() ==
                           DXIL::PrimitiveTopology::Undefined ||
                       M.GetStreamPrimitiveTopology() ==
                           GS.streamPrimitiveTopologies[i]);
        M.SetStreamPrimitiveTopology(GS.streamPrimitiveTopologies[i]);
      }
    }
    M.SetGSInstanceCount(GS.instanceCount);
  }

  // Hull and Domain shaders.
  if (FnProps != nullptr && FnProps->shaderKind == DXIL::ShaderKind::Domain) {
    auto &DS = FnProps->ShaderProps.DS;
    M.SetTessellatorDomain(DS.domain);
    M.SetInputControlPointCount(DS.inputControlPoints);
  }

  // Hull shader.
  if (FnProps != nullptr && FnProps->shaderKind == DXIL::ShaderKind::Hull) {
    auto &HS = FnProps->ShaderProps.HS;
    M.SetPatchConstantFunction(HS.patchConstantFunc);
    M.SetTessellatorDomain(HS.domain);
    M.SetTessellatorPartitioning(HS.partition);
    M.SetTessellatorOutputPrimitive(HS.outputPrimitive);
    M.SetInputControlPointCount(HS.inputControlPoints);
    M.SetOutputControlPointCount(HS.outputControlPoints);
    M.SetMaxTessellationFactor(HS.maxTessFactor);
  }

  // Pixel shader.
  if (FnProps != nullptr && FnProps->shaderKind == DXIL::ShaderKind::Pixel) {
    auto &PS = FnProps->ShaderProps.PS;
    M.m_ShaderFlags.SetForceEarlyDepthStencil(PS.EarlyDepthStencil);
  }

  // DXIL type system.
  M.ResetTypeSystem(H.ReleaseTypeSystem());
  // Dxil OP.
  M.ResetOP(H.ReleaseOP());
  // Keep llvm used.
  M.EmitLLVMUsed();

  M.m_ShaderFlags.SetAllResourcesBound(H.GetHLOptions().bAllResourcesBound);

  // Update Validator Version
  M.UpgradeToMinValidatorVersion();
}

class DxilGenerationPass : public ModulePass {
  HLModule *m_pHLModule;
  bool m_HasDbgInfo;
  HLSLExtensionsCodegenHelper *m_extensionsCodegenHelper;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilGenerationPass(bool NoOpt = false)
      : ModulePass(ID), m_pHLModule(nullptr), NotOptimized(NoOpt), m_extensionsCodegenHelper(nullptr) {}

  const char *getPassName() const override { return "DXIL Generator"; }

  void SetExtensionsHelper(HLSLExtensionsCodegenHelper *helper) {
    m_extensionsCodegenHelper = helper;
  }

  bool runOnModule(Module &M) override {
    m_pHLModule = &M.GetOrCreateHLModule();
    const ShaderModel *SM = m_pHLModule->GetShaderModel();

    // Load up debug information, to cross-reference values and the instructions
    // used to load them.
    m_HasDbgInfo = getDebugMetadataVersionFromModule(M) != 0;
    if (!SM->IsCS() && !SM->IsLib()) {
      CreateDxilSignatures();

      // Allocate input output.
      AllocateDxilInputOutputs();

      GenerateDxilInputs();
      GenerateDxilOutputs();
    } else if (SM->IsCS())
      GenerateDxilCSInputs();

    if (SM->IsDS() || SM->IsHS())
      GenerateDxilPatchConstantLdSt();
    if (SM->IsHS())
      GenerateDxilPatchConstantFunctionInputs();
    std::unordered_set<LoadInst *> UpdateCounterSet;
    std::unordered_set<Value *> NonUniformSet;

    GenerateDxilOperations(M, UpdateCounterSet, NonUniformSet);

    std::unordered_map<Instruction *, Value *> handleMap;
    GenerateDxilCBufferHandles(NonUniformSet);
    GenerateParamDxilResourceHandles(handleMap);
    GenerateDxilResourceHandles(UpdateCounterSet, NonUniformSet);
    AddCreateHandleForPhiNodeAndSelect(m_pHLModule->GetOP());

    // For module which not promote mem2reg.
    // Remove local resource alloca/load/store/phi.
    for (auto It = M.begin(); It != M.end();) {
      Function &F = *(It++);
      if (!F.isDeclaration()) {
        RemoveLocalDxilResourceAllocas(&F);
        if (hlsl::GetHLOpcodeGroupByName(&F) == HLOpcodeGroup::HLCreateHandle) {
          if (F.user_empty()) {
            F.eraseFromParent();
          } else {
            M.getContext().emitError("Fail to lower createHandle.");
          }
        }
      }
    }

    // Translate precise on allocas into function call to keep the information after mem2reg.
    // The function calls will be removed after propagate precise attribute.
    TranslatePreciseAttribute();
    // Change struct type to legacy layout for cbuf and struct buf.
    UpdateStructTypeForLegacyLayout();

    // High-level metadata should now be turned into low-level metadata.
    const bool SkipInit = true;
    hlsl::DxilModule &DxilMod = M.GetOrCreateDxilModule(SkipInit);
    InitDxilModuleFromHLModule(*m_pHLModule, DxilMod, m_HasDbgInfo);
    HLModule::ClearHLMetadata(M);
    M.ResetHLModule();

    // Remove debug code when not debug info.
    if (!m_HasDbgInfo)
      DxilMod.StripDebugRelatedCode();

    return true;
  }

private:
  void ProcessArgument(Function *func, DxilFunctionAnnotation *EntryAnnotation, Argument &arg, bool isPatchConstantFunction, bool forceOut, bool &hasClipPlane);
  void CreateDxilSignatures();
  // Allocate DXIL input/output.
  void AllocateDxilInputOutputs();
  // generate DXIL input load, output store
  void GenerateDxilInputs();
  void GenerateDxilOutputs();
  void GenerateDxilInputsOutputs(bool bInput);
  void GenerateDxilCSInputs();
  void GenerateDxilPatchConstantLdSt();
  void GenerateDxilPatchConstantFunctionInputs();
  void GenerateClipPlanesForVS(Value *outPosition);
  bool HasClipPlanes();

  void RemoveLocalDxilResourceAllocas(Function *F);
  void
  TranslateDxilResourceUses(DxilResourceBase &res,
                            std::unordered_set<LoadInst *> &UpdateCounterSet,
                            std::unordered_set<Value *> &NonUniformSet);
  void
  GenerateDxilResourceHandles(std::unordered_set<LoadInst *> &UpdateCounterSet,
                              std::unordered_set<Value *> &NonUniformSet);
  void AddCreateHandleForPhiNodeAndSelect(OP *hlslOP);
  void TranslateParamDxilResourceHandles(Function *F, std::unordered_map<Instruction *, Value *> &handleMap);
  void GenerateParamDxilResourceHandles(
      std::unordered_map<Instruction *, Value *> &handleMap);
  // Generate DXIL cbuffer handles.
  void
  GenerateDxilCBufferHandles(std::unordered_set<Value *> &NonUniformSet);

  // Generate DXIL stream output operation.
  void GenerateStreamOutputOperation(Value *streamVal, unsigned streamID);
  // Generate DXIL stream output operations.
  void GenerateStreamOutputOperations();

  // change built-in funtion into DXIL operations
  void GenerateDxilOperations(Module &M,
                              std::unordered_set<LoadInst *> &UpdateCounterSet,
                              std::unordered_set<Value *> &NonUniformSet);

  // Change struct type to legacy layout for cbuf and struct buf.
  void UpdateStructTypeForLegacyLayout();

  // Translate precise attribute into HL function call.
  void TranslatePreciseAttribute();

  // SignatureElement to Value map.
  std::unordered_map<DxilSignatureElement *, Value *> m_sigValueMap;
  // Set to save inout arguments.
  std::unordered_set<Value *> m_inoutArgSet;

  // SignatureElement which has precise attribute.
  std::unordered_set<DxilSignatureElement *> m_preciseSigSet;
  // Patch constant function inputs to signature element map.
  std::unordered_map<unsigned, DxilSignatureElement *> m_patchConstantInputsSigMap;
  // Input module is not optimized.
  bool NotOptimized;

  // For validation
  std::unordered_map<unsigned, std::unordered_set<unsigned> > m_InputSemanticsUsed,
    m_OutputSemanticsUsed[4], m_PatchConstantSemanticsUsed, m_OtherSemanticsUsed;
};

class SimplifyInst : public FunctionPass {
public:
  static char ID;

  SimplifyInst() : FunctionPass(ID) {
    initializeScalarizerPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

private:
};
}

char SimplifyInst::ID = 0;

FunctionPass *llvm::createSimplifyInstPass() { return new SimplifyInst(); }

INITIALIZE_PASS(SimplifyInst, "simplify-inst", "Simplify Instructions", false, false)

bool SimplifyInst::runOnFunction(Function &F) {
  for (Function::iterator BBI = F.begin(), BBE = F.end(); BBI != BBE; ++BBI) {
    BasicBlock *BB = BBI;
    llvm::SimplifyInstructionsInBlock(BB, nullptr);
  }
  return true;
}

// Decompose semantic name (eg FOO1=>FOO,1), change interp mode for SV_Position.
// Return semantic index.
static unsigned UpateSemanticAndInterpMode(StringRef &semName,
                                           DXIL::InterpolationMode &mode,
                                           DXIL::SigPointKind kind,
                                           LLVMContext &Context) {
  llvm::StringRef baseSemName; // The 'FOO' in 'FOO1'.
  uint32_t semIndex; // The '1' in 'FOO1'

  // Split semName and index.
  Semantic::DecomposeNameAndIndex(semName, &baseSemName, &semIndex);
  semName = baseSemName;
  const Semantic *semantic = Semantic::GetByName(semName, kind);
  if (semantic && semantic->GetKind() == Semantic::Kind::Position) {
    // Update interp mode to no_perspective version for SV_Position.
    switch (mode) {
    case InterpolationMode::Kind::LinearCentroid:
      mode = InterpolationMode::Kind::LinearNoperspectiveCentroid;
      break;
    case InterpolationMode::Kind::LinearSample:
      mode = InterpolationMode::Kind::LinearNoperspectiveSample;
      break;
    case InterpolationMode::Kind::Linear:
      mode = InterpolationMode::Kind::LinearNoperspective;
      break;
    case InterpolationMode::Kind::Constant: {
      Context.emitError("invalid interpolation mode for SV_Position");
    } break;
    }
  }
  return semIndex;
}

static DxilSignatureElement *FindArgInSignature(Argument &arg, llvm::StringRef semantic, DXIL::InterpolationMode interpMode, DXIL::SigPointKind kind,
    DxilSignature &sig) {
  // Match output ID.
  unsigned semIndex =
      UpateSemanticAndInterpMode(semantic, interpMode, kind, arg.getContext());

  for (uint32_t i = 0; i < sig.GetElements().size(); i++) {
    DxilSignatureElement &SE = sig.GetElement(i);
    bool semNameMatch = semantic.equals_lower(SE.GetName());
    bool semIndexMatch = semIndex == SE.GetSemanticIndexVec()[0];

    if (semNameMatch && semIndexMatch) {
      // Find a match.
      return &SE;
    }
  }
  return nullptr;
}

static void replaceInputOutputWithIntrinsic(DXIL::SemanticKind semKind, Value * GV, OP * hlslOP, IRBuilder<>& Builder);

void DxilGenerationPass::ProcessArgument(Function *func,
                                         DxilFunctionAnnotation *funcAnnotation,
                                         Argument &arg,
                                         bool isPatchConstantFunction,
                                         bool forceOut, bool &hasClipPlane) {
  const ShaderModel *SM = m_pHLModule->GetShaderModel();

  Type *Ty = arg.getType();
  DxilParameterAnnotation &paramAnnotation =
      funcAnnotation->GetParameterAnnotation(arg.getArgNo());
  hlsl::DxilParamInputQual qual =
      forceOut ? DxilParamInputQual::Out : paramAnnotation.GetParamInputQual();
  bool isInout = qual == DxilParamInputQual::Inout;

  // If this was an inout param, do the output side first
  if (isInout) {
    DXASSERT(!isPatchConstantFunction,
             "Patch Constant function should not have inout param");
    m_inoutArgSet.insert(&arg);
    ProcessArgument(func, funcAnnotation, arg, isPatchConstantFunction,
                    /*forceOut*/ true, hasClipPlane);
    qual = DxilParamInputQual::In;
  }

  // Get stream index
  unsigned streamIdx = 0;
  switch (qual) {
  case DxilParamInputQual::OutStream1:
    streamIdx = 1;
    break;
  case DxilParamInputQual::OutStream2:
    streamIdx = 2;
    break;
  case DxilParamInputQual::OutStream3:
    streamIdx = 3;
    break;
  default:
    // Use streamIdx = 0 by default.
    break;
  }

  const SigPoint *sigPoint = SigPoint::GetSigPoint(
      SigPointFromInputQual(qual, SM->GetKind(), isPatchConstantFunction));

  unsigned rows, cols;
  HLModule::GetParameterRowsAndCols(Ty, rows, cols, paramAnnotation);
  CompType EltTy = paramAnnotation.GetCompType();
  DXIL::InterpolationMode interpMode =
      paramAnnotation.GetInterpolationMode().GetKind();

  // Set undefined interpMode.
  if (!sigPoint->NeedsInterpMode())
    interpMode = InterpolationMode::Kind::Undefined;
  else if (interpMode == InterpolationMode::Kind::Undefined) {
    // Type-based default: linear for floats, constant for others.
    if (EltTy.IsFloatTy())
      interpMode = InterpolationMode::Kind::Linear;
    else
      interpMode = InterpolationMode::Kind::Constant;
  }

  llvm::StringRef semanticStr = paramAnnotation.GetSemanticString();
  if (semanticStr.empty()) {
    m_pHLModule->GetModule()->getContext().emitError(
        "Semantic must be defined for all parameters of an entry function or "
        "patch constant function");
    return;
  }
  UpateSemanticAndInterpMode(semanticStr, interpMode, sigPoint->GetKind(),
                             arg.getContext());

  // Get Semantic interpretation, skipping if not in signature
  const Semantic *pSemantic = Semantic::GetByName(semanticStr);
  DXIL::SemanticInterpretationKind interpretation =
      SigPoint::GetInterpretation(pSemantic->GetKind(), sigPoint->GetKind(),
                                  SM->GetMajor(), SM->GetMinor());

  // Verify system value semantics do not overlap.
  // Note: Arbitrary are always in the signature and will be verified with a different mechanism.
  // For patch constant function, only validate patch constant elements (others already validated on hull function)
  if (pSemantic->GetKind() != DXIL::SemanticKind::Arbitrary &&
      (!isPatchConstantFunction || (!sigPoint->IsInput() && !sigPoint->IsOutput()))) {
    auto &SemanticUseMap = sigPoint->IsInput() ? m_InputSemanticsUsed :
      (sigPoint->IsOutput() ? m_OutputSemanticsUsed[streamIdx] :
       (sigPoint->IsPatchConstant() ? m_PatchConstantSemanticsUsed : m_OtherSemanticsUsed));
    if (SemanticUseMap.count((unsigned)pSemantic->GetKind()) > 0) {
      auto &SemanticIndexSet = SemanticUseMap[(unsigned)pSemantic->GetKind()];
      for (unsigned idx : paramAnnotation.GetSemanticIndexVec()) {
        if (SemanticIndexSet.count(idx) > 0) {
          m_pHLModule->GetModule()->getContext().emitError(
              Twine("Parameter with semantic ") + semanticStr +
              Twine(" has overlapping semantic index at ") + Twine(idx));
          return;
        }
      }
    }
    auto &SemanticIndexSet = SemanticUseMap[(unsigned)pSemantic->GetKind()];
    for (unsigned idx : paramAnnotation.GetSemanticIndexVec()) {
      SemanticIndexSet.emplace(idx);
    }
    // Enforce Coverage and InnerCoverage input mutual exclusivity
    if (sigPoint->IsInput()) {
      if ((pSemantic->GetKind() == DXIL::SemanticKind::Coverage && SemanticUseMap.count((unsigned)DXIL::SemanticKind::InnerCoverage) > 0) ||
          (pSemantic->GetKind() == DXIL::SemanticKind::InnerCoverage && SemanticUseMap.count((unsigned)DXIL::SemanticKind::Coverage) > 0)) {
        m_pHLModule->GetModule()->getContext().emitError(
          "Pixel shader inputs SV_Coverage and SV_InnerCoverage are mutually exclusive");
        return;
      }
    }
  }

  // Validate interpretation and replace argument usage with load/store
  // intrinsics
  {
    switch (interpretation) {
    case DXIL::SemanticInterpretationKind::NA:
      m_pHLModule->GetModule()->getContext().emitError(
          Twine("Semantic ") + semanticStr +
          Twine(" is invalid for shader model: ") + SM->GetKindName());
      return;
    case DXIL::SemanticInterpretationKind::NotInSig:
    case DXIL::SemanticInterpretationKind::Shadow: {
      IRBuilder<> funcBuilder(func->getEntryBlock().getFirstInsertionPt());
      if (DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(&arg)) {
        funcBuilder.SetCurrentDebugLocation(DDI->getDebugLoc());
      }
      replaceInputOutputWithIntrinsic(pSemantic->GetKind(), &arg,
                                      m_pHLModule->GetOP(), funcBuilder);
      if (interpretation == DXIL::SemanticInterpretationKind::NotInSig)
        return; // This argument should not be included in the signature
      break;
    }
    case DXIL::SemanticInterpretationKind::SV:
    case DXIL::SemanticInterpretationKind::SGV:
    case DXIL::SemanticInterpretationKind::Arb:
    case DXIL::SemanticInterpretationKind::Target:
    case DXIL::SemanticInterpretationKind::TessFactor:
    case DXIL::SemanticInterpretationKind::NotPacked:
      // Will be replaced with load/store intrinsics in
      // GenerateDxilInputsOutputs
      break;
    default:
      DXASSERT(false, "Unexpected SemanticInterpretationKind");
      return;
    }
  }

  // Determine signature this argument belongs in, if any
  DxilSignature *pSig = nullptr;
  DXIL::SignatureKind sigKind = sigPoint->GetSignatureKindWithFallback();
  switch (sigKind) {
  case DXIL::SignatureKind::Input:
    pSig = &m_pHLModule->GetInputSignature();
    break;
  case DXIL::SignatureKind::Output:
    pSig = &m_pHLModule->GetOutputSignature();
    break;
  case DXIL::SignatureKind::PatchConstant:
    pSig = &m_pHLModule->GetPatchConstantSignature();
    break;
  default:
    DXASSERT(false, "Expected real signature kind at this point");
    return; // No corresponding signature
  }

  // Create and add element to signature
  DxilSignatureElement *pSE = nullptr;
  {
    // Add signature element to appropriate maps
    if (isPatchConstantFunction &&
        sigKind != DXIL::SignatureKind::PatchConstant) {
      pSE = FindArgInSignature(arg, paramAnnotation.GetSemanticString(),
                               interpMode, sigPoint->GetKind(), *pSig);
      if (!pSE) {
        m_pHLModule->GetModule()->getContext().emitError(
          Twine("Signature element ") + semanticStr +
          Twine(", referred to by patch constant function, is not found in corresponding hull shader ") + 
          (sigKind == DXIL::SignatureKind::Input ? "input." : "output."));
        return;
      }
      m_patchConstantInputsSigMap[arg.getArgNo()] = pSE;
    } else {
      std::unique_ptr<DxilSignatureElement> SE = pSig->CreateElement();
      pSE = SE.get();
      pSig->AppendElement(std::move(SE));
      pSE->SetSigPointKind(sigPoint->GetKind());
      pSE->Initialize(semanticStr, EltTy, interpMode, rows, cols,
                      Semantic::kUndefinedRow, Semantic::kUndefinedCol,
                      pSE->GetID(), paramAnnotation.GetSemanticIndexVec());
      m_sigValueMap[pSE] = &arg;
    }
  }

  if (paramAnnotation.IsPrecise())
    m_preciseSigSet.insert(pSE);
  if (sigKind == DXIL::SignatureKind::Output &&
      pSemantic->GetKind() == Semantic::Kind::Position && hasClipPlane) {
    GenerateClipPlanesForVS(&arg);
    hasClipPlane = false;
  }

  // Set Output Stream.
  if (streamIdx > 0)
    pSE->SetOutputStream(streamIdx);
}

void DxilGenerationPass::CreateDxilSignatures() {
  const ShaderModel *SM = m_pHLModule->GetShaderModel();

  Function *EntryFunc = m_pHLModule->GetEntryFunction();
  DXASSERT(EntryFunc->getReturnType()->isVoidTy(), "Should changed in SROA_Parameter_HLSL");

  DxilFunctionAnnotation *EntryAnnotation = m_pHLModule->GetFunctionAnnotation(EntryFunc);
  DXASSERT(EntryAnnotation, "must have function annotation for entry function");
  bool bHasClipPlane = SM->IsVS() ? HasClipPlanes() : false;

  for (Argument &arg : EntryFunc->getArgumentList()) {
    Type *Ty = arg.getType();
    // Skip streamout obj.
    if (HLModule::IsStreamOutputPtrType(Ty))
      continue;

    ProcessArgument(EntryFunc, EntryAnnotation, arg, /*isPatchConstantFunction*/false, /*forceOut*/false, bHasClipPlane);
  }

  if (bHasClipPlane) {
    EntryFunc->getContext().emitError("Cannot use clipplanes attribute without "
                                      "specifying a 4-component SV_Position "
                                      "output");
  }

  m_OtherSemanticsUsed.clear();

  if (SM->IsHS()) {
    DxilFunctionProps &EntryProps = m_pHLModule->GetDxilFunctionProps(EntryFunc);
    Function *patchConstantFunc = EntryProps.ShaderProps.HS.patchConstantFunc;
    if (patchConstantFunc == nullptr) {
      EntryFunc->getContext().emitError("Patch constant function is not specified.");
    }

    DxilFunctionAnnotation *patchFuncAnnotation = m_pHLModule->GetFunctionAnnotation(patchConstantFunc);
    DXASSERT(patchFuncAnnotation, "must have function annotation for patch constant function");

    for (Argument &arg : patchConstantFunc->getArgumentList()) {
      ProcessArgument(patchConstantFunc, patchFuncAnnotation, arg, /*isPatchConstantFunction*/true, /*forceOut*/false, bHasClipPlane);
    }
  }
}

// Allocate input/output slots
void DxilGenerationPass::AllocateDxilInputOutputs() {
  const HLOptions &opts = m_pHLModule->GetHLOptions();
  DXASSERT_NOMSG(opts.PackingStrategy < (unsigned)DXIL::PackingStrategy::Invalid);
  DXIL::PackingStrategy packing = (DXIL::PackingStrategy)opts.PackingStrategy;
  if (packing == DXIL::PackingStrategy::Default)
    packing = m_pHLModule->GetShaderModel()->GetDefaultPackingStrategy();

  m_pHLModule->GetInputSignature().PackElements(packing);
  if (!m_pHLModule->GetInputSignature().IsFullyAllocated()) {
    m_pHLModule->GetCtx().emitError("Failed to allocate all input signature elements in available space.");
  }

  m_pHLModule->GetOutputSignature().PackElements(packing);
  if (!m_pHLModule->GetOutputSignature().IsFullyAllocated()) {
    m_pHLModule->GetCtx().emitError("Failed to allocate all output signature elements in available space.");
  }

  if (m_pHLModule->GetShaderModel()->IsHS() ||
      m_pHLModule->GetShaderModel()->IsDS()) {
    m_pHLModule->GetPatchConstantSignature().PackElements(packing);
    if (!m_pHLModule->GetPatchConstantSignature().IsFullyAllocated()) {
      m_pHLModule->GetCtx().emitError("Failed to allocate all patch constant signature elements in available space.");
    }
  }
}

void DxilGenerationPass::GenerateDxilInputs() {
  GenerateDxilInputsOutputs(/*bInput*/ true);
}

void DxilGenerationPass::GenerateDxilOutputs() {
  GenerateDxilInputsOutputs(/*bInput*/ false);
}

static void GenerateStOutput(Function *stOutput, MutableArrayRef<Value *> args,
                             IRBuilder<> &Builder, bool cast) {
  if (cast) {
    Value *value = args[DXIL::OperandIndex::kStoreOutputValOpIdx];
    args[DXIL::OperandIndex::kStoreOutputValOpIdx] =
        Builder.CreateZExt(value, Builder.getInt32Ty());
  }

  Builder.CreateCall(stOutput, args);
}

static void replaceStWithStOutput(Function *stOutput, StoreInst *stInst,
                                  Constant *OpArg, Constant *outputID,
                                  Value *idx, unsigned cols, bool bI1Cast) {
  IRBuilder<> Builder(stInst);
  Value *val = stInst->getValueOperand();

  if (VectorType *VT = dyn_cast<VectorType>(val->getType())) {
    DXASSERT(cols == VT->getNumElements(), "vec size must match");
    for (unsigned col = 0; col < cols; col++) {
      Value *subVal = Builder.CreateExtractElement(val, col);
      Value *colIdx = Builder.getInt8(col);
      Value *args[] = {OpArg, outputID, idx, colIdx, subVal};
      GenerateStOutput(stOutput, args, Builder, bI1Cast);
    }
    // remove stInst
    stInst->eraseFromParent();
  } else if (!val->getType()->isArrayTy()) {
    // TODO: support case cols not 1
    DXASSERT(cols == 1, "only support scalar here");
    Value *colIdx = Builder.getInt8(0);
    Value *args[] = {OpArg, outputID, idx, colIdx, val};
    GenerateStOutput(stOutput, args, Builder, bI1Cast);
    // remove stInst
    stInst->eraseFromParent();
  } else {
    DXASSERT(0, "not support array yet");
    // TODO: support array.
    Value *colIdx = Builder.getInt8(0);
    ArrayType *AT = cast<ArrayType>(val->getType());
    Value *args[] = {OpArg, outputID, idx, colIdx, /*val*/nullptr};
    args;
    AT;
  }
}

static Value *GenerateLdInput(Function *loadInput, ArrayRef<Value *> args,
                              IRBuilder<> &Builder, Value *zero, bool bCast, Type *Ty) {
  Value *input = Builder.CreateCall(loadInput, args);
  if (!bCast)
    return input;
  else {
    Value *bVal = Builder.CreateICmpNE(input, zero);
    IntegerType *IT = cast<IntegerType>(Ty);
    if (IT->getBitWidth() == 1)
      return bVal;
    else
      return Builder.CreateZExt(bVal, Ty);
  }
}

static Value *replaceLdWithLdInput(Function *loadInput,
                                 LoadInst *ldInst,
                                 unsigned cols, 
                                 MutableArrayRef<Value *>args,
                                 bool bCast) {
  IRBuilder<> Builder(ldInst);
  Type *Ty = ldInst->getType();
  Type *EltTy = Ty->getScalarType();
  // Change i1 to i32 for load input.
  Value *zero = Builder.getInt32(0);

  if (VectorType *VT = dyn_cast<VectorType>(Ty)) {
    Value *newVec = llvm::UndefValue::get(VT);
    DXASSERT(cols == VT->getNumElements(), "vec size must match");
    for (unsigned col = 0; col < cols; col++) {
      Value *colIdx = Builder.getInt8(col);
      args[DXIL::OperandIndex::kLoadInputColOpIdx] = colIdx;
      Value *input = GenerateLdInput(loadInput, args, Builder, zero, bCast, EltTy);
      newVec = Builder.CreateInsertElement(newVec, input, col);
    }
    ldInst->replaceAllUsesWith(newVec);
    ldInst->eraseFromParent();
    return newVec;
  } else {
    Value *colIdx = args[DXIL::OperandIndex::kLoadInputColOpIdx];
    if (colIdx == nullptr) {
      DXASSERT(cols == 1, "only support scalar here");
      colIdx = Builder.getInt8(0);
    } else {
      if (colIdx->getType() == Builder.getInt32Ty()) {
        colIdx = Builder.CreateTrunc(colIdx, Builder.getInt8Ty());
      }
    }

    if (isa<ConstantInt>(colIdx)) {
      args[DXIL::OperandIndex::kLoadInputColOpIdx] = colIdx;
      Value *input = GenerateLdInput(loadInput, args, Builder, zero, bCast, EltTy);
      ldInst->replaceAllUsesWith(input);
      ldInst->eraseFromParent();
      return input;
    }
    else {
      // Vector indexing.
      // Load to array.
      ArrayType *AT = ArrayType::get(ldInst->getType(), cols);
      Value *arrayVec = Builder.CreateAlloca(AT);
      Value *zeroIdx = Builder.getInt32(0);

      for (unsigned col = 0; col < cols; col++) {
        Value *colIdx = Builder.getInt8(col);
        args[DXIL::OperandIndex::kLoadInputColOpIdx] = colIdx;
        Value *input = GenerateLdInput(loadInput, args, Builder, zero, bCast, EltTy);
        Value *GEP = Builder.CreateInBoundsGEP(arrayVec, {zeroIdx, colIdx});
        Builder.CreateStore(input, GEP);
      }
      Value *vecIndexingPtr = Builder.CreateInBoundsGEP(arrayVec, {zeroIdx, colIdx});
      Value *input = Builder.CreateLoad(vecIndexingPtr);
      ldInst->replaceAllUsesWith(input);
      ldInst->eraseFromParent();
      return input;
    }
  }
}

static void replaceDirectInputParameter(Value *param, Function *loadInput, 
    unsigned cols, 
    MutableArrayRef<Value *>args, bool bCast,
    OP *hlslOP, IRBuilder<> &Builder) {
  Value *zero = hlslOP->GetU32Const(0);
  Type *Ty = param->getType();
  Type *EltTy = Ty->getScalarType();

  if (VectorType *VT = dyn_cast<VectorType>(Ty)) {
    Value *newVec = llvm::UndefValue::get(VT);
    DXASSERT(cols == VT->getNumElements(), "vec size must match");
    for (unsigned col = 0; col < cols; col++) {
      Value *colIdx = hlslOP->GetU8Const(col);
      args[DXIL::OperandIndex::kLoadInputColOpIdx] = colIdx;
      Value *input = GenerateLdInput(loadInput, args, Builder, zero, bCast, EltTy);
      newVec = Builder.CreateInsertElement(newVec, input, col);
    }
    param->replaceAllUsesWith(newVec);
  } else if (!Ty->isArrayTy() && !HLMatrixLower::IsMatrixType(Ty)) {
    DXASSERT(cols == 1, "only support scalar here");
    Value *colIdx = hlslOP->GetU8Const(0);
    args[DXIL::OperandIndex::kLoadInputColOpIdx] = colIdx;
    Value *input = GenerateLdInput(loadInput, args, Builder, zero, bCast, EltTy);
    param->replaceAllUsesWith(input);
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    Value *colIdx = hlslOP->GetU8Const(0);
    colIdx;
    DXASSERT(param->hasOneUse(), "matrix arg should only has one use as matrix to vec");
    CallInst *CI = cast<CallInst>(param->user_back());
    HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
    DXASSERT_LOCALVAR(group, group == HLOpcodeGroup::HLCast, "must be hlcast here");
    unsigned opcode = GetHLOpcode(CI);
    HLCastOpcode matOp = static_cast<HLCastOpcode>(opcode);
    switch (matOp) {
    case HLCastOpcode::ColMatrixToVecCast: {
      IRBuilder<> LocalBuilder(CI);
      Type *matTy = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx)
                        ->getType();
      unsigned col, row;
      Type *EltTy = HLMatrixLower::GetMatrixInfo(matTy, col, row);
      std::vector<Value *> matElts(col * row);
      for (unsigned c = 0; c < col; c++) {
        Value *rowIdx = hlslOP->GetI32Const(c);
        args[DXIL::OperandIndex::kLoadInputRowOpIdx] = rowIdx;
        for (unsigned r = 0; r < row; r++) {
          Value *colIdx = hlslOP->GetU8Const(r);
          args[DXIL::OperandIndex::kLoadInputColOpIdx] = colIdx;
          Value *input = GenerateLdInput(loadInput, args, Builder, zero, bCast, EltTy);
          unsigned matIdx = c * row + r;
          matElts[matIdx] = input;
        }
      }
      Value *newVec =
          HLMatrixLower::BuildVector(EltTy, col * row, matElts, LocalBuilder);
      CI->replaceAllUsesWith(newVec);
      CI->eraseFromParent();
    } break;
    case HLCastOpcode::RowMatrixToVecCast: {
      IRBuilder<> LocalBuilder(CI);
      Type *matTy = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx)
                        ->getType();
      unsigned col, row;
      Type *EltTy = HLMatrixLower::GetMatrixInfo(matTy, col, row);
      std::vector<Value *> matElts(col * row);
      for (unsigned r = 0; r < row; r++) {
        Value *rowIdx = hlslOP->GetI32Const(r);
        args[DXIL::OperandIndex::kLoadInputRowOpIdx] = rowIdx;
        for (unsigned c = 0; c < col; c++) {
          Value *colIdx = hlslOP->GetU8Const(c);
          args[DXIL::OperandIndex::kLoadInputColOpIdx] = colIdx;
          Value *input = GenerateLdInput(loadInput, args, Builder, zero, bCast, EltTy);
          unsigned matIdx = r * col + c;
          matElts[matIdx] = input;
        }
      }
      Value *newVec =
          HLMatrixLower::BuildVector(EltTy, col * row, matElts, LocalBuilder);
      CI->replaceAllUsesWith(newVec);
      CI->eraseFromParent();
    } break;
    }
  } else {
    DXASSERT(0, "invalid type for direct input");
  }
}


namespace {
struct InputOutputAccessInfo {
  // For input output which has only 1 row, idx is 0.
  Value *idx;
  // VertexID for HS/DS/GS input.
  Value *vertexID;
  // Vector index.
  Value *vectorIdx;
  // Load/Store/LoadMat/StoreMat on input/output.
  Instruction *user;
  InputOutputAccessInfo(Value *index, Instruction *I)
      : idx(index), user(I), vertexID(nullptr), vectorIdx(nullptr) {}
  InputOutputAccessInfo(Value *index, Instruction *I, Value *ID, Value *vecIdx)
      : idx(index), user(I), vertexID(ID), vectorIdx(vecIdx) {}
};
}

static void collectInputOutputAccessInfo(Value *GV, Constant *constZero,
                             std::vector<InputOutputAccessInfo> &accessInfoList,
                             bool hasVertexID, bool bInput) {
  auto User = GV->user_begin();
  auto UserE = GV->user_end();
  for (; User != UserE;) {
    Value *I = *(User++);
    if (LoadInst *ldInst = dyn_cast<LoadInst>(I)) {
      if (bInput) {
        InputOutputAccessInfo info = {constZero, ldInst};
        accessInfoList.push_back(info);
      }
    } else if (StoreInst *stInst = dyn_cast<StoreInst>(I)) {
      if (!bInput) {
        InputOutputAccessInfo info = {constZero, stInst};
        accessInfoList.push_back(info);
      }
    } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(I)) {
      // Vector indexing may has more indices.
      // Vector indexing changed to array indexing in SROA_HLSL.
      auto idx = GEP->idx_begin();
      DXASSERT_LOCALVAR(idx, idx->get() == constZero,
                        "only support 0 offset for input pointer");

      Value *vertexID = nullptr;
      Value *vectorIdx = nullptr;
      gep_type_iterator GEPIt = gep_type_begin(GEP), E = gep_type_end(GEP);

      // Skip first pointer idx which must be 0.
      GEPIt++;
      if (hasVertexID) {
        // Save vertexID.
        vertexID = GEPIt.getOperand();
        GEPIt++;
      }
      // Start from first index.
      Value *rowIdx = GEPIt.getOperand();
      if (GEPIt != E) {
        if ((*GEPIt)->isVectorTy()) {
          // Vector indexing.
          rowIdx = constZero;
          vectorIdx = GEPIt.getOperand();
          DXASSERT_NOMSG((++GEPIt) == E);
        } else {
          // Array which may have vector indexing.
          GEPIt++;
          IRBuilder<> Builder(GEP);
          Type *idxTy = rowIdx->getType();
          for (; GEPIt != E; ++GEPIt) {
            DXASSERT(!GEPIt->isStructTy(),
                     "Struct should be flattened SROA_Parameter_HLSL");
            DXASSERT(!GEPIt->isPointerTy(),
                     "not support pointer type in middle of GEP");
            if (GEPIt->isArrayTy()) {
              Constant *arraySize =
                  ConstantInt::get(idxTy, GEPIt->getArrayNumElements());
              rowIdx = Builder.CreateMul(rowIdx, arraySize);
              rowIdx = Builder.CreateAdd(rowIdx, GEPIt.getOperand());
            } else {
              Type *Ty = *GEPIt;
              DXASSERT_LOCALVAR(Ty, Ty->isVectorTy(),
                                "must be vector type here to index");
              // Save vector idx.
              vectorIdx = GEPIt.getOperand();
            }
          }
        }
      } else
        rowIdx = constZero;

      auto GepUser = GEP->user_begin();
      auto GepUserE = GEP->user_end();
      Value *idxVal = rowIdx;

      for (; GepUser != GepUserE;) {
        auto GepUserIt = GepUser++;
        if (LoadInst *ldInst = dyn_cast<LoadInst>(*GepUserIt)) {
          if (bInput) {
            InputOutputAccessInfo info = {idxVal, ldInst, vertexID, vectorIdx};
            accessInfoList.push_back(info);
          }
        } else if (StoreInst *stInst = dyn_cast<StoreInst>(*GepUserIt)) {
          if (!bInput) {
            InputOutputAccessInfo info = {idxVal, stInst, vertexID, vectorIdx};
            accessInfoList.push_back(info);
          }
        } else if (CallInst *CI = dyn_cast<CallInst>(*GepUserIt)) {
          HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
          DXASSERT_LOCALVAR(group, group == HLOpcodeGroup::HLMatLoadStore,
                            "input/output should only used by ld/st");
          HLMatLoadStoreOpcode opcode = (HLMatLoadStoreOpcode)GetHLOpcode(CI);
          if ((opcode == HLMatLoadStoreOpcode::ColMatLoad || opcode == HLMatLoadStoreOpcode::RowMatLoad) ? bInput : !bInput) {
            InputOutputAccessInfo info = {idxVal, CI, vertexID, vectorIdx};
            accessInfoList.push_back(info);
          }
        } else
          DXASSERT(0, "input output should only used by ld/st");
      }
    } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
      InputOutputAccessInfo info = {constZero, CI};
      accessInfoList.push_back(info);  
    } else
      DXASSERT(0, "input output should only used by ld/st");
  }
}

static void replaceInputOutputWithIntrinsic(DXIL::SemanticKind semKind, Value *GV, OP *hlslOP, IRBuilder<> &Builder) {
  Type *Ty = GV->getType();
  if (Ty->isPointerTy())
    Ty = Ty->getPointerElementType();

  OP::OpCode opcode;
  switch (semKind) {
  case Semantic::Kind::DomainLocation:        opcode = OP::OpCode::DomainLocation;        break;
  case Semantic::Kind::OutputControlPointID:  opcode = OP::OpCode::OutputControlPointID;  break;
  case Semantic::Kind::GSInstanceID:          opcode = OP::OpCode::GSInstanceID;          break;
  case Semantic::Kind::PrimitiveID:           opcode = OP::OpCode::PrimitiveID;           break;
  case Semantic::Kind::SampleIndex:           opcode = OP::OpCode::SampleIndex;           break;
  case Semantic::Kind::Coverage:              opcode = OP::OpCode::Coverage;              break;
  case Semantic::Kind::InnerCoverage:         opcode = OP::OpCode::InnerCoverage;         break;
  case Semantic::Kind::ViewID:                opcode = OP::OpCode::ViewID;                break;
  default:
    DXASSERT(0, "invalid semantic");
    return;
  }

  Function *dxilFunc = hlslOP->GetOpFunc(opcode, Ty->getScalarType());
  Constant *OpArg = hlslOP->GetU32Const((unsigned)opcode);

  Value *newArg = nullptr;
  if (semKind == Semantic::Kind::DomainLocation) {
    unsigned vecSize = 1;
    if (Ty->isVectorTy())
      vecSize = Ty->getVectorNumElements();

    newArg = Builder.CreateCall(dxilFunc, {OpArg, hlslOP->GetU8Const(0)});
    if (vecSize > 1) {
      Value *result = UndefValue::get(Ty);
      result = Builder.CreateInsertElement(result, newArg, (uint64_t)0);

      for (unsigned i = 1; i < vecSize; i++) {
        Value *newElt =
            Builder.CreateCall(dxilFunc, {OpArg, hlslOP->GetU8Const(i)});
        result = Builder.CreateInsertElement(result, newElt, i);
      }
      newArg = result;
    }
  } else {
    newArg = Builder.CreateCall(dxilFunc, {OpArg});
  }

  if (newArg->getType() != GV->getType()) {
    DXASSERT_NOMSG(GV->getType()->isPointerTy());
    for (User *U : GV->users()) {
      if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
        LI->replaceAllUsesWith(newArg);
      }
    }
  } else {
    GV->replaceAllUsesWith(newArg);
  }
}

void GenerateInputOutputUserCall(InputOutputAccessInfo &info, Value *undefVertexIdx,
    Function *ldStFunc, Constant *OpArg, Constant *ID, unsigned cols, bool bI1Cast,
    Constant *columnConsts[],
    bool bNeedVertexID, bool isArrayTy, bool bInput, bool bIsInout) {
  Value *idxVal = info.idx;
  Value *vertexID = undefVertexIdx;
  if (bNeedVertexID && isArrayTy) {
    vertexID = info.vertexID;
  }

  if (LoadInst *ldInst = dyn_cast<LoadInst>(info.user)) {
    SmallVector<Value *, 4> args = {OpArg, ID, idxVal, info.vectorIdx};
    if (vertexID)
      args.emplace_back(vertexID);

    replaceLdWithLdInput(ldStFunc, ldInst, cols, args, bI1Cast);
  } else if (StoreInst *stInst = dyn_cast<StoreInst>(info.user)) {
    if (bInput) {
      DXASSERT_LOCALVAR(bIsInout, bIsInout, "input should not have store use.");
    } else {
      if (!info.vectorIdx) {
        replaceStWithStOutput(ldStFunc, stInst, OpArg, ID, idxVal, cols,
                              bI1Cast);
      } else {
        Value *V = stInst->getValueOperand();
        Type *Ty = V->getType();
        DXASSERT_LOCALVAR(Ty, Ty == Ty->getScalarType() && !Ty->isAggregateType(),
                          "only support scalar here");

        if (ConstantInt *ColIdx = dyn_cast<ConstantInt>(info.vectorIdx)) {
          IRBuilder<> Builder(stInst);
          if (ColIdx->getType()->getBitWidth() != 8) {
            ColIdx = Builder.getInt8(ColIdx->getValue().getLimitedValue());
          }
          Value *args[] = {OpArg, ID, idxVal, ColIdx, V};
          GenerateStOutput(ldStFunc, args, Builder, bI1Cast);
        } else {
          BasicBlock *BB = stInst->getParent();
          BasicBlock *EndBB = BB->splitBasicBlock(stInst);

          TerminatorInst *TI = BB->getTerminator();
          IRBuilder<> SwitchBuilder(TI);
          LLVMContext &Ctx = stInst->getContext();
          SwitchInst *Switch =
              SwitchBuilder.CreateSwitch(info.vectorIdx, EndBB, cols);
          TI->eraseFromParent();

          Function *F = EndBB->getParent();
          for (unsigned i = 0; i < cols; i++) {
            BasicBlock *CaseBB = BasicBlock::Create(Ctx, "case", F, EndBB);
            Switch->addCase(SwitchBuilder.getInt32(i), CaseBB);
            IRBuilder<> CaseBuilder(CaseBB);

            ConstantInt *CaseIdx = SwitchBuilder.getInt8(i);

            Value *args[] = {OpArg, ID, idxVal, CaseIdx, V};
            GenerateStOutput(ldStFunc, args, CaseBuilder, bI1Cast);

            CaseBuilder.CreateBr(EndBB);
          }
        }
        // remove stInst
        stInst->eraseFromParent();
      }
    }
  } else if (CallInst *CI = dyn_cast<CallInst>(info.user)) {
    HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
    // Intrinsic will be translated later.
    if (group == HLOpcodeGroup::HLIntrinsic)
      return;
    unsigned opcode = GetHLOpcode(CI);
    DXASSERT(group == HLOpcodeGroup::HLMatLoadStore, "");
    HLMatLoadStoreOpcode matOp = static_cast<HLMatLoadStoreOpcode>(opcode);
    switch (matOp) {
    case HLMatLoadStoreOpcode::ColMatLoad: {
      IRBuilder<> LocalBuilder(CI);
      Type *matTy = CI->getArgOperand(HLOperandIndex::kMatLoadPtrOpIdx)
                        ->getType()
                        ->getPointerElementType();
      unsigned col, row;
      Type *EltTy = HLMatrixLower::GetMatrixInfo(matTy, col, row);
      std::vector<Value *> matElts(col * row);
      for (unsigned c = 0; c < col; c++) {
        Constant *constRowIdx = LocalBuilder.getInt32(c);
        Value *rowIdx = LocalBuilder.CreateAdd(idxVal, constRowIdx);
        for (unsigned r = 0; r < row; r++) {
          SmallVector<Value *, 4> args = {OpArg, ID, rowIdx, columnConsts[r]};
          if (vertexID)
            args.emplace_back(vertexID);

          Value *input = LocalBuilder.CreateCall(ldStFunc, args);
          unsigned matIdx = c * row + r;
          matElts[matIdx] = input;
        }
      }
      Value *newVec =
          HLMatrixLower::BuildVector(EltTy, col * row, matElts, LocalBuilder);
      CI->replaceAllUsesWith(newVec);
      CI->eraseFromParent();
    } break;
    case HLMatLoadStoreOpcode::RowMatLoad: {
      IRBuilder<> LocalBuilder(CI);
      Type *matTy = CI->getArgOperand(HLOperandIndex::kMatLoadPtrOpIdx)
                        ->getType()
                        ->getPointerElementType();
      unsigned col, row;
      Type *EltTy = HLMatrixLower::GetMatrixInfo(matTy, col, row);
      std::vector<Value *> matElts(col * row);
      for (unsigned r = 0; r < row; r++) {
        Constant *constRowIdx = LocalBuilder.getInt32(r);
        Value *rowIdx = LocalBuilder.CreateAdd(idxVal, constRowIdx);
        for (unsigned c = 0; c < col; c++) {
          SmallVector<Value *, 4> args = {OpArg, ID, rowIdx, columnConsts[c]};
          if (vertexID)
            args.emplace_back(vertexID);

          Value *input = LocalBuilder.CreateCall(ldStFunc, args);
          unsigned matIdx = r * col + c;
          matElts[matIdx] = input;
        }
      }
      Value *newVec =
          HLMatrixLower::BuildVector(EltTy, col * row, matElts, LocalBuilder);
      CI->replaceAllUsesWith(newVec);
      CI->eraseFromParent();
    } break;
    case HLMatLoadStoreOpcode::ColMatStore: {
      IRBuilder<> LocalBuilder(CI);
      Value *Val = CI->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);
      Type *matTy = CI->getArgOperand(HLOperandIndex::kMatStoreDstPtrOpIdx)
                        ->getType()
                        ->getPointerElementType();
      unsigned col, row;
      HLMatrixLower::GetMatrixInfo(matTy, col, row);

      for (unsigned c = 0; c < col; c++) {
        Constant *constColIdx = LocalBuilder.getInt32(c);
        Value *colIdx = LocalBuilder.CreateAdd(idxVal, constColIdx);

        for (unsigned r = 0; r < row; r++) {
          unsigned matIdx = HLMatrixLower::GetColMajorIdx(r, c, row);
          Value *Elt = LocalBuilder.CreateExtractElement(Val, matIdx);
          LocalBuilder.CreateCall(ldStFunc,
                                  {OpArg, ID, colIdx, columnConsts[r], Elt});
        }
      }
      CI->eraseFromParent();
    } break;
    case HLMatLoadStoreOpcode::RowMatStore: {
      IRBuilder<> LocalBuilder(CI);
      Value *Val = CI->getArgOperand(HLOperandIndex::kMatStoreValOpIdx);
      Type *matTy = CI->getArgOperand(HLOperandIndex::kMatStoreDstPtrOpIdx)
                        ->getType()
                        ->getPointerElementType();
      unsigned col, row;
      HLMatrixLower::GetMatrixInfo(matTy, col, row);

      for (unsigned r = 0; r < row; r++) {
        Constant *constRowIdx = LocalBuilder.getInt32(r);
        Value *rowIdx = LocalBuilder.CreateAdd(idxVal, constRowIdx);
        for (unsigned c = 0; c < col; c++) {
          unsigned matIdx = HLMatrixLower::GetRowMajorIdx(r, c, col);
          Value *Elt = LocalBuilder.CreateExtractElement(Val, matIdx);
          LocalBuilder.CreateCall(ldStFunc,
                                  {OpArg, ID, rowIdx, columnConsts[c], Elt});
        }
      }
      CI->eraseFromParent();
    } break;
    }
  } else
    DXASSERT(0, "invalid operation on input output");
}

void DxilGenerationPass::GenerateDxilInputsOutputs(bool bInput) {
  OP *hlslOP = m_pHLModule->GetOP();
  const ShaderModel *pSM = m_pHLModule->GetShaderModel();
  Module &M = *(m_pHLModule->GetModule());

  OP::OpCode opcode = bInput ? OP::OpCode::LoadInput : OP::OpCode::StoreOutput;
  bool bNeedVertexID = bInput && (pSM->IsGS() || pSM->IsDS() || pSM->IsHS());

  Constant *OpArg = hlslOP->GetU32Const((unsigned)opcode);

  Constant *columnConsts[] = {
      hlslOP->GetU8Const(0),  hlslOP->GetU8Const(1),  hlslOP->GetU8Const(2),
      hlslOP->GetU8Const(3),  hlslOP->GetU8Const(4),  hlslOP->GetU8Const(5),
      hlslOP->GetU8Const(6),  hlslOP->GetU8Const(7),  hlslOP->GetU8Const(8),
      hlslOP->GetU8Const(9),  hlslOP->GetU8Const(10), hlslOP->GetU8Const(11),
      hlslOP->GetU8Const(12), hlslOP->GetU8Const(13), hlslOP->GetU8Const(14),
      hlslOP->GetU8Const(15)};

  Constant *constZero = hlslOP->GetU32Const(0);

  Value *undefVertexIdx =
      UndefValue::get(Type::getInt32Ty(m_pHLModule->GetCtx()));

  DxilSignature &Sig = bInput ? m_pHLModule->GetInputSignature()
                            : m_pHLModule->GetOutputSignature();

  Function *EntryFunc = m_pHLModule->GetEntryFunction();
  Type *i1Ty = Type::getInt1Ty(constZero->getContext());
  Type *i32Ty = constZero->getType();

  llvm::SmallVector<unsigned, 8> removeIndices;
  for (unsigned i = 0; i < Sig.GetElements().size(); i++) {
    DxilSignatureElement *SE = &Sig.GetElement(i);
    llvm::Type *Ty = SE->GetCompType().GetLLVMType(m_pHLModule->GetCtx());
    // Cast i1 to i32 for load input.
    bool bI1Cast = false;
    if (Ty == i1Ty) {
      bI1Cast = true;
      Ty = i32Ty;
    }
    if (!hlslOP->IsOverloadLegal(opcode, Ty)) {
      std::string O;
      raw_string_ostream OSS(O);
      Ty->print(OSS);
      OSS << "(type for " << SE->GetName() << ")";
      OSS << " cannot be used as shader inputs or outputs.";
      OSS.flush();
      M.getContext().emitError(O);
      continue;
    }
    Function *dxilFunc = hlslOP->GetOpFunc(opcode, Ty);
    Constant *ID = hlslOP->GetU32Const(i);
    unsigned cols = SE->GetCols();
    Value *GV = m_sigValueMap[SE];

    bool bIsInout = m_inoutArgSet.count(GV) > 0;

    IRBuilder<> EntryBuilder(EntryFunc->getEntryBlock().getFirstInsertionPt());

    if (DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(GV)) {
      EntryBuilder.SetCurrentDebugLocation(DDI->getDebugLoc());
    }

    DXIL::SemanticInterpretationKind SI = SE->GetInterpretation();
    DXASSERT_NOMSG(SI < DXIL::SemanticInterpretationKind::Invalid);
    DXASSERT_NOMSG(SI != DXIL::SemanticInterpretationKind::NA);
    DXASSERT_NOMSG(SI != DXIL::SemanticInterpretationKind::NotInSig);
    if (SI == DXIL::SemanticInterpretationKind::Shadow)
      continue;   // Handled in ProcessArgument

    if (!GV->getType()->isPointerTy()) {
      DXASSERT(bInput, "direct parameter must be input");
      Value *vertexID = undefVertexIdx;
      Value *args[] = {OpArg, ID, /*rowIdx*/constZero, /*colIdx*/nullptr, vertexID};
      replaceDirectInputParameter(GV, dxilFunc, cols, args, bI1Cast, hlslOP, EntryBuilder);
      continue;
    }

    bool bIsArrayTy = GV->getType()->getPointerElementType()->isArrayTy();
    bool bIsPrecise = m_preciseSigSet.count(SE);
    if (bIsPrecise)
      HLModule::MarkPreciseAttributeOnPtrWithFunctionCall(GV, M);

    std::vector<InputOutputAccessInfo> accessInfoList;
    collectInputOutputAccessInfo(GV, constZero, accessInfoList, bNeedVertexID && bIsArrayTy, bInput);

    for (InputOutputAccessInfo &info : accessInfoList) {
      GenerateInputOutputUserCall(info, undefVertexIdx, dxilFunc, OpArg, ID,
                                  cols, bI1Cast, columnConsts, bNeedVertexID,
                                  bIsArrayTy, bInput, bIsInout);
    }
  }
}

void DxilGenerationPass::GenerateDxilCSInputs() {
  OP *hlslOP = m_pHLModule->GetOP();

  Function *entry = m_pHLModule->GetEntryFunction();
  DxilFunctionAnnotation *funcAnnotation = m_pHLModule->GetFunctionAnnotation(entry);
  DXASSERT(funcAnnotation, "must find annotation for entry function");
  IRBuilder<> Builder(entry->getEntryBlock().getFirstInsertionPt());

  for (Argument &arg : entry->args()) {
    DxilParameterAnnotation &paramAnnotation = funcAnnotation->GetParameterAnnotation(arg.getArgNo());

    llvm::StringRef semanticStr = paramAnnotation.GetSemanticString();
    if (semanticStr.empty()) {
      m_pHLModule->GetCtx().emitError(
        "Semantic must be defined for all parameters of an entry function or patch constant function");
      return;
    }

    const Semantic *semantic =
        Semantic::GetByName(semanticStr, DXIL::SigPointKind::CSIn);
    OP::OpCode opcode;
    switch (semantic->GetKind()) {
    case Semantic::Kind::GroupThreadID:
      opcode = OP::OpCode::ThreadIdInGroup;
      break;
    case Semantic::Kind::GroupID:
      opcode = OP::OpCode::GroupId;
      break;
    case Semantic::Kind::DispatchThreadID:
      opcode = OP::OpCode::ThreadId;
      break;
    case Semantic::Kind::GroupIndex:
      opcode = OP::OpCode::FlattenedThreadIdInGroup;
      break;
    default:
      DXASSERT(semantic->IsInvalid(), "else compute shader semantics out-of-date");
      this->m_pHLModule->GetCtx().emitError("invalid semantic found in CS");
      return;
    }

    Constant *OpArg = hlslOP->GetU32Const((unsigned)opcode);
    Type *Ty = arg.getType();
    if (Ty->isPointerTy())
      Ty = Ty->getPointerElementType();
    Function *dxilFunc = hlslOP->GetOpFunc(opcode, Ty->getScalarType());
    Value *newArg = nullptr;
    if (opcode == OP::OpCode::FlattenedThreadIdInGroup) {
      newArg = Builder.CreateCall(dxilFunc, {OpArg});
    } else {
      unsigned vecSize = 1;
      if (Ty->isVectorTy())
        vecSize = Ty->getVectorNumElements();

      newArg = Builder.CreateCall(dxilFunc, {OpArg, hlslOP->GetU32Const(0)});
      if (vecSize > 1) {
        Value *result = UndefValue::get(Ty);
        result = Builder.CreateInsertElement(result, newArg, (uint64_t)0);

        for (unsigned i = 1; i < vecSize; i++) {
          Value *newElt =
              Builder.CreateCall(dxilFunc, {OpArg, hlslOP->GetU32Const(i)});
          result = Builder.CreateInsertElement(result, newElt, i);
        }
        newArg = result;
      }
    }
    if (newArg->getType() != arg.getType()) {
      DXASSERT_NOMSG(arg.getType()->isPointerTy());
      for (User *U : arg.users()) {
        LoadInst *LI = cast<LoadInst>(U);
        LI->replaceAllUsesWith(newArg);
      }
    } else {
      arg.replaceAllUsesWith(newArg);
    }
  }
}

void DxilGenerationPass::GenerateDxilPatchConstantLdSt() {
  OP *hlslOP = m_pHLModule->GetOP();
  Module &M = *(m_pHLModule->GetModule());
  Constant *constZero = hlslOP->GetU32Const(0);
  DxilSignature &Sig = m_pHLModule->GetPatchConstantSignature();
  Function *EntryFunc = m_pHLModule->GetEntryFunction();
  auto InsertPt = EntryFunc->getEntryBlock().getFirstInsertionPt();
  const bool bIsHs = m_pHLModule->GetShaderModel()->IsHS();
  const bool bIsInput = !bIsHs;
  const bool bIsInout = false;
  const bool bNeedVertexID = false;
  if (bIsHs) {
    DxilFunctionProps &EntryQual = m_pHLModule->GetDxilFunctionProps(EntryFunc);
    Function *patchConstantFunc = EntryQual.ShaderProps.HS.patchConstantFunc;
    InsertPt = patchConstantFunc->getEntryBlock().getFirstInsertionPt();
  }
  IRBuilder<> Builder(InsertPt);
  Type *i1Ty = Builder.getInt1Ty();
  Type *i32Ty = Builder.getInt32Ty();
  // LoadPatchConst don't have vertexIdx operand.
  Value *undefVertexIdx = nullptr;

  Constant *columnConsts[] = {
      hlslOP->GetU8Const(0),  hlslOP->GetU8Const(1),  hlslOP->GetU8Const(2),
      hlslOP->GetU8Const(3),  hlslOP->GetU8Const(4),  hlslOP->GetU8Const(5),
      hlslOP->GetU8Const(6),  hlslOP->GetU8Const(7),  hlslOP->GetU8Const(8),
      hlslOP->GetU8Const(9),  hlslOP->GetU8Const(10), hlslOP->GetU8Const(11),
      hlslOP->GetU8Const(12), hlslOP->GetU8Const(13), hlslOP->GetU8Const(14),
      hlslOP->GetU8Const(15)};

  OP::OpCode opcode =
      bIsInput ? OP::OpCode::LoadPatchConstant : OP::OpCode::StorePatchConstant;
  Constant *OpArg = hlslOP->GetU32Const((unsigned)opcode);

  for (unsigned i = 0; i < Sig.GetElements().size(); i++) {
    DxilSignatureElement *SE = &Sig.GetElement(i);
    Value *GV = m_sigValueMap[SE];

    DXIL::SemanticInterpretationKind SI = SE->GetInterpretation();
    DXASSERT_NOMSG(SI < DXIL::SemanticInterpretationKind::Invalid);
    DXASSERT_NOMSG(SI != DXIL::SemanticInterpretationKind::NA);
    DXASSERT_NOMSG(SI != DXIL::SemanticInterpretationKind::NotInSig);
    if (SI == DXIL::SemanticInterpretationKind::Shadow)
      continue;   // Handled in ProcessArgument

    Constant *ID = hlslOP->GetU32Const(i);
    // Generate LoadPatchConstant.
    Type *Ty = SE->GetCompType().GetLLVMType(m_pHLModule->GetCtx());
    // Cast i1 to i32 for load input.
    bool bI1Cast = false;
    if (Ty == i1Ty) {
      bI1Cast = true;
      Ty = i32Ty;
    }

    unsigned cols = SE->GetCols();

    Function *dxilFunc = hlslOP->GetOpFunc(opcode, Ty);

    if (!GV->getType()->isPointerTy()) {
      DXASSERT(bIsInput, "Must be DS input.");
      Constant *OpArg = hlslOP->GetU32Const(static_cast<unsigned>(OP::OpCode::LoadPatchConstant));
      Value *args[] = {OpArg, ID, /*rowIdx*/constZero, /*colIdx*/nullptr};
      replaceDirectInputParameter(GV, dxilFunc, cols, args, bI1Cast, hlslOP, Builder);
      continue;
    }
    
    std::vector<InputOutputAccessInfo> accessInfoList;
    collectInputOutputAccessInfo(GV, constZero, accessInfoList, bNeedVertexID,
      bIsInput);

    bool bIsArrayTy = GV->getType()->getPointerElementType()->isArrayTy();
    bool isPrecise = m_preciseSigSet.count(SE);
    if (isPrecise)
      HLModule::MarkPreciseAttributeOnPtrWithFunctionCall(GV, M);

    for (InputOutputAccessInfo &info : accessInfoList) {
      GenerateInputOutputUserCall(info, undefVertexIdx, dxilFunc, OpArg, ID,
                                  cols, bI1Cast, columnConsts, bNeedVertexID,
                                  bIsArrayTy, bIsInput, bIsInout);
    }
  }
}

void DxilGenerationPass::GenerateDxilPatchConstantFunctionInputs() {
  // Map input patch, to input sig
  // LoadOutputControlPoint for output patch .
  OP *hlslOP = m_pHLModule->GetOP();
  Constant *constZero = hlslOP->GetU32Const(0);
  Function *EntryFunc = m_pHLModule->GetEntryFunction();
  DxilFunctionProps &EntryQual = m_pHLModule->GetDxilFunctionProps(EntryFunc);

  Function *patchConstantFunc = EntryQual.ShaderProps.HS.patchConstantFunc;
  DxilFunctionAnnotation *patchFuncAnnotation = m_pHLModule->GetFunctionAnnotation(patchConstantFunc);
  DXASSERT(patchFuncAnnotation, "must find annotation for patch constant function");
  Type *i1Ty = Type::getInt1Ty(constZero->getContext());
  Type *i32Ty = constZero->getType();

  for (Argument &arg : patchConstantFunc->args()) {
    DxilParameterAnnotation &paramAnnotation =
        patchFuncAnnotation->GetParameterAnnotation(arg.getArgNo());
    DxilParamInputQual inputQual = paramAnnotation.GetParamInputQual();
    if (inputQual == DxilParamInputQual::InputPatch ||
        inputQual == DxilParamInputQual::OutputPatch) {
      DxilSignatureElement *SE = m_patchConstantInputsSigMap[arg.getArgNo()];
      if (!SE)  // Error should have been reported at an earlier stage.
        continue;

      Constant *inputID = hlslOP->GetU32Const(SE->GetID());
      unsigned cols = SE->GetCols();
      Type *Ty = SE->GetCompType().GetLLVMType(m_pHLModule->GetCtx());
      // Cast i1 to i32 for load input.
      bool bI1Cast = false;
      if (Ty == i1Ty) {
        bI1Cast = true;
        Ty = i32Ty;
      }
      OP::OpCode opcode = inputQual == DxilParamInputQual::InputPatch?
          OP::OpCode::LoadInput : OP::OpCode::LoadOutputControlPoint;
      Function *dxilLdFunc = hlslOP->GetOpFunc(opcode, Ty);

      std::vector<InputOutputAccessInfo> accessInfoList;
      collectInputOutputAccessInfo(&arg, constZero, accessInfoList, /*hasVertexID*/ true, true);
      for (InputOutputAccessInfo &info : accessInfoList) {
        if (LoadInst *ldInst = dyn_cast<LoadInst>(info.user)) {
          Constant *OpArg = hlslOP->GetU32Const((unsigned)opcode);
          Value *args[] = {OpArg, inputID, info.idx, info.vectorIdx, info.vertexID};
          replaceLdWithLdInput(dxilLdFunc, ldInst, cols, args, bI1Cast);
        } else
          DXASSERT(0, "input should only be ld");
      }
    }
  }
}

bool DxilGenerationPass::HasClipPlanes() {
  Function *EntryFunc = m_pHLModule->GetEntryFunction();
  if (!m_pHLModule->HasDxilFunctionProps(EntryFunc))
    return false;


  DxilFunctionProps &EntryQual = m_pHLModule->GetDxilFunctionProps(EntryFunc);
  auto &VS = EntryQual.ShaderProps.VS;
  unsigned numClipPlanes = 0;

  for (unsigned i=0;i<DXIL::kNumClipPlanes;i++) {
    if (!VS.clipPlanes[i])
      break;
    numClipPlanes++;
  }

  return numClipPlanes != 0;
}

void DxilGenerationPass::GenerateClipPlanesForVS(Value *outPosition) {
  Function *EntryFunc = m_pHLModule->GetEntryFunction();

  DxilFunctionProps &EntryQual = m_pHLModule->GetDxilFunctionProps(EntryFunc);
  auto &VS = EntryQual.ShaderProps.VS;
  unsigned numClipPlanes = 0;

  for (unsigned i=0;i<DXIL::kNumClipPlanes;i++) {
    if (!VS.clipPlanes[i])
      break;
    numClipPlanes++;
  }

  if (!numClipPlanes)
    return;

  LLVMContext &Ctx = EntryFunc->getContext();

  Function *dp4 = m_pHLModule->GetOP()->GetOpFunc(DXIL::OpCode::Dot4, Type::getFloatTy(Ctx));
  Value *dp4Args[] = {
    ConstantInt::get(Type::getInt32Ty(Ctx), static_cast<unsigned>(DXIL::OpCode::Dot4)),
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr,
  };

  // out SV_Position should only have StoreInst use.
  // Done by LegalizeDxilInputOutputs in ScalarReplAggregatesHLSL.cpp
  for (User *U : outPosition->users()) {
    StoreInst *ST = cast<StoreInst>(U);
    Value *posVal = ST->getValueOperand();
    DXASSERT(posVal->getType()->isVectorTy(), "SV_Position must be a vector");
    IRBuilder<> Builder(ST);
    // Put position to args.
    for (unsigned i=0;i<4;i++)
      dp4Args[i+1] = Builder.CreateExtractElement(posVal, i);

    // For each clip plane.
    // clipDistance = dp4 position, clipPlane.
    auto argIt = EntryFunc->getArgumentList().rbegin();

    for (int clipIdx = numClipPlanes - 1; clipIdx >= 0; clipIdx--) {
      Constant *GV = VS.clipPlanes[clipIdx];
      DXASSERT_NOMSG(GV->hasOneUse());
      StoreInst *ST = cast<StoreInst>(GV->user_back());
      Value *clipPlane = ST->getValueOperand();
      ST->eraseFromParent();

      Argument &arg = *(argIt++);

      // Put clipPlane to args.
      for (unsigned i = 0; i < 4; i++)
        dp4Args[i + 5] = Builder.CreateExtractElement(clipPlane, i);

      Value *clipDistance = Builder.CreateCall(dp4, dp4Args);
      Builder.CreateStore(clipDistance, &arg);
    }
  }
}

static Value *MergeImmResClass(Value *resClass) {
  if (ConstantInt *Imm = dyn_cast<ConstantInt>(resClass)) {
    return resClass;
  } else {
    PHINode *phi = cast<PHINode>(resClass);
    Value *immResClass = MergeImmResClass(phi->getIncomingValue(0));
    unsigned numOperands = phi->getNumOperands();
    for (unsigned i=0;i<numOperands;i++)
      phi->setIncomingValue(i, immResClass);
    return immResClass;
  }
}

static const StringRef kResourceMapErrorMsg = "local resource not guaranteed to map to unique global resource.";

static Value *SelectOnOperand(Value *Cond, CallInst *CIT, CallInst *CIF,
                              unsigned idx, IRBuilder<> &Builder) {
  Value *OpT = CIT->getArgOperand(idx);
  Value *OpF = CIF->getArgOperand(idx);
  Value *OpSel = OpT;
  if (OpT != OpF) {
    OpSel = Builder.CreateSelect(Cond, OpT, OpF);
  }
  return OpSel;
}

static void ReplaceResourceUserWithHandle(LoadInst *Res, Value *handle) {
  for (auto resUser = Res->user_begin(); resUser != Res->user_end();) {
    CallInst *CI = dyn_cast<CallInst>(*(resUser++));
    DXASSERT(GetHLOpcodeGroupByName(CI->getCalledFunction()) ==
                 HLOpcodeGroup::HLCreateHandle,
             "must be createHandle");
    CI->replaceAllUsesWith(handle);
    CI->eraseFromParent();
  }
  Res->eraseFromParent();
}

static bool IsResourceType(Type *Ty) {
  bool isResource = HLModule::IsHLSLObjectType(Ty);

  if (ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
    Type *EltTy = AT->getElementType();
    while (isa<ArrayType>(EltTy)) {
      EltTy = EltTy->getArrayElementType();
    }
    isResource = HLModule::IsHLSLObjectType(EltTy);
    // TODO: support local resource array.
    DXASSERT(!isResource, "local resource array");
  }
  return isResource;
}

void DxilGenerationPass::RemoveLocalDxilResourceAllocas(Function *F) {
  BasicBlock &BB = F->getEntryBlock(); // Get the entry node for the function
  std::unordered_set<AllocaInst *> localResources;
  for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
    if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) { // Is it an alloca?
      if (IsResourceType(AI->getAllocatedType())) {
        localResources.insert(AI);
      }
    }

  SSAUpdater SSA;
  SmallVector<Instruction *, 4> Insts;

  for (AllocaInst *AI : localResources) {
    // Build list of instructions to promote.
    for (User *U : AI->users())
      Insts.emplace_back(cast<Instruction>(U));

    ResourceRemover(Insts, SSA).run(AI, Insts);

    Insts.clear();
  }
}

void DxilGenerationPass::TranslateParamDxilResourceHandles(Function *F, std::unordered_map<Instruction *, Value *> &handleMap) {
  Type *handleTy = m_pHLModule->GetOP()->GetHandleType();

  IRBuilder<> Builder(F->getEntryBlock().getFirstInsertionPt());
  for (Argument &arg : F->args()) {
    Type *Ty = arg.getType();

    if (isa<PointerType>(Ty))
      Ty = Ty->getPointerElementType();

    SmallVector<unsigned,4> arraySizeList;
    while (isa<ArrayType>(Ty)) {
      arraySizeList.push_back(Ty->getArrayNumElements());
      Ty = Ty->getArrayElementType();
    }
    DXIL::ResourceClass RC = m_pHLModule->GetResourceClass(Ty);
    if (RC != DXIL::ResourceClass::Invalid) {
      Type *curTy = handleTy;
      for (auto it = arraySizeList.rbegin(), E = arraySizeList.rend(); it != E;
           it++) {
        curTy = ArrayType::get(curTy, *it);
      }
      curTy = PointerType::get(curTy, 0);
      CallInst *castToHandle = cast<CallInst>(HLModule::EmitHLOperationCall(
          Builder, HLOpcodeGroup::HLCast, 0, curTy,
          {UndefValue::get(arg.getType())}, *F->getParent()));

      for (User *U : arg.users()) {
        Instruction *I = cast<Instruction>(U);
        IRBuilder<> userBuilder(I);
        if (LoadInst *ldInst = dyn_cast<LoadInst>(U)) {
          Value *handleLd = userBuilder.CreateLoad(castToHandle);
          handleMap[ldInst] = handleLd;
        } else if (StoreInst *stInst = dyn_cast<StoreInst>(U)) {
          Value *res = stInst->getValueOperand();
          Value *handle = HLModule::EmitHLOperationCall(
              userBuilder, HLOpcodeGroup::HLCast, 0, handleTy, {res},
              *F->getParent());
          userBuilder.CreateStore(handle, castToHandle);
        } else if (CallInst *CI = dyn_cast<CallInst>(U)) {
          // Don't flatten argument here.
          continue;
        } else {
          DXASSERT(
              dyn_cast<GEPOperator>(U) != nullptr,
              "else AddOpcodeParamForIntrinsic in CodeGen did not patch uses "
              "to only have ld/st refer to temp object");
          GEPOperator *GEP = cast<GEPOperator>(U);
          std::vector<Value *> idxList(GEP->idx_begin(), GEP->idx_end());
          Value *handleGEP = userBuilder.CreateGEP(castToHandle, idxList);
          for (auto GEPU : GEP->users()) {
            Instruction *GEPI = cast<Instruction>(GEPU);
            IRBuilder<> gepUserBuilder(GEPI);
            if (LoadInst *ldInst = dyn_cast<LoadInst>(GEPU)) {
              handleMap[ldInst] = gepUserBuilder.CreateLoad(handleGEP);
            } else {
              StoreInst *stInst = cast<StoreInst>(GEPU);
              Value *res = stInst->getValueOperand();
              Value *handle = HLModule::EmitHLOperationCall(
                  gepUserBuilder, HLOpcodeGroup::HLCast, 0, handleTy, {res},
                  *F->getParent());
              gepUserBuilder.CreateStore(handle, handleGEP);
            }
          }
        }
      }

      castToHandle->setArgOperand(0, &arg);
    }
  }
}

void DxilGenerationPass::GenerateParamDxilResourceHandles(
    std::unordered_map<Instruction *, Value *> &handleMap) {
  Module &M = *m_pHLModule->GetModule();
  for (Function &F : M.functions()) {
    if (!F.isDeclaration())
      TranslateParamDxilResourceHandles(&F, handleMap);
  }
}

void DxilGenerationPass::TranslateDxilResourceUses(
    DxilResourceBase &res, std::unordered_set<LoadInst *> &UpdateCounterSet,
    std::unordered_set<Value *> &NonUniformSet) {
  OP *hlslOP = m_pHLModule->GetOP();
  Function *createHandle = hlslOP->GetOpFunc(
      OP::OpCode::CreateHandle, llvm::Type::getVoidTy(m_pHLModule->GetCtx()));
  Value *opArg = hlslOP->GetU32Const((unsigned)OP::OpCode::CreateHandle);
  bool isViewResource = res.GetClass() == DXIL::ResourceClass::SRV || res.GetClass() == DXIL::ResourceClass::UAV;
  bool isROV = isViewResource && static_cast<DxilResource &>(res).IsROV();
  std::string handleName = (res.GetGlobalName() + Twine("_") + Twine(res.GetResClassName())).str();
  if (isViewResource)
    handleName += (Twine("_") + Twine(res.GetResDimName())).str();
  if (isROV)
    handleName += "_ROV";

  Value *resClassArg = hlslOP->GetU8Const(
      static_cast<std::underlying_type<DxilResourceBase::Class>::type>(
          res.GetClass()));
  Value *resIDArg = hlslOP->GetU32Const(res.GetID());
  // resLowerBound will be added after allocation in DxilCondenseResources.
  Value *resLowerBound = hlslOP->GetU32Const(0);
  // TODO: Set Non-uniform resource bit based on whether index comes from IOP_NonUniformResourceIndex.
  Value *isUniformRes = hlslOP->GetI1Const(0);

  Value *GV = res.GetGlobalSymbol();
  Module *pM = m_pHLModule->GetModule();
  // TODO: add debug info to create handle.
  DIVariable *DIV = nullptr;
  DILocation *DL = nullptr;
  if (m_HasDbgInfo) {
    DebugInfoFinder &Finder = m_pHLModule->GetOrCreateDebugInfoFinder();
    DIV =
        HLModule::FindGlobalVariableDebugInfo(cast<GlobalVariable>(GV), Finder);
    if (DIV)
      // TODO: how to get col?
      DL =
          DILocation::get(pM->getContext(), DIV->getLine(), 1, DIV->getScope());
  }

  bool isResArray = res.GetRangeSize() > 1;
  std::unordered_map<Function *, Instruction *> handleMapOnFunction;

  Value *createHandleArgs[] = {opArg, resClassArg, resIDArg, resLowerBound,
                               isUniformRes};

  for (iplist<Function>::iterator F : pM->getFunctionList()) {
    if (!F->isDeclaration()) {
      if (!isResArray) {
        IRBuilder<> Builder(F->getEntryBlock().getFirstInsertionPt());
        if (m_HasDbgInfo) {
          // TODO: set debug info.
          //Builder.SetCurrentDebugLocation(DL);
        }
        handleMapOnFunction[F] = Builder.CreateCall(createHandle, createHandleArgs, handleName);
      }
    }
  }

  for (auto U = GV->user_begin(), E = GV->user_end(); U != E; ) {
    User *user = *(U++);
    // Skip unused user.
    if (user->user_empty())
      continue;

    if (LoadInst *ldInst = dyn_cast<LoadInst>(user)) {
      if (UpdateCounterSet.count(ldInst)) {
        DxilResource *resource = llvm::dyn_cast<DxilResource>(&res);
        DXASSERT_NOMSG(resource);
        DXASSERT_NOMSG(resource->GetClass() == DXIL::ResourceClass::UAV);
        resource->SetHasCounter(true);
      }
      Function *userF = ldInst->getParent()->getParent();
      DXASSERT(handleMapOnFunction.count(userF), "must exist");
      Value *handle = handleMapOnFunction[userF];
      ReplaceResourceUserWithHandle(ldInst, handle);
    } else {
      DXASSERT(dyn_cast<GEPOperator>(user) != nullptr,
               "else AddOpcodeParamForIntrinsic in CodeGen did not patch uses "
               "to only have ld/st refer to temp object");
      GEPOperator *GEP = cast<GEPOperator>(user);
      Value *idx = nullptr;
      if (GEP->getNumIndices() == 2) {
        // one dim array of resource
        idx = (GEP->idx_begin() + 1)->get();
      } else {
        gep_type_iterator GEPIt = gep_type_begin(GEP), E = gep_type_end(GEP);
        // Must be instruction for multi dim array.
        std::unique_ptr<IRBuilder<> > Builder;
        if (GetElementPtrInst *GEPInst = dyn_cast<GetElementPtrInst>(GEP)) {
          Builder = std::make_unique<IRBuilder<> >(GEPInst);
        } else {
          Builder = std::make_unique<IRBuilder<> >(GV->getContext());
        }
        for (; GEPIt != E; ++GEPIt) {
          if (GEPIt->isArrayTy()) {
            unsigned arraySize = GEPIt->getArrayNumElements();
            Value * tmpIdx = GEPIt.getOperand();
            if (idx == nullptr)
              idx = tmpIdx;
            else {
              idx = Builder->CreateMul(idx, Builder->getInt32(arraySize));
              idx = Builder->CreateAdd(idx, tmpIdx);
            }
          }
        }
      }

      createHandleArgs[DXIL::OperandIndex::kCreateHandleResIndexOpIdx] = idx;
      if (!NonUniformSet.count(idx))
        createHandleArgs[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
            isUniformRes;
      else
        createHandleArgs[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
            hlslOP->GetI1Const(1);

      Value *handle = nullptr;
      if (GetElementPtrInst *GEPInst = dyn_cast<GetElementPtrInst>(GEP)) {
        IRBuilder<> Builder = IRBuilder<>(GEPInst);
        handle = Builder.CreateCall(createHandle, createHandleArgs, handleName);
      }

      for (auto GEPU = GEP->user_begin(), GEPE = GEP->user_end(); GEPU != GEPE; ) {
        // Must be load inst.
        LoadInst *ldInst = cast<LoadInst>(*(GEPU++));
        if (UpdateCounterSet.count(ldInst)) {
          DxilResource *resource = dyn_cast<DxilResource>(&res);
          DXASSERT_NOMSG(resource);
          DXASSERT_NOMSG(resource->GetClass() == DXIL::ResourceClass::UAV);
          resource->SetHasCounter(true);
        }
        if (handle) {
          ReplaceResourceUserWithHandle(ldInst, handle);
        }
        else {
          IRBuilder<> Builder = IRBuilder<>(ldInst);
          Value *localHandle = Builder.CreateCall(createHandle, createHandleArgs, handleName);
          ReplaceResourceUserWithHandle(ldInst, localHandle);
        }
      }
    }
  }
  // Erase unused handle.
  for (auto It : handleMapOnFunction) {
    Instruction *I = It.second;
    if (I->user_empty())
      I->eraseFromParent();
  }
}

void DxilGenerationPass::GenerateDxilResourceHandles(
    std::unordered_set<LoadInst *> &UpdateCounterSet,
    std::unordered_set<Value *> &NonUniformSet) {
  // Create sampler handle first, may be used by SRV operations.
  for (size_t i = 0; i < m_pHLModule->GetSamplers().size(); i++) {
    DxilSampler &S = m_pHLModule->GetSampler(i);
    TranslateDxilResourceUses(S, UpdateCounterSet, NonUniformSet);
  }

  for (size_t i = 0; i < m_pHLModule->GetSRVs().size(); i++) {
    HLResource &SRV = m_pHLModule->GetSRV(i);
    TranslateDxilResourceUses(SRV, UpdateCounterSet, NonUniformSet);
  }

  for (size_t i = 0; i < m_pHLModule->GetUAVs().size(); i++) {
    HLResource &UAV = m_pHLModule->GetUAV(i);
    TranslateDxilResourceUses(UAV, UpdateCounterSet, NonUniformSet);
  }
}

static void
AddResourceToSet(Instruction *Res, std::unordered_set<Instruction *> &resSet) {
  unsigned startOpIdx = 0;
  // Skip Cond for Select.
  if (isa<SelectInst>(Res))
    startOpIdx = 1;
  else if (!isa<PHINode>(Res))
    // Only check phi and select here.
    return;

  // Already add.
  if (resSet.count(Res))
    return;

  resSet.insert(Res);

  // Scan operand to add resource node which only used by phi/select.
  unsigned numOperands = Res->getNumOperands();
  for (unsigned i = startOpIdx; i < numOperands; i++) {
    Value *V = Res->getOperand(i);
    if (Instruction *I = dyn_cast<Instruction>(V)) {
      AddResourceToSet(I, resSet);
    }
  }
}

// Transform
//
//  %g_texture_texture_2d1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 0, i1 false)
//  %g_texture_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 2, i1 false)
//  %13 = select i1 %cmp, %dx.types.Handle %g_texture_texture_2d1, %dx.types.Handle %g_texture_texture_2d
// Into
//  %11 = select i1 %cmp, i32 0, i32 2
//  %12 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 %11, i1 false)
//

static bool MergeHandleOpWithSameValue(Instruction *HandleOp,
                                       unsigned startOpIdx,
                                       unsigned numOperands) {
  Value *op0 = nullptr;
  for (unsigned i = startOpIdx; i < numOperands; i++) {
    Value *op = HandleOp->getOperand(i);
    if (i == startOpIdx) {
      op0 = op;
    } else {
      if (op0 != op)
        op0 = nullptr;
    }
  }
  if (op0) {
    HandleOp->replaceAllUsesWith(op0);
    return true;
  }
  return false;
}

static void
UpdateHandleOperands(Instruction *Res,
                     std::unordered_map<Instruction *, CallInst *> &handleMap,
                     std::unordered_set<Instruction *> &nonUniformOps) {
  unsigned numOperands = Res->getNumOperands();

  unsigned startOpIdx = 0;
  // Skip Cond for Select.
  if (SelectInst *Sel = dyn_cast<SelectInst>(Res))
    startOpIdx = 1;

  CallInst *Handle = handleMap[Res];

  Instruction *resClass = cast<Instruction>(
      Handle->getArgOperand(DXIL::OperandIndex::kCreateHandleResClassOpIdx));
  Instruction *resID = cast<Instruction>(
      Handle->getArgOperand(DXIL::OperandIndex::kCreateHandleResIDOpIdx));
  Instruction *resAddr = cast<Instruction>(
      Handle->getArgOperand(DXIL::OperandIndex::kCreateHandleResIndexOpIdx));

  for (unsigned i = startOpIdx; i < numOperands; i++) {
    if (!isa<Instruction>(Res->getOperand(i))) {
      Res->getContext().emitError(Res, kResourceMapErrorMsg);
      continue;
    }
    Instruction *ResOp = cast<Instruction>(Res->getOperand(i));
    CallInst *HandleOp = dyn_cast<CallInst>(ResOp);

    if (!HandleOp) {
      if (handleMap.count(ResOp)) {
        Res->getContext().emitError(Res, kResourceMapErrorMsg);
        continue;
      }
      HandleOp = handleMap[ResOp];
    }

    Value *resClassOp =
        HandleOp->getArgOperand(DXIL::OperandIndex::kCreateHandleResClassOpIdx);
    Value *resIDOp =
        HandleOp->getArgOperand(DXIL::OperandIndex::kCreateHandleResIDOpIdx);
    Value *resAddrOp =
        HandleOp->getArgOperand(DXIL::OperandIndex::kCreateHandleResIndexOpIdx);

    resClass->setOperand(i, resClassOp);
    resID->setOperand(i, resIDOp);
    resAddr->setOperand(i, resAddrOp);
  }

  if (!MergeHandleOpWithSameValue(resClass, startOpIdx, numOperands))
    nonUniformOps.insert(resClass);
  if (!MergeHandleOpWithSameValue(resID, startOpIdx, numOperands))
    nonUniformOps.insert(resID);
  MergeHandleOpWithSameValue(resAddr, startOpIdx, numOperands);
}

void DxilGenerationPass::AddCreateHandleForPhiNodeAndSelect(OP *hlslOP) {
  Function *createHandle = hlslOP->GetOpFunc(
      OP::OpCode::CreateHandle, llvm::Type::getVoidTy(hlslOP->GetCtx()));

  std::unordered_set<PHINode *> objPhiList;
  std::unordered_set<SelectInst *> objSelectList;
  std::unordered_set<Instruction *> resSelectSet;
  for (User *U : createHandle->users()) {
    for (User *HandleU : U->users()) {
      Instruction *I = cast<Instruction>(HandleU);
      if (!isa<CallInst>(I))
        AddResourceToSet(I, resSelectSet);
    }
  }

  // Generate Handle inst for Res inst.
  FunctionType *FT = createHandle->getFunctionType();
  Value *opArg = hlslOP->GetU32Const((unsigned)OP::OpCode::CreateHandle);
  Type *resClassTy =
      FT->getParamType(DXIL::OperandIndex::kCreateHandleResClassOpIdx);
  Type *resIDTy = FT->getParamType(DXIL::OperandIndex::kCreateHandleResIDOpIdx);
  Type *resAddrTy =
      FT->getParamType(DXIL::OperandIndex::kCreateHandleResIndexOpIdx);
  Value *UndefResClass = UndefValue::get(resClassTy);
  Value *UndefResID = UndefValue::get(resIDTy);
  Value *UndefResAddr = UndefValue::get(resAddrTy);

  // phi/select node resource is not uniform
  Value *nonUniformRes = hlslOP->GetI1Const(1);
  std::unordered_map<Instruction *, CallInst *> handleMap;
  for (Instruction *Res : resSelectSet) {
    unsigned numOperands = Res->getNumOperands();
    IRBuilder<> Builder(Res);

    unsigned startOpIdx = 0;
    // Skip Cond for Select.
    if (SelectInst *Sel = dyn_cast<SelectInst>(Res)) {
      startOpIdx = 1;
      Value *Cond = Sel->getCondition();

      Value *resClassSel =
          Builder.CreateSelect(Cond, UndefResClass, UndefResClass);
      Value *resIDSel = Builder.CreateSelect(Cond, UndefResID, UndefResID);
      Value *resAddrSel =
          Builder.CreateSelect(Cond, UndefResAddr, UndefResAddr);

      CallInst *HandleSel =
          Builder.CreateCall(createHandle, {opArg, resClassSel, resIDSel,
                                            resAddrSel, nonUniformRes});
      handleMap[Res] = HandleSel;
      Res->replaceAllUsesWith(HandleSel);
    } else {
      PHINode *Phi = cast<PHINode>(Res); // res class must be same.
      PHINode *resClassPhi = Builder.CreatePHI(resClassTy, numOperands);
      PHINode *resIDPhi = Builder.CreatePHI(resIDTy, numOperands);
      PHINode *resAddrPhi = Builder.CreatePHI(resAddrTy, numOperands);
      for (unsigned i = 0; i < numOperands; i++) {
        BasicBlock *BB = Phi->getIncomingBlock(i);
        resClassPhi->addIncoming(UndefResClass, BB);
        resIDPhi->addIncoming(UndefResID, BB);
        resAddrPhi->addIncoming(UndefResAddr, BB);
      }
      IRBuilder<> HandleBuilder(Phi->getParent()->getFirstNonPHI());
      CallInst *HandlePhi =
          HandleBuilder.CreateCall(createHandle, {opArg, resClassPhi, resIDPhi,
                                                  resAddrPhi, nonUniformRes});
      handleMap[Res] = HandlePhi;
      Res->replaceAllUsesWith(HandlePhi);
    }
  }

  // Update operand for Handle phi/select.
  // If ResClass or ResID is phi/select, save to nonUniformOps.
  std::unordered_set<Instruction *> nonUniformOps;
  for (Instruction *Res : resSelectSet) {
    UpdateHandleOperands(Res, handleMap, nonUniformOps);
  }

  // ResClass and ResID must be uniform.
  // Try to merge res class, res id into imm.
  while (1) {
    bool bUpdated = false;

    for (auto It = nonUniformOps.begin(); It != nonUniformOps.end();) {
      Instruction *I = *(It++);
      unsigned numOperands = I->getNumOperands();

      unsigned startOpIdx = 0;
      // Skip Cond for Select.
      if (SelectInst *Sel = dyn_cast<SelectInst>(I))
        startOpIdx = 1;
      if (MergeHandleOpWithSameValue(I, startOpIdx, numOperands)) {
        nonUniformOps.erase(I);
        bUpdated = true;
      }
    }

    if (!bUpdated) {
      if (!nonUniformOps.empty()) {
        for (Instruction *I : nonUniformOps) {
          // Non uniform res class or res id.
          FT->getContext().emitError(I, kResourceMapErrorMsg);
        }
        return;
      }
      break;
    }
  }

  // Remove useless select/phi.
  for (Instruction *Res : resSelectSet) {
    Res->eraseFromParent();
  }
}

void DxilGenerationPass::GenerateDxilCBufferHandles(
    std::unordered_set<Value *> &NonUniformSet) {
  // For CBuffer, handle are mapped to HLCreateHandle.
  OP *hlslOP = m_pHLModule->GetOP();
  Function *createHandle = hlslOP->GetOpFunc(
      OP::OpCode::CreateHandle, llvm::Type::getVoidTy(m_pHLModule->GetCtx()));
  Value *opArg = hlslOP->GetU32Const((unsigned)OP::OpCode::CreateHandle);

  Value *resClassArg = hlslOP->GetU8Const(
      static_cast<std::underlying_type<DxilResourceBase::Class>::type>(
          DXIL::ResourceClass::CBuffer));


  for (size_t i = 0; i < m_pHLModule->GetCBuffers().size(); i++) {
    DxilCBuffer &CB = m_pHLModule->GetCBuffer(i);
    GlobalVariable *GV = cast<GlobalVariable>(CB.GetGlobalSymbol());
    // Remove GEP created in HLObjectOperationLowerHelper::UniformCbPtr.
    GV->removeDeadConstantUsers();
    std::string handleName = std::string(GV->getName()) + "_buffer";

    Value *args[] = {opArg, resClassArg, nullptr, nullptr,
                     hlslOP->GetI1Const(0)};
    DIVariable *DIV = nullptr;
    DILocation *DL = nullptr;
    if (m_HasDbgInfo) {
      DebugInfoFinder &Finder = m_pHLModule->GetOrCreateDebugInfoFinder();
      DIV = HLModule::FindGlobalVariableDebugInfo(GV, Finder);
      if (DIV)
        // TODO: how to get col?
        DL = DILocation::get(createHandle->getContext(), DIV->getLine(), 1,
                             DIV->getScope());
    }

    Value *resIDArg = hlslOP->GetU32Const(CB.GetID());
    args[DXIL::OperandIndex::kCreateHandleResIDOpIdx] = resIDArg;

    // resLowerBound will be added after allocation in DxilCondenseResources.
    Value *resLowerBound = hlslOP->GetU32Const(0);

    if (CB.GetRangeSize() == 1) {
      args[DXIL::OperandIndex::kCreateHandleResIndexOpIdx] = resLowerBound;
      for (auto U = GV->user_begin(); U != GV->user_end(); ) {
        // Must HLCreateHandle.
        CallInst *CI = cast<CallInst>(*(U++));
        // Put createHandle to entry block.
        auto InsertPt =
            CI->getParent()->getParent()->getEntryBlock().getFirstInsertionPt();
        IRBuilder<> Builder(InsertPt);

        CallInst *handle = Builder.CreateCall(createHandle, args, handleName);
        if (m_HasDbgInfo) {
          // TODO: add debug info.
          //handle->setDebugLoc(DL);
        }
        CI->replaceAllUsesWith(handle);
        CI->eraseFromParent();
      }
    } else {
      for (auto U = GV->user_begin(); U != GV->user_end(); ) {
        // Must HLCreateHandle.
        CallInst *CI = cast<CallInst>(*(U++));
        IRBuilder<> Builder(CI);
        Value *CBIndex = CI->getArgOperand(HLOperandIndex::kCreateHandleIndexOpIdx);
        args[DXIL::OperandIndex::kCreateHandleResIndexOpIdx] =
            CBIndex;
        if (isa<ConstantInt>(CBIndex)) {
          // Put createHandle to entry block for const index.
          auto InsertPt = CI->getParent()
                              ->getParent()
                              ->getEntryBlock()
                              .getFirstInsertionPt();
          Builder.SetInsertPoint(InsertPt);
        }
        if (!NonUniformSet.count(CBIndex))
          args[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
              hlslOP->GetI1Const(0);
        else
          args[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
              hlslOP->GetI1Const(1);

        CallInst *handle = Builder.CreateCall(createHandle, args, handleName);
        CI->replaceAllUsesWith(handle);
        CI->eraseFromParent();
      }
    }
  } 
}

static void GenerateStOutput(Function *stOutput, Value *eltVal, Value *outputID,
                             Value *rowIdx, Value *colIdx, OP *hlslOP,
                             IRBuilder<> Builder) {
  Constant *OpArg = hlslOP->GetU32Const((unsigned)OP::OpCode::StoreOutput);
  Builder.CreateCall(stOutput, {OpArg, outputID, rowIdx, colIdx, eltVal});
}

static Value *TranslateStreamAppend(CallInst *CI, unsigned ID, hlsl::OP *OP) {
  Function *DxilFunc = OP->GetOpFunc(OP::OpCode::EmitStream, CI->getType());
  // TODO: generate a emit which has the data being emited as its argment.
  // Value *data = CI->getArgOperand(HLOperandIndex::kStreamAppendDataOpIndex);
  Constant *opArg = OP->GetU32Const((unsigned)OP::OpCode::EmitStream);
  IRBuilder<> Builder(CI);

  Constant *streamID = OP->GetU8Const(ID);
  Value *args[] = {opArg, streamID};
  return Builder.CreateCall(DxilFunc, args);
}

static Value *TranslateStreamCut(CallInst *CI, unsigned ID, hlsl::OP *OP) {
  Function *DxilFunc = OP->GetOpFunc(OP::OpCode::CutStream, CI->getType());
  // TODO: generate a emit which has the data being emited as its argment.
  // Value *data = CI->getArgOperand(HLOperandIndex::kStreamAppendDataOpIndex);
  Constant *opArg = OP->GetU32Const((unsigned)OP::OpCode::CutStream);
  IRBuilder<> Builder(CI);

  Constant *streamID = OP->GetU8Const(ID);
  Value *args[] = {opArg, streamID};
  return Builder.CreateCall(DxilFunc, args);
}

// Generate DXIL stream output operation.
void DxilGenerationPass::GenerateStreamOutputOperation(Value *streamVal, unsigned ID) {
  OP * hlslOP = m_pHLModule->GetOP();

  for (auto U = streamVal->user_begin(); U != streamVal->user_end();) {
    Value *user = *(U++);
    // Should only used by append, restartStrip .
    CallInst *CI = cast<CallInst>(user);
    HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
    unsigned opcode = GetHLOpcode(CI);
    DXASSERT_LOCALVAR(group, group == HLOpcodeGroup::HLIntrinsic, "");
    IntrinsicOp IOP = static_cast<IntrinsicOp>(opcode);
    switch (IOP) {
    case IntrinsicOp::MOP_Append:
      TranslateStreamAppend(CI, ID, hlslOP);
    break;
    case IntrinsicOp::MOP_RestartStrip:
      TranslateStreamCut(CI, ID, hlslOP);
      break;
    default:
      DXASSERT(0, "invalid operation on stream");
    }
    CI->eraseFromParent();
  }
}
// Generate DXIL stream output operations.
void DxilGenerationPass::GenerateStreamOutputOperations() {
  Function *EntryFunc = m_pHLModule->GetEntryFunction();
  DxilFunctionAnnotation *EntryAnnotation = m_pHLModule->GetFunctionAnnotation(EntryFunc);
  DXASSERT(EntryAnnotation, "must find annotation for entry function");

  for (Argument &arg : EntryFunc->getArgumentList()) {
    if (HLModule::IsStreamOutputPtrType(arg.getType())) {
      unsigned streamID = 0;
      DxilParameterAnnotation &paramAnnotation = EntryAnnotation->GetParameterAnnotation(arg.getArgNo());
      DxilParamInputQual inputQual = paramAnnotation.GetParamInputQual();
      switch (inputQual) {
      case DxilParamInputQual::OutStream0:
        streamID = 0;
        break;
      case DxilParamInputQual::OutStream1:
        streamID = 1;
        break;
      case DxilParamInputQual::OutStream2:
        streamID = 2;
        break;
      case DxilParamInputQual::OutStream3:
      default:
        DXASSERT(inputQual == DxilParamInputQual::OutStream3, "invalid input qual.");
        streamID = 3;
        break;
      }
      GenerateStreamOutputOperation(&arg, streamID);
    }
  }
}

void DxilGenerationPass::GenerateDxilOperations(
    Module &M, std::unordered_set<LoadInst *> &UpdateCounterSet,
    std::unordered_set<Value *> &NonUniformSet) {
  // remove all functions except entry function
  Function *entry = m_pHLModule->GetEntryFunction();
  const ShaderModel *pSM = m_pHLModule->GetShaderModel();
  Function *patchConstantFunc = nullptr;
  if (pSM->IsHS()) {
    DxilFunctionProps &funcProps = m_pHLModule->GetDxilFunctionProps(entry);
    patchConstantFunc = funcProps.ShaderProps.HS.patchConstantFunc;
  }

  if (!pSM->IsLib()) {
    for (auto F = M.begin(); F != M.end();) {
      Function *func = F++;

      if (func->isDeclaration())
        continue;
      if (func == entry)
        continue;
      if (func == patchConstantFunc)
        continue;
      if (func->user_empty())
        func->eraseFromParent();
    }
  }

  TranslateBuiltinOperations(*m_pHLModule, m_extensionsCodegenHelper,
                             UpdateCounterSet, NonUniformSet);

  if (pSM->IsGS())
    GenerateStreamOutputOperations();

  // Remove unused HL Operation functions.
  std::vector<Function *> deadList;
  for (iplist<Function>::iterator F : M.getFunctionList()) {
    if (F->isDeclaration()) {
      hlsl::HLOpcodeGroup group = hlsl::GetHLOpcodeGroupByName(F);
      if (group != HLOpcodeGroup::NotHL || F->isIntrinsic())
        if (F->user_empty())
          deadList.emplace_back(F);
    }
  }

  for (Function *F : deadList)
    F->eraseFromParent();
}

static void TranslatePreciseAttributeOnFunction(Function &F, Module &M) {
  BasicBlock &BB = F.getEntryBlock(); // Get the entry node for the function

  // Find allocas that has precise attribute, by looking at all instructions in
  // the entry node
  for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E;) {
    Instruction *Inst = (I++);
    if (AllocaInst *AI = dyn_cast<AllocaInst>(Inst)) {
      if (HLModule::HasPreciseAttributeWithMetadata(AI)) {
        HLModule::MarkPreciseAttributeOnPtrWithFunctionCall(AI, M);
      }
    } else {
      DXASSERT(!HLModule::HasPreciseAttributeWithMetadata(Inst), "Only alloca can has precise metadata.");
    }
  }

  FastMathFlags FMF;
  FMF.setUnsafeAlgebra();
  // Set fast math for all FPMathOperators.
  // Already set FastMath in options. But that only enable things like fadd.
  // Every inst which type is float can be cast to FPMathOperator.
  for (Function::iterator BBI = F.begin(), BBE = F.end(); BBI != BBE; ++BBI) {
    BasicBlock *BB = BBI;
    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
      if (FPMathOperator *FPMath = dyn_cast<FPMathOperator>(I)) {
        I->copyFastMathFlags(FMF);
      }
    }
  }
}

void DxilGenerationPass::TranslatePreciseAttribute() {  
  bool bIEEEStrict = m_pHLModule->GetHLOptions().bIEEEStrict;
  // If IEEE strict, everying is precise, don't need to mark it.
  if (bIEEEStrict)
    return;

  Module &M = *m_pHLModule->GetModule();
  // TODO: If not inline every function, for function has call site with precise
  // argument and call site without precise argument, need to clone the function
  // to propagate the precise for the precise call site.
  // This should be done at CGMSHLSLRuntime::FinishCodeGen.
  Function *EntryFn = m_pHLModule->GetEntryFunction();
  if (!m_pHLModule->GetShaderModel()->IsLib()) {
    TranslatePreciseAttributeOnFunction(*EntryFn, M);
  }

  if (m_pHLModule->GetShaderModel()->IsHS()) {
    DxilFunctionProps &EntryQual = m_pHLModule->GetDxilFunctionProps(EntryFn);
    Function *patchConstantFunc = EntryQual.ShaderProps.HS.patchConstantFunc;
    TranslatePreciseAttributeOnFunction(*patchConstantFunc, M);
  }
}

char DxilGenerationPass::ID = 0;

ModulePass *llvm::createDxilGenerationPass(bool NotOptimized, hlsl::HLSLExtensionsCodegenHelper *extensionsHelper) {
  DxilGenerationPass *dxilPass = new DxilGenerationPass(NotOptimized);
  dxilPass->SetExtensionsHelper(extensionsHelper);
  return dxilPass;
}

INITIALIZE_PASS(DxilGenerationPass, "dxilgen", "HLSL DXIL Generation", false, false)

///////////////////////////////////////////////////////////////////////////////

namespace {

StructType *UpdateStructTypeForLegacyLayout(StructType *ST, bool IsCBuf,
                                            DxilTypeSystem &TypeSys, Module &M);

Type *UpdateFieldTypeForLegacyLayout(Type *Ty, bool IsCBuf, DxilFieldAnnotation &annotation,
                      DxilTypeSystem &TypeSys, Module &M) {
  DXASSERT(!Ty->isPointerTy(), "struct field should not be a pointer");

  if (Ty->isArrayTy()) {
    Type *EltTy = Ty->getArrayElementType();
    Type *UpdatedTy = UpdateFieldTypeForLegacyLayout(EltTy, IsCBuf, annotation, TypeSys, M);
    if (EltTy == UpdatedTy)
      return Ty;
    else
      return ArrayType::get(UpdatedTy, Ty->getArrayNumElements());
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    DXASSERT(annotation.HasMatrixAnnotation(), "must a matrix");
    unsigned rows, cols;
    Type *EltTy = HLMatrixLower::GetMatrixInfo(Ty, cols, rows);

    // Get cols and rows from annotation.
    const DxilMatrixAnnotation &matrix = annotation.GetMatrixAnnotation();
    if (matrix.Orientation == MatrixOrientation::RowMajor) {
      rows = matrix.Rows;
      cols = matrix.Cols;
    } else {
      DXASSERT(matrix.Orientation == MatrixOrientation::ColumnMajor, "");
      cols = matrix.Rows;
      rows = matrix.Cols;
    }
    // CBuffer matrix must 4 * 4 bytes align.
    if (IsCBuf)
      cols = 4;

    EltTy = UpdateFieldTypeForLegacyLayout(EltTy, IsCBuf, annotation, TypeSys, M);
    Type *rowTy = VectorType::get(EltTy, cols);
    return ArrayType::get(rowTy, rows);
  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    return UpdateStructTypeForLegacyLayout(ST, IsCBuf, TypeSys, M);
  } else if (Ty->isVectorTy()) {
    Type *EltTy = Ty->getVectorElementType();
    Type *UpdatedTy = UpdateFieldTypeForLegacyLayout(EltTy, IsCBuf, annotation, TypeSys, M);
    if (EltTy == UpdatedTy)
      return Ty;
    else
      return VectorType::get(UpdatedTy, Ty->getVectorNumElements());
  } else {
    Type *i32Ty = Type::getInt32Ty(Ty->getContext());
    // Basic types.
    if (Ty->isHalfTy()) {
      return Type::getFloatTy(Ty->getContext());
    } else if (IntegerType *ITy = dyn_cast<IntegerType>(Ty)) {
      if (ITy->getBitWidth() <= 32)
        return i32Ty;
      else
        return Ty;
    } else
      return Ty;
  }
}

StructType *UpdateStructTypeForLegacyLayout(StructType *ST, bool IsCBuf,
                                            DxilTypeSystem &TypeSys, Module &M) {
  bool bUpdated = false;
  unsigned fieldsCount = ST->getNumElements();
  std::vector<Type *> fieldTypes(fieldsCount);
  DxilStructAnnotation *SA = TypeSys.GetStructAnnotation(ST);
  DXASSERT(SA, "must have annotation for struct type");

  for (unsigned i = 0; i < fieldsCount; i++) {
    Type *EltTy = ST->getElementType(i);
    Type *UpdatedTy =
        UpdateFieldTypeForLegacyLayout(EltTy, IsCBuf, SA->GetFieldAnnotation(i), TypeSys, M);
    fieldTypes[i] = UpdatedTy;
    if (EltTy != UpdatedTy)
      bUpdated = true;
  }

  if (!bUpdated) {
    return ST;
  } else {
    std::string legacyName = "dx.alignment.legacy." + ST->getName().str();
    if (StructType *legacyST = M.getTypeByName(legacyName))
      return legacyST;

    StructType *NewST = StructType::create(ST->getContext(), fieldTypes, legacyName);
    DxilStructAnnotation *NewSA = TypeSys.AddStructAnnotation(NewST);
    // Clone annotation.
    *NewSA = *SA;
    return NewST;
  }
}

void UpdateStructTypeForLegacyLayout(DxilResourceBase &Res, DxilTypeSystem &TypeSys, Module &M) {
  GlobalVariable *GV = cast<GlobalVariable>(Res.GetGlobalSymbol());
  Type *Ty = GV->getType()->getPointerElementType();
  bool IsResourceArray = Res.GetRangeSize() != 1;
  if (IsResourceArray) {
    // Support Array of struct buffer.
    if (Ty->isArrayTy())
      Ty = Ty->getArrayElementType();
  }
  StructType *ST = cast<StructType>(Ty);
  if (ST->isOpaque()) {
    DXASSERT(Res.GetClass() == DxilResourceBase::Class::CBuffer,
             "Only cbuffer can have opaque struct.");
    return;
  }

  Type *UpdatedST = UpdateStructTypeForLegacyLayout(ST, IsResourceArray, TypeSys, M);
  if (ST != UpdatedST) {
    Type *Ty = GV->getType()->getPointerElementType();
    if (IsResourceArray) {
      // Support Array of struct buffer.
      if (Ty->isArrayTy()) {
        UpdatedST = ArrayType::get(UpdatedST, Ty->getArrayNumElements());
      }
    }
    GlobalVariable *NewGV = cast<GlobalVariable>(M.getOrInsertGlobal(GV->getName().str() + "_legacy", UpdatedST));
    Res.SetGlobalSymbol(NewGV);
    // Delete old GV.
    for (auto UserIt = GV->user_begin(); UserIt != GV->user_end(); ) {
      Value *User = *(UserIt++);
      if (Instruction *I = dyn_cast<Instruction>(User)) {
        if (!User->user_empty())
          I->replaceAllUsesWith(UndefValue::get(I->getType()));

        I->eraseFromParent();
      } else {
        ConstantExpr *CE = cast<ConstantExpr>(User);
        if (!CE->user_empty())
          CE->replaceAllUsesWith(UndefValue::get(CE->getType()));
      }
    }
    GV->removeDeadConstantUsers();
    GV->eraseFromParent();
  }
}

void UpdateStructTypeForLegacyLayoutOnHLM(HLModule &HLM) {
  DxilTypeSystem &TypeSys = HLM.GetTypeSystem();
  Module &M = *HLM.GetModule();
  for (auto &CBuf : HLM.GetCBuffers()) {
    UpdateStructTypeForLegacyLayout(*CBuf.get(), TypeSys, M);
  }

  for (auto &UAV : HLM.GetUAVs()) {
    if (UAV->GetKind() == DxilResourceBase::Kind::StructuredBuffer)
      UpdateStructTypeForLegacyLayout(*UAV.get(), TypeSys, M);
  }

  for (auto &SRV : HLM.GetSRVs()) {
    if (SRV->GetKind() == DxilResourceBase::Kind::StructuredBuffer)
      UpdateStructTypeForLegacyLayout(*SRV.get(), TypeSys, M);
  }
}

}

void DxilGenerationPass::UpdateStructTypeForLegacyLayout() {
  UpdateStructTypeForLegacyLayoutOnHLM(*m_pHLModule);
}

///////////////////////////////////////////////////////////////////////////////

namespace {
class HLEmitMetadata : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit HLEmitMetadata() : ModulePass(ID) {}

  const char *getPassName() const override { return "HLSL High-Level Metadata Emit"; }

  bool runOnModule(Module &M) override {
    if (M.HasHLModule()) {
      M.GetHLModule().EmitHLMetadata();
      return true;
    }

    return false;
  }
};
}

char HLEmitMetadata::ID = 0;

ModulePass *llvm::createHLEmitMetadataPass() {
  return new HLEmitMetadata();
}

INITIALIZE_PASS(HLEmitMetadata, "hlsl-hlemit", "HLSL High-Level Metadata Emit", false, false)

///////////////////////////////////////////////////////////////////////////////

namespace {
class HLEnsureMetadata : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit HLEnsureMetadata() : ModulePass(ID) {}

  const char *getPassName() const override { return "HLSL High-Level Metadata Ensure"; }

  bool runOnModule(Module &M) override {
    if (!M.HasHLModule()) {
      M.GetOrCreateHLModule();
      return true;
    }

    return false;
  }
};
}

char HLEnsureMetadata::ID = 0;

ModulePass *llvm::createHLEnsureMetadataPass() {
  return new HLEnsureMetadata();
}

INITIALIZE_PASS(HLEnsureMetadata, "hlsl-hlensure", "HLSL High-Level Metadata Ensure", false, false)

///////////////////////////////////////////////////////////////////////////////

namespace {
class DxilLoadMetadata : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilLoadMetadata () : ModulePass(ID) {}

  const char *getPassName() const override { return "HLSL load DxilModule from metadata"; }

  bool runOnModule(Module &M) override {
    if (!M.HasDxilModule()) {
      (void)M.GetOrCreateDxilModule();
      return true;
    }

    return false;
  }
};
}

char DxilLoadMetadata::ID = 0;

ModulePass *llvm::createDxilLoadMetadataPass() {
  return new DxilLoadMetadata();
}

INITIALIZE_PASS(DxilLoadMetadata, "hlsl-dxilload", "HLSL load DxilModule from metadata", false, false)


///////////////////////////////////////////////////////////////////////////////

namespace {

Function *StripFunctionParameter(Function *F, DxilModule &DM,
    DenseMap<const Function *, DISubprogram *> &FunctionDIs) {
  Module &M = *DM.GetModule();
  Type *VoidTy = Type::getVoidTy(M.getContext());
  FunctionType *FT = FunctionType::get(VoidTy, false);
  for (auto &arg : F->args()) {
    if (!arg.user_empty())
      return nullptr;
    DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(&arg);
    if (DDI) {
      DDI->eraseFromParent();
    }
  }

  Function *NewFunc = Function::Create(FT, F->getLinkage(), F->getName());
  M.getFunctionList().insert(F, NewFunc);
  // Splice the body of the old function right into the new function.
  NewFunc->getBasicBlockList().splice(NewFunc->begin(), F->getBasicBlockList());

  // Patch the pointer to LLVM function in debug info descriptor.
  auto DI = FunctionDIs.find(F);
  if (DI != FunctionDIs.end()) {
    DISubprogram *SP = DI->second;
    SP->replaceFunction(NewFunc);
    // Ensure the map is updated so it can be reused on subsequent argument
    // promotions of the same function.
    FunctionDIs.erase(DI);
    FunctionDIs[NewFunc] = SP;
  }
  NewFunc->takeName(F);
  DM.GetTypeSystem().EraseFunctionAnnotation(F);
  F->eraseFromParent();
  DM.GetTypeSystem().AddFunctionAnnotation(NewFunc);
  return NewFunc;
}

void CheckInBoundForTGSM(GlobalVariable &GV, const DataLayout &DL) {
  for (User *U : GV.users()) {
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(U)) {
      bool allImmIndex = true;
      for (auto Idx = GEP->idx_begin(), E = GEP->idx_end(); Idx != E; Idx++) {
        if (!isa<ConstantInt>(Idx)) {
          allImmIndex = false;
          break;
        }
      }
      if (!allImmIndex)
        GEP->setIsInBounds(false);
      else {
        Value *Ptr = GEP->getPointerOperand();
        unsigned size =
            DL.getTypeAllocSize(Ptr->getType()->getPointerElementType());
        unsigned valSize =
            DL.getTypeAllocSize(GEP->getType()->getPointerElementType());
        SmallVector<Value *, 8> Indices(GEP->idx_begin(), GEP->idx_end());
        unsigned offset =
            DL.getIndexedOffset(GEP->getPointerOperandType(), Indices);
        if ((offset + valSize) > size)
          GEP->setIsInBounds(false);
      }
    }
  }
}

class DxilEmitMetadata : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilEmitMetadata() : ModulePass(ID) {}

  const char *getPassName() const override { return "HLSL DXIL Metadata Emit"; }

  void patchValidation_1_0(Module &M) {
    for (iplist<Function>::iterator F : M.getFunctionList()) {
      for (Function::iterator BBI = F->begin(), BBE = F->end(); BBI != BBE;
           ++BBI) {
        BasicBlock *BB = BBI;
        for (BasicBlock::iterator II = BB->begin(), IE = BB->end(); II != IE;
             ++II) {
          Instruction *I = II;
          if (I->hasMetadataOtherThanDebugLoc()) {
            SmallVector<std::pair<unsigned, MDNode*>, 2> MDs;
            I->getAllMetadataOtherThanDebugLoc(MDs);
            for (auto &MD : MDs) {
              unsigned kind = MD.first;
              // Remove Metadata which validation_1_0 not allowed.
              bool bNeedPatch = kind == LLVMContext::MD_tbaa ||
                  kind == LLVMContext::MD_prof ||
                  (kind > LLVMContext::MD_fpmath &&
                  kind <= LLVMContext::MD_dereferenceable_or_null);
              if (bNeedPatch)
                I->setMetadata(kind, nullptr);
            }
          }
        }
      }
    }
  }

  bool runOnModule(Module &M) override {
    if (M.HasDxilModule()) {
      // Remove store undef output.
      hlsl::OP *hlslOP = M.GetDxilModule().GetOP();
      unsigned ValMajor = 0;
      unsigned ValMinor = 0;
      M.GetDxilModule().GetValidatorVersion(ValMajor, ValMinor);
      if (ValMajor == 1 && ValMinor <= 1) {
        patchValidation_1_0(M);
      }
      for (iplist<Function>::iterator F : M.getFunctionList()) {
        if (!hlslOP->IsDxilOpFunc(F))
          continue;

        // Check store output.
        FunctionType *FT = F->getFunctionType();
        // Num params not match.
        if (FT->getNumParams() !=
            (DXIL::OperandIndex::kStoreOutputValOpIdx + 1))
          continue;

        Type *overloadTy =
            FT->getParamType(DXIL::OperandIndex::kStoreOutputValOpIdx);
        // overload illegal.
        if (!hlslOP->IsOverloadLegal(DXIL::OpCode::StoreOutput, overloadTy))
          continue;
        Function *storeOutput =
            hlslOP->GetOpFunc(DXIL::OpCode::StoreOutput, overloadTy);
        // Not store output.
        if (storeOutput != F)
          continue;

        for (auto it = F->user_begin(); it != F->user_end();) {
          CallInst *CI = dyn_cast<CallInst>(*(it++));
          if (!CI)
            continue;

          Value *V =
              CI->getArgOperand(DXIL::OperandIndex::kStoreOutputValOpIdx);
          // Remove the store of undef.
          if (isa<UndefValue>(V))
            CI->eraseFromParent();
        }
      }
      // Remove unused external functions.
      std::vector<Function *> deadList;
      for (iplist<Function>::iterator F : M.getFunctionList()) {
        if (F->isDeclaration()) {
          if (F->user_empty())
            deadList.emplace_back(F);
        }
      }

      for (Function *F : deadList)
        F->eraseFromParent();

      // Remove unused internal global.
      std::vector<GlobalVariable *> staticGVs;
      for (GlobalVariable &GV : M.globals()) {
        if (HLModule::IsStaticGlobal(&GV) ||
            HLModule::IsSharedMemoryGlobal(&GV)) {
          staticGVs.emplace_back(&GV);
        }
      }

      for (GlobalVariable *GV : staticGVs) {
        bool onlyStoreUse = true;
        for (User *user : GV->users()) {
          if (isa<StoreInst>(user))
            continue;
          if (isa<ConstantExpr>(user) && user->user_empty())
            continue;
          onlyStoreUse = false;
          break;
        }
        if (onlyStoreUse) {
          for (auto UserIt = GV->user_begin(); UserIt != GV->user_end();) {
            Value *User = *(UserIt++);
            if (Instruction *I = dyn_cast<Instruction>(User)) {
              I->eraseFromParent();
            } else {
              ConstantExpr *CE = cast<ConstantExpr>(User);
              CE->dropAllReferences();
            }
          }
          GV->eraseFromParent();
        }
      }

      const DataLayout &DL = M.getDataLayout();
      // Clear inbound for GEP which has none-const index.
      for (GlobalVariable &GV : M.globals()) {
        if (HLModule::IsSharedMemoryGlobal(&GV)) {
          CheckInBoundForTGSM(GV, DL);
        }
      }

      DxilModule &DM = M.GetDxilModule();
      DenseMap<const Function *, DISubprogram *> FunctionDIs =
          makeSubprogramMap(M);
      if (Function *PatchConstantFunc = DM.GetPatchConstantFunction()) {
        PatchConstantFunc =
            StripFunctionParameter(PatchConstantFunc, DM, FunctionDIs);
        if (PatchConstantFunc)
          DM.SetPatchConstantFunction(PatchConstantFunc);
      }

      if (Function *EntryFunc = DM.GetEntryFunction()) {
        StringRef Name = DM.GetEntryFunctionName();
        EntryFunc->setName(Name);
        EntryFunc = StripFunctionParameter(EntryFunc, DM, FunctionDIs);
        if (EntryFunc)
          DM.SetEntryFunction(EntryFunc);
      }

      DM.CollectShaderFlags(); // Update flags to reflect any changes.
                               // Update Validator Version
      DM.UpgradeToMinValidatorVersion();
      DM.EmitDxilMetadata();
      return true;
    }

    return false;
  }
};
}

char DxilEmitMetadata::ID = 0;

ModulePass *llvm::createDxilEmitMetadataPass() {
  return new DxilEmitMetadata();
}

INITIALIZE_PASS(DxilEmitMetadata, "hlsl-dxilemit", "HLSL DXIL Metadata Emit", false, false)


///////////////////////////////////////////////////////////////////////////////
// Precise propagate.

namespace {
class DxilPrecisePropagatePass : public ModulePass {
  HLModule *m_pHLModule;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilPrecisePropagatePass() : ModulePass(ID), m_pHLModule(nullptr) {}

  const char *getPassName() const override { return "DXIL Precise Propagate"; }

  bool runOnModule(Module &M) override {
    DxilModule &dxilModule = M.GetOrCreateDxilModule();
    DxilTypeSystem &typeSys = dxilModule.GetTypeSystem();

    std::vector<Function*> deadList;
    for (Function &F : M.functions()) {
      if (HLModule::HasPreciseAttribute(&F)) {
        PropagatePreciseOnFunctionUser(F, typeSys);
        deadList.emplace_back(&F);
      }
    }
    for (Function *F : deadList)
      F->eraseFromParent();
    return true;
  }
private:
  void PropagatePreciseOnFunctionUser(Function &F, DxilTypeSystem &typeSys);
};

char DxilPrecisePropagatePass::ID = 0;

}

static void PropagatePreciseAttribute(Instruction *I, DxilTypeSystem &typeSys);

static void PropagatePreciseAttributeOnOperand(Value *V, DxilTypeSystem &typeSys, LLVMContext &Context) {
  Instruction *I = dyn_cast<Instruction>(V);
  // Skip none inst.
  if (!I)
    return;

  FPMathOperator *FPMath = dyn_cast<FPMathOperator>(I);
  // Skip none FPMath
  if (!FPMath)
    return;

  // Skip inst already marked.
  if (DxilModule::HasPreciseFastMathFlags(I))
    return;
  // TODO: skip precise on integer type, sample instruction...

  // Set precise fast math on those instructions that support it.
  if (DxilModule::PreservesFastMathFlags(I))
    DxilModule::SetPreciseFastMathFlags(I);

  // Fast math not work on call, use metadata.
  if (CallInst *CI = dyn_cast<CallInst>(I))
    HLModule::MarkPreciseAttributeWithMetadata(CI);
  PropagatePreciseAttribute(I, typeSys);
}

static void PropagatePreciseAttributeOnPointer(Value *Ptr, DxilTypeSystem &typeSys, LLVMContext &Context) {
  // Find all store and propagate on the val operand of store.
  // For CallInst, if Ptr is used as out parameter, mark it.
  for (User *U : Ptr->users()) {
    Instruction *user = cast<Instruction>(U);
    if (StoreInst *stInst = dyn_cast<StoreInst>(user)) {
      Value *val = stInst->getValueOperand();
      PropagatePreciseAttributeOnOperand(val, typeSys, Context);
    }
    else if (CallInst *CI = dyn_cast<CallInst>(user)) {
      bool bReadOnly = true;

      Function *F = CI->getCalledFunction();
      const DxilFunctionAnnotation *funcAnnotation = typeSys.GetFunctionAnnotation(F);
      for (unsigned i = 0; i < CI->getNumArgOperands(); ++i) {
        if (Ptr != CI->getArgOperand(i))
          continue;

        const DxilParameterAnnotation &paramAnnotation =
            funcAnnotation->GetParameterAnnotation(i);
        // OutputPatch and OutputStream will be checked after scalar repl.
        // Here only check out/inout
        if (paramAnnotation.GetParamInputQual() == DxilParamInputQual::Out ||
            paramAnnotation.GetParamInputQual() == DxilParamInputQual::Inout) {
          bReadOnly = false;
          break;
        }

      }

      if (!bReadOnly)
        PropagatePreciseAttributeOnOperand(CI, typeSys, Context);
    }
  }
}

static void PropagatePreciseAttribute(Instruction *I, DxilTypeSystem &typeSys) {
  LLVMContext &Context = I->getContext();
  if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
    PropagatePreciseAttributeOnPointer(AI, typeSys, Context);
  } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
    // Propagate every argument.
    // TODO: only propagate precise argument.
    for (Value *src : I->operands())
      PropagatePreciseAttributeOnOperand(src, typeSys, Context);
  } else if (FPMathOperator *FPMath = dyn_cast<FPMathOperator>(I)) {
    // TODO: only propagate precise argument.
    for (Value *src : I->operands())
      PropagatePreciseAttributeOnOperand(src, typeSys, Context);
  }
  else if (LoadInst *ldInst = dyn_cast<LoadInst>(I)) {
    Value *Ptr = ldInst->getPointerOperand();
    PropagatePreciseAttributeOnPointer(Ptr, typeSys, Context);
  } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(I))
    PropagatePreciseAttributeOnPointer(GEP, typeSys, Context);
  // TODO: support more case which need 
}

void DxilPrecisePropagatePass::PropagatePreciseOnFunctionUser(Function &F, DxilTypeSystem &typeSys) {
  LLVMContext &Context = F.getContext();
  for (auto U=F.user_begin(), E=F.user_end();U!=E;) {
    CallInst *CI = cast<CallInst>(*(U++));
    Value *V = CI->getArgOperand(0);
    PropagatePreciseAttributeOnOperand(V, typeSys, Context);
    CI->eraseFromParent();
  }
}

ModulePass *llvm::createDxilPrecisePropagatePass() {
  return new DxilPrecisePropagatePass();
}

INITIALIZE_PASS(DxilPrecisePropagatePass, "hlsl-dxil-precise", "DXIL precise attribute propagate", false, false)

///////////////////////////////////////////////////////////////////////////////
// Legalize resource use.
// Map local or static global resource to global resource.
// Require inline for static global resource.

namespace {

class DxilLegalizeStaticResourceUsePass : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilLegalizeStaticResourceUsePass()
      : ModulePass(ID) {}

  const char *getPassName() const override {
    return "DXIL Legalize Static Resource Use";
  }

  bool runOnModule(Module &M) override {
    HLModule &HLM = M.GetOrCreateHLModule();
    OP *hlslOP = HLM.GetOP();
    Type *HandleTy = hlslOP->GetHandleType();
    // Promote static global variables.
    PromoteStaticGlobalResources(M);

    // Lower handle cast.
    for (Function &F : M.functions()) {
      if (!F.isDeclaration())
        continue;
      HLOpcodeGroup group = hlsl::GetHLOpcodeGroupByName(&F);
      if (group != HLOpcodeGroup::HLCast)
        continue;
      Type *Ty = F.getFunctionType()->getReturnType();
      if (Ty->isPointerTy())
        Ty = Ty->getPointerElementType();
      if (HLModule::IsHLSLObjectType(Ty)) {
        TransformHandleCast(F);
      }
    }

    Value *UndefHandle = UndefValue::get(HandleTy);
    if (!UndefHandle->user_empty()) {
      for (User *U : UndefHandle->users()) {
        // Report error if undef handle used for function call.
        if (isa<CallInst>(U)) {
          M.getContext().emitError(kResourceMapErrorMsg);
        }
      }
    }
    return true;
  }

private:
  void PromoteStaticGlobalResources(Module &M);
  void TransformHandleCast(Function &F);
};

char DxilLegalizeStaticResourceUsePass::ID = 0;

class DxilLegalizeResourceUsePass : public FunctionPass {
  HLModule *m_pHLModule;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilLegalizeResourceUsePass()
      : FunctionPass(ID), m_pHLModule(nullptr) {}

  const char *getPassName() const override {
    return "DXIL Legalize Resource Use";
  }

  bool runOnFunction(Function &F) override {
    // Promote local resource first.
    PromoteLocalResource(F);
    return true;
  }

private:
  void PromoteLocalResource(Function &F);
};

char DxilLegalizeResourceUsePass::ID = 0;

}

void DxilLegalizeResourceUsePass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AssumptionCacheTracker>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.setPreservesAll();
}

void DxilLegalizeResourceUsePass::PromoteLocalResource(Function &F) {
  std::vector<AllocaInst *> Allocas;
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  AssumptionCache &AC =
      getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
  HLModule &HLM = F.getParent()->GetOrCreateHLModule();
  OP *hlslOP = HLM.GetOP();
  Type *HandleTy = hlslOP->GetHandleType();

  BasicBlock &BB = F.getEntryBlock();
  unsigned allocaSize = 0;
  while (1) {
    Allocas.clear();

    // Find allocas that are safe to promote, by looking at all instructions in
    // the entry node
    for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
      if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) { // Is it an alloca?
        if (HandleTy == HLModule::GetArrayEltTy(AI->getAllocatedType()))
          Allocas.push_back(AI);
      }
    if (Allocas.empty())
      break;

    // No update.
    // Report error and break.
    if (allocaSize == Allocas.size()) {
      F.getContext().emitError(kResourceMapErrorMsg);
      break;
    }
    allocaSize = Allocas.size();

    PromoteMemToReg(Allocas, *DT, nullptr, &AC);
  }

  return;
}

FunctionPass *llvm::createDxilLegalizeResourceUsePass() {
  return new DxilLegalizeResourceUsePass();
}

INITIALIZE_PASS_BEGIN(DxilLegalizeResourceUsePass,
                      "hlsl-dxil-legalize-resource-use",
                      "DXIL legalize resource use", false, true)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(DxilLegalizeResourceUsePass,
                    "hlsl-dxil-legalize-resource-use",
                    "DXIL legalize resource use", false, true)

void DxilLegalizeStaticResourceUsePass::PromoteStaticGlobalResources(
    Module &M) {
  HLModule &HLM = M.GetOrCreateHLModule();
  Type *HandleTy = HLM.GetOP()->GetHandleType();

  std::set<GlobalVariable *> staticResources;
  for (auto &GV : M.globals()) {
    if (GV.getLinkage() == GlobalValue::LinkageTypes::InternalLinkage &&
        HandleTy == HLModule::GetArrayEltTy(GV.getType())) {
      staticResources.insert(&GV);
    }
  }
  SSAUpdater SSA;
  SmallVector<Instruction *, 4> Insts;
  // Make sure every resource load has mapped to global variable.
  while (!staticResources.empty()) {
    bool bUpdated = false;
    for (auto it = staticResources.begin(); it != staticResources.end();) {
      GlobalVariable *GV = *(it++);
      // Build list of instructions to promote.
      for (User *U : GV->users()) {
        Instruction *I = cast<Instruction>(U);
        Insts.emplace_back(I);
      }

      LoadAndStorePromoter(Insts, SSA).run(Insts);
      if (GV->user_empty()) {
        bUpdated = true;
        staticResources.erase(GV);
      }

      Insts.clear();
    }
    if (!bUpdated) {
      M.getContext().emitError(kResourceMapErrorMsg);
      break;
    }
  }
}

static void ReplaceResUseWithHandle(Instruction *Res, Value *Handle) {
  Type *HandleTy = Handle->getType();
  for (auto ResU = Res->user_begin(); ResU != Res->user_end();) {
    Instruction *I = cast<Instruction>(*(ResU++));
    if (isa<LoadInst>(I)) {
      ReplaceResUseWithHandle(I, Handle);
    } else if (isa<CallInst>(I)) {
      if (I->getType() == HandleTy)
        I->replaceAllUsesWith(Handle);
      else
        DXASSERT(0, "must createHandle here");
    } else {
      DXASSERT(0, "should only used by load and createHandle");
    }
    if (I->user_empty()) {
      I->eraseFromParent();
    }
  }
}

void DxilLegalizeStaticResourceUsePass::TransformHandleCast(Function &F) {
  for (auto U = F.user_begin(); U != F.user_end(); ) {
    CallInst *CI = cast<CallInst>(*(U++));
    Value *Handle = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
    ReplaceResUseWithHandle(CI, Handle);
    if (CI->user_empty())
      CI->eraseFromParent();
  }
}

ModulePass *llvm::createDxilLegalizeStaticResourceUsePass() {
  return new DxilLegalizeStaticResourceUsePass();
}

INITIALIZE_PASS(DxilLegalizeStaticResourceUsePass,
                "hlsl-dxil-legalize-static-resource-use",
                "DXIL legalize static resource use", false, false)

///////////////////////////////////////////////////////////////////////////////
// Legalize EvalOperations.
// Make sure src of EvalOperations are from function parameter.
// This is needed in order to translate EvaluateAttribute operations that traces
// back to LoadInput operations during translation stage. Promoting load/store
// instructions beforehand will allow us to easily trace back to loadInput from
// function call.
namespace {

class DxilLegalizeEvalOperations : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilLegalizeEvalOperations() : ModulePass(ID) {}

  const char *getPassName() const override {
    return "DXIL Legalize EvalOperations";
  }

  bool runOnModule(Module &M) override {
    for (Function &F : M.getFunctionList()) {
      hlsl::HLOpcodeGroup group = hlsl::GetHLOpcodeGroup(&F);
      if (group != HLOpcodeGroup::NotHL) {
        std::vector<CallInst *> EvalFunctionCalls;
        // Find all EvaluateAttribute calls
        for (User *U : F.users()) {
          if (CallInst *CI = dyn_cast<CallInst>(U)) {
            IntrinsicOp evalOp =
                static_cast<IntrinsicOp>(hlsl::GetHLOpcode(CI));
            if (evalOp == IntrinsicOp::IOP_EvaluateAttributeAtSample ||
                evalOp == IntrinsicOp::IOP_EvaluateAttributeCentroid ||
                evalOp == IntrinsicOp::IOP_EvaluateAttributeSnapped) {
              EvalFunctionCalls.push_back(CI);
            }
          }
        }
        if (EvalFunctionCalls.empty()) {
          continue;
        }
        // Start from the call instruction, find all allocas that this call
        // uses.
        std::unordered_set<AllocaInst *> allocas;
        for (CallInst *CI : EvalFunctionCalls) {
          FindAllocasForEvalOperations(CI, allocas);
        }
        SSAUpdater SSA;
        SmallVector<Instruction *, 4> Insts;
        for (AllocaInst *AI : allocas) {
          for (User *user : AI->users()) {
            if (isa<LoadInst>(user) || isa<StoreInst>(user)) {
              Insts.emplace_back(cast<Instruction>(user));
            }
          }
          LoadAndStorePromoter(Insts, SSA).run(Insts);
          Insts.clear();
        }
      }
    }
    return true;
  }

private:
  void FindAllocasForEvalOperations(Value *val,
                                    std::unordered_set<AllocaInst *> &allocas);
};

char DxilLegalizeEvalOperations::ID = 0;

// Find allocas for EvaluateAttribute operations
void DxilLegalizeEvalOperations::FindAllocasForEvalOperations(
    Value *val, std::unordered_set<AllocaInst *> &allocas) {
  Value *CurVal = val;
  while (!isa<AllocaInst>(CurVal)) {
    if (CallInst *CI = dyn_cast<CallInst>(CurVal)) {
      CurVal = CI->getOperand(HLOperandIndex::kUnaryOpSrc0Idx);
    } else if (InsertElementInst *IE = dyn_cast<InsertElementInst>(CurVal)) {
      Value *arg0 =
          IE->getOperand(0); // Could be another insertelement or undef
      Value *arg1 = IE->getOperand(1);
      FindAllocasForEvalOperations(arg0, allocas);
      CurVal = arg1;
    } else if (ShuffleVectorInst *SV = dyn_cast<ShuffleVectorInst>(CurVal)) {
      Value *arg0 = SV->getOperand(0);
      Value *arg1 = SV->getOperand(1);
      FindAllocasForEvalOperations(
          arg0, allocas); // Shuffle vector could come from different allocas
      CurVal = arg1;
    } else if (ExtractElementInst *EE = dyn_cast<ExtractElementInst>(CurVal)) {
      CurVal = EE->getOperand(0);
    } else if (LoadInst *LI = dyn_cast<LoadInst>(CurVal)) {
      CurVal = LI->getOperand(0);
    } else {
      break;
    }
  }
  if (AllocaInst *AI = dyn_cast<AllocaInst>(CurVal)) {
    allocas.insert(AI);
  }
}
} // namespace

ModulePass *llvm::createDxilLegalizeEvalOperationsPass() {
  return new DxilLegalizeEvalOperations();
}

INITIALIZE_PASS(DxilLegalizeEvalOperations,
                "hlsl-dxil-legalize-eval-operations",
                "DXIL legalize eval operations", false, false)
