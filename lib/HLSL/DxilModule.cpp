///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilModule.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/HLSL/DxilRootSignature.h"
#include "dxc/HLSL/DxilFunctionProps.h"
#include "DxilEntryProps.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_set>

using namespace llvm;
using std::string;
using std::vector;
using std::unique_ptr;


namespace {
class DxilErrorDiagnosticInfo : public DiagnosticInfo {
private:
  const char *m_message;
public:
  DxilErrorDiagnosticInfo(const char *str)
    : DiagnosticInfo(DK_FirstPluginKind, DiagnosticSeverity::DS_Error),
    m_message(str) { }

  void print(DiagnosticPrinter &DP) const override {
    DP << m_message;
  }
};
} // anon namespace

namespace hlsl {

namespace DXIL {
// Define constant variables exposed in DxilConstants.h
// TODO: revisit data layout descriptions for the following:
//      - x64 pointers?
//      - Keep elf manging(m:e)?

// For legacy data layout, everything less than 32 align to 32.
const char* kLegacyLayoutString = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f:64:64-n8:16:32:64";

// New data layout with native low precision types
const char* kNewLayoutString = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64";

// Function Attributes
// TODO: consider generating attributes from hctdb
const char* kFP32DenormKindString          = "fp32-denorm-mode";
const char* kFP32DenormValueAnyString      = "any";
const char* kFP32DenormValuePreserveString = "preserve";
const char* kFP32DenormValueFtzString      = "ftz";
}

//------------------------------------------------------------------------------
//
//  DxilModule methods.
//
DxilModule::DxilModule(Module *pModule)
: m_Ctx(pModule->getContext())
, m_pModule(pModule)
, m_pOP(llvm::make_unique<OP>(pModule->getContext(), pModule))
, m_pTypeSystem(llvm::make_unique<DxilTypeSystem>(pModule))
, m_pViewIdState(llvm::make_unique<DxilViewIdState>(this))
, m_pMDHelper(llvm::make_unique<DxilMDHelper>(pModule, llvm::make_unique<DxilExtraPropertyHelper>(pModule)))
, m_pDebugInfoFinder(nullptr)
, m_pEntryFunc(nullptr)
, m_EntryName("")
, m_pPatchConstantFunc(nullptr)
, m_pSM(nullptr)
, m_DxilMajor(DXIL::kDxilMajor)
, m_DxilMinor(DXIL::kDxilMinor)
, m_ValMajor(1)
, m_ValMinor(0)
, m_InputPrimitive(DXIL::InputPrimitive::Undefined)
, m_MaxVertexCount(0)
, m_StreamPrimitiveTopology(DXIL::PrimitiveTopology::Undefined)
, m_ActiveStreamMask(0)
, m_NumGSInstances(1)
, m_InputControlPointCount(0)
, m_TessellatorDomain(DXIL::TessellatorDomain::Undefined)
, m_OutputControlPointCount(0)
, m_TessellatorPartitioning(DXIL::TessellatorPartitioning::Undefined)
, m_TessellatorOutputPrimitive(DXIL::TessellatorOutputPrimitive::Undefined)
, m_MaxTessellationFactor(0.f)
, m_RootSignature(nullptr)
, m_bUseMinPrecision(true) // use min precision by default
, m_bDisableOptimizations(false)
, m_bAllResourcesBound(false)
, m_AutoBindingSpace(UINT_MAX) {
  DXASSERT_NOMSG(m_pModule != nullptr);

  m_NumThreads[0] = m_NumThreads[1] = m_NumThreads[2] = 0;

#if defined(_DEBUG) || defined(DBG)
  // Pin LLVM dump methods.
  void (__thiscall Module::*pfnModuleDump)() const = &Module::dump;
  void (__thiscall Type::*pfnTypeDump)() const = &Type::dump;
  void (__thiscall Function::*pfnViewCFGOnly)() const = &Function::viewCFGOnly;
  m_pUnused = (char *)&pfnModuleDump - (char *)&pfnTypeDump;
  m_pUnused -= (size_t)&pfnViewCFGOnly;
#endif
}

DxilModule::~DxilModule() {
}

LLVMContext &DxilModule::GetCtx() const { return m_Ctx; }
Module *DxilModule::GetModule() const { return m_pModule; }
OP *DxilModule::GetOP() const { return m_pOP.get(); }

void DxilModule::SetShaderModel(const ShaderModel *pSM) {
  DXASSERT(m_pSM == nullptr || (pSM != nullptr && *m_pSM == *pSM), "shader model must not change for the module");
  DXASSERT(pSM != nullptr && pSM->IsValidForDxil(), "shader model must be valid");
  DXASSERT(pSM->IsValidForModule(), "shader model must be valid for top-level module use");
  m_pSM = pSM;
  m_pSM->GetDxilVersion(m_DxilMajor, m_DxilMinor);
  m_pMDHelper->SetShaderModel(m_pSM);
  m_RootSignature.reset(new RootSignatureHandle());
}

const ShaderModel *DxilModule::GetShaderModel() const {
  return m_pSM;
}

void DxilModule::GetDxilVersion(unsigned &DxilMajor, unsigned &DxilMinor) const {
  DxilMajor = m_DxilMajor;
  DxilMinor = m_DxilMinor;
}

void DxilModule::SetValidatorVersion(unsigned ValMajor, unsigned ValMinor) {
  m_ValMajor = ValMajor;
  m_ValMinor = ValMinor;
}

bool DxilModule::UpgradeValidatorVersion(unsigned ValMajor, unsigned ValMinor) {
  // Don't upgrade if validation was disabled.
  if (m_ValMajor == 0 && m_ValMinor == 0) {
    return false;
  }
  if (ValMajor > m_ValMajor || (ValMajor == m_ValMajor && ValMinor > m_ValMinor)) {
    // Module requires higher validator version than previously set
    SetValidatorVersion(ValMajor, ValMinor);
    return true;
  }
  return false;
}

void DxilModule::GetValidatorVersion(unsigned &ValMajor, unsigned &ValMinor) const {
  ValMajor = m_ValMajor;
  ValMinor = m_ValMinor;
}

bool DxilModule::GetMinValidatorVersion(unsigned &ValMajor, unsigned &ValMinor) const {
  if (!m_pSM)
    return false;
  m_pSM->GetMinValidatorVersion(ValMajor, ValMinor);
  if (ValMajor == 1 && ValMinor == 0 && (m_ShaderFlags.GetFeatureInfo() & hlsl::ShaderFeatureInfo_ViewID))
    ValMinor = 1;
  return true;
}

bool DxilModule::UpgradeToMinValidatorVersion() {
  unsigned ValMajor = 1, ValMinor = 0;
  if (GetMinValidatorVersion(ValMajor, ValMinor)) {
    return UpgradeValidatorVersion(ValMajor, ValMinor);
  }
  return false;
}

Function *DxilModule::GetEntryFunction() {
  return m_pEntryFunc;
}

const Function *DxilModule::GetEntryFunction() const {
  return m_pEntryFunc;
}

void DxilModule::SetEntryFunction(Function *pEntryFunc) {
  m_pEntryFunc = pEntryFunc;
}

const string &DxilModule::GetEntryFunctionName() const {
  return m_EntryName;
}

void DxilModule::SetEntryFunctionName(const string &name) {
  m_EntryName = name;
}

llvm::Function *DxilModule::GetPatchConstantFunction() {
  return m_pPatchConstantFunc;
}

const llvm::Function *DxilModule::GetPatchConstantFunction() const {
  return m_pPatchConstantFunc;
}

void DxilModule::SetPatchConstantFunction(llvm::Function *pFunc) {
  m_pPatchConstantFunc = pFunc;
}

unsigned DxilModule::GetGlobalFlags() const {
  unsigned Flags = m_ShaderFlags.GetGlobalFlags();
  return Flags;
}

void DxilModule::CollectShaderFlagsForModule(ShaderFlags &Flags) {
  for (Function &F : GetModule()->functions()) {
    ShaderFlags funcFlags = ShaderFlags::CollectShaderFlags(&F, this);
    Flags.CombineShaderFlags(funcFlags);
  };

  const ShaderModel *SM = GetShaderModel();
  if (SM->IsPS()) {
    bool hasStencilRef = false;
    DxilSignature &outS = GetOutputSignature();
    for (auto &&E : outS.GetElements()) {
      if (E->GetKind() == Semantic::Kind::StencilRef) {
        hasStencilRef = true;
      } else if (E->GetKind() == Semantic::Kind::InnerCoverage) {
        Flags.SetInnerCoverage(true);
      }
    }

    Flags.SetStencilRef(hasStencilRef);
  }

  bool checkInputRTArrayIndex =
      SM->IsGS() || SM->IsDS() || SM->IsHS() || SM->IsPS();
  if (checkInputRTArrayIndex) {
    bool hasViewportArrayIndex = false;
    bool hasRenderTargetArrayIndex = false;
    DxilSignature &inS = GetInputSignature();
    for (auto &E : inS.GetElements()) {
      if (E->GetKind() == Semantic::Kind::ViewPortArrayIndex) {
        hasViewportArrayIndex = true;
      } else if (E->GetKind() == Semantic::Kind::RenderTargetArrayIndex) {
        hasRenderTargetArrayIndex = true;
      }
    }
    Flags.SetViewportAndRTArrayIndex(hasViewportArrayIndex |
                                     hasRenderTargetArrayIndex);
  }

  bool checkOutputRTArrayIndex =
      SM->IsVS() || SM->IsDS() || SM->IsHS() || SM->IsPS();
  if (checkOutputRTArrayIndex) {
    bool hasViewportArrayIndex = false;
    bool hasRenderTargetArrayIndex = false;
    DxilSignature &outS = GetOutputSignature();
    for (auto &E : outS.GetElements()) {
      if (E->GetKind() == Semantic::Kind::ViewPortArrayIndex) {
        hasViewportArrayIndex = true;
      } else if (E->GetKind() == Semantic::Kind::RenderTargetArrayIndex) {
        hasRenderTargetArrayIndex = true;
      }
    }
    Flags.SetViewportAndRTArrayIndex(hasViewportArrayIndex |
                                     hasRenderTargetArrayIndex);
  }

  unsigned NumUAVs = m_UAVs.size();
  const unsigned kSmallUAVCount = 8;
  if (NumUAVs > kSmallUAVCount)
    Flags.Set64UAVs(true);
  if (NumUAVs && !(SM->IsCS() || SM->IsPS()))
    Flags.SetUAVsAtEveryStage(true);

  bool hasRawAndStructuredBuffer = false;

  for (auto &UAV : m_UAVs) {
    if (UAV->IsROV())
      Flags.SetROVs(true);
    switch (UAV->GetKind()) {
    case DXIL::ResourceKind::RawBuffer:
    case DXIL::ResourceKind::StructuredBuffer:
      hasRawAndStructuredBuffer = true;
      break;
    default:
      // Not raw/structured.
      break;
    }
  }
  for (auto &SRV : m_SRVs) {
    switch (SRV->GetKind()) {
    case DXIL::ResourceKind::RawBuffer:
    case DXIL::ResourceKind::StructuredBuffer:
      hasRawAndStructuredBuffer = true;
      break;
    default:
      // Not raw/structured.
      break;
    }
  }
  
  Flags.SetEnableRawAndStructuredBuffers(hasRawAndStructuredBuffer);

  bool hasCSRawAndStructuredViaShader4X =
      hasRawAndStructuredBuffer && m_pSM->GetMajor() == 4 && m_pSM->IsCS();
  Flags.SetCSRawAndStructuredViaShader4X(hasCSRawAndStructuredViaShader4X);
}

void DxilModule::CollectShaderFlagsForModule() {
  CollectShaderFlagsForModule(m_ShaderFlags);
}

DXIL::InputPrimitive DxilModule::GetInputPrimitive() const {
  return m_InputPrimitive;
}

void DxilModule::SetInputPrimitive(DXIL::InputPrimitive IP) {
  DXASSERT_NOMSG(m_InputPrimitive == DXIL::InputPrimitive::Undefined);
  DXASSERT_NOMSG(DXIL::InputPrimitive::Undefined < IP && IP < DXIL::InputPrimitive::LastEntry);
  m_InputPrimitive = IP;
}

unsigned DxilModule::GetMaxVertexCount() const {
  DXASSERT_NOMSG(m_MaxVertexCount != 0);
  return m_MaxVertexCount;
}

void DxilModule::SetMaxVertexCount(unsigned Count) {
  DXASSERT_NOMSG(m_MaxVertexCount == 0);
  m_MaxVertexCount = Count;
}

DXIL::PrimitiveTopology DxilModule::GetStreamPrimitiveTopology() const {
  return m_StreamPrimitiveTopology;
}

void DxilModule::SetStreamPrimitiveTopology(DXIL::PrimitiveTopology Topology) {
  m_StreamPrimitiveTopology = Topology;
}

bool DxilModule::HasMultipleOutputStreams() const {
  if (!m_pSM->IsGS()) {
    return false;
  } else {
    unsigned NumStreams = (m_ActiveStreamMask & 0x1) + 
                          ((m_ActiveStreamMask & 0x2) >> 1) + 
                          ((m_ActiveStreamMask & 0x4) >> 2) + 
                          ((m_ActiveStreamMask & 0x8) >> 3);
    DXASSERT_NOMSG(NumStreams <= DXIL::kNumOutputStreams);
    return NumStreams > 1;
  }
}

unsigned DxilModule::GetOutputStream() const {
  if (!m_pSM->IsGS()) {
    return 0;
  } else {
    DXASSERT_NOMSG(!HasMultipleOutputStreams());
    switch (m_ActiveStreamMask) {
    case 0x1: return 0;
    case 0x2: return 1;
    case 0x4: return 2;
    case 0x8: return 3;
    default: DXASSERT_NOMSG(false);
    }
    return (unsigned)(-1);
  }
}

unsigned DxilModule::GetGSInstanceCount() const {
  return m_NumGSInstances;
}

void DxilModule::SetGSInstanceCount(unsigned Count) {
  m_NumGSInstances = Count;
}

bool DxilModule::IsStreamActive(unsigned Stream) const {
  return (m_ActiveStreamMask & (1<<Stream)) != 0;
}

void DxilModule::SetStreamActive(unsigned Stream, bool bActive) {
  if (bActive) {
    m_ActiveStreamMask |= (1<<Stream);
  } else {
    m_ActiveStreamMask &= ~(1<<Stream);
  }
}

void DxilModule::SetActiveStreamMask(unsigned Mask) {
  m_ActiveStreamMask = Mask;
}

unsigned DxilModule::GetActiveStreamMask() const {
  return m_ActiveStreamMask;
}

void DxilModule::SetUseMinPrecision(bool UseMinPrecision) {
  m_bUseMinPrecision = UseMinPrecision;
}

bool DxilModule::GetUseMinPrecision() const {
  return m_bUseMinPrecision;
}

void DxilModule::SetDisableOptimization(bool DisableOptimization) {
  m_bDisableOptimizations = DisableOptimization;
}

bool DxilModule::GetDisableOptimization() const {
  return m_bDisableOptimizations;
}

void DxilModule::SetAllResourcesBound(bool ResourcesBound) {
  m_bAllResourcesBound = ResourcesBound;
}

bool DxilModule::GetAllResourcesBound() const {
  return m_bAllResourcesBound;
}

unsigned DxilModule::GetInputControlPointCount() const {
  return m_InputControlPointCount;
}

void DxilModule::SetInputControlPointCount(unsigned NumICPs) {
  m_InputControlPointCount = NumICPs;
}

DXIL::TessellatorDomain DxilModule::GetTessellatorDomain() const {
  return m_TessellatorDomain;
}

void DxilModule::SetTessellatorDomain(DXIL::TessellatorDomain TessDomain) {
  m_TessellatorDomain = TessDomain;
}

unsigned DxilModule::GetOutputControlPointCount() const {
  return m_OutputControlPointCount;
}

void DxilModule::SetOutputControlPointCount(unsigned NumOCPs) {
  m_OutputControlPointCount = NumOCPs;
}

DXIL::TessellatorPartitioning DxilModule::GetTessellatorPartitioning() const {
  return m_TessellatorPartitioning;
}

void DxilModule::SetTessellatorPartitioning(DXIL::TessellatorPartitioning TessPartitioning) {
  m_TessellatorPartitioning = TessPartitioning;
}

DXIL::TessellatorOutputPrimitive DxilModule::GetTessellatorOutputPrimitive() const {
  return m_TessellatorOutputPrimitive;
}

void DxilModule::SetTessellatorOutputPrimitive(DXIL::TessellatorOutputPrimitive TessOutputPrimitive) {
  m_TessellatorOutputPrimitive = TessOutputPrimitive;
}

float DxilModule::GetMaxTessellationFactor() const {
  return m_MaxTessellationFactor;
}

void DxilModule::SetMaxTessellationFactor(float MaxTessellationFactor) {
  m_MaxTessellationFactor = MaxTessellationFactor;
}

void DxilModule::SetAutoBindingSpace(uint32_t Space) {
  m_AutoBindingSpace = Space;
}
uint32_t DxilModule::GetAutoBindingSpace() const {
  return m_AutoBindingSpace;
}

void DxilModule::SetShaderProperties(DxilFunctionProps *props) {
  if (!props)
    return;
  switch (props->shaderKind) {
  case DXIL::ShaderKind::Pixel: {
    auto &PS = props->ShaderProps.PS;
    m_ShaderFlags.SetForceEarlyDepthStencil(PS.EarlyDepthStencil);
  } break;
  case DXIL::ShaderKind::Compute: {
    auto &CS = props->ShaderProps.CS;
    for (size_t i = 0; i < _countof(m_NumThreads); ++i)
      m_NumThreads[i] = CS.numThreads[i];
  } break;
  case DXIL::ShaderKind::Domain: {
    auto &DS = props->ShaderProps.DS;
    SetTessellatorDomain(DS.domain);
    SetInputControlPointCount(DS.inputControlPoints);
  } break;
  case DXIL::ShaderKind::Hull: {
    auto &HS = props->ShaderProps.HS;
    SetPatchConstantFunction(HS.patchConstantFunc);
    SetTessellatorDomain(HS.domain);
    SetTessellatorPartitioning(HS.partition);
    SetTessellatorOutputPrimitive(HS.outputPrimitive);
    SetInputControlPointCount(HS.inputControlPoints);
    SetOutputControlPointCount(HS.outputControlPoints);
    SetMaxTessellationFactor(HS.maxTessFactor);
  } break;
  case DXIL::ShaderKind::Vertex:
    break;
  default: {
    DXASSERT(props->shaderKind == DXIL::ShaderKind::Geometry,
             "else invalid shader kind");
    auto &GS = props->ShaderProps.GS;
    SetInputPrimitive(GS.inputPrimitive);
    SetMaxVertexCount(GS.maxVertexCount);
    for (size_t i = 0; i < _countof(GS.streamPrimitiveTopologies); ++i) {
      if (GS.streamPrimitiveTopologies[i] !=
          DXIL::PrimitiveTopology::Undefined) {
        SetStreamActive(i, true);
        DXASSERT_NOMSG(GetStreamPrimitiveTopology() ==
                           DXIL::PrimitiveTopology::Undefined ||
                       GetStreamPrimitiveTopology() ==
                           GS.streamPrimitiveTopologies[i]);
        SetStreamPrimitiveTopology(GS.streamPrimitiveTopologies[i]);
      }
    }
    SetGSInstanceCount(GS.instanceCount);
  } break;
  }
}

template<typename T> unsigned 
DxilModule::AddResource(vector<unique_ptr<T> > &Vec, unique_ptr<T> pRes) {
  DXASSERT_NOMSG((unsigned)Vec.size() < UINT_MAX);
  unsigned Id = (unsigned)Vec.size();
  Vec.emplace_back(std::move(pRes));
  return Id;
}

unsigned DxilModule::AddCBuffer(unique_ptr<DxilCBuffer> pCB) {
  return AddResource<DxilCBuffer>(m_CBuffers, std::move(pCB));
}

DxilCBuffer &DxilModule::GetCBuffer(unsigned idx) {
  return *m_CBuffers[idx];
}

const DxilCBuffer &DxilModule::GetCBuffer(unsigned idx) const {
  return *m_CBuffers[idx];
}

const vector<unique_ptr<DxilCBuffer> > &DxilModule::GetCBuffers() const {
  return m_CBuffers;
}

unsigned DxilModule::AddSampler(unique_ptr<DxilSampler> pSampler) {
  return AddResource<DxilSampler>(m_Samplers, std::move(pSampler));
}

DxilSampler &DxilModule::GetSampler(unsigned idx) {
  return *m_Samplers[idx];
}

const DxilSampler &DxilModule::GetSampler(unsigned idx) const {
  return *m_Samplers[idx];
}

const vector<unique_ptr<DxilSampler> > &DxilModule::GetSamplers() const {
  return m_Samplers;
}

unsigned DxilModule::AddSRV(unique_ptr<DxilResource> pSRV) {
  return AddResource<DxilResource>(m_SRVs, std::move(pSRV));
}

DxilResource &DxilModule::GetSRV(unsigned idx) {
  return *m_SRVs[idx];
}

const DxilResource &DxilModule::GetSRV(unsigned idx) const {
  return *m_SRVs[idx];
}

const vector<unique_ptr<DxilResource> > &DxilModule::GetSRVs() const {
  return m_SRVs;
}

unsigned DxilModule::AddUAV(unique_ptr<DxilResource> pUAV) {
  return AddResource<DxilResource>(m_UAVs, std::move(pUAV));
}

DxilResource &DxilModule::GetUAV(unsigned idx) {
  return *m_UAVs[idx];
}

const DxilResource &DxilModule::GetUAV(unsigned idx) const {
  return *m_UAVs[idx];
}

const vector<unique_ptr<DxilResource> > &DxilModule::GetUAVs() const {
  return m_UAVs;
}

void DxilModule::LoadDxilResourceBaseFromMDNode(MDNode *MD, DxilResourceBase &R) {
  return m_pMDHelper->LoadDxilResourceBaseFromMDNode(MD, R);
}
void DxilModule::LoadDxilResourceFromMDNode(llvm::MDNode *MD, DxilResource &R) {
  return m_pMDHelper->LoadDxilResourceFromMDNode(MD, R);
}
void DxilModule::LoadDxilSamplerFromMDNode(llvm::MDNode *MD, DxilSampler &S) {
  return m_pMDHelper->LoadDxilSamplerFromMDNode(MD, S);
}

template <typename TResource>
static void RemoveResources(std::vector<std::unique_ptr<TResource>> &vec,
                    std::unordered_set<unsigned> &immResID) {
  for (auto p = vec.begin(); p != vec.end();) {
    auto c = p++;
    if (immResID.count((*c)->GetID()) == 0) {
      p = vec.erase(c);
    }
  }
}

static void CollectUsedResource(Value *resID,
                                std::unordered_set<Value *> &usedResID) {
  if (usedResID.count(resID) > 0)
    return;

  usedResID.insert(resID);
  if (ConstantInt *cResID = dyn_cast<ConstantInt>(resID)) {
    // Do nothing
  } else if (ZExtInst *ZEI = dyn_cast<ZExtInst>(resID)) {
    if (ZEI->getSrcTy()->isIntegerTy()) {
      IntegerType *ITy = cast<IntegerType>(ZEI->getSrcTy());
      if (ITy->getBitWidth() == 1) {
        usedResID.insert(ConstantInt::get(ZEI->getDestTy(), 0));
        usedResID.insert(ConstantInt::get(ZEI->getDestTy(), 1));
      }
    }
  } else if (SelectInst *SI = dyn_cast<SelectInst>(resID)) {
    CollectUsedResource(SI->getTrueValue(), usedResID);
    CollectUsedResource(SI->getFalseValue(), usedResID);
  } else if (PHINode *Phi = dyn_cast<PHINode>(resID)) {
    for (Use &U : Phi->incoming_values()) {
      CollectUsedResource(U.get(), usedResID);
    }
  }
  // TODO: resID could be other types of instructions depending on the compiler optimization.
}

static void ConvertUsedResource(std::unordered_set<unsigned> &immResID,
                                std::unordered_set<Value *> &usedResID) {
  for (Value *V : usedResID) {
    if (ConstantInt *cResID = dyn_cast<ConstantInt>(V)) {
      immResID.insert(cResID->getLimitedValue());
    }
  }
}

void DxilModule::RemoveFunction(llvm::Function *F) {
  DXASSERT_NOMSG(F != nullptr);
  m_DxilEntryPropsMap.erase(F);
  if (m_pTypeSystem.get()->GetFunctionAnnotation(F))
    m_pTypeSystem.get()->EraseFunctionAnnotation(F);
  m_pOP->RemoveFunction(F);
}

void DxilModule::RemoveUnusedResources() {
  DXASSERT(!m_pSM->IsLib(), "this function not work on library");
  hlsl::OP *hlslOP = GetOP();
  Function *createHandleFunc = hlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(GetCtx()));
  if (createHandleFunc->user_empty()) {
    m_CBuffers.clear();
    m_UAVs.clear();
    m_SRVs.clear();
    m_Samplers.clear();
    createHandleFunc->eraseFromParent();
    return;
  }

