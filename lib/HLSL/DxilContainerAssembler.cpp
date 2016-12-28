///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerAssembler.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Provides support for serializing a module into DXIL container structures. //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ADT/MapVector.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilRootSignature.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/HLSL/DxilPipelineStateValidation.h"
#include <algorithm>
#include <functional>

using namespace llvm;
using namespace hlsl;

static DxilProgramSigSemantic KindToSystemValue(Semantic::Kind kind, DXIL::TessellatorDomain domain) {
  switch (kind) {
  case Semantic::Kind::Arbitrary: return DxilProgramSigSemantic::Undefined;
  case Semantic::Kind::VertexID: return DxilProgramSigSemantic::VertexID;
  case Semantic::Kind::InstanceID: return DxilProgramSigSemantic::InstanceID;
  case Semantic::Kind::Position: return DxilProgramSigSemantic::Position;
  case Semantic::Kind::Coverage: return DxilProgramSigSemantic::Coverage;
  case Semantic::Kind::InnerCoverage: return DxilProgramSigSemantic::InnerCoverage;
  case Semantic::Kind::PrimitiveID: return DxilProgramSigSemantic::PrimitiveID;
  case Semantic::Kind::SampleIndex: return DxilProgramSigSemantic::SampleIndex;
  case Semantic::Kind::IsFrontFace: return DxilProgramSigSemantic::IsFrontFace;
  case Semantic::Kind::RenderTargetArrayIndex: return DxilProgramSigSemantic::RenderTargetArrayIndex;
  case Semantic::Kind::ViewPortArrayIndex: return DxilProgramSigSemantic::ViewPortArrayIndex;
  case Semantic::Kind::ClipDistance: return DxilProgramSigSemantic::ClipDistance;
  case Semantic::Kind::CullDistance: return DxilProgramSigSemantic::CullDistance;
  case Semantic::Kind::TessFactor: {
    switch (domain) {
    case DXIL::TessellatorDomain::IsoLine:
      // Will bu updated to DetailTessFactor in next row.
      return DxilProgramSigSemantic::FinalLineDensityTessfactor;
    case DXIL::TessellatorDomain::Tri:
      return DxilProgramSigSemantic::FinalTriEdgeTessfactor;
    case DXIL::TessellatorDomain::Quad:
      return DxilProgramSigSemantic::FinalQuadEdgeTessfactor;
    }
  }
  case Semantic::Kind::InsideTessFactor: {
    switch (domain) {
    case DXIL::TessellatorDomain::IsoLine:
      DXASSERT(0, "invalid semantic");
      return DxilProgramSigSemantic::Undefined;
    case DXIL::TessellatorDomain::Tri:
      return DxilProgramSigSemantic::FinalTriInsideTessfactor;
    case DXIL::TessellatorDomain::Quad:
      return DxilProgramSigSemantic::FinalQuadInsideTessfactor;
    }
  }
  case Semantic::Kind::Invalid:
    return DxilProgramSigSemantic::Undefined;
  case Semantic::Kind::Target: return DxilProgramSigSemantic::Target;
  case Semantic::Kind::Depth: return DxilProgramSigSemantic::Depth;
  case Semantic::Kind::DepthLessEqual: return DxilProgramSigSemantic::DepthLE;
  case Semantic::Kind::DepthGreaterEqual: return DxilProgramSigSemantic::DepthGE;
  case Semantic::Kind::StencilRef:
    __fallthrough;
  default:
    DXASSERT(kind == Semantic::Kind::StencilRef, "else Invalid or switch is missing a case");
    return DxilProgramSigSemantic::StencilRef;
  }
  // TODO: Final_* values need mappings
}

static DxilProgramSigCompType CompTypeToSigCompType(hlsl::CompType value) {
  switch (value.GetKind()) {
  case CompType::Kind::I32: return DxilProgramSigCompType::SInt32;
  case CompType::Kind::U32: return DxilProgramSigCompType::UInt32;
  case CompType::Kind::F32: return DxilProgramSigCompType::Float32;
  case CompType::Kind::I16: return DxilProgramSigCompType::SInt16;
  case CompType::Kind::I64: return DxilProgramSigCompType::SInt64;
  case CompType::Kind::U16: return DxilProgramSigCompType::UInt16;
  case CompType::Kind::U64: return DxilProgramSigCompType::UInt64;
  case CompType::Kind::F16: return DxilProgramSigCompType::Float16;
  case CompType::Kind::F64: return DxilProgramSigCompType::Float64;
  case CompType::Kind::Invalid: __fallthrough;
  case CompType::Kind::I1: __fallthrough;
  default:
    return DxilProgramSigCompType::Unknown;
  }
}

