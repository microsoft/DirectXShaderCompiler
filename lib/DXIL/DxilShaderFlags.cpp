///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilShaderFlags.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilShaderFlags.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResource.h"
#include "dxc/Support/Global.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Casting.h"
#include "dxc/DXIL/DxilEntryProps.h"

using namespace hlsl;
using namespace llvm;

ShaderFlags::ShaderFlags():
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
, m_bShadingRate(false)
, m_align0(0)
, m_align1(0)
{}

uint64_t ShaderFlags::GetFeatureInfo() const {
  uint64_t Flags = 0;
  Flags |= m_bEnableDoublePrecision ? hlsl::DXIL::ShaderFeatureInfo_Doubles : 0;
  Flags |= m_bLowPrecisionPresent && !m_bUseNativeLowPrecision
               ? hlsl::DXIL::ShaderFeatureInfo_MinimumPrecision
               : 0;
  Flags |= m_bLowPrecisionPresent && m_bUseNativeLowPrecision
               ? hlsl::DXIL::ShaderFeatureInfo_NativeLowPrecision
               : 0;
  Flags |= m_bEnableDoubleExtensions
               ? hlsl::DXIL::ShaderFeatureInfo_11_1_DoubleExtensions
               : 0;
  Flags |= m_bWaveOps ? hlsl::DXIL::ShaderFeatureInfo_WaveOps : 0;
  Flags |= m_bInt64Ops ? hlsl::DXIL::ShaderFeatureInfo_Int64Ops : 0;
  Flags |= m_bROVS ? hlsl::DXIL::ShaderFeatureInfo_ROVs : 0;
  Flags |=
      m_bViewportAndRTArrayIndex
          ? hlsl::DXIL::
                ShaderFeatureInfo_ViewportAndRTArrayIndexFromAnyShaderFeedingRasterizer
          : 0;
  Flags |= m_bInnerCoverage ? hlsl::DXIL::ShaderFeatureInfo_InnerCoverage : 0;
  Flags |= m_bStencilRef ? hlsl::DXIL::ShaderFeatureInfo_StencilRef : 0;
  Flags |= m_bTiledResources ? hlsl::DXIL::ShaderFeatureInfo_TiledResources : 0;
  Flags |=
      m_bEnableMSAD ? hlsl::DXIL::ShaderFeatureInfo_11_1_ShaderExtensions : 0;
  Flags |=
      m_bCSRawAndStructuredViaShader4X
          ? hlsl::DXIL::
                ShaderFeatureInfo_ComputeShadersPlusRawAndStructuredBuffersViaShader4X
          : 0;
  Flags |=
      m_UAVsAtEveryStage ? hlsl::DXIL::ShaderFeatureInfo_UAVsAtEveryStage : 0;
  Flags |= m_b64UAVs ? hlsl::DXIL::ShaderFeatureInfo_64UAVs : 0;
  Flags |= m_bLevel9ComparisonFiltering
               ? hlsl::DXIL::ShaderFeatureInfo_LEVEL9ComparisonFiltering
               : 0;
  Flags |= m_bUAVLoadAdditionalFormats
               ? hlsl::DXIL::ShaderFeatureInfo_TypedUAVLoadAdditionalFormats
               : 0;
  Flags |= m_bViewID ? hlsl::DXIL::ShaderFeatureInfo_ViewID : 0;
  Flags |= m_bBarycentrics ? hlsl::DXIL::ShaderFeatureInfo_Barycentrics : 0;
  Flags |= m_bShadingRate ? hlsl::DXIL::ShaderFeatureInfo_ShadingRate : 0;

  return Flags;
}

uint64_t ShaderFlags::GetShaderFlagsRaw() const {
  union Cast {
    Cast(const ShaderFlags &flags) {
      shaderFlags = flags;
    }
    ShaderFlags shaderFlags;
    uint64_t  rawData;
  };
  static_assert(sizeof(uint64_t) == sizeof(ShaderFlags),
                "size must match to make sure no undefined bits when cast");
  Cast rawCast(*this);
  return rawCast.rawData;
}