  std::unordered_set<Value *> usedUAVID;
  std::unordered_set<Value *> usedSRVID;
  std::unordered_set<Value *> usedSamplerID;
  std::unordered_set<Value *> usedCBufID;
  // Collect used ID.
  for (User *U : createHandleFunc->users()) {
    CallInst *CI = cast<CallInst>(U);
    Value *vResClass =
        CI->getArgOperand(DXIL::OperandIndex::kCreateHandleResClassOpIdx);
    ConstantInt *cResClass = cast<ConstantInt>(vResClass);
    DXIL::ResourceClass resClass =
        static_cast<DXIL::ResourceClass>(cResClass->getLimitedValue());
    // Skip unused resource handle.
    if (CI->user_empty())
      continue;

    Value *resID =
        CI->getArgOperand(DXIL::OperandIndex::kCreateHandleResIDOpIdx);
    switch (resClass) {
    case DXIL::ResourceClass::CBuffer:
      CollectUsedResource(resID, usedCBufID);
      break;
    case DXIL::ResourceClass::Sampler:
      CollectUsedResource(resID, usedSamplerID);
      break;
    case DXIL::ResourceClass::SRV:
      CollectUsedResource(resID, usedSRVID);
      break;
    case DXIL::ResourceClass::UAV:
      CollectUsedResource(resID, usedUAVID);
      break;
    default:
      DXASSERT(0, "invalid res class");
      break;
    }
  }