static DxilProgramSigMinPrecision CompTypeToSigMinPrecision(hlsl::CompType value) {
  switch (value.GetKind()) {
  case CompType::Kind::I32: return DxilProgramSigMinPrecision::Default;
  case CompType::Kind::U32: return DxilProgramSigMinPrecision::Default;
  case CompType::Kind::F32: return DxilProgramSigMinPrecision::Default;
  case CompType::Kind::I1: return DxilProgramSigMinPrecision::Default;
  case CompType::Kind::U64: __fallthrough;
  case CompType::Kind::I64: __fallthrough;
  case CompType::Kind::F64: return DxilProgramSigMinPrecision::Default;
  case CompType::Kind::I16: return DxilProgramSigMinPrecision::SInt16;
  case CompType::Kind::U16: return DxilProgramSigMinPrecision::UInt16;
  case CompType::Kind::F16: return DxilProgramSigMinPrecision::Float16; // Float2_8 is not supported in DXIL.
  case CompType::Kind::Invalid: __fallthrough;
  default:
    return DxilProgramSigMinPrecision::Default;
  }
}

template <typename T>
struct sort_second {
  bool operator()(const T &a, const T &b) {
    return std::less<decltype(a.second)>()(a.second, b.second);
  }
};

struct sort_sig {
  bool operator()(const DxilProgramSignatureElement &a,
                  const DxilProgramSignatureElement &b) {
    return (a.Stream < b.Stream) |
           ((a.Stream == b.Stream) & (a.Register < b.Register));
  }
};

class DxilProgramSignatureWriter {
private:
  const DxilSignature &m_signature;
  DXIL::TessellatorDomain m_domain;
  bool   m_isInput;
  size_t m_fixedSize;
  typedef std::pair<const char *, uint32_t> NameOffsetPair;
  typedef llvm::SmallMapVector<const char *, uint32_t, 8> NameOffsetMap;
  uint32_t m_lastOffset;
  NameOffsetMap m_semanticNameOffsets;
  unsigned m_paramCount;

  const char *GetSemanticName(const hlsl::DxilSignatureElement *pElement) {
    DXASSERT_NOMSG(pElement != nullptr);
    DXASSERT(pElement->GetName() != nullptr, "else sig is malformed");
    return pElement->GetName();
  }

  uint32_t GetSemanticOffset(const hlsl::DxilSignatureElement *pElement) {
    const char *pName = GetSemanticName(pElement);
    NameOffsetMap::iterator nameOffset = m_semanticNameOffsets.find(pName);
    uint32_t result;
    if (nameOffset == m_semanticNameOffsets.end()) {
      result = m_lastOffset;
      m_semanticNameOffsets.insert(NameOffsetPair(pName, result));
      m_lastOffset += strlen(pName) + 1;
    }
    else {
      result = nameOffset->second;
    }
    return result;
  }

  void write(std::vector<DxilProgramSignatureElement> &orderedSig,
             const hlsl::DxilSignatureElement *pElement) {
    const std::vector<unsigned> &indexVec = pElement->GetSemanticIndexVec();
    unsigned eltCount = pElement->GetSemanticIndexVec().size();
    unsigned eltRows = 0;
    if (eltCount)
      eltRows = pElement->GetRows() / eltCount;

    DxilProgramSignatureElement sig;
    sig.Stream = pElement->GetOutputStream();
    sig.SemanticName = GetSemanticOffset(pElement);
    sig.SystemValue = KindToSystemValue(pElement->GetKind(), m_domain);
    sig.CompType = CompTypeToSigCompType(pElement->GetCompType());
    sig.Register = pElement->GetStartRow();

    sig.Mask = pElement->GetColsAsMask();
    // Only mark exist channel write for output.
    // All channel not used for input.
    if (!m_isInput)
      sig.NeverWrites_Mask = ~(sig.Mask);
    else
      sig.AlwaysReads_Mask = 0;

    sig.MinPrecision = CompTypeToSigMinPrecision(pElement->GetCompType());

    for (unsigned i = 0; i < eltCount; ++i) {
      sig.SemanticIndex = indexVec[i];
      orderedSig.emplace_back(sig);
      if (pElement->IsAllocated())
        sig.Register += eltRows;
      if (sig.SystemValue == DxilProgramSigSemantic::FinalLineDensityTessfactor)
        sig.SystemValue = DxilProgramSigSemantic::FinalLineDetailTessfactor;
    }
  }

