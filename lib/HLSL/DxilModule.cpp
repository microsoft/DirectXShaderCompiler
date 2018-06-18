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

//------------------------------------------------------------------------------
//
//  DxilModule methods.
//
DxilModule::DxilModule(Module *pModule)
: m_RootSignature(nullptr)
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
, m_Ctx(pModule->getContext())
, m_pModule(pModule)
, m_pEntryFunc(nullptr)
, m_pPatchConstantFunc(nullptr)
, m_EntryName("")
, m_pMDHelper(llvm::make_unique<DxilMDHelper>(pModule, llvm::make_unique<DxilExtraPropertyHelper>(pModule)))
, m_pDebugInfoFinder(nullptr)
, m_pSM(nullptr)
, m_DxilMajor(DXIL::kDxilMajor)
, m_DxilMinor(DXIL::kDxilMinor)
, m_ValMajor(1)
, m_ValMinor(0)
, m_pOP(llvm::make_unique<OP>(pModule->getContext(), pModule))
, m_pTypeSystem(llvm::make_unique<DxilTypeSystem>(pModule))
, m_pViewIdState(llvm::make_unique<DxilViewIdState>(this)) {

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

DxilModule::ShaderFlags::ShaderFlags():
  m_bDisableOptimizations(false)
, m_bDisableMathRefactoring(false)
, m_bEnableDoublePrecision(false)
, m_bForceEarlyDepthStencil(false)
, m_bEnableRawAndStructuredBuffers(false)
, m_bLowPrecisionPresent(false)
, m_bEnableDoubleExtensions(false)
, m_bEnableMSAD(false)
, m_bAllResourcesBound(false)
, m_bViewportAndRTArrayIndex(false)
, m_bInnerCoverage(false)
, m_bStencilRef(false)
, m_bTiledResources(false)
, m_bUAVLoadAdditionalFormats(false)
, m_bLevel9ComparisonFiltering(false)
, m_b64UAVs(false)
, m_UAVsAtEveryStage(false)
, m_bCSRawAndStructuredViaShader4X(false)
, m_bROVS(false)
, m_bWaveOps(false)
, m_bInt64Ops(false)
, m_bViewID(false)
, m_bBarycentrics(false)
, m_bUseNativeLowPrecision(false)
, m_align0(0)
, m_align1(0)
{}

LLVMContext &DxilModule::GetCtx() const { return m_Ctx; }
Module *DxilModule::GetModule() const { return m_pModule; }
OP *DxilModule::GetOP() const { return m_pOP.get(); }

void DxilModule::SetShaderModel(const ShaderModel *pSM) {
  DXASSERT(m_pSM == nullptr || (pSM != nullptr && *m_pSM == *pSM), "shader model must not change for the module");
  DXASSERT(pSM != nullptr && pSM->IsValidForDxil(), "shader model must be valid");
  m_pSM = pSM;
  m_pSM->GetDxilVersion(m_DxilMajor, m_DxilMinor);
  m_pMDHelper->SetShaderModel(m_pSM);
  DXIL::ShaderKind shaderKind = pSM->GetKind();
  m_EntrySignature = llvm::make_unique<DxilEntrySignature>(shaderKind, !m_ShaderFlags.GetUseNativeLowPrecision());
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

unsigned DxilModule::ShaderFlags::GetGlobalFlags() const {
  unsigned Flags = 0;
  Flags |= m_bDisableOptimizations ? DXIL::kDisableOptimizations : 0;
  Flags |= m_bDisableMathRefactoring ? DXIL::kDisableMathRefactoring : 0;
  Flags |= m_bEnableDoublePrecision ? DXIL::kEnableDoublePrecision : 0;
  Flags |= m_bForceEarlyDepthStencil ? DXIL::kForceEarlyDepthStencil : 0;
  Flags |= m_bEnableRawAndStructuredBuffers ? DXIL::kEnableRawAndStructuredBuffers : 0;
  Flags |= m_bLowPrecisionPresent && !m_bUseNativeLowPrecision? DXIL::kEnableMinPrecision : 0;
  Flags |= m_bEnableDoubleExtensions ? DXIL::kEnableDoubleExtensions : 0;
  Flags |= m_bEnableMSAD ? DXIL::kEnableMSAD : 0;
  Flags |= m_bAllResourcesBound ? DXIL::kAllResourcesBound : 0;
  return Flags;
}

uint64_t DxilModule::ShaderFlags::GetFeatureInfo() const {
  uint64_t Flags = 0;
  Flags |= m_bEnableDoublePrecision ? hlsl::ShaderFeatureInfo_Doubles : 0;
  Flags |= m_bLowPrecisionPresent && !m_bUseNativeLowPrecision ? hlsl::ShaderFeatureInfo_MinimumPrecision: 0;
  Flags |= m_bLowPrecisionPresent && m_bUseNativeLowPrecision ? hlsl::ShaderFeatureInfo_NativeLowPrecision : 0;
  Flags |= m_bEnableDoubleExtensions ? hlsl::ShaderFeatureInfo_11_1_DoubleExtensions : 0;
  Flags |= m_bWaveOps ? hlsl::ShaderFeatureInfo_WaveOps : 0;
  Flags |= m_bInt64Ops ? hlsl::ShaderFeatureInfo_Int64Ops : 0;
  Flags |= m_bROVS ? hlsl::ShaderFeatureInfo_ROVs : 0;
  Flags |= m_bViewportAndRTArrayIndex ? hlsl::ShaderFeatureInfo_ViewportAndRTArrayIndexFromAnyShaderFeedingRasterizer : 0;
  Flags |= m_bInnerCoverage ? hlsl::ShaderFeatureInfo_InnerCoverage : 0;
  Flags |= m_bStencilRef ? hlsl::ShaderFeatureInfo_StencilRef : 0;
  Flags |= m_bTiledResources ? hlsl::ShaderFeatureInfo_TiledResources : 0;
  Flags |= m_bEnableMSAD ? hlsl::ShaderFeatureInfo_11_1_ShaderExtensions : 0;
  Flags |= m_bCSRawAndStructuredViaShader4X ? hlsl::ShaderFeatureInfo_ComputeShadersPlusRawAndStructuredBuffersViaShader4X : 0;
  Flags |= m_UAVsAtEveryStage ? hlsl::ShaderFeatureInfo_UAVsAtEveryStage : 0;
  Flags |= m_b64UAVs ? hlsl::ShaderFeatureInfo_64UAVs : 0;
  Flags |= m_bLevel9ComparisonFiltering ? hlsl::ShaderFeatureInfo_LEVEL9ComparisonFiltering : 0;
  Flags |= m_bUAVLoadAdditionalFormats ? hlsl::ShaderFeatureInfo_TypedUAVLoadAdditionalFormats : 0;
  Flags |= m_bViewID ? hlsl::ShaderFeatureInfo_ViewID : 0;
  Flags |= m_bBarycentrics ? hlsl::ShaderFeatureInfo_Barycentrics : 0;

  return Flags;
}

uint64_t DxilModule::ShaderFlags::GetShaderFlagsRaw() const {
  union Cast {
    Cast(const DxilModule::ShaderFlags &flags) {
      shaderFlags = flags;
    }
    DxilModule::ShaderFlags shaderFlags;
    uint64_t  rawData;
  };
  static_assert(sizeof(uint64_t) == sizeof(DxilModule::ShaderFlags),
                "size must match to make sure no undefined bits when cast");
  Cast rawCast(*this);
  return rawCast.rawData;
}
void DxilModule::ShaderFlags::SetShaderFlagsRaw(uint64_t data) {
  union Cast {
    Cast(uint64_t data) {
      rawData = data;
    }
    DxilModule::ShaderFlags shaderFlags;
    uint64_t  rawData;
  };

  Cast rawCast(data);
  *this = rawCast.shaderFlags;
}

unsigned DxilModule::GetGlobalFlags() const {
  unsigned Flags = m_ShaderFlags.GetGlobalFlags();
  return Flags;
}

static bool IsResourceSingleComponent(llvm::Type *Ty) {
  if (llvm::ArrayType *arrType = llvm::dyn_cast<llvm::ArrayType>(Ty)) {
    if (arrType->getArrayNumElements() > 1) {
      return false;
    }
    return IsResourceSingleComponent(arrType->getArrayElementType());
  } else if (llvm::StructType *structType =
                 llvm::dyn_cast<llvm::StructType>(Ty)) {
    if (structType->getStructNumElements() > 1) {
      return false;
    }
    return IsResourceSingleComponent(structType->getStructElementType(0));
  } else if (llvm::VectorType *vectorType =
                 llvm::dyn_cast<llvm::VectorType>(Ty)) {
    if (vectorType->getNumElements() > 1) {
      return false;
    }
    return IsResourceSingleComponent(vectorType->getVectorElementType());
  }
  return true;
}

// Given a CreateHandle call, returns arbitrary ConstantInt rangeID
// Note: HLSL is currently assuming that rangeID is a constant value, but this code is assuming
// that it can be either constant, phi node, or select instruction
static ConstantInt *GetArbitraryConstantRangeID(CallInst *handleCall) {
  Value *rangeID =
      handleCall->getArgOperand(DXIL::OperandIndex::kCreateHandleResIDOpIdx);
  ConstantInt *ConstantRangeID = dyn_cast<ConstantInt>(rangeID);
  while (ConstantRangeID == nullptr) {
    if (ConstantInt *CI = dyn_cast<ConstantInt>(rangeID)) {
      ConstantRangeID = CI;
    } else if (PHINode *PN = dyn_cast<PHINode>(rangeID)) {
      rangeID = PN->getIncomingValue(0);
    } else if (SelectInst *SI = dyn_cast<SelectInst>(rangeID)) {
      rangeID = SI->getTrueValue();
    } else {
      return nullptr;
    }
  }
  return ConstantRangeID;
}

void DxilModule::CollectShaderFlags(ShaderFlags &Flags) {
  bool hasDouble = false;
  // ddiv dfma drcp d2i d2u i2d u2d.
  // fma has dxil op. Others should check IR instruction div/cast.
  bool hasDoubleExtension = false;
  bool has64Int = false;
  bool has16 = false;
  bool hasWaveOps = false;
  bool hasCheckAccessFully = false;
  bool hasMSAD = false;
  bool hasInnerCoverage = false;
  bool hasViewID = false;
  bool hasMulticomponentUAVLoads = false;
  bool hasMulticomponentUAVLoadsBackCompat = false;

  // Try to maintain compatibility with a v1.0 validator if that's what we have.
  {
    unsigned valMajor, valMinor;
    GetValidatorVersion(valMajor, valMinor);
    hasMulticomponentUAVLoadsBackCompat = valMajor <= 1 && valMinor == 0;
  }

  Type *int16Ty = Type::getInt16Ty(GetCtx());
  Type *int64Ty = Type::getInt64Ty(GetCtx());

  for (Function &F : GetModule()->functions()) {
    for (BasicBlock &BB : F.getBasicBlockList()) {
      for (Instruction &I : BB.getInstList()) {
        // Skip none dxil function call.
        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          if (!OP::IsDxilOpFunc(CI->getCalledFunction()))
            continue;
        }
        Type *Ty = I.getType();
        bool isDouble = Ty->isDoubleTy();
        bool isHalf = Ty->isHalfTy();
        bool isInt16 = Ty == int16Ty;
        bool isInt64 = Ty == int64Ty;
        if (isa<ExtractElementInst>(&I) ||
            isa<InsertElementInst>(&I))
          continue;
        for (Value *operand : I.operands()) {
          Type *Ty = operand->getType();
          isDouble |= Ty->isDoubleTy();
          isHalf |= Ty->isHalfTy();
          isInt16 |= Ty == int16Ty;
          isInt64 |= Ty == int64Ty;
        }

        if (isDouble) {
          hasDouble = true;
          switch (I.getOpcode()) {
          case Instruction::FDiv:
          case Instruction::UIToFP:
          case Instruction::SIToFP:
          case Instruction::FPToUI:
          case Instruction::FPToSI:
            hasDoubleExtension = true;
            break;
          }
        }
        
        has16 |= isHalf;
        has16 |= isInt16;
        has64Int |= isInt64;

        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          if (!OP::IsDxilOpFunc(CI->getCalledFunction()))
            continue;
          Value *opcodeArg = CI->getArgOperand(DXIL::OperandIndex::kOpcodeIdx);
          ConstantInt *opcodeConst = dyn_cast<ConstantInt>(opcodeArg);
          DXASSERT(opcodeConst, "DXIL opcode arg must be immediate");
          unsigned opcode = opcodeConst->getLimitedValue();
          DXASSERT(opcode < static_cast<unsigned>(DXIL::OpCode::NumOpCodes),
                   "invalid DXIL opcode");
          DXIL::OpCode dxilOp = static_cast<DXIL::OpCode>(opcode);
          if (hlsl::OP::IsDxilOpWave(dxilOp))
            hasWaveOps = true;
          switch (dxilOp) {
          case DXIL::OpCode::CheckAccessFullyMapped:
            hasCheckAccessFully = true;
            break;
          case DXIL::OpCode::Msad:
            hasMSAD = true;
            break;
          case DXIL::OpCode::BufferLoad:
          case DXIL::OpCode::TextureLoad: {
            if (hasMulticomponentUAVLoads) continue;
            // This is the old-style computation (overestimating requirements).
            Value *resHandle = CI->getArgOperand(DXIL::OperandIndex::kBufferStoreHandleOpIdx);
            CallInst *handleCall = cast<CallInst>(resHandle);

            if (ConstantInt *resClassArg =
              dyn_cast<ConstantInt>(handleCall->getArgOperand(
                DXIL::OperandIndex::kCreateHandleResClassOpIdx))) {
              DXIL::ResourceClass resClass = static_cast<DXIL::ResourceClass>(
                resClassArg->getLimitedValue());
              if (resClass == DXIL::ResourceClass::UAV) {
                // Validator 1.0 assumes that all uav load is multi component load.
                if (hasMulticomponentUAVLoadsBackCompat) {
                  hasMulticomponentUAVLoads = true;
                  continue;
                }
                else {
                  ConstantInt *rangeID = GetArbitraryConstantRangeID(handleCall);
                  if (rangeID) {
                      DxilResource resource = GetUAV(rangeID->getLimitedValue());
                      if ((resource.IsTypedBuffer() ||
                           resource.IsAnyTexture()) &&
                          !IsResourceSingleComponent(resource.GetRetType())) {
                        hasMulticomponentUAVLoads = true;
                      }
                  }
                }
              }
            }
            else {
                DXASSERT(false, "Resource class must be constant.");
            }
          } break;
          case DXIL::OpCode::Fma:
            hasDoubleExtension |= isDouble;
            break;
          case DXIL::OpCode::InnerCoverage:
            hasInnerCoverage = true;
            break;
          case DXIL::OpCode::ViewID:
            hasViewID = true;
            break;
          default:
            // Normal opcodes.
            break;
          }
        }
      }
    }

  }

  Flags.SetEnableDoublePrecision(hasDouble);
  Flags.SetInt64Ops(has64Int);
  Flags.SetLowPrecisionPresent(has16);
  Flags.SetEnableDoubleExtensions(hasDoubleExtension);
  Flags.SetWaveOps(hasWaveOps);
  Flags.SetTiledResources(hasCheckAccessFully);
  Flags.SetEnableMSAD(hasMSAD);
  Flags.SetUAVLoadAdditionalFormats(hasMulticomponentUAVLoads);
  Flags.SetViewID(hasViewID);

  const ShaderModel *SM = GetShaderModel();
  if (SM->IsPS()) {
    bool hasStencilRef = false;
    DxilSignature &outS = GetOutputSignature();
    for (auto &&E : outS.GetElements()) {
      if (E->GetKind() == Semantic::Kind::StencilRef) {
        hasStencilRef = true;
      } else if (E->GetKind() == Semantic::Kind::InnerCoverage) {
        hasInnerCoverage = true;
      }
    }

    Flags.SetStencilRef(hasStencilRef);
    Flags.SetInnerCoverage(hasInnerCoverage);
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

void DxilModule::CollectShaderFlags() {
  CollectShaderFlags(m_ShaderFlags);
}

uint64_t DxilModule::ShaderFlags::GetShaderFlagsRawForCollection() {
  // This should be all the flags that can be set by DxilModule::CollectShaderFlags.
  ShaderFlags Flags;
  Flags.SetEnableDoublePrecision(true);
  Flags.SetInt64Ops(true);
  Flags.SetLowPrecisionPresent(true);
  Flags.SetEnableDoubleExtensions(true);
  Flags.SetWaveOps(true);
  Flags.SetTiledResources(true);
  Flags.SetEnableMSAD(true);
  Flags.SetUAVLoadAdditionalFormats(true);
  Flags.SetStencilRef(true);
  Flags.SetInnerCoverage(true);
  Flags.SetViewportAndRTArrayIndex(true);
  Flags.Set64UAVs(true);
  Flags.SetUAVsAtEveryStage(true);
  Flags.SetEnableRawAndStructuredBuffers(true);
  Flags.SetCSRawAndStructuredViaShader4X(true);
  Flags.SetViewID(true);
  Flags.SetBarycentrics(true);
  return Flags.GetShaderFlagsRaw();
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

static void CreateResourceLinkConstant(Module &M, DxilResourceBase *pRes,
    std::vector<DxilModule::ResourceLinkInfo> &resLinkInfo) {
  Type *i32Ty = Type::getInt32Ty(M.getContext());
  const bool IsConstantTrue = true;
  Constant *NullInitVal = nullptr;
  GlobalVariable *rangeID = new GlobalVariable(
      M, i32Ty, IsConstantTrue, llvm::GlobalValue::ExternalLinkage, NullInitVal,
      pRes->GetGlobalName() + "_rangeID");

  resLinkInfo.emplace_back(DxilModule::ResourceLinkInfo{rangeID});
}

void DxilModule::CreateResourceLinkInfo() {
  DXASSERT(GetShaderModel()->IsLib(), "only for library profile");
  DXASSERT(m_SRVsLinkInfo.empty() && m_UAVsLinkInfo.empty() &&
               m_CBuffersLinkInfo.empty() && m_SamplersLinkInfo.empty(),
           "else resource link info was already created");
  Module &M = *m_pModule;
  for (auto &SRV : m_SRVs) {
    CreateResourceLinkConstant(M, SRV.get(), m_SRVsLinkInfo);
  }
  for (auto &UAV : m_UAVs) {
    CreateResourceLinkConstant(M, UAV.get(), m_UAVsLinkInfo);
  }
  for (auto &CBuffer : m_CBuffers) {
    CreateResourceLinkConstant(M, CBuffer.get(), m_CBuffersLinkInfo);
  }
  for (auto &Sampler : m_Samplers) {
    CreateResourceLinkConstant(M, Sampler.get(), m_SamplersLinkInfo);
  }
}

const DxilModule::ResourceLinkInfo &
DxilModule::GetResourceLinkInfo(DXIL::ResourceClass resClass,
                                unsigned rangeID) const {
  switch (resClass) {
  case DXIL::ResourceClass::UAV:
    return m_UAVsLinkInfo[rangeID];
  case DXIL::ResourceClass::CBuffer:
    return m_CBuffersLinkInfo[rangeID];
  case DXIL::ResourceClass::Sampler:
    return m_SamplersLinkInfo[rangeID];
  default:
    DXASSERT(DXIL::ResourceClass::SRV == resClass,
             "else invalid resource class");
    return m_SRVsLinkInfo[rangeID];
  }
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
  if (dyn_cast<ConstantInt>(resID)) {
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
  m_DxilFunctionPropsMap.erase(F);
  m_DxilEntrySignatureMap.erase(F);
  if (m_pTypeSystem.get()->GetFunctionAnnotation(F))
    m_pTypeSystem.get()->EraseFunctionAnnotation(F);
  m_pOP->RemoveFunction(F);
}

void DxilModule::RemoveUnusedResources() {
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

DxilSignature &DxilModule::GetInputSignature() {
  return m_EntrySignature->InputSignature;
}

const DxilSignature &DxilModule::GetInputSignature() const {
  return m_EntrySignature->InputSignature;
}

DxilSignature &DxilModule::GetOutputSignature() {
  return m_EntrySignature->OutputSignature;
}

const DxilSignature &DxilModule::GetOutputSignature() const {
  return m_EntrySignature->OutputSignature;
}

DxilSignature &DxilModule::GetPatchConstantSignature() {
  return m_EntrySignature->PatchConstantSignature;
}

const DxilSignature &DxilModule::GetPatchConstantSignature() const {
  return m_EntrySignature->PatchConstantSignature;
}

const RootSignatureHandle &DxilModule::GetRootSignature() const {
  return *m_RootSignature;
}

bool DxilModule::HasDxilEntrySignature(llvm::Function *F) const {
  return m_DxilEntrySignatureMap.find(F) != m_DxilEntrySignatureMap.end();
}
DxilEntrySignature &DxilModule::GetDxilEntrySignature(llvm::Function *F) {
  DXASSERT(m_DxilEntrySignatureMap.count(F) != 0, "cannot find F in map");
  return *m_DxilEntrySignatureMap[F];
}
void DxilModule::ReplaceDxilEntrySignature(llvm::Function *F,
                                           llvm::Function *NewF) {
  DXASSERT(m_DxilEntrySignatureMap.count(F) != 0, "cannot find F in map");
  std::unique_ptr<DxilEntrySignature> Sig =
      std::move(m_DxilEntrySignatureMap[F]);
  m_DxilEntrySignatureMap.erase(F);
  m_DxilEntrySignatureMap[NewF] = std::move(Sig);
}

bool DxilModule::HasDxilFunctionProps(llvm::Function *F) const {
  return m_DxilFunctionPropsMap.find(F) != m_DxilFunctionPropsMap.end();
}
DxilFunctionProps &DxilModule::GetDxilFunctionProps(llvm::Function *F) {
  DXASSERT(m_DxilFunctionPropsMap.count(F) != 0, "cannot find F in map");
  return *m_DxilFunctionPropsMap[F];
}
void DxilModule::ReplaceDxilFunctionProps(llvm::Function *F,
                                          llvm::Function *NewF) {
  DXASSERT(m_DxilFunctionPropsMap.count(F) != 0, "cannot find F in map");
  std::unique_ptr<DxilFunctionProps> props =
      std::move(m_DxilFunctionPropsMap[F]);
  m_DxilFunctionPropsMap.erase(F);
  m_DxilFunctionPropsMap[NewF] = std::move(props);
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

void DxilModule::ResetEntrySignature(DxilEntrySignature *pValue) {
  m_EntrySignature.reset(pValue);
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

void DxilModule::ResetFunctionPropsMap(
    std::unordered_map<llvm::Function *, std::unique_ptr<DxilFunctionProps>>
        &&propsMap) {
  m_DxilFunctionPropsMap = std::move(propsMap);
}

void DxilModule::ResetEntrySignatureMap(
    std::unordered_map<llvm::Function *, std::unique_ptr<DxilEntrySignature>>
        &&SigMap) {
  m_DxilEntrySignatureMap = std::move(SigMap);
}

void DxilModule::EmitLLVMUsed() {
  if (m_LLVMUsed.empty())
    return;

  vector<llvm::Constant*> GVs;
  Type *pI8PtrType = Type::getInt8PtrTy(m_Ctx, DXIL::kDefaultAddrSpace);

  GVs.resize(m_LLVMUsed.size());
  for (size_t i = 0, e = m_LLVMUsed.size(); i != e; i++) {
    Constant *pConst = cast<Constant>(&*m_LLVMUsed[i]);
    PointerType * pPtrType = dyn_cast<PointerType>(pConst->getType());
    if (pPtrType->getPointerAddressSpace() != DXIL::kDefaultAddrSpace) {
      // Cast pointer to addrspace 0, as LLVMUsed elements must have the same type.
      GVs[i] = ConstantExpr::getAddrSpaceCast(pConst, pI8PtrType);
    } else {
      GVs[i] = ConstantExpr::getPointerCast(pConst, pI8PtrType);
    }
  }

  ArrayType *pATy = ArrayType::get(pI8PtrType, GVs.size());

  StringRef llvmUsedName = "llvm.used";

  if (GlobalVariable *oldGV = m_pModule->getGlobalVariable(llvmUsedName)) {
    oldGV->eraseFromParent();
  }

  GlobalVariable *pGV = new GlobalVariable(*m_pModule, pATy, false,
                                           GlobalValue::AppendingLinkage,
                                           ConstantArray::get(pATy, GVs),
                                           llvmUsedName);

  pGV->setSection("llvm.metadata");
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
      name == DxilMDHelper::kDxilFunctionPropertiesMDName || // used in libraries
      name == DxilMDHelper::kDxilEntrySignaturesMDName || // used in libraries
      name == DxilMDHelper::kDxilResourcesLinkInfoMDName || // used in libraries
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

  MDTuple *pMDProperties = EmitDxilShaderProperties();

  MDTuple *pMDSignatures = m_pMDHelper->EmitDxilSignatures(*m_EntrySignature);
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
  m_pMDHelper->EmitDxilEntryPoints(Entries);

  if (!m_RootSignature->IsEmpty()) {
    m_pMDHelper->EmitRootSignature(*m_RootSignature.get());
  }
  if (m_pSM->IsLib()) {
    EmitDxilResourcesLinkInfo();
    NamedMDNode *fnProps = m_pModule->getOrInsertNamedMetadata(
        DxilMDHelper::kDxilFunctionPropertiesMDName);
    for (auto &&pair : m_DxilFunctionPropsMap) {
      const hlsl::DxilFunctionProps *props = pair.second.get();
      MDTuple *pProps = m_pMDHelper->EmitDxilFunctionProps(props, pair.first);
      fnProps->addOperand(pProps);
    }

    NamedMDNode *entrySigs = m_pModule->getOrInsertNamedMetadata(
        DxilMDHelper::kDxilEntrySignaturesMDName);
    for (auto &&pair : m_DxilEntrySignatureMap) {
      Function *F = pair.first;
      DxilEntrySignature *Sig = pair.second.get();
      MDTuple *pSig = m_pMDHelper->EmitDxilSignatures(*Sig);
      entrySigs->addOperand(
          MDTuple::get(m_Ctx, {ValueAsMetadata::get(F), pSig}));
    }
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
  DXASSERT(m_EntrySignature != nullptr, "else SetShaderModel didn't create entry signature");

  const llvm::NamedMDNode *pEntries = m_pMDHelper->GetDxilEntryPoints();
  IFTBOOL(pEntries->getNumOperands() == 1, DXC_E_INCORRECT_DXIL_METADATA);

  Function *pEntryFunc;
  string EntryName;
  const llvm::MDOperand *pSignatures, *pResources, *pProperties;
  m_pMDHelper->GetDxilEntryPoint(pEntries->getOperand(0), pEntryFunc, EntryName, pSignatures, pResources, pProperties);

  SetEntryFunction(pEntryFunc);
  SetEntryFunctionName(EntryName);

  LoadDxilShaderProperties(*pProperties);

  m_pMDHelper->LoadDxilSignatures(*pSignatures, *m_EntrySignature);
  LoadDxilResources(*pResources);

  m_pMDHelper->LoadDxilTypeSystem(*m_pTypeSystem.get());

  m_pMDHelper->LoadRootSignature(*m_RootSignature.get());

  m_pMDHelper->LoadDxilViewIdState(*m_pViewIdState.get());

  if (loadedModule->IsLib()) {
    LoadDxilResourcesLinkInfo();
    NamedMDNode *fnProps = m_pModule->getNamedMetadata(
        DxilMDHelper::kDxilFunctionPropertiesMDName);
    size_t propIdx = 0;
    while (propIdx < fnProps->getNumOperands()) {
      MDTuple *pProps = dyn_cast<MDTuple>(fnProps->getOperand(propIdx++));

      std::unique_ptr<hlsl::DxilFunctionProps> props =
          llvm::make_unique<hlsl::DxilFunctionProps>();

      Function *F = m_pMDHelper->LoadDxilFunctionProps(pProps, props.get());

      m_DxilFunctionPropsMap[F] = std::move(props);
    }

    NamedMDNode *entrySigs = m_pModule->getOrInsertNamedMetadata(
        DxilMDHelper::kDxilEntrySignaturesMDName);
    size_t sigIdx = 0;
    while (sigIdx < entrySigs->getNumOperands()) {
      MDTuple *pSig = dyn_cast<MDTuple>(entrySigs->getOperand(sigIdx++));

      unsigned idx = 0;
      Function *F = dyn_cast<Function>(
          dyn_cast<ValueAsMetadata>(pSig->getOperand(idx++))->getValue());
      // Entry must have props.
      IFTBOOL(m_DxilFunctionPropsMap.count(F), DXC_E_INCORRECT_DXIL_METADATA);

      DXIL::ShaderKind shaderKind = m_DxilFunctionPropsMap[F]->shaderKind;

      std::unique_ptr<hlsl::DxilEntrySignature> Sig =
          llvm::make_unique<hlsl::DxilEntrySignature>(shaderKind, !m_ShaderFlags.GetUseNativeLowPrecision());

      m_pMDHelper->LoadDxilSignatures(pSig->getOperand(idx), *Sig);

      m_DxilEntrySignatureMap[F] = std::move(Sig);
    }
  }
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

static MDTuple *CreateResourcesLinkInfo(std::vector<DxilModule::ResourceLinkInfo> &LinkInfoList,
                                    unsigned size, LLVMContext &Ctx) {
  DXASSERT(size == LinkInfoList.size(), "link info size must match resource size");
  if (LinkInfoList.empty())
    return nullptr;

  vector<Metadata *> MDVals;
  for (size_t i = 0; i < size; i++) {
    MDVals.emplace_back(ValueAsMetadata::get(LinkInfoList[i].ResRangeID));
  }
  return MDNode::get(Ctx, MDVals);
}

void DxilModule::EmitDxilResourcesLinkInfo() {
  // Emit SRV base records.
  MDTuple *pTupleSRVs =
      CreateResourcesLinkInfo(m_SRVsLinkInfo, m_SRVs.size(), m_Ctx);

  // Emit UAV base records.
  MDTuple *pTupleUAVs =
      CreateResourcesLinkInfo(m_UAVsLinkInfo, m_UAVs.size(), m_Ctx);

  // Emit CBuffer base records.
  MDTuple *pTupleCBuffers =
      CreateResourcesLinkInfo(m_CBuffersLinkInfo, m_CBuffers.size(), m_Ctx);

  // Emit Sampler records.
  MDTuple *pTupleSamplers =
      CreateResourcesLinkInfo(m_SamplersLinkInfo, m_Samplers.size(), m_Ctx);

  if (pTupleSRVs != nullptr || pTupleUAVs != nullptr ||
      pTupleCBuffers != nullptr || pTupleSamplers != nullptr) {
    m_pMDHelper->EmitDxilResourceLinkInfoTuple(pTupleSRVs, pTupleUAVs,
                                               pTupleCBuffers, pTupleSamplers);
  }
}

static void
LoadResourcesLinkInfo(const llvm::MDTuple *pMD,
                      std::vector<DxilModule::ResourceLinkInfo> &LinkInfoList,
                      unsigned size, DxilMDHelper *pMDHelper) {
  if (!pMD) {
    IFTBOOL(size == 0, DXC_E_INCORRECT_DXIL_METADATA);
    return;
  }
  unsigned operandSize = pMD->getNumOperands();
  IFTBOOL(operandSize == size, DXC_E_INCORRECT_DXIL_METADATA);
  for (unsigned i = 0; i < operandSize; i++) {
    Constant *rangeID =
        dyn_cast<Constant>(pMDHelper->ValueMDToValue(pMD->getOperand(i)));
    LinkInfoList.emplace_back(DxilModule::ResourceLinkInfo{rangeID});
  }
}

void DxilModule::LoadDxilResourcesLinkInfo() {
  const llvm::MDTuple *pSRVs, *pUAVs, *pCBuffers, *pSamplers;
  m_pMDHelper->LoadDxilResourceLinkInfoTuple(pSRVs, pUAVs, pCBuffers,
                                             pSamplers);

  // Load SRV base records.
  LoadResourcesLinkInfo(pSRVs, m_SRVsLinkInfo, m_SRVs.size(),
                        m_pMDHelper.get());

  // Load UAV base records.
  LoadResourcesLinkInfo(pUAVs, m_UAVsLinkInfo, m_UAVs.size(),
                        m_pMDHelper.get());

  // Load CBuffer records.
  LoadResourcesLinkInfo(pCBuffers, m_CBuffersLinkInfo, m_CBuffers.size(),
                        m_pMDHelper.get());

  // Load Sampler records.
  LoadResourcesLinkInfo(pSamplers, m_SamplersLinkInfo, m_Samplers.size(),
                        m_pMDHelper.get());
}

MDTuple *DxilModule::EmitDxilShaderProperties() {
  vector<Metadata *> MDVals;

  // DXIL shader flags.
  uint64_t flag = m_ShaderFlags.GetShaderFlagsRaw();
  if (flag != 0) {
    MDVals.emplace_back(m_pMDHelper->Uint32ToConstMD(DxilMDHelper::kDxilShaderFlagsTag));
    MDVals.emplace_back(m_pMDHelper->Uint64ToConstMD(flag));
  }

  // Compute shader.
  if (m_pSM->IsCS()) {
    MDVals.emplace_back(m_pMDHelper->Uint32ToConstMD(DxilMDHelper::kDxilNumThreadsTag));
    vector<Metadata *> NumThreadVals;
    NumThreadVals.emplace_back(m_pMDHelper->Uint32ToConstMD(m_NumThreads[0]));
    NumThreadVals.emplace_back(m_pMDHelper->Uint32ToConstMD(m_NumThreads[1]));
    NumThreadVals.emplace_back(m_pMDHelper->Uint32ToConstMD(m_NumThreads[2]));
    MDVals.emplace_back(MDNode::get(m_Ctx, NumThreadVals));
  }

  // Geometry shader.
  if (m_pSM->IsGS()) {
    MDVals.emplace_back(m_pMDHelper->Uint32ToConstMD(DxilMDHelper::kDxilGSStateTag));
    MDTuple *pMDTuple = m_pMDHelper->EmitDxilGSState(m_InputPrimitive,
                                                     m_MaxVertexCount,
                                                     GetActiveStreamMask(),
                                                     m_StreamPrimitiveTopology,
                                                     m_NumGSInstances);
    MDVals.emplace_back(pMDTuple);
  }

  // Domain shader.
  if (m_pSM->IsDS()) {
    MDVals.emplace_back(m_pMDHelper->Uint32ToConstMD(DxilMDHelper::kDxilDSStateTag));
    MDTuple *pMDTuple = m_pMDHelper->EmitDxilDSState(m_TessellatorDomain,
                                                     m_InputControlPointCount);
    MDVals.emplace_back(pMDTuple);
  }

  // Hull shader.
  if (m_pSM->IsHS()) {
    MDVals.emplace_back(m_pMDHelper->Uint32ToConstMD(DxilMDHelper::kDxilHSStateTag));
    MDTuple *pMDTuple = m_pMDHelper->EmitDxilHSState(m_pPatchConstantFunc,
                                                     m_InputControlPointCount,
                                                     m_OutputControlPointCount,
                                                     m_TessellatorDomain,
                                                     m_TessellatorPartitioning,
                                                     m_TessellatorOutputPrimitive,
                                                     m_MaxTessellationFactor);
    MDVals.emplace_back(pMDTuple);
  }

  if (!MDVals.empty())
    return MDNode::get(m_Ctx, MDVals);
  else
    return nullptr;
}

void DxilModule::LoadDxilShaderProperties(const MDOperand &MDO) {
  if (MDO.get() == nullptr)
    return;

  const MDTuple *pTupleMD = dyn_cast<MDTuple>(MDO.get());
  IFTBOOL(pTupleMD != nullptr, DXC_E_INCORRECT_DXIL_METADATA);
  IFTBOOL((pTupleMD->getNumOperands() & 0x1) == 0, DXC_E_INCORRECT_DXIL_METADATA);

  for (unsigned iNode = 0; iNode < pTupleMD->getNumOperands(); iNode += 2) {
    unsigned Tag = DxilMDHelper::ConstMDToUint32(pTupleMD->getOperand(iNode));
    const MDOperand &MDO = pTupleMD->getOperand(iNode + 1);
    IFTBOOL(MDO.get() != nullptr, DXC_E_INCORRECT_DXIL_METADATA);

    switch (Tag) {
    case DxilMDHelper::kDxilShaderFlagsTag:
      m_ShaderFlags.SetShaderFlagsRaw(DxilMDHelper::ConstMDToUint64(MDO));
      break;

    case DxilMDHelper::kDxilNumThreadsTag: {
      MDNode *pNode = cast<MDNode>(MDO.get());
      m_NumThreads[0] = DxilMDHelper::ConstMDToUint32(pNode->getOperand(0));
      m_NumThreads[1] = DxilMDHelper::ConstMDToUint32(pNode->getOperand(1));
      m_NumThreads[2] = DxilMDHelper::ConstMDToUint32(pNode->getOperand(2));
      break;
    }

    case DxilMDHelper::kDxilGSStateTag: {
      m_pMDHelper->LoadDxilGSState(MDO, m_InputPrimitive, m_MaxVertexCount, m_ActiveStreamMask, 
                                   m_StreamPrimitiveTopology, m_NumGSInstances);
      break;
    }

    case DxilMDHelper::kDxilDSStateTag:
      m_pMDHelper->LoadDxilDSState(MDO, m_TessellatorDomain, m_InputControlPointCount);
      break;

    case DxilMDHelper::kDxilHSStateTag:
      m_pMDHelper->LoadDxilHSState(MDO,
                                   m_pPatchConstantFunc,
                                   m_InputControlPointCount,
                                   m_OutputControlPointCount,
                                   m_TessellatorDomain,
                                   m_TessellatorPartitioning,
                                   m_TessellatorOutputPrimitive,
                                   m_MaxTessellationFactor);
      break;

    default:
      DXASSERT(false, "Unknown extended shader properties tag");
      break;
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