  std::unordered_set<unsigned> immUAVID;
  std::unordered_set<unsigned> immSRVID;
  std::unordered_set<unsigned> immSamplerID;
  std::unordered_set<unsigned> immCBufID;
  ConvertUsedResource(immUAVID, usedUAVID);
  RemoveResources(m_UAVs, immUAVID);
  ConvertUsedResource(immSRVID, usedSRVID);
  ConvertUsedResource(immSamplerID, usedSamplerID);
  ConvertUsedResource(immCBufID, usedCBufID);

  RemoveResources(m_SRVs, immSRVID);
  RemoveResources(m_Samplers, immSamplerID);
  RemoveResources(m_CBuffers, immCBufID);
}

namespace {
template <typename TResource>
static void RemoveResourceSymbols(std::vector<std::unique_ptr<TResource>> &vec) {
  unsigned resID = 0;
  for (std::vector<std::unique_ptr<TResource>>::iterator p = vec.begin(); p != vec.end();) {
    std::vector<std::unique_ptr<TResource>>::iterator c = p++;
    GlobalVariable *GV = cast<GlobalVariable>((*c)->GetGlobalSymbol());
    GV->removeDeadConstantUsers();
    if (GV->user_empty()) {
      p = vec.erase(c);
      GV->eraseFromParent();
      continue;
    }
    if ((*c)->GetID() != resID) {
      (*c)->SetID(resID);
    }
    resID++;
  }
}
}