  void calcSizes() {
    // Calculate size for signature elements.
    const std::vector<std::unique_ptr<hlsl::DxilSignatureElement>> &elements = m_signature.GetElements();
    uint32_t result = sizeof(DxilProgramSignature);
    m_paramCount = 0;
    for (size_t i = 0; i < elements.size(); ++i) {
      DXIL::SemanticInterpretationKind I = elements[i]->GetInterpretation();
      if (I == DXIL::SemanticInterpretationKind::NA || I == DXIL::SemanticInterpretationKind::NotInSig)
        continue;
      unsigned semanticCount = elements[i]->GetSemanticIndexVec().size();
      result += semanticCount * sizeof(DxilProgramSignatureElement);
      m_paramCount += semanticCount;
    }
    m_fixedSize = result;
    m_lastOffset = m_fixedSize;

    // Calculate size for semantic strings.
    for (size_t i = 0; i < elements.size(); ++i) {
      GetSemanticOffset(elements[i].get());
    }
  }

public:
  DxilProgramSignatureWriter(const DxilSignature &signature,
                             DXIL::TessellatorDomain domain, bool isInput)
      : m_signature(signature), m_domain(domain), m_isInput(isInput) {
    calcSizes();
  }

  uint32_t size() const {
    return m_lastOffset;
  }

  void write(AbstractMemoryStream *pStream) {
    UINT64 startPos = pStream->GetPosition();
    const std::vector<std::unique_ptr<hlsl::DxilSignatureElement>> &elements = m_signature.GetElements();

    DxilProgramSignature programSig;
    programSig.ParamCount = m_paramCount;
    programSig.ParamOffset = sizeof(DxilProgramSignature);
    IFT(WriteStreamValue(pStream, programSig));

    // Write structures in register order.
    std::vector<DxilProgramSignatureElement> orderedSig;
    for (size_t i = 0; i < elements.size(); ++i) {
      DXIL::SemanticInterpretationKind I = elements[i]->GetInterpretation();
      if (I == DXIL::SemanticInterpretationKind::NA || I == DXIL::SemanticInterpretationKind::NotInSig)
        continue;
      write(orderedSig, elements[i].get());
    }
    std::sort(orderedSig.begin(), orderedSig.end(), sort_sig());
    for (size_t i = 0; i < orderedSig.size(); ++i) {
      DxilProgramSignatureElement &sigElt = orderedSig[i];
      IFT(WriteStreamValue(pStream, sigElt));
    }

    // Write strings in the offset order.
    std::vector<NameOffsetPair> ordered;
    ordered.assign(m_semanticNameOffsets.begin(), m_semanticNameOffsets.end());
    std::sort(ordered.begin(), ordered.end(), sort_second<NameOffsetPair>());
    for (size_t i = 0; i < ordered.size(); ++i) {
      const char *pName = ordered[i].first;
      ULONG cbWritten;
      UINT64 offsetPos = pStream->GetPosition();
      DXASSERT_LOCALVAR(offsetPos, offsetPos - startPos == ordered[i].second, "else str offset is incorrect");
      IFT(pStream->Write(pName, strlen(pName) + 1, &cbWritten));
    }

    // Verify we wrote the bytes we though we would.
    UINT64 endPos = pStream->GetPosition();
    DXASSERT_LOCALVAR(endPos - startPos, endPos - startPos == size(), "else size is incorrect");
  }
};

class DxilProgramRootSignatureWriter {
private:
  const RootSignatureHandle &m_Sig;
public:
  DxilProgramRootSignatureWriter(const RootSignatureHandle &S) : m_Sig(S) {}
  uint32_t size() const {
    return m_Sig.GetSerializedSize();
  }
  void write(AbstractMemoryStream *pStream) {
    ULONG cbWritten;
    IFT(pStream->Write(m_Sig.GetSerializedBytes(), size(), &cbWritten));
  }
};