void ShaderFlags::SetShaderFlagsRaw(uint64_t data) {
  union Cast {
    Cast(uint64_t data) {
      rawData = data;
    }
    ShaderFlags shaderFlags;
    uint64_t  rawData;
  };

  Cast rawCast(data);
  *this = rawCast.shaderFlags;
}

uint64_t ShaderFlags::GetShaderFlagsRawForCollection() {
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
  Flags.SetShadingRate(true);
  return Flags.GetShaderFlagsRaw();
}

unsigned ShaderFlags::GetGlobalFlags() const {
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

// Given a handle type, find an arbitrary call instructions to create handle
static CallInst *FindCallToCreateHandle(Value *handleType) {
  Value *curVal = handleType;
  CallInst *CI = dyn_cast<CallInst>(handleType);
  while (CI == nullptr) {
    if (PHINode *PN = dyn_cast<PHINode>(curVal)) {
      curVal = PN->getIncomingValue(0);
    }
    else if (SelectInst *SI = dyn_cast<SelectInst>(curVal)) {
      curVal = SI->getTrueValue();
    }
    else {
      return nullptr;
    }
    CI = dyn_cast<CallInst>(curVal);
  }
  return CI;
}

ShaderFlags ShaderFlags::CollectShaderFlags(const Function *F,
                                           const hlsl::DxilModule *M) {
  ShaderFlags flag;
  // Module level options
  flag.SetUseNativeLowPrecision(!M->GetUseMinPrecision());
  flag.SetDisableOptimizations(M->GetDisableOptimization());
  flag.SetAllResourcesBound(M->GetAllResourcesBound());

  bool hasDouble = false;
  // ddiv dfma drcp d2i d2u i2d u2d.
  // fma has dxil op. Others should check IR instruction div/cast.
  bool hasDoubleExtension = false;
  bool has64Int = false;
  bool has16 = false;
  bool hasWaveOps = false;
  bool hasCheckAccessFully = false;
  bool hasMSAD = false;
  bool hasStencilRef = false;
  bool hasInnerCoverage = false;
  bool hasViewID = false;
  bool hasMulticomponentUAVLoads = false;
  bool hasViewportOrRTArrayIndex = false;
  bool hasShadingRate = false;

  // Try to maintain compatibility with a v1.0 validator if that's what we have.
  uint32_t valMajor, valMinor;
  M->GetValidatorVersion(valMajor, valMinor);
  bool hasMulticomponentUAVLoadsBackCompat = valMajor == 1 && valMinor == 0;
  bool hasViewportOrRTArrayIndexBackCombat = valMajor == 1 && valMinor < 4;

  Type *int16Ty = Type::getInt16Ty(F->getContext());
  Type *int64Ty = Type::getInt64Ty(F->getContext());

  for (const BasicBlock &BB : F->getBasicBlockList()) {
    for (const Instruction &I : BB.getInstList()) {
      // Skip none dxil function call.
      if (const CallInst *CI = dyn_cast<CallInst>(&I)) {
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
      if (const CallInst *CI = dyn_cast<CallInst>(&I)) {
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
          CallInst *handleCall = FindCallToCreateHandle(resHandle);
          // Check if this is a library handle or general create handle
          if (handleCall) {
            ConstantInt *HandleOpCodeConst = cast<ConstantInt>(
                handleCall->getArgOperand(DXIL::OperandIndex::kOpcodeIdx));
            DXIL::OpCode handleOp = static_cast<DXIL::OpCode>(HandleOpCodeConst->getLimitedValue());
            if (handleOp == DXIL::OpCode::CreateHandle) {
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
                      DxilResource resource = M->GetUAV(rangeID->getLimitedValue());
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
            }
            else if (handleOp == DXIL::OpCode::CreateHandleForLib) {
              // If library handle, find DxilResource by checking the name
              if (LoadInst *LI = dyn_cast<LoadInst>(handleCall->getArgOperand(
                      DXIL::OperandIndex::
                          kCreateHandleForLibResOpIdx))) {
                Value *resType = LI->getOperand(0);
                for (auto &&res : M->GetUAVs()) {
                  if (res->GetGlobalSymbol() == resType) {
                    if ((res->IsTypedBuffer() || res->IsAnyTexture()) &&
                        !IsResourceSingleComponent(res->GetRetType())) {
                      hasMulticomponentUAVLoads = true;
                    }
                  }
                }
              }
            }
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

  // If this function is a shader, add flags based on signatures
  if (M->HasDxilEntryProps(F)) {
    const DxilEntryProps &entryProps = M->GetDxilEntryProps(F);

    // Val ver < 1.4 has a bug where input case was always clobbered by the
    // output check.  The only case where it made a difference such that an
    // incorrect flag would be set was for the HS and DS input cases.
    // It was also checking PS input and output, but PS output could not have
    // the semantic, and since it was clobbering the result, it would always
    // clear it.  Since this flag should not be set for PS at all,
    // it produced the correct result for PS by accident.
    bool checkInputRTArrayIndex = entryProps.props.IsGS();
    if (!hasViewportOrRTArrayIndexBackCombat)
      checkInputRTArrayIndex |= entryProps.props.IsDS() ||
                                entryProps.props.IsHS();
    bool checkOutputRTArrayIndex =
      entryProps.props.IsVS() || entryProps.props.IsDS() ||
      entryProps.props.IsHS();

    for (auto &&E : entryProps.sig.InputSignature.GetElements()) {
      switch (E->GetKind()) {
      case Semantic::Kind::ViewPortArrayIndex:
      case Semantic::Kind::RenderTargetArrayIndex:
        if (checkInputRTArrayIndex)
          hasViewportOrRTArrayIndex = true;
        break;
      case Semantic::Kind::ShadingRate:
        hasShadingRate = true;
        break;
      default:
        break;
      }
    }

    for (auto &&E : entryProps.sig.OutputSignature.GetElements()) {
      switch (E->GetKind()) {
      case Semantic::Kind::ViewPortArrayIndex:
      case Semantic::Kind::RenderTargetArrayIndex:
        if (checkOutputRTArrayIndex)
          hasViewportOrRTArrayIndex = true;
        break;
      case Semantic::Kind::StencilRef:
        if (entryProps.props.IsPS())
          hasStencilRef = true;
        break;
      case Semantic::Kind::InnerCoverage:
        if (entryProps.props.IsPS())
          hasInnerCoverage = true;
        break;
      case Semantic::Kind::ShadingRate:
        hasShadingRate = true;
        break;
      default:
        break;
      }
    }
  }

  flag.SetEnableDoublePrecision(hasDouble);
  flag.SetStencilRef(hasStencilRef);
  flag.SetInnerCoverage(hasInnerCoverage);
  flag.SetInt64Ops(has64Int);
  flag.SetLowPrecisionPresent(has16);
  flag.SetEnableDoubleExtensions(hasDoubleExtension);
  flag.SetWaveOps(hasWaveOps);
  flag.SetTiledResources(hasCheckAccessFully);
  flag.SetEnableMSAD(hasMSAD);
  flag.SetUAVLoadAdditionalFormats(hasMulticomponentUAVLoads);
  flag.SetViewID(hasViewID);
  flag.SetViewportAndRTArrayIndex(hasViewportOrRTArrayIndex);
  flag.SetShadingRate(hasShadingRate);

  return flag;
}

void ShaderFlags::CombineShaderFlags(const ShaderFlags &other) {
  SetShaderFlagsRaw(GetShaderFlagsRaw() | other.GetShaderFlagsRaw());
}