void DxilModule::RemoveUnusedResourceSymbols() {
  RemoveResourceSymbols(m_SRVs);
  RemoveResourceSymbols(m_UAVs);
  RemoveResourceSymbols(m_CBuffers);
  RemoveResourceSymbols(m_Samplers);
}

DxilSignature &DxilModule::GetInputSignature() {
  DXASSERT(m_DxilEntryPropsMap.size() == 1 && !m_pSM->IsLib(),
           "only works for none lib profile");
  return m_DxilEntryPropsMap.begin()->second->sig.InputSignature;
}

const DxilSignature &DxilModule::GetInputSignature() const {
  DXASSERT(m_DxilEntryPropsMap.size() == 1 && !m_pSM->IsLib(),
           "only works for none lib profile");
  return m_DxilEntryPropsMap.begin()->second->sig.InputSignature;
}

DxilSignature &DxilModule::GetOutputSignature() {
  DXASSERT(m_DxilEntryPropsMap.size() == 1 && !m_pSM->IsLib(),
           "only works for none lib profile");
  return m_DxilEntryPropsMap.begin()->second->sig.OutputSignature;
}

const DxilSignature &DxilModule::GetOutputSignature() const {
  DXASSERT(m_DxilEntryPropsMap.size() == 1 && !m_pSM->IsLib(),
           "only works for none lib profile");
  return m_DxilEntryPropsMap.begin()->second->sig.OutputSignature;
}