class DxilFeatureInfoWriter {
private:
  // Only save the shader properties after create class for it.
  DxilShaderFeatureInfo featureInfo;
public:
  DxilFeatureInfoWriter(const DxilModule &M) {
    featureInfo.FeatureFlags = M.m_ShaderFlags.GetFeatureInfo();
  }
  uint32_t size() const {
    return sizeof(DxilShaderFeatureInfo);
  }
  void write(AbstractMemoryStream *pStream) {
    IFT(WriteStreamValue(pStream, featureInfo.FeatureFlags));
  }
};

class DxilPSVWriter {
private:
  DxilModule &m_Module;
  UINT m_uTotalResources;
  DxilPipelineStateValidation m_PSV;
  uint32_t m_PSVBufferSize;
  SmallVector<char, 512> m_PSVBuffer;

public:
  DxilPSVWriter(DxilModule &module) : m_Module(module) {
    UINT uCBuffers = m_Module.GetCBuffers().size();
    UINT uSamplers = m_Module.GetSamplers().size();
    UINT uSRVs = m_Module.GetSRVs().size();
    UINT uUAVs = m_Module.GetUAVs().size();
    m_uTotalResources = uCBuffers + uSamplers + uSRVs + uUAVs;
    m_PSV.InitNew(m_uTotalResources, nullptr, &m_PSVBufferSize);
  }
  size_t size() {
    return m_PSVBufferSize;
  }