DxilSignature &DxilModule::GetPatchConstantSignature() {
  DXASSERT(m_DxilEntryPropsMap.size() == 1 && !m_pSM->IsLib(),
           "only works for none lib profile");
  return m_DxilEntryPropsMap.begin()->second->sig.PatchConstantSignature;
}

const DxilSignature &DxilModule::GetPatchConstantSignature() const {
  DXASSERT(m_DxilEntryPropsMap.size() == 1 && !m_pSM->IsLib(),
           "only works for none lib profile");
  return m_DxilEntryPropsMap.begin()->second->sig.PatchConstantSignature;
}

const RootSignatureHandle &DxilModule::GetRootSignature() const {
  return *m_RootSignature;
}

// Entry props.
bool DxilModule::HasDxilEntrySignature(const llvm::Function *F) const {
  return m_DxilEntryPropsMap.find(F) != m_DxilEntryPropsMap.end();
}
DxilEntrySignature &DxilModule::GetDxilEntrySignature(const llvm::Function *F) {
  DXASSERT(m_DxilEntryPropsMap.count(F) != 0, "cannot find F in map");
  return m_DxilEntryPropsMap[F].get()->sig;
}
void DxilModule::ReplaceDxilEntryProps(llvm::Function *F,
                                       llvm::Function *NewF) {
  DXASSERT(m_DxilEntryPropsMap.count(F) != 0, "cannot find F in map");
  std::unique_ptr<DxilEntryProps> Props = std::move(m_DxilEntryPropsMap[F]);
  m_DxilEntryPropsMap.erase(F);
  m_DxilEntryPropsMap[NewF] = std::move(Props);
}
void DxilModule::CloneDxilEntryProps(llvm::Function *F, llvm::Function *NewF) {
  DXASSERT(m_DxilEntryPropsMap.count(F) != 0, "cannot find F in map");
  std::unique_ptr<DxilEntryProps> Props =
      llvm::make_unique<DxilEntryProps>(*m_DxilEntryPropsMap[F]);
  m_DxilEntryPropsMap[NewF] = std::move(Props);
}

bool DxilModule::HasDxilEntryProps(const llvm::Function *F) const {
  return m_DxilEntryPropsMap.find(F) != m_DxilEntryPropsMap.end();
}
DxilEntryProps &DxilModule::GetDxilEntryProps(const llvm::Function *F) {
  DXASSERT(m_DxilEntryPropsMap.count(F) != 0, "cannot find F in map");
  return *m_DxilEntryPropsMap.find(F)->second.get();
}

bool DxilModule::HasDxilFunctionProps(const llvm::Function *F) const {
  return m_DxilEntryPropsMap.find(F) != m_DxilEntryPropsMap.end();
}
DxilFunctionProps &DxilModule::GetDxilFunctionProps(const llvm::Function *F) {
  return const_cast<DxilFunctionProps &>(
      static_cast<const DxilModule *>(this)->GetDxilFunctionProps(F));
}

const DxilFunctionProps &
DxilModule::GetDxilFunctionProps(const llvm::Function *F) const {
  DXASSERT(m_DxilEntryPropsMap.count(F) != 0, "cannot find F in map");
  return m_DxilEntryPropsMap.find(F)->second.get()->props;
}

void DxilModule::SetPatchConstantFunctionForHS(llvm::Function *hullShaderFunc, llvm::Function *patchConstantFunc) {
  auto propIter = m_DxilEntryPropsMap.find(hullShaderFunc);
  DXASSERT(propIter != m_DxilEntryPropsMap.end(),
           "Hull shader must already have function props!");
  DxilFunctionProps &props = propIter->second->props;
  DXASSERT(props.IsHS(), "else hullShaderFunc is not a Hull Shader");
  if (props.ShaderProps.HS.patchConstantFunc)
    m_PatchConstantFunctions.erase(props.ShaderProps.HS.patchConstantFunc);
  props.ShaderProps.HS.patchConstantFunc = patchConstantFunc;
  if (patchConstantFunc)
    m_PatchConstantFunctions.insert(patchConstantFunc);
}
bool DxilModule::IsGraphicsShader(const llvm::Function *F) const {
  return HasDxilFunctionProps(F) && GetDxilFunctionProps(F).IsGraphics();
}
bool DxilModule::IsPatchConstantShader(const llvm::Function *F) const {
  return m_PatchConstantFunctions.count(F) != 0;
}
bool DxilModule::IsComputeShader(const llvm::Function *F) const {
  return HasDxilFunctionProps(F) && GetDxilFunctionProps(F).IsCS();
}
bool DxilModule::IsEntryThatUsesSignatures(const llvm::Function *F) const {
  auto propIter = m_DxilEntryPropsMap.find(F);
  if (propIter != m_DxilEntryPropsMap.end()) {
    DxilFunctionProps &props = propIter->second->props;
    return props.IsGraphics() || props.IsCS();
  }
  // Otherwise, return true if patch constant function
  return IsPatchConstantShader(F);
}

void DxilModule::StripRootSignatureFromMetadata() {
  NamedMDNode *pRootSignatureNamedMD = GetModule()->getNamedMetadata(DxilMDHelper::kDxilRootSignatureMDName);
  if (pRootSignatureNamedMD) {
    GetModule()->eraseNamedMetadata(pRootSignatureNamedMD);
  }
}

void DxilModule::UpdateValidatorVersionMetadata() {
  m_pMDHelper->EmitValidatorVersion(m_ValMajor, m_ValMinor);
}

void DxilModule::ResetRootSignature(RootSignatureHandle *pValue) {
  m_RootSignature.reset(pValue);
}

DxilTypeSystem &DxilModule::GetTypeSystem() {
  return *m_pTypeSystem;
}

DxilViewIdState &DxilModule::GetViewIdState() {
  return *m_pViewIdState;
}
const DxilViewIdState &DxilModule::GetViewIdState() const {
  return *m_pViewIdState;
}

void DxilModule::ResetTypeSystem(DxilTypeSystem *pValue) {
  m_pTypeSystem.reset(pValue);
}

void DxilModule::ResetOP(hlsl::OP *hlslOP) { m_pOP.reset(hlslOP); }

void DxilModule::ResetEntryPropsMap(DxilEntryPropsMap &&PropMap) {
  m_DxilEntryPropsMap.clear();
  std::move(PropMap.begin(), PropMap.end(),
            inserter(m_DxilEntryPropsMap, m_DxilEntryPropsMap.begin()));
}

static const StringRef llvmUsedName = "llvm.used";