  void write(AbstractMemoryStream *pStream) {
    m_PSVBuffer.resize(m_PSVBufferSize);
    m_PSV.InitNew(m_uTotalResources, m_PSVBuffer.data(), &m_PSVBufferSize);
    DXASSERT_NOMSG(m_PSVBuffer.size() == m_PSVBufferSize);

    // Set DxilRuntimInfo
    PSVRuntimeInfo0* pInfo = m_PSV.GetPSVRuntimeInfo0();
    const ShaderModel* SM = m_Module.GetShaderModel();
    pInfo->MinimumExpectedWaveLaneCount = 0;
    pInfo->MaximumExpectedWaveLaneCount = (UINT)-1;

    switch (SM->GetKind()) {
      case ShaderModel::Kind::Vertex: {
        pInfo->VS.OutputPositionPresent = 0;
        DxilSignature &S = m_Module.GetOutputSignature();
        for (auto &&E : S.GetElements()) {
          if (E->GetKind() == Semantic::Kind::Position) {
            // Ideally, we might check never writes mask here,
            // but this is not yet part of the signature element in Dxil
            pInfo->VS.OutputPositionPresent = 1;
            break;
          }
        }
        break;
      }
      case ShaderModel::Kind::Hull: {
        pInfo->HS.InputControlPointCount = (UINT)m_Module.GetInputControlPointCount();
        pInfo->HS.OutputControlPointCount = (UINT)m_Module.GetOutputControlPointCount();
        pInfo->HS.TessellatorDomain = (UINT)m_Module.GetTessellatorDomain();
        pInfo->HS.TessellatorOutputPrimitive = (UINT)m_Module.GetTessellatorOutputPrimitive();
        break;
      }
      case ShaderModel::Kind::Domain: {
        pInfo->DS.InputControlPointCount = (UINT)m_Module.GetInputControlPointCount();
        pInfo->DS.OutputPositionPresent = 0;
        DxilSignature &S = m_Module.GetOutputSignature();
        for (auto &&E : S.GetElements()) {
          if (E->GetKind() == Semantic::Kind::Position) {
            // Ideally, we might check never writes mask here,
            // but this is not yet part of the signature element in Dxil
            pInfo->DS.OutputPositionPresent = 1;
            break;
          }
        }
        pInfo->DS.TessellatorDomain = (UINT)m_Module.GetTessellatorDomain();
        break;
      }
      case ShaderModel::Kind::Geometry: {
        pInfo->GS.InputPrimitive = (UINT)m_Module.GetInputPrimitive();
        // NOTE: For OutputTopology, pick one from a used stream, or if none
        // are used, use stream 0, and set OutputStreamMask to 1.
        pInfo->GS.OutputTopology = (UINT)m_Module.GetStreamPrimitiveTopology();
        pInfo->GS.OutputStreamMask = m_Module.GetActiveStreamMask();
        if (pInfo->GS.OutputStreamMask == 0) {
          pInfo->GS.OutputStreamMask = 1; // This is what runtime expects.
        }
        pInfo->GS.OutputPositionPresent = 0;
        DxilSignature &S = m_Module.GetOutputSignature();
        for (auto &&E : S.GetElements()) {
          if (E->GetKind() == Semantic::Kind::Position) {
            // Ideally, we might check never writes mask here,
            // but this is not yet part of the signature element in Dxil
            pInfo->GS.OutputPositionPresent = 1;
            break;
          }
        }
        break;
      }
      case ShaderModel::Kind::Pixel: {
        pInfo->PS.DepthOutput = 0;
        pInfo->PS.SampleFrequency = 0;
        {
          DxilSignature &S = m_Module.GetInputSignature();
          for (auto &&E : S.GetElements()) {
            if (E->GetInterpolationMode()->IsAnySample() ||
                E->GetKind() == Semantic::Kind::SampleIndex) {
              pInfo->PS.SampleFrequency = 1;
            }
          }
        }
        {
          DxilSignature &S = m_Module.GetOutputSignature();
          for (auto &&E : S.GetElements()) {
            if (E->IsAnyDepth()) {
              pInfo->PS.DepthOutput = 1;
              break;
            }
          }
        }
        break;
      }
    }

    // Set resource binding information
    UINT uResIndex = 0;
    for (auto &&R : m_Module.GetCBuffers()) {
      DXASSERT_NOMSG(uResIndex < m_uTotalResources);
      PSVResourceBindInfo0* pBindInfo = m_PSV.GetPSVResourceBindInfo0(uResIndex);
      DXASSERT_NOMSG(pBindInfo);
      pBindInfo->ResType = (UINT)PSVResourceType::CBV;
      pBindInfo->Space = R->GetSpaceID();
      pBindInfo->LowerBound = R->GetLowerBound();
      pBindInfo->UpperBound = R->GetUpperBound();
      uResIndex++;
    }
    for (auto &&R : m_Module.GetSamplers()) {
      DXASSERT_NOMSG(uResIndex < m_uTotalResources);
      PSVResourceBindInfo0* pBindInfo = m_PSV.GetPSVResourceBindInfo0(uResIndex);
      DXASSERT_NOMSG(pBindInfo);
      pBindInfo->ResType = (UINT)PSVResourceType::Sampler;
      pBindInfo->Space = R->GetSpaceID();
      pBindInfo->LowerBound = R->GetLowerBound();
      pBindInfo->UpperBound = R->GetUpperBound();
      uResIndex++;
    }
    for (auto &&R : m_Module.GetSRVs()) {
      DXASSERT_NOMSG(uResIndex < m_uTotalResources);
      PSVResourceBindInfo0* pBindInfo = m_PSV.GetPSVResourceBindInfo0(uResIndex);
      DXASSERT_NOMSG(pBindInfo);
      if (R->IsStructuredBuffer()) {
        pBindInfo->ResType = (UINT)PSVResourceType::SRVStructured;
      } else if (R->IsRawBuffer()) {
        pBindInfo->ResType = (UINT)PSVResourceType::SRVRaw;
      } else {
        pBindInfo->ResType = (UINT)PSVResourceType::SRVTyped;
      }
      pBindInfo->Space = R->GetSpaceID();
      pBindInfo->LowerBound = R->GetLowerBound();
      pBindInfo->UpperBound = R->GetUpperBound();
      uResIndex++;
    }
    for (auto &&R : m_Module.GetUAVs()) {
      DXASSERT_NOMSG(uResIndex < m_uTotalResources);
      PSVResourceBindInfo0* pBindInfo = m_PSV.GetPSVResourceBindInfo0(uResIndex);
      DXASSERT_NOMSG(pBindInfo);
      if (R->IsStructuredBuffer()) {
        if (R->HasCounter())
          pBindInfo->ResType = (UINT)PSVResourceType::UAVStructuredWithCounter;
        else
          pBindInfo->ResType = (UINT)PSVResourceType::UAVStructured;
      } else if (R->IsRawBuffer()) {
        pBindInfo->ResType = (UINT)PSVResourceType::UAVRaw;
      } else {
        pBindInfo->ResType = (UINT)PSVResourceType::UAVTyped;
      }
      pBindInfo->Space = R->GetSpaceID();
      pBindInfo->LowerBound = R->GetLowerBound();
      pBindInfo->UpperBound = R->GetUpperBound();
      uResIndex++;
    }
    DXASSERT_NOMSG(uResIndex == m_uTotalResources);

    ULONG cbWritten;
    IFT(pStream->Write(m_PSVBuffer.data(), m_PSVBufferSize, &cbWritten));
    DXASSERT_NOMSG(cbWritten == m_PSVBufferSize);
  }
};

class DxilContainerWriter {
public:
  typedef std::function<void(AbstractMemoryStream*)> WriteFn;

private:
  class DxilPart {
  public:
    DxilPartHeader Header;
    WriteFn Write;
    DxilPart(uint32_t fourCC, uint32_t size, WriteFn write) : Write(write) {
      Header.PartFourCC = fourCC;
      Header.PartSize = size;
    }
  };

  llvm::SmallVector<DxilPart, 8> m_Parts;

public:
  void AddPart(uint32_t FourCC, uint32_t Size, WriteFn Write) {
    m_Parts.emplace_back(FourCC, Size, Write);
  }

  void write(AbstractMemoryStream *pStream) {
    DxilContainerHeader header;
    const uint32_t PartCount = (uint32_t)m_Parts.size();
    const uint32_t OffsetTableSize = sizeof(uint32_t) * PartCount;
    uint32_t containerSizeInBytes =
      (uint32_t)sizeof(DxilContainerHeader) + OffsetTableSize +
      (uint32_t)sizeof(DxilPartHeader) * PartCount;
    for (auto &&part : m_Parts) {
      containerSizeInBytes += part.Header.PartSize;
    }
    InitDxilContainer(&header, PartCount, containerSizeInBytes);
    IFT(pStream->Reserve(header.ContainerSizeInBytes));
    IFT(WriteStreamValue(pStream, header));
    uint32_t offset = sizeof(header) + OffsetTableSize;
    for (auto &&part : m_Parts) {
      IFT(WriteStreamValue(pStream, offset));
      offset += sizeof(DxilPartHeader) + part.Header.PartSize;
    }
    for (auto &&part : m_Parts) {
      IFT(WriteStreamValue(pStream, part.Header));
      size_t start = pStream->GetPosition();
      part.Write(pStream);
      DXASSERT_LOCALVAR(start, pStream->GetPosition() - start == (size_t)part.Header.PartSize, "out of bound");
    }
    DXASSERT(containerSizeInBytes == (uint32_t)pStream->GetPosition(), "else stream size is incorrect");
  }
};

static bool HasDebugInfo(const Module &M) {
  for (Module::const_named_metadata_iterator NMI = M.named_metadata_begin(),
                                             NME = M.named_metadata_end();
       NMI != NME; ++NMI) {
    if (NMI->getName().startswith("llvm.dbg.")) {
      return true;
    }
  }
  return false;
}

static void GetPaddedProgramPartSize(AbstractMemoryStream *pStream,
                                     uint32_t &bitcodeInUInt32,
                                     uint32_t &bitcodePaddingBytes) {
  bitcodeInUInt32 = pStream->GetPtrSize();
  bitcodePaddingBytes = (bitcodeInUInt32 % 4);
  bitcodeInUInt32 = (bitcodeInUInt32 / 4) + (bitcodePaddingBytes ? 1 : 0);
}

static void WriteProgramPart(const ShaderModel *pModel,
                             AbstractMemoryStream *pModuleBitcode,
                             AbstractMemoryStream *pStream) {
  DXASSERT(pModel != nullptr, "else generation should have failed");
  DxilProgramHeader programHeader;
  uint32_t ver =
      EncodeVersion(pModel->GetKind(), pModel->GetMajor(), pModel->GetMinor());
  InitProgramHeader(programHeader, ver, pModuleBitcode->GetPtrSize());

  uint32_t programInUInt32, programPaddingBytes;
  GetPaddedProgramPartSize(pModuleBitcode, programInUInt32,
                           programPaddingBytes);

  ULONG cbWritten;
  IFT(WriteStreamValue(pStream, programHeader));
  IFT(pStream->Write(pModuleBitcode->GetPtr(), pModuleBitcode->GetPtrSize(),
                     &cbWritten));
  if (programPaddingBytes) {
    uint32_t paddingValue = 0;
    IFT(pStream->Write(&paddingValue, programPaddingBytes, &cbWritten));
  }
}