void DxilModule::EmitLLVMUsed() {
  if (GlobalVariable *oldGV = m_pModule->getGlobalVariable(llvmUsedName)) {
    oldGV->eraseFromParent();
  }
  if (m_LLVMUsed.empty())
    return;

  vector<llvm::Constant *> GVs;
  Type *pI8PtrType = Type::getInt8PtrTy(m_Ctx, DXIL::kDefaultAddrSpace);

  GVs.resize(m_LLVMUsed.size());
  for (size_t i = 0, e = m_LLVMUsed.size(); i != e; i++) {
    Constant *pConst = cast<Constant>(&*m_LLVMUsed[i]);
    PointerType *pPtrType = dyn_cast<PointerType>(pConst->getType());
    if (pPtrType->getPointerAddressSpace() != DXIL::kDefaultAddrSpace) {
      // Cast pointer to addrspace 0, as LLVMUsed elements must have the same
      // type.
      GVs[i] = ConstantExpr::getAddrSpaceCast(pConst, pI8PtrType);
    } else {
      GVs[i] = ConstantExpr::getPointerCast(pConst, pI8PtrType);
    }
  }

  ArrayType *pATy = ArrayType::get(pI8PtrType, GVs.size());

  GlobalVariable *pGV =
      new GlobalVariable(*m_pModule, pATy, false, GlobalValue::AppendingLinkage,
                         ConstantArray::get(pATy, GVs), llvmUsedName);

  pGV->setSection("llvm.metadata");
}

void DxilModule::ClearLLVMUsed() {
  if (GlobalVariable *oldGV = m_pModule->getGlobalVariable(llvmUsedName)) {
    oldGV->eraseFromParent();
  }
  if (m_LLVMUsed.empty())
    return;

  for (size_t i = 0, e = m_LLVMUsed.size(); i != e; i++) {
    Constant *pConst = cast<Constant>(&*m_LLVMUsed[i]);
    pConst->removeDeadConstantUsers();
  }
  m_LLVMUsed.clear();
}

vector<GlobalVariable* > &DxilModule::GetLLVMUsed() {
  return m_LLVMUsed;
}

// DXIL metadata serialization/deserialization.
void DxilModule::ClearDxilMetadata(Module &M) {
  // Delete: DXIL version, validator version, DXIL shader model,
  // entry point tuples (shader properties, signatures, resources)
  // type system, view ID state, LLVM used, entry point tuples,
  // root signature, function properties.
  // Other cases for libs pending.
  // LLVM used is a global variable - handle separately.
  Module::named_metadata_iterator
    b = M.named_metadata_begin(),
    e = M.named_metadata_end();
  SmallVector<NamedMDNode*, 8> nodes;
  for (; b != e; ++b) {
    StringRef name = b->getName();
    if (name == DxilMDHelper::kDxilVersionMDName ||
      name == DxilMDHelper::kDxilValidatorVersionMDName ||
      name == DxilMDHelper::kDxilShaderModelMDName ||
      name == DxilMDHelper::kDxilEntryPointsMDName ||
      name == DxilMDHelper::kDxilRootSignatureMDName ||
      name == DxilMDHelper::kDxilResourcesMDName ||
      name == DxilMDHelper::kDxilTypeSystemMDName ||
      name == DxilMDHelper::kDxilViewIdStateMDName ||
      name.startswith(DxilMDHelper::kDxilTypeSystemHelperVariablePrefix)) {
      nodes.push_back(b);
    }
  }
  for (size_t i = 0; i < nodes.size(); ++i) {
    M.eraseNamedMetadata(nodes[i]);
  }
}

void DxilModule::EmitDxilMetadata() {
  m_pMDHelper->EmitDxilVersion(m_DxilMajor, m_DxilMinor);
  m_pMDHelper->EmitValidatorVersion(m_ValMajor, m_ValMinor);
  m_pMDHelper->EmitDxilShaderModel(m_pSM);

  MDTuple *pMDProperties = nullptr;
  uint64_t flag = m_ShaderFlags.GetShaderFlagsRaw();
  if (m_pSM->IsLib()) {
    DxilFunctionProps props;
    props.shaderKind = DXIL::ShaderKind::Library;
    pMDProperties = m_pMDHelper->EmitDxilEntryProperties(flag, props,
                                                         GetAutoBindingSpace());
  } else {
    pMDProperties = m_pMDHelper->EmitDxilEntryProperties(
        flag, m_DxilEntryPropsMap.begin()->second->props,
        GetAutoBindingSpace());
  }

  MDTuple *pMDSignatures = nullptr;
  if (!m_pSM->IsLib()) {
    pMDSignatures = m_pMDHelper->EmitDxilSignatures(
        m_DxilEntryPropsMap.begin()->second->sig);
  }
  MDTuple *pMDResources = EmitDxilResources();
  if (pMDResources)
    m_pMDHelper->EmitDxilResources(pMDResources);
  m_pMDHelper->EmitDxilTypeSystem(GetTypeSystem(), m_LLVMUsed);
  if (!m_pSM->IsLib() && !m_pSM->IsCS() &&
      ((m_ValMajor == 0 &&  m_ValMinor == 0) ||
       (m_ValMajor > 1 || (m_ValMajor == 1 && m_ValMinor >= 1)))) {
    m_pMDHelper->EmitDxilViewIdState(GetViewIdState());
  }
  EmitLLVMUsed();
  MDTuple *pEntry = m_pMDHelper->EmitDxilEntryPointTuple(GetEntryFunction(), m_EntryName, pMDSignatures, pMDResources, pMDProperties);
  vector<MDNode *> Entries;
  Entries.emplace_back(pEntry);

  if (m_pSM->IsLib()) {
    // Sort functions by name to keep metadata deterministic
    vector<const Function *> funcOrder;
    funcOrder.reserve(m_DxilEntryPropsMap.size());

    std::transform( m_DxilEntryPropsMap.begin(),
                    m_DxilEntryPropsMap.end(),
                    std::back_inserter(funcOrder),
                    [](auto &p) -> const Function* { return p.first; } );
    std::sort(funcOrder.begin(), funcOrder.end(), [](const Function *F1, const Function *F2) {
      return F1->getName() < F2->getName();
    });

    for (auto F : funcOrder) {
      auto &entryProps = m_DxilEntryPropsMap[F];
      MDTuple *pProps = m_pMDHelper->EmitDxilEntryProperties(0, entryProps->props, 0);
      MDTuple *pSig = m_pMDHelper->EmitDxilSignatures(entryProps->sig);

      MDTuple *pSubEntry = m_pMDHelper->EmitDxilEntryPointTuple(
          const_cast<Function *>(F), F->getName(), pSig, nullptr, pProps);

      Entries.emplace_back(pSubEntry);
    }
    funcOrder.clear();
  }
  m_pMDHelper->EmitDxilEntryPoints(Entries);

  if (!m_RootSignature->IsEmpty()) {
    m_pMDHelper->EmitRootSignature(*m_RootSignature.get());
  }
}

bool DxilModule::IsKnownNamedMetaData(llvm::NamedMDNode &Node) {
  return DxilMDHelper::IsKnownNamedMetaData(Node);
}