void hlsl::SerializeDxilContainerForModule(Module *pModule,
                                           AbstractMemoryStream *pModuleBitcode,
                                           AbstractMemoryStream *pFinalStream) {
  // TODO: add a flag to update the module and remove information that is not part
  // of DXIL proper and is used only to assemble the container.

  DXASSERT_NOMSG(pModule != nullptr);
  DXASSERT_NOMSG(pModuleBitcode != nullptr);
  DXASSERT_NOMSG(pFinalStream != nullptr);

  CComPtr<AbstractMemoryStream> pProgramStream;
  DxilModule dxilModule(pModule);
  dxilModule.LoadDxilMetadata();

  DxilProgramSignatureWriter inputSigWriter(dxilModule.GetInputSignature(),
                                            dxilModule.GetTessellatorDomain(),
                                            /*IsInput*/ true);
  DxilProgramSignatureWriter outputSigWriter(dxilModule.GetOutputSignature(),
                                             dxilModule.GetTessellatorDomain(),
                                             /*IsInput*/ false);
  DxilPSVWriter PSVWriter(dxilModule);
  DxilContainerWriter writer;

  // Write the feature part.
  DxilFeatureInfoWriter featureInfoWriter(dxilModule);
  writer.AddPart(DFCC_FeatureInfo, featureInfoWriter.size(), [&](AbstractMemoryStream *pStream) {
    featureInfoWriter.write(pStream);
  });

  // Write the input and output signature parts.
  writer.AddPart(DFCC_InputSignature, inputSigWriter.size(), [&](AbstractMemoryStream *pStream) {
    inputSigWriter.write(pStream);
  });
  writer.AddPart(DFCC_OutputSignature, outputSigWriter.size(), [&](AbstractMemoryStream *pStream) {
    outputSigWriter.write(pStream);
  });

  DxilProgramSignatureWriter patchConstantSigWriter(
      dxilModule.GetPatchConstantSignature(), dxilModule.GetTessellatorDomain(),
      /*IsInput*/ dxilModule.GetShaderModel()->IsDS());

  if (dxilModule.GetPatchConstantSignature().GetElements().size()) {
    writer.AddPart(DFCC_PatchConstantSignature, patchConstantSigWriter.size(),
                   [&](AbstractMemoryStream *pStream) {
                     patchConstantSigWriter.write(pStream);
                   });
  }

  // Write the DxilPipelineStateValidation (PSV0) part.
  writer.AddPart(DFCC_PipelineStateValidation, PSVWriter.size(), [&](AbstractMemoryStream *pStream) {
    PSVWriter.write(pStream);
  });

  // Write the root signature (RTS0) part.
  DxilProgramRootSignatureWriter rootSigWriter(dxilModule.GetRootSignature());
  if (!dxilModule.GetRootSignature().IsEmpty()) {
    writer.AddPart(
        DFCC_RootSignature, rootSigWriter.size(),
        [&](AbstractMemoryStream *pStream) { rootSigWriter.write(pStream); });
  }

  // If we have debug information present, serialize it to a debug part, then use the stripped version as the canonical program version.
  pProgramStream = pModuleBitcode;
  if (HasDebugInfo(*pModule)) {
    uint32_t debugInUInt32, debugPaddingBytes;
    GetPaddedProgramPartSize(pModuleBitcode, debugInUInt32, debugPaddingBytes);
    writer.AddPart(DFCC_ShaderDebugInfoDXIL, debugInUInt32 * sizeof(uint32_t) + sizeof(DxilProgramHeader), [&](AbstractMemoryStream *pStream) {
      WriteProgramPart(dxilModule.GetShaderModel(), pModuleBitcode, pStream);
    });

    pProgramStream.Release();

    llvm::StripDebugInfo(*pModule);
    dxilModule.StripDebugRelatedCode();

    CComPtr<IMalloc> pMalloc;
    IFT(CoGetMalloc(1, &pMalloc));
    IFT(CreateMemoryStream(pMalloc, &pProgramStream));
    raw_stream_ostream outStream(pProgramStream.p);
    WriteBitcodeToFile(pModule, outStream, true);
  }

  // Compute padded bitcode size.
  uint32_t programInUInt32, programPaddingBytes;
  GetPaddedProgramPartSize(pProgramStream, programInUInt32, programPaddingBytes);

  // Write the program part.
  writer.AddPart(DFCC_DXIL, programInUInt32 * sizeof(uint32_t) + sizeof(DxilProgramHeader), [&](AbstractMemoryStream *pStream) {
    WriteProgramPart(dxilModule.GetShaderModel(), pProgramStream, pStream);
  });

  writer.write(pFinalStream);
}