void DxilModule::LoadDxilMetadata() {
  m_pMDHelper->LoadDxilVersion(m_DxilMajor, m_DxilMinor);
  m_pMDHelper->LoadValidatorVersion(m_ValMajor, m_ValMinor);
  const ShaderModel *loadedModule;
  m_pMDHelper->LoadDxilShaderModel(loadedModule);
  SetShaderModel(loadedModule);

  const llvm::NamedMDNode *pEntries = m_pMDHelper->GetDxilEntryPoints();
  if (!loadedModule->IsLib()) {
    IFTBOOL(pEntries->getNumOperands() == 1, DXC_E_INCORRECT_DXIL_METADATA);
  }
  Function *pEntryFunc;
  string EntryName;
  const llvm::MDOperand *pSignatures, *pResources, *pProperties;
  m_pMDHelper->GetDxilEntryPoint(pEntries->getOperand(0), pEntryFunc, EntryName, pSignatures, pResources, pProperties);

  if (loadedModule->IsLib()) {
    for (unsigned i = 1; i < pEntries->getNumOperands(); i++) {
      Function *pFunc;
      string Name;
      const llvm::MDOperand *pSignatures, *pResources, *pProperties;
      m_pMDHelper->GetDxilEntryPoint(pEntries->getOperand(i), pFunc, Name,
                                     pSignatures, pResources, pProperties);
      DxilFunctionProps props;

      uint64_t rawShaderFlags = 0;
      unsigned autoBindingSpace = 0;
      m_pMDHelper->LoadDxilEntryProperties(
          *pProperties, rawShaderFlags, props, autoBindingSpace);
      if (props.IsHS() && props.ShaderProps.HS.patchConstantFunc) {
        // Add patch constant function to m_PatchConstantFunctions
        m_PatchConstantFunctions.insert(props.ShaderProps.HS.patchConstantFunc);
      }

      std::unique_ptr<DxilEntryProps> pEntryProps =
          llvm::make_unique<DxilEntryProps>(props, GetUseMinPrecision());
      DXASSERT(pSignatures->get() == nullptr || !props.IsRay(),
               "Raytracing has no signature");
      m_pMDHelper->LoadDxilSignatures(*pSignatures, pEntryProps->sig);

      m_DxilEntryPropsMap[pFunc] = std::move(pEntryProps);
    }
  } else {
    DxilFunctionProps props;
    props.shaderKind = loadedModule->GetKind();

    std::unique_ptr<DxilEntryProps> pEntryProps =
        llvm::make_unique<DxilEntryProps>(props, GetUseMinPrecision());
    m_pMDHelper->LoadDxilSignatures(*pSignatures, pEntryProps->sig);

    m_DxilEntryPropsMap[pEntryFunc] = std::move(pEntryProps);
  }
  SetEntryFunction(pEntryFunc);
  SetEntryFunctionName(EntryName);

  uint64_t rawShaderFlags = 0;
  if (m_pSM->IsLib()) {
    DxilFunctionProps props;
    m_pMDHelper->LoadDxilEntryProperties(*pProperties, rawShaderFlags, props,
                                          m_AutoBindingSpace);
  } else {
    DxilFunctionProps &props = m_DxilEntryPropsMap.begin()->second->props;
    m_pMDHelper->LoadDxilEntryProperties(
        *pProperties, rawShaderFlags,
        props, m_AutoBindingSpace);
    SetShaderProperties(&props);
  }
  if (rawShaderFlags) {
    m_ShaderFlags.SetShaderFlagsRaw(rawShaderFlags);
    m_bUseMinPrecision = !m_ShaderFlags.GetUseNativeLowPrecision();
    m_bDisableOptimizations = m_ShaderFlags.GetDisableOptimizations();
    m_bAllResourcesBound = m_ShaderFlags.GetAllResourcesBound();
  }
  LoadDxilResources(*pResources);

  m_pMDHelper->LoadDxilTypeSystem(*m_pTypeSystem.get());

  m_pMDHelper->LoadRootSignature(*m_RootSignature.get());

  m_pMDHelper->LoadDxilViewIdState(*m_pViewIdState.get());
}

MDTuple *DxilModule::EmitDxilResources() {
  // Emit SRV records.
  MDTuple *pTupleSRVs = nullptr;
  if (!m_SRVs.empty()) {
    vector<Metadata *> MDVals;
    for (size_t i = 0; i < m_SRVs.size(); i++) {
      MDVals.emplace_back(m_pMDHelper->EmitDxilSRV(*m_SRVs[i]));
    }
    pTupleSRVs = MDNode::get(m_Ctx, MDVals);
  }

  // Emit UAV records.
  MDTuple *pTupleUAVs = nullptr;
  if (!m_UAVs.empty()) {
    vector<Metadata *> MDVals;
    for (size_t i = 0; i < m_UAVs.size(); i++) {
      MDVals.emplace_back(m_pMDHelper->EmitDxilUAV(*m_UAVs[i]));
    }
    pTupleUAVs = MDNode::get(m_Ctx, MDVals);
  }

  // Emit CBuffer records.
  MDTuple *pTupleCBuffers = nullptr;
  if (!m_CBuffers.empty()) {
    vector<Metadata *> MDVals;
    for (size_t i = 0; i < m_CBuffers.size(); i++) {
      MDVals.emplace_back(m_pMDHelper->EmitDxilCBuffer(*m_CBuffers[i]));
    }
    pTupleCBuffers = MDNode::get(m_Ctx, MDVals);
  }

  // Emit Sampler records.
  MDTuple *pTupleSamplers = nullptr;
  if (!m_Samplers.empty()) {
    vector<Metadata *> MDVals;
    for (size_t i = 0; i < m_Samplers.size(); i++) {
      MDVals.emplace_back(m_pMDHelper->EmitDxilSampler(*m_Samplers[i]));
    }
    pTupleSamplers = MDNode::get(m_Ctx, MDVals);
  }

  if (pTupleSRVs != nullptr || pTupleUAVs != nullptr || pTupleCBuffers != nullptr || pTupleSamplers != nullptr) {
    return m_pMDHelper->EmitDxilResourceTuple(pTupleSRVs, pTupleUAVs, pTupleCBuffers, pTupleSamplers);
  } else {
    return nullptr;
  }
}

void DxilModule::ReEmitDxilResources() {
  ClearDxilMetadata(*m_pModule);
  if (!m_pSM->IsCS() && !m_pSM->IsLib())
    m_pViewIdState->Compute();
  EmitDxilMetadata();
}

void DxilModule::LoadDxilResources(const llvm::MDOperand &MDO) {
  if (MDO.get() == nullptr)
    return;

  const llvm::MDTuple *pSRVs, *pUAVs, *pCBuffers, *pSamplers;
  m_pMDHelper->GetDxilResources(MDO, pSRVs, pUAVs, pCBuffers, pSamplers);

  // Load SRV records.
  if (pSRVs != nullptr) {
    for (unsigned i = 0; i < pSRVs->getNumOperands(); i++) {
      unique_ptr<DxilResource> pSRV(new DxilResource);
      m_pMDHelper->LoadDxilSRV(pSRVs->getOperand(i), *pSRV);
      AddSRV(std::move(pSRV));
    }
  }

  // Load UAV records.
  if (pUAVs != nullptr) {
    for (unsigned i = 0; i < pUAVs->getNumOperands(); i++) {
      unique_ptr<DxilResource> pUAV(new DxilResource);
      m_pMDHelper->LoadDxilUAV(pUAVs->getOperand(i), *pUAV);
      AddUAV(std::move(pUAV));
    }
  }

  // Load CBuffer records.
  if (pCBuffers != nullptr) {
    for (unsigned i = 0; i < pCBuffers->getNumOperands(); i++) {
      unique_ptr<DxilCBuffer> pCB(new DxilCBuffer);
      m_pMDHelper->LoadDxilCBuffer(pCBuffers->getOperand(i), *pCB);
      AddCBuffer(std::move(pCB));
    }
  }

  // Load Sampler records.
  if (pSamplers != nullptr) {
    for (unsigned i = 0; i < pSamplers->getNumOperands(); i++) {
      unique_ptr<DxilSampler> pSampler(new DxilSampler);
      m_pMDHelper->LoadDxilSampler(pSamplers->getOperand(i), *pSampler);
      AddSampler(std::move(pSampler));
    }
  }
}

void DxilModule::StripDebugRelatedCode() {
  // Remove all users of global resources.
  for (GlobalVariable &GV : m_pModule->globals()) {
    if (GV.hasInternalLinkage())
      continue;
    if (GV.getType()->getPointerAddressSpace() == DXIL::kTGSMAddrSpace)
      continue;
    for (auto git = GV.user_begin(); git != GV.user_end();) {
      User *U = *(git++);
      // Try to remove load of GV.
      if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
        for (auto it = LI->user_begin(); it != LI->user_end();) {
          Instruction *LIUser = cast<Instruction>(*(it++));
          if (StoreInst *SI = dyn_cast<StoreInst>(LIUser)) {
            Value *Ptr = SI->getPointerOperand();
            SI->eraseFromParent();
            if (Instruction *PtrInst = dyn_cast<Instruction>(Ptr)) {
              if (Ptr->user_empty())
                PtrInst->eraseFromParent();
            }
          }
        }
        if (LI->user_empty())
          LI->eraseFromParent();
      } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(U)) {
        for (auto GEPIt = GEP->user_begin(); GEPIt != GEP->user_end();) {
          User *GEPU = *(GEPIt++);
          // Try to remove load of GEP.
          if (LoadInst *LI = dyn_cast<LoadInst>(GEPU)) {
            for (auto it = LI->user_begin(); it != LI->user_end();) {
              Instruction *LIUser = cast<Instruction>(*(it++));
              if (StoreInst *SI = dyn_cast<StoreInst>(LIUser)) {
                Value *Ptr = SI->getPointerOperand();
                SI->eraseFromParent();
                if (Instruction *PtrInst = dyn_cast<Instruction>(Ptr)) {
                  if (Ptr->user_empty())
                    PtrInst->eraseFromParent();
                }
              }
              if (LI->user_empty())
                LI->eraseFromParent();
            }
          }
        }
        if (GEP->user_empty())
          GEP->eraseFromParent();
      }
    }
  }
  // Remove dx.source metadata.
  if (NamedMDNode *contents = m_pModule->getNamedMetadata(
          DxilMDHelper::kDxilSourceContentsMDName)) {
    contents->eraseFromParent();
  }
  if (NamedMDNode *defines =
          m_pModule->getNamedMetadata(DxilMDHelper::kDxilSourceDefinesMDName)) {
    defines->eraseFromParent();
  }
  if (NamedMDNode *mainFileName = m_pModule->getNamedMetadata(
          DxilMDHelper::kDxilSourceMainFileNameMDName)) {
    mainFileName->eraseFromParent();
  }
  if (NamedMDNode *arguments =
          m_pModule->getNamedMetadata(DxilMDHelper::kDxilSourceArgsMDName)) {
    arguments->eraseFromParent();
  }
}
DebugInfoFinder &DxilModule::GetOrCreateDebugInfoFinder() {
  if (m_pDebugInfoFinder == nullptr) {
    m_pDebugInfoFinder = llvm::make_unique<llvm::DebugInfoFinder>();
    m_pDebugInfoFinder->processModule(*m_pModule);
  }
  return *m_pDebugInfoFinder;
}

hlsl::DxilModule *hlsl::DxilModule::TryGetDxilModule(llvm::Module *pModule) {
  LLVMContext &Ctx = pModule->getContext();
  std::string diagStr;
  raw_string_ostream diagStream(diagStr);

  hlsl::DxilModule *pDxilModule = nullptr;
  // TODO: add detail error in DxilMDHelper.
  try {
    pDxilModule = &pModule->GetOrCreateDxilModule();
  } catch (const ::hlsl::Exception &hlslException) {
    diagStream << "load dxil metadata failed -";
    try {
      const char *msg = hlslException.what();
      if (msg == nullptr || *msg == '\0')
        diagStream << " error code " << hlslException.hr << "\n";
      else
        diagStream << msg;
    } catch (...) {
      diagStream << " unable to retrieve error message.\n";
    }
    Ctx.diagnose(DxilErrorDiagnosticInfo(diagStream.str().c_str()));
  } catch (...) {
    Ctx.diagnose(DxilErrorDiagnosticInfo("load dxil metadata failed - unknown error.\n"));
  }
  return pDxilModule;
}

// Check if the instruction has fast math flags configured to indicate
// the instruction is precise.
// Precise fast math flags means none of the fast math flags are set.
bool DxilModule::HasPreciseFastMathFlags(const Instruction *inst) {
  return isa<FPMathOperator>(inst) && !inst->getFastMathFlags().any();
}

// Set fast math flags configured to indicate the instruction is precise.
void DxilModule::SetPreciseFastMathFlags(llvm::Instruction *inst) {
  assert(isa<FPMathOperator>(inst));
  inst->copyFastMathFlags(FastMathFlags());
}

// True if fast math flags are preserved across serialization/deserialization
// of the dxil module.
//
// We need to check for this when querying fast math flags for preciseness
// otherwise we will be overly conservative by reporting instructions precise
// because their fast math flags were not preserved.
//
// Currently we restrict it to the instruction types that have fast math
// preserved in the bitcode. We can expand this by converting fast math
// flags to dx.precise metadata during serialization and back to fast
// math flags during deserialization.
bool DxilModule::PreservesFastMathFlags(const llvm::Instruction *inst) {
  return
    isa<FPMathOperator>(inst) && (isa<BinaryOperator>(inst) || isa<FCmpInst>(inst));
}

bool DxilModule::IsPrecise(const Instruction *inst) const {
  if (m_ShaderFlags.GetDisableMathRefactoring())
    return true;
  else if (DxilMDHelper::IsMarkedPrecise(inst))
    return true;
  else if (PreservesFastMathFlags(inst))
    return HasPreciseFastMathFlags(inst);
  else
    return false;
}

} // namespace hlsl

namespace llvm {
hlsl::DxilModule &Module::GetOrCreateDxilModule(bool skipInit) {
  std::unique_ptr<hlsl::DxilModule> M;
  if (!HasDxilModule()) {
    M = llvm::make_unique<hlsl::DxilModule>(this);
    if (!skipInit) {
      M->LoadDxilMetadata();
    }
    SetDxilModule(M.release());
  }
  return GetDxilModule();
}

void Module::ResetDxilModule() {
  if (HasDxilModule()) {
    delete TheDxilModule;
    TheDxilModule = nullptr;
  }
}

}
