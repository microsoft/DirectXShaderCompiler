///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilRootSignature.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for manipulating root signature structures.              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/HLSL/DxilRootSignature.h"
#include "dxc/HLSL/DxilPipelineStateValidation.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/WinFunctions.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/dxcapi.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DiagnosticPrinter.h"

#include <string>
#include <algorithm>
#include <utility>
#include <vector>
#include <set>

using namespace llvm;
using std::string;

namespace hlsl {

DEFINE_ENUM_FLAG_OPERATORS(DxilRootSignatureFlags)
DEFINE_ENUM_FLAG_OPERATORS(DxilRootDescriptorFlags)
DEFINE_ENUM_FLAG_OPERATORS(DxilDescriptorRangeType)
DEFINE_ENUM_FLAG_OPERATORS(DxilDescriptorRangeFlags)

// Execute (error) and throw.
#define EAT(x) { (x); throw ::hlsl::Exception(E_FAIL); }

//////////////////////////////////////////////////////////////////////////////
// Root signature handler.

RootSignatureHandle::RootSignatureHandle(RootSignatureHandle&& other) {
  m_pDesc = nullptr;
  m_pSerialized = nullptr;
  std::swap(m_pDesc, other.m_pDesc);
  std::swap(m_pSerialized, other.m_pSerialized);
}

void RootSignatureHandle::Assign(const DxilVersionedRootSignatureDesc *pDesc,
                                 IDxcBlob *pSerialized) {
  Clear();
  m_pDesc = pDesc;
  m_pSerialized = pSerialized;
  if (m_pSerialized)
    m_pSerialized->AddRef();
}

void RootSignatureHandle::Clear() {
  hlsl::DeleteRootSignature(m_pDesc);
  m_pDesc = nullptr;
  if (m_pSerialized != nullptr) {
    m_pSerialized->Release();
    m_pSerialized = nullptr;
  }
}

const uint8_t *RootSignatureHandle::GetSerializedBytes() const {
  DXASSERT_NOMSG(m_pSerialized != nullptr);
  return (uint8_t *)m_pSerialized->GetBufferPointer();
}

unsigned RootSignatureHandle::GetSerializedSize() const {
  DXASSERT_NOMSG(m_pSerialized != nullptr);
  return m_pSerialized->GetBufferSize();
}

void RootSignatureHandle::EnsureSerializedAvailable() {
  DXASSERT_NOMSG(!IsEmpty());
  if (m_pSerialized == nullptr) {
    CComPtr<IDxcBlob> pResult;
    hlsl::SerializeRootSignature(m_pDesc, &pResult, nullptr, false);
    IFTBOOL(pResult != nullptr, E_FAIL);
    m_pSerialized = pResult.Detach();
  }
}

void RootSignatureHandle::Deserialize() {
  DXASSERT_NOMSG(m_pSerialized && !m_pDesc);
  DeserializeRootSignature((uint8_t*)m_pSerialized->GetBufferPointer(), (uint32_t)m_pSerialized->GetBufferSize(), &m_pDesc);
}

void RootSignatureHandle::LoadSerialized(const uint8_t *pData,
                                         unsigned length) {
  DXASSERT_NOMSG(IsEmpty());
  IDxcBlobEncoding *pCreated;
  IFT(DxcCreateBlobWithEncodingOnHeapCopy(pData, length, CP_UTF8, &pCreated));
  m_pSerialized = pCreated;
}

//////////////////////////////////////////////////////////////////////////////
// Simple serializer.

class SimpleSerializer {
  struct Segment {
    void *pData;
    unsigned cbSize;
    bool bOwner;
    unsigned Offset;
    Segment *pNext;
  };

public:
  SimpleSerializer();
  ~SimpleSerializer();

  HRESULT AddBlock(void *pData, unsigned cbSize, unsigned *pOffset);
  HRESULT ReserveBlock(void **ppData, unsigned cbSize, unsigned *pOffset);

  HRESULT Compact(_Out_writes_bytes_(cbSize) char *pData, unsigned cbSize);
  unsigned GetSize();

protected:
  unsigned m_cbSegments;
  Segment *m_pSegment;
  Segment **m_ppSegment;
};

SimpleSerializer::SimpleSerializer() {
  m_cbSegments = 0;
  m_pSegment = nullptr;
  m_ppSegment = &m_pSegment;
}

SimpleSerializer::~SimpleSerializer() {
  while (m_pSegment) {
    Segment *pSegment = m_pSegment;
    m_pSegment = pSegment->pNext;

    if (pSegment->bOwner) {
      delete[] (char*)pSegment->pData;
    }

    delete pSegment;
  }
}

HRESULT SimpleSerializer::AddBlock(void *pData, unsigned cbSize,
                                   unsigned *pOffset) {
  Segment *pSegment = nullptr;
  IFRBOOL(!(cbSize != 0 && pData == nullptr), E_FAIL);

  IFROOM(pSegment = new (std::nothrow) Segment);
  pSegment->pData = pData;

  m_cbSegments = (m_cbSegments + 3) & ~3;
  pSegment->Offset = m_cbSegments;
  pSegment->cbSize = cbSize;
  pSegment->bOwner = false;
  pSegment->pNext = nullptr;

  m_cbSegments += pSegment->cbSize;
  *m_ppSegment = pSegment;
  m_ppSegment = &pSegment->pNext;

  if (pOffset != nullptr) {
    *pOffset = pSegment->Offset;
  }

  return S_OK;
}

HRESULT SimpleSerializer::ReserveBlock(void **ppData, unsigned cbSize,
                                       unsigned *pOffset) {
  HRESULT hr = S_OK;
  Segment *pSegment = nullptr;
  void *pClonedData = nullptr;

  IFCOOM(pSegment = new (std::nothrow) Segment);
  pSegment->pData = nullptr;

  IFCOOM(pClonedData = new (std::nothrow) char[cbSize]);
  pSegment->pData = pClonedData;

  m_cbSegments = (m_cbSegments + 3) & ~3;
  pSegment->Offset = m_cbSegments;
  pSegment->cbSize = cbSize;
  pSegment->bOwner = true;
  pSegment->pNext = nullptr;

  m_cbSegments += pSegment->cbSize;
  *m_ppSegment = pSegment;
  m_ppSegment = &pSegment->pNext;

  *ppData = pClonedData;
  if (pOffset) {
    *pOffset = pSegment->Offset;
  }

Cleanup:
  if (FAILED(hr)) {
    delete[] (char*)pClonedData;
    delete pSegment;
  }
  return hr;
}

HRESULT SimpleSerializer::Compact(_Out_writes_bytes_(cbSize) char *pData,
                                  unsigned cbSize) {
  unsigned cb = GetSize();
  IFRBOOL(cb <= cbSize, E_FAIL);
  DXASSERT_NOMSG(cb <= UINT32_MAX / 2);

  char *p = (char *)pData;
  cb = 0;

  for (Segment *pSegment = m_pSegment; pSegment; pSegment = pSegment->pNext) {
    unsigned cbAlign = ((cb + 3) & ~3) - cb;

    _Analysis_assume_(p + cbAlign <= pData + cbSize);
    memset(p, 0xab, cbAlign);

    p += cbAlign;
    cb += cbAlign;

    _Analysis_assume_(p + pSegment->cbSize <= pData + cbSize);
    memcpy(p, pSegment->pData, pSegment->cbSize);

    p += pSegment->cbSize;
    cb += pSegment->cbSize;
  }

  // Trailing zeros
  _Analysis_assume_(p + cbSize - cb <= pData + cbSize);
  memset(p, 0xab, cbSize - cb);

  return S_OK;
}

unsigned SimpleSerializer::GetSize() {
  // Round up to 4==sizeof(unsigned).
  return ((m_cbSegments + 3) >> 2) * 4;
}

//////////////////////////////////////////////////////////////////////////////
// Interval helper.

template <typename T>
class CIntervalCollection {
private:
  std::set<T> m_set;
public:
  const T* FindIntersectingInterval(const T &I) {
    auto it = m_set.find(I);
    if (it != m_set.end())
      return &*it;
    return nullptr;
  }
  void Insert(const T& value) {
    auto result = m_set.insert(value);
    UNREFERENCED_PARAMETER(result);
#if DBG
    DXASSERT(result.second, "otherwise interval collides with existing in collection");
#endif
  }
};

//////////////////////////////////////////////////////////////////////////////
// Verifier classes.

class DescriptorTableVerifier {
public:
  void Verify(const DxilDescriptorRange1 *pRanges, unsigned NumRanges,
              unsigned iRTS, DiagnosticPrinter &DiagPrinter);
};

class StaticSamplerVerifier {
public:
  void Verify(const DxilStaticSamplerDesc *pDesc, DiagnosticPrinter &DiagPrinter);
};

class RootSignatureVerifier {
public:
  RootSignatureVerifier();
  ~RootSignatureVerifier();

  void AllowReservedRegisterSpace(bool bAllow);

  // Call this before calling VerifyShader, as it accumulates root signature state.
  void VerifyRootSignature(const DxilVersionedRootSignatureDesc *pRootSignature,
                           DiagnosticPrinter &DiagPrinter);

  void VerifyShader(DxilShaderVisibility VisType,
                    const void *pPSVData,
                    uint32_t PSVSize,
                    DiagnosticPrinter &DiagPrinter);

  typedef enum NODE_TYPE {
    DESCRIPTOR_TABLE_ENTRY,
    ROOT_DESCRIPTOR,
    ROOT_CONSTANT,
    STATIC_SAMPLER
  } NODE_TYPE;

private:
  static const unsigned kMinVisType = (unsigned)DxilShaderVisibility::All;
  static const unsigned kMaxVisType = (unsigned)DxilShaderVisibility::Pixel;
  static const unsigned kMinDescType = (unsigned)DxilDescriptorRangeType::SRV;
  static const unsigned kMaxDescType = (unsigned)DxilDescriptorRangeType::Sampler;

  struct RegisterRange {
    NODE_TYPE nt;
    unsigned space;
    unsigned lb;    // inclusive lower bound
    unsigned ub;    // inclusive upper bound
    unsigned iRP;
    unsigned iDTS;
    // Sort by space, then lower bound.
    bool operator<(const RegisterRange& other) const {
      return space < other.space ||
        (space == other.space && ub < other.lb);
    }
    // Like a regular -1,0,1 comparison, but 0 indicates overlap.
    int overlap(const RegisterRange& other) const {
      if (space < other.space) return -1;
      if (space > other.space) return 1;
      if (ub < other.lb) return -1;
      if (lb > other.ub) return 1;
      return 0;
    }
    // Check containment.
    bool contains(const RegisterRange& other) const {
      return (space == other.space) && (lb <= other.lb && other.ub <= ub);
    }
  };
  typedef CIntervalCollection<RegisterRange> RegisterRanges;

  void AddRegisterRange(unsigned iRTS, NODE_TYPE nt, unsigned iDTS,
                        DxilDescriptorRangeType DescType,
                        DxilShaderVisibility VisType,
                        unsigned NumRegisters, unsigned BaseRegister,
                        unsigned RegisterSpace, DiagnosticPrinter &DiagPrinter);

  const RegisterRange *FindCoveringInterval(DxilDescriptorRangeType RangeType,
                                            DxilShaderVisibility VisType,
                                            unsigned Num,
                                            unsigned LB,
                                            unsigned Space);

  RegisterRanges &
  GetRanges(DxilShaderVisibility VisType, DxilDescriptorRangeType DescType) {
    return RangeKinds[(unsigned)VisType][(unsigned)DescType];
  }

  RegisterRanges RangeKinds[kMaxVisType + 1][kMaxDescType + 1];
  bool m_bAllowReservedRegisterSpace;
  DxilRootSignatureFlags m_RootSignatureFlags;
};

void DescriptorTableVerifier::Verify(const DxilDescriptorRange1 *pRanges,
                                     uint32_t NumRanges, uint32_t iRP,
                                     DiagnosticPrinter &DiagPrinter) {
  bool bHasSamplers = false;
  bool bHasResources = false;

  uint64_t iAppendStartSlot = 0;
  for (unsigned iDTS = 0; iDTS < NumRanges; iDTS++) {
    const DxilDescriptorRange1 *pRange = &pRanges[iDTS];

    switch (pRange->RangeType) {
    case DxilDescriptorRangeType::SRV:
    case DxilDescriptorRangeType::UAV:
    case DxilDescriptorRangeType::CBV:
      bHasResources = true;
      break;
    case DxilDescriptorRangeType::Sampler:
      bHasSamplers = true;
      break;
    default:
      EAT(DiagPrinter << "Unsupported RangeType value " << (uint32_t)pRange->RangeType
                      << " (descriptor table slot [" << iDTS << "], root parameter [" << iRP << "]).\n");
    }

    // Samplers cannot be mixed with other resources.
    if (bHasResources && bHasSamplers) {
      EAT(DiagPrinter << "Samplers cannot be mixed with other "
                      << "resource types in a descriptor table (root "
                      << "parameter [" << iRP << "]).\n");
    }

    // NumDescriptors is not 0.
    if (pRange->NumDescriptors == 0) {
      EAT(DiagPrinter << "NumDescriptors cannot be 0 (descriptor "
                      << "table slot [" << iDTS << "], root parameter [" << iRP << "]).\n");
    }

    // Range start.
    uint64_t iStartSlot = iAppendStartSlot;
    if (pRange->OffsetInDescriptorsFromTableStart != DxilDescriptorRangeOffsetAppend) {
      iStartSlot = pRange->OffsetInDescriptorsFromTableStart;
    }
    if (iStartSlot > UINT_MAX) {
      EAT(DiagPrinter << "Cannot append range with implicit lower "
                      << "bound after an unbounded range (descriptor "
                      << "table slot [" << iDTS << "], root parameter [" << iRP << "]).\n");
    }

    // Descriptor range and shader register range overlow.
    if (pRange->NumDescriptors != UINT_MAX) {
      // Bounded range.
      uint64_t ub1 = (uint64_t)pRange->BaseShaderRegister +
                     (uint64_t)pRange->NumDescriptors - 1ull;
      if (ub1 > UINT_MAX) {
        EAT(DiagPrinter << "Overflow for shader register range: "
                        << "BaseShaderRegister=" << pRange->BaseShaderRegister
                        << ", NumDescriptor=" << pRange->NumDescriptors
                        << "; (descriptor table slot [" << iDTS
                        << "], root parameter [" << iRP << "]).\n");
      }

      uint64_t ub2 = (uint64_t)iStartSlot + (uint64_t)pRange->NumDescriptors - 1ull;
      if (ub2 > UINT_MAX) {
        EAT(DiagPrinter << "Overflow for descriptor range (descriptor "
                        << "table slot [" << iDTS << "], root parameter [" << iRP << "])\n");
      }

      iAppendStartSlot = iStartSlot + (uint64_t)pRange->NumDescriptors;
    } else {
      // Unbounded range.
      iAppendStartSlot = 1ull + (uint64_t)UINT_MAX;
    }
  }
}

RootSignatureVerifier::RootSignatureVerifier() {
  m_RootSignatureFlags = DxilRootSignatureFlags::None;
  m_bAllowReservedRegisterSpace = false;
}

RootSignatureVerifier::~RootSignatureVerifier() {}

void RootSignatureVerifier::AllowReservedRegisterSpace(bool bAllow) {
  m_bAllowReservedRegisterSpace = bAllow;
}

const char* RangeTypeString(DxilDescriptorRangeType rt)
{
  static const char *RangeType[] = {"SRV", "UAV", "CBV", "SAMPLER"};
  return (rt <= DxilDescriptorRangeType::Sampler) ? RangeType[(unsigned)rt]
                                                  : "unknown";
}

const char *VisTypeString(DxilShaderVisibility vis) {
  static const char *Vis[] = {"ALL",    "VERTEX",   "HULL",
                              "DOMAIN", "GEOMETRY", "PIXEL"};
  unsigned idx = (unsigned)vis;
  return vis <= DxilShaderVisibility::Pixel ? Vis[idx] : "unknown";
}

static bool IsDxilShaderVisibility(DxilShaderVisibility v) {
  return v <= DxilShaderVisibility::Pixel;
}

void RootSignatureVerifier::AddRegisterRange(unsigned iRP,
                                             NODE_TYPE nt,
                                             unsigned iDTS,
                                             DxilDescriptorRangeType DescType,
                                             DxilShaderVisibility VisType,
                                             unsigned NumRegisters,
                                             unsigned BaseRegister,
                                             unsigned RegisterSpace,
                                             DiagnosticPrinter &DiagPrinter) {
  RegisterRange interval;
  interval.space = RegisterSpace;
  interval.lb = BaseRegister;
  interval.ub = (NumRegisters != UINT_MAX) ? BaseRegister + NumRegisters - 1 : UINT_MAX;
  interval.nt = nt;
  interval.iDTS = iDTS;
  interval.iRP = iRP;

  if (!m_bAllowReservedRegisterSpace &&
       (RegisterSpace >= DxilSystemReservedRegisterSpaceValuesStart) &&
       (RegisterSpace <= DxilSystemReservedRegisterSpaceValuesEnd)) {
    if (nt == DESCRIPTOR_TABLE_ENTRY) {
      EAT(DiagPrinter << "Root parameter [" << iRP << "] descriptor table entry [" << iDTS
                      << "] specifies RegisterSpace=" << std::hex << RegisterSpace
                      << ", which is invalid since RegisterSpace values in the range "
                      << "[" << std::hex << DxilSystemReservedRegisterSpaceValuesStart
                      << "," << std::hex << DxilSystemReservedRegisterSpaceValuesEnd
                      << "] are reserved for system use.\n");
    }
    else {
      EAT(DiagPrinter << "Root parameter [" << iRP
                      << "] specifies RegisterSpace=" << std::hex << RegisterSpace
                      << ", which is invalid since RegisterSpace values in the range "
                      << "[" << std::hex << DxilSystemReservedRegisterSpaceValuesStart
                      << "," << std::hex << DxilSystemReservedRegisterSpaceValuesEnd
                      << "] are reserved for system use.\n");
    }
  }

  const RegisterRange *pNode = nullptr;
  DxilShaderVisibility NodeVis = VisType;
  if (VisType == DxilShaderVisibility::All) {
    // Check for overlap with each visibility type.
    for (unsigned iVT = kMinVisType; iVT <= kMaxVisType; iVT++) {
      pNode = GetRanges((DxilShaderVisibility)iVT, DescType).FindIntersectingInterval(interval);
      if (pNode != nullptr)
        break;
    }
  } else {
    // Check for overlap with the same visibility.
    pNode = GetRanges(VisType, DescType).FindIntersectingInterval(interval);

    // Check for overlap with ALL visibility.
    if (pNode == nullptr) {
      pNode = GetRanges(DxilShaderVisibility::All, DescType).FindIntersectingInterval(interval);
      NodeVis = DxilShaderVisibility::All;
    }
  }

  if (pNode != nullptr) {
    const int strSize = 132;
    char testString[strSize];
    char nodeString[strSize];
    switch (nt) {
    case DESCRIPTOR_TABLE_ENTRY:
      StringCchPrintfA(testString, strSize, "(root parameter [%u], visibility %s, descriptor table slot [%u])",
        iRP, VisTypeString(VisType), iDTS);
      break;
    case ROOT_DESCRIPTOR:
    case ROOT_CONSTANT:
      StringCchPrintfA(testString, strSize, "(root parameter [%u], visibility %s)",
        iRP, VisTypeString(VisType));
      break;
    case STATIC_SAMPLER:
      StringCchPrintfA(testString, strSize, "(static sampler [%u], visibility %s)",
        iRP, VisTypeString(VisType));
      break;
    default:
      DXASSERT_NOMSG(false);
      break;
    }

    switch (pNode->nt)
    {
    case DESCRIPTOR_TABLE_ENTRY:
      StringCchPrintfA(nodeString, strSize, "(root parameter[%u], visibility %s, descriptor table slot [%u])",
        pNode->iRP, VisTypeString(NodeVis), pNode->iDTS);
      break;
    case ROOT_DESCRIPTOR:
    case ROOT_CONSTANT:
      StringCchPrintfA(nodeString, strSize, "(root parameter [%u], visibility %s)",
        pNode->iRP, VisTypeString(NodeVis));
      break;
    case STATIC_SAMPLER:
      StringCchPrintfA(nodeString, strSize, "(static sampler [%u], visibility %s)",
        pNode->iRP, VisTypeString(NodeVis));
      break;
    default:
      DXASSERT_NOMSG(false);
      break;
    }
    EAT(DiagPrinter << "Shader register range of type " << RangeTypeString(DescType)
                    << " " << testString << " overlaps with another "
                    << "shader register range " << nodeString << ".\n");
  }

  // Insert node.
  GetRanges(VisType, DescType).Insert(interval);
}

const RootSignatureVerifier::RegisterRange *
RootSignatureVerifier::FindCoveringInterval(DxilDescriptorRangeType RangeType,
                                            DxilShaderVisibility VisType,
                                            unsigned Num,
                                            unsigned LB,
                                            unsigned Space) {
  RegisterRange RR;
  RR.space = Space;
  RR.lb = LB;
  RR.ub = LB + Num - 1;
  const RootSignatureVerifier::RegisterRange *pRange = GetRanges(DxilShaderVisibility::All, RangeType).FindIntersectingInterval(RR);
  if (!pRange && VisType != DxilShaderVisibility::All) {
    pRange = GetRanges(VisType, RangeType).FindIntersectingInterval(RR);
  }
  if (pRange && !pRange->contains(RR)) {
    pRange = nullptr;
  }
  return pRange;
}

static DxilDescriptorRangeType GetRangeType(DxilRootParameterType RPT) {
  switch (RPT) {
  case DxilRootParameterType::CBV: return DxilDescriptorRangeType::CBV;
  case DxilRootParameterType::SRV: return DxilDescriptorRangeType::SRV;
  case DxilRootParameterType::UAV: return DxilDescriptorRangeType::UAV;
  default:
    break;
  }

  DXASSERT_NOMSG(false);
  return DxilDescriptorRangeType::SRV;
}

void RootSignatureVerifier::VerifyRootSignature(
                                const DxilVersionedRootSignatureDesc *pVersionedRootSignature,
                                DiagnosticPrinter &DiagPrinter) {
  const DxilVersionedRootSignatureDesc *pUpconvertedRS = nullptr;

  // Up-convert root signature to the latest RS version.
  ConvertRootSignature(pVersionedRootSignature, DxilRootSignatureVersion::Version_1_1, &pUpconvertedRS);
  DXASSERT_NOMSG(pUpconvertedRS->Version == DxilRootSignatureVersion::Version_1_1);

  // Ensure this gets deleted as necessary.
  struct SigGuard {
    const DxilVersionedRootSignatureDesc *Orig, *Guard;
    SigGuard(const DxilVersionedRootSignatureDesc *pOrig, const DxilVersionedRootSignatureDesc *pGuard)
      : Orig(pOrig), Guard(pGuard) { }
    ~SigGuard() {
      if (Orig != Guard) {
        DeleteRootSignature(Guard);
      }
    }
  };
  SigGuard S(pVersionedRootSignature, pUpconvertedRS);

  const DxilRootSignatureDesc1 *pRootSignature = &pUpconvertedRS->Desc_1_1;

  // Flags (assume they are bits that can be combined with OR).
  if ((pRootSignature->Flags & ~DxilRootSignatureFlags::ValidFlags) != DxilRootSignatureFlags::None) {
    EAT(DiagPrinter << "Unsupported bit-flag set (root signature flags " 
                    << std::hex << (uint32_t)pRootSignature->Flags << ").\n");
  }

  m_RootSignatureFlags = pRootSignature->Flags;

  for (unsigned iRP = 0; iRP < pRootSignature->NumParameters; iRP++) {
    const DxilRootParameter1 *pSlot = &pRootSignature->pParameters[iRP];
    // Shader visibility.
    DxilShaderVisibility Visibility = pSlot->ShaderVisibility;
    if (!IsDxilShaderVisibility(Visibility)) {
      EAT(DiagPrinter << "Unsupported ShaderVisibility value " << (uint32_t)Visibility
                      << " (root parameter [" << iRP << "]).\n");
    }

    DxilRootParameterType ParameterType = pSlot->ParameterType;
    switch (ParameterType) {
    case DxilRootParameterType::DescriptorTable: {
      DescriptorTableVerifier DTV;
      DTV.Verify(pSlot->DescriptorTable.pDescriptorRanges,
                 pSlot->DescriptorTable.NumDescriptorRanges, iRP, DiagPrinter);

      for (unsigned iDTS = 0; iDTS < pSlot->DescriptorTable.NumDescriptorRanges; iDTS++) {
        const DxilDescriptorRange1 *pRange = &pSlot->DescriptorTable.pDescriptorRanges[iDTS];
        unsigned RangeFlags = (unsigned)pRange->Flags;

        // Verify range flags.
        if (RangeFlags & ~(unsigned)DxilDescriptorRangeFlags::ValidFlags) {
          EAT(DiagPrinter << "Unsupported bit-flag set (descriptor range flags "
                          << (uint32_t)pRange->Flags << ").\n");
        }
        switch (pRange->RangeType) {
        case DxilDescriptorRangeType::Sampler: {
          if (RangeFlags & (unsigned)(DxilDescriptorRangeFlags::DataVolatile |
                                      DxilDescriptorRangeFlags::DataStatic |
                                      DxilDescriptorRangeFlags::DataStaticWhileSetAtExecute)) {
            EAT(DiagPrinter << "Sampler descriptor ranges can't specify DATA_* flags "
                            << "since there is no data pointed to by samplers "
                            << "(descriptor range flags " << (uint32_t)pRange->Flags << ").\n");
          }
          break;
        }
        default: {
          unsigned NumDataFlags = 0;
          if (RangeFlags & (unsigned)DxilDescriptorRangeFlags::DataVolatile) { NumDataFlags++; }
          if (RangeFlags & (unsigned)DxilDescriptorRangeFlags::DataStatic) { NumDataFlags++; }
          if (RangeFlags & (unsigned)DxilDescriptorRangeFlags::DataStaticWhileSetAtExecute) { NumDataFlags++; }
          if (NumDataFlags > 1) {
            EAT(DiagPrinter << "Descriptor range flags cannot specify more than one DATA_* flag "
                            << "at a time (descriptor range flags " << (uint32_t)pRange->Flags << ").\n");
          }
          if ((RangeFlags & (unsigned)DxilDescriptorRangeFlags::DataStatic) && 
              (RangeFlags & (unsigned)DxilDescriptorRangeFlags::DescriptorsVolatile)) {
            EAT(DiagPrinter << "Descriptor range flags cannot specify DESCRIPTORS_VOLATILE with the DATA_STATIC flag at the same time (descriptor range flags " << (uint32_t)pRange->Flags << "). "
                            << "DATA_STATIC_WHILE_SET_AT_EXECUTE is fine to combine with DESCRIPTORS_VOLATILE, since DESCRIPTORS_VOLATILE still requires descriptors don't change during execution. \n");
          }
          break;
        }
        }

        AddRegisterRange(iRP,
                         DESCRIPTOR_TABLE_ENTRY,
                         iDTS,
                         pRange->RangeType,
                         Visibility,
                         pRange->NumDescriptors,
                         pRange->BaseShaderRegister,
                         pRange->RegisterSpace,
                         DiagPrinter);
      }
      break;
    }

    case DxilRootParameterType::Constants32Bit:
      AddRegisterRange(iRP,
                       ROOT_CONSTANT,
                       (unsigned)-1,
                       DxilDescriptorRangeType::CBV,
                       Visibility,
                       1,
                       pSlot->Constants.ShaderRegister,
                       pSlot->Constants.RegisterSpace,
                       DiagPrinter);
      break;

    case DxilRootParameterType::CBV:
    case DxilRootParameterType::SRV:
    case DxilRootParameterType::UAV: {
      // Verify root descriptor flags.
      unsigned Flags = (unsigned)pSlot->Descriptor.Flags;
      if (Flags & ~(unsigned)DxilRootDescriptorFlags::ValidFlags) {
        EAT(DiagPrinter << "Unsupported bit-flag set (root descriptor flags " << std::hex << Flags << ").\n");
      }

      unsigned NumDataFlags = 0;
      if (Flags & (unsigned)DxilRootDescriptorFlags::DataVolatile) { NumDataFlags++; }
      if (Flags & (unsigned)DxilRootDescriptorFlags::DataStatic) { NumDataFlags++; }
      if (Flags & (unsigned)DxilRootDescriptorFlags::DataStaticWhileSetAtExecute) { NumDataFlags++; }
      if (NumDataFlags > 1) {
        EAT(DiagPrinter << "Root descriptor flags cannot specify more "
                        << "than one DATA_* flag at a time (root "
                        << "descriptor flags " << NumDataFlags << ").\n");
      }

      AddRegisterRange(iRP, ROOT_DESCRIPTOR, (unsigned)-1,
                       GetRangeType(ParameterType), Visibility, 1,
                       pSlot->Descriptor.ShaderRegister,
                       pSlot->Descriptor.RegisterSpace, DiagPrinter);
      break;
    }

    default:
      EAT(DiagPrinter << "Unsupported ParameterType value " << (uint32_t)ParameterType
                      << " (root parameter " << iRP << ")\n");
    }
  }

  for (unsigned iSS = 0; iSS < pRootSignature->NumStaticSamplers; iSS++) {
    const DxilStaticSamplerDesc *pSS = &pRootSignature->pStaticSamplers[iSS];
    // Shader visibility.
    DxilShaderVisibility Visibility = pSS->ShaderVisibility;
    if (!IsDxilShaderVisibility(Visibility)) {
      EAT(DiagPrinter << "Unsupported ShaderVisibility value " << (uint32_t)Visibility
                      << " (static sampler [" << iSS << "]).\n");
    }

    StaticSamplerVerifier SSV;
    SSV.Verify(pSS, DiagPrinter);
    AddRegisterRange(iSS, STATIC_SAMPLER, (unsigned)-1,
                     DxilDescriptorRangeType::Sampler, Visibility, 1,
                     pSS->ShaderRegister, pSS->RegisterSpace, DiagPrinter);
  }
}

void RootSignatureVerifier::VerifyShader(DxilShaderVisibility VisType,
                                         const void *pPSVData,
                                         uint32_t PSVSize,
                                         DiagnosticPrinter &DiagPrinter) {
  DxilPipelineStateValidation PSV;
  IFTBOOL(PSV.InitFromPSV0(pPSVData, PSVSize), E_INVALIDARG);

  bool bShaderDeniedByRootSig = false;
  switch (VisType) {
  case DxilShaderVisibility::Vertex:
    if ((m_RootSignatureFlags & DxilRootSignatureFlags::DenyVertexShaderRootAccess) != DxilRootSignatureFlags::None) {
      bShaderDeniedByRootSig = true;
    }
    break;
  case DxilShaderVisibility::Hull:
    if ((m_RootSignatureFlags & DxilRootSignatureFlags::DenyHullShaderRootAccess) != DxilRootSignatureFlags::None) {
      bShaderDeniedByRootSig = true;
    }
    break;
  case DxilShaderVisibility::Domain:
    if ((m_RootSignatureFlags & DxilRootSignatureFlags::DenyDomainShaderRootAccess) != DxilRootSignatureFlags::None) {
      bShaderDeniedByRootSig = true;
    }
    break;
  case DxilShaderVisibility::Geometry:
    if ((m_RootSignatureFlags & DxilRootSignatureFlags::DenyGeometryShaderRootAccess) != DxilRootSignatureFlags::None) {
      bShaderDeniedByRootSig = true;
    }
    break;
  case DxilShaderVisibility::Pixel:
    if ((m_RootSignatureFlags & DxilRootSignatureFlags::DenyPixelShaderRootAccess) != DxilRootSignatureFlags::None) {
      bShaderDeniedByRootSig = true;
    }
    break;
  default:
    break;
  }

  bool bShaderHasRootBindings = false;

  for (unsigned iResource = 0; iResource < PSV.GetBindCount(); iResource++) {
    const PSVResourceBindInfo0 *pBindInfo0 = PSV.GetPSVResourceBindInfo0(iResource);
    DXASSERT_NOMSG(pBindInfo0);

    unsigned Space = pBindInfo0->Space;
    unsigned LB = pBindInfo0->LowerBound;
    unsigned UB = pBindInfo0->UpperBound;
    unsigned Num = (UB != UINT_MAX) ? (UB - LB + 1) : 1;
    PSVResourceType ResType = (PSVResourceType)pBindInfo0->ResType;

    switch(ResType) {
    case PSVResourceType::Sampler: {
      bShaderHasRootBindings = true;
      auto pCoveringRange = FindCoveringInterval(DxilDescriptorRangeType::Sampler, VisType, Num, LB, Space);
      if(!pCoveringRange) {
        EAT(DiagPrinter << "Shader sampler descriptor range (RegisterSpace=" << Space 
                        << ", NumDescriptors=" << Num << ", BaseShaderRegister=" << LB 
                        << ") is not fully bound in root signature.\n");
      }
      break;
    }

    case PSVResourceType::SRVTyped:
    case PSVResourceType::SRVRaw:
    case PSVResourceType::SRVStructured: {
      bShaderHasRootBindings = true;
      auto pCoveringRange = FindCoveringInterval(DxilDescriptorRangeType::SRV, VisType, Num, LB, Space);
      if (pCoveringRange) {
        if(pCoveringRange->nt == ROOT_DESCRIPTOR && ResType == PSVResourceType::SRVTyped) {
          EAT(DiagPrinter << "A Shader is declaring a resource object as a texture using "
                          << "a register mapped to a root descriptor SRV (RegisterSpace=" << Space
                          << ", ShaderRegister=" << LB << ").  "
                          << "SRV or UAV root descriptors can only be Raw or Structured buffers.\n");
        }
      }
      else {
        EAT(DiagPrinter << "Shader SRV descriptor range (RegisterSpace=" << Space
                        << ", NumDescriptors=" << Num << ", BaseShaderRegister=" << LB
                        << ") is not fully bound in root signature.\n");
      }
      break;
    }

    case PSVResourceType::UAVTyped:
    case PSVResourceType::UAVRaw:
    case PSVResourceType::UAVStructured:
    case PSVResourceType::UAVStructuredWithCounter: {
      bShaderHasRootBindings = true;
      auto pCoveringRange = FindCoveringInterval(DxilDescriptorRangeType::UAV, VisType, Num, LB, Space);
      if (pCoveringRange) {
        if (pCoveringRange->nt == ROOT_DESCRIPTOR) {
          if (ResType == PSVResourceType::UAVTyped) {
            EAT(DiagPrinter << "A shader is declaring a typed UAV using a register mapped "
                            << "to a root descriptor UAV (RegisterSpace=" << Space 
                            << ", ShaderRegister=" << LB << ").  "
                            << "SRV or UAV root descriptors can only be Raw or Structured buffers.\n");
          }
          if (ResType == PSVResourceType::UAVStructuredWithCounter) {
            EAT(DiagPrinter << "A Shader is declaring a structured UAV with counter using "
                            << "a register mapped to a root descriptor UAV (RegisterSpace=" << Space
                            << ", ShaderRegister=" << LB << ").  "
                            << "SRV or UAV root descriptors can only be Raw or Structured buffers.\n");
          }
        }
      }
      else {
        EAT(DiagPrinter << "Shader UAV descriptor range (RegisterSpace=" << Space
                        << ", NumDescriptors=" << Num << ", BaseShaderRegister=" << LB
                        << ") is not fully bound in root signature.\n");
      }
      break;
    }

    case PSVResourceType::CBV: {
      bShaderHasRootBindings = true;
      auto pCoveringRange = FindCoveringInterval(DxilDescriptorRangeType::CBV, VisType, Num, LB, Space);
      if (!pCoveringRange) {
        EAT(DiagPrinter << "Shader CBV descriptor range (RegisterSpace=" << Space
                        << ", NumDescriptors=" << Num << ", BaseShaderRegister=" << LB
                        << ") is not fully bound in root signature.\n");
      }
      break;
    }

    default:
      break;
    }
  }

  if (bShaderHasRootBindings && bShaderDeniedByRootSig) {
    EAT(DiagPrinter << "Shader has root bindings but root signature uses a DENY flag "
                    << "to disallow root binding access to the shader stage.\n");
  }
}

BOOL isNaN(const float &a) {
  static const unsigned exponentMask = 0x7f800000;
  static const unsigned mantissaMask = 0x007fffff;
  unsigned u = *(const unsigned *)&a;
  return (((u & exponentMask) == exponentMask) && (u & mantissaMask)); // NaN
}

static bool IsDxilTextureAddressMode(DxilTextureAddressMode v) {
  return DxilTextureAddressMode::Wrap <= v &&
         v <= DxilTextureAddressMode::MirrorOnce;
}
static bool IsDxilComparisonFunc(DxilComparisonFunc v) {
  return DxilComparisonFunc::Never <= v && v <= DxilComparisonFunc::Always;
}

// This validation closely mirrors CCreateSamplerStateValidator's checks
void StaticSamplerVerifier::Verify(const DxilStaticSamplerDesc* pDesc,
                                   DiagnosticPrinter &DiagPrinter) {
  if (!pDesc) {
    EAT(DiagPrinter << "Static sampler: A nullptr pSamplerDesc was specified.\n");
  }

  bool bIsComparison = false;
  switch (pDesc->Filter) {
  case DxilFilter::MINIMUM_MIN_MAG_MIP_POINT:
  case DxilFilter::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:
  case DxilFilter::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
  case DxilFilter::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:
  case DxilFilter::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:
  case DxilFilter::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
  case DxilFilter::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:
  case DxilFilter::MINIMUM_MIN_MAG_MIP_LINEAR:
  case DxilFilter::MINIMUM_ANISOTROPIC:
  case DxilFilter::MAXIMUM_MIN_MAG_MIP_POINT:
  case DxilFilter::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:
  case DxilFilter::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
  case DxilFilter::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:
  case DxilFilter::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:
  case DxilFilter::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
  case DxilFilter::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:
  case DxilFilter::MAXIMUM_MIN_MAG_MIP_LINEAR:
  case DxilFilter::MAXIMUM_ANISOTROPIC:
    break;
  case DxilFilter::MIN_MAG_MIP_POINT:
  case DxilFilter::MIN_MAG_POINT_MIP_LINEAR:
  case DxilFilter::MIN_POINT_MAG_LINEAR_MIP_POINT:
  case DxilFilter::MIN_POINT_MAG_MIP_LINEAR:
  case DxilFilter::MIN_LINEAR_MAG_MIP_POINT:
  case DxilFilter::MIN_LINEAR_MAG_POINT_MIP_LINEAR:
  case DxilFilter::MIN_MAG_LINEAR_MIP_POINT:
  case DxilFilter::MIN_MAG_MIP_LINEAR:
  case DxilFilter::ANISOTROPIC:
    break;
  case DxilFilter::COMPARISON_MIN_MAG_MIP_POINT:
  case DxilFilter::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:
  case DxilFilter::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
  case DxilFilter::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:
  case DxilFilter::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:
  case DxilFilter::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
  case DxilFilter::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:
  case DxilFilter::COMPARISON_MIN_MAG_MIP_LINEAR:
  case DxilFilter::COMPARISON_ANISOTROPIC:
    bIsComparison = true;
    break;
  default:
    EAT(DiagPrinter << "Static sampler: Filter unrecognized.\n");
  }

  if (!IsDxilTextureAddressMode(pDesc->AddressU)) {
    EAT(DiagPrinter << "Static sampler: AddressU unrecognized.\n");
  }
  if (!IsDxilTextureAddressMode(pDesc->AddressV)) {
    EAT(DiagPrinter << "Static sampler: AddressV unrecognized.\n");
  }
  if (!IsDxilTextureAddressMode(pDesc->AddressW)) {
    EAT(DiagPrinter << "Static sampler: AddressW unrecognized.\n");
  }

  if (isNaN(pDesc->MipLODBias) || (pDesc->MipLODBias < DxilMipLodBiaxMin) ||
      (pDesc->MipLODBias > DxilMipLodBiaxMax)) {
    EAT(DiagPrinter << "Static sampler: MipLODBias must be in the "
                    << "range [" << DxilMipLodBiaxMin << " to " << DxilMipLodBiaxMax
                    <<"].  " << pDesc->MipLODBias << "specified.\n");
  }

  if (pDesc->MaxAnisotropy > DxilMapAnisotropy) {
    EAT(DiagPrinter << "Static sampler: MaxAnisotropy must be in "
                    << "the range [0 to " << DxilMapAnisotropy << "].  "
                    << pDesc->MaxAnisotropy << " specified.\n");
  }

  if (bIsComparison && !IsDxilComparisonFunc(pDesc->ComparisonFunc)) {
    EAT(DiagPrinter << "Static sampler: ComparisonFunc unrecognized.");
  }

  if (isNaN(pDesc->MinLOD)) {
    EAT(DiagPrinter << "Static sampler: MinLOD be in the range [-INF to +INF].  "
                    << pDesc->MinLOD << " specified.\n");
  }

  if (isNaN(pDesc->MaxLOD)) {
    EAT(DiagPrinter << "Static sampler: MaxLOD be in the range [-INF to +INF].  "
                    << pDesc->MaxLOD << " specified.\n");
  }
}

//////////////////////////////////////////////////////////////////////////////

template <typename T>
void DeleteRootSignatureTemplate(const T &RS) {
  for (unsigned i = 0; i < RS.NumParameters; i++) {
    const auto &P = RS.pParameters[i];
    if (P.ParameterType == DxilRootParameterType::DescriptorTable) {
      delete[] P.DescriptorTable.pDescriptorRanges;
    }
  }

  delete[] RS.pParameters;
  delete[] RS.pStaticSamplers;
}

void DeleteRootSignature(const DxilVersionedRootSignatureDesc * pRootSignature)
{
  if (pRootSignature == nullptr)
    return;

  switch (pRootSignature->Version)
  {
  case DxilRootSignatureVersion::Version_1_0:
    DeleteRootSignatureTemplate<DxilRootSignatureDesc>(pRootSignature->Desc_1_0);
    break;
  case DxilRootSignatureVersion::Version_1_1:
  default:
    DXASSERT(pRootSignature->Version == DxilRootSignatureVersion::Version_1_1, "else version is incorrect");
    DeleteRootSignatureTemplate<DxilRootSignatureDesc1>(pRootSignature->Desc_1_1);
    break;
  }

  delete pRootSignature;
}

// GetFlags/SetFlags overloads.
static DxilRootDescriptorFlags GetFlags(const DxilRootDescriptor &)
{
  // Upconvert root parameter flags to be volatile.
  return DxilRootDescriptorFlags::DataVolatile;
}
static void SetFlags(DxilRootDescriptor &, DxilRootDescriptorFlags)
{
  // Drop the flags; none existed in rs_1_0.
}
static DxilRootDescriptorFlags GetFlags(const DxilRootDescriptor1 &D)
{
  return D.Flags;
}
static void SetFlags(DxilRootDescriptor1 &D, DxilRootDescriptorFlags Flags)
{
  D.Flags = Flags;
}
static void SetFlags(DxilContainerRootDescriptor1 &D, DxilRootDescriptorFlags Flags)
{
  D.Flags = (uint32_t)Flags;
}
static DxilDescriptorRangeFlags GetFlags(const DxilDescriptorRange &D)
{
  // Upconvert range flags to be volatile.
  DxilDescriptorRangeFlags Flags = DxilDescriptorRangeFlags::DescriptorsVolatile;

  // Sampler does not have data.
  if (D.RangeType != DxilDescriptorRangeType::Sampler)
    Flags = (DxilDescriptorRangeFlags)((unsigned)Flags | (unsigned)DxilDescriptorRangeFlags::DataVolatile);

  return Flags;
}
static void SetFlags(DxilDescriptorRange &, DxilDescriptorRangeFlags)
{
}
static DxilDescriptorRangeFlags GetFlags(const DxilContainerDescriptorRange &D)
{
  // Upconvert range flags to be volatile.
  DxilDescriptorRangeFlags Flags = DxilDescriptorRangeFlags::DescriptorsVolatile;

  // Sampler does not have data.
  if (D.RangeType != (uint32_t)DxilDescriptorRangeType::Sampler)
    Flags |= DxilDescriptorRangeFlags::DataVolatile;

  return Flags;
}
static void SetFlags(DxilContainerDescriptorRange &, DxilDescriptorRangeFlags)
{
}
static DxilDescriptorRangeFlags GetFlags(const DxilDescriptorRange1 &D)
{
  return D.Flags;
}
static void SetFlags(DxilDescriptorRange1 &D, DxilDescriptorRangeFlags Flags)
{
  D.Flags = Flags;
}
static DxilDescriptorRangeFlags GetFlags(const DxilContainerDescriptorRange1 &D)
{
  return (DxilDescriptorRangeFlags)D.Flags;
}
static void SetFlags(DxilContainerDescriptorRange1 &D, DxilDescriptorRangeFlags Flags)
{
  D.Flags = (uint32_t)Flags;
}

template<typename IN_DXIL_ROOT_SIGNATURE_DESC,
  typename OUT_DXIL_ROOT_SIGNATURE_DESC,
  typename OUT_DXIL_ROOT_PARAMETER,
  typename OUT_DXIL_ROOT_DESCRIPTOR,
  typename OUT_DXIL_DESCRIPTOR_RANGE>
void ConvertRootSignatureTemplate(const IN_DXIL_ROOT_SIGNATURE_DESC &DescIn,
                                  DxilRootSignatureVersion DescVersionOut,
                                  OUT_DXIL_ROOT_SIGNATURE_DESC &DescOut)
{
  const IN_DXIL_ROOT_SIGNATURE_DESC *pDescIn = &DescIn;
  OUT_DXIL_ROOT_SIGNATURE_DESC *pDescOut = &DescOut;

  // Root signature descriptor.
  pDescOut->Flags = pDescIn->Flags;
  pDescOut->NumParameters = 0;
  pDescOut->NumStaticSamplers = 0;
  // Intialize all pointers early so that clean up works properly.
  pDescOut->pParameters = nullptr;
  pDescOut->pStaticSamplers = nullptr;

  // Root signature parameters.
  if (pDescIn->NumParameters > 0) {
    pDescOut->pParameters = new OUT_DXIL_ROOT_PARAMETER[pDescIn->NumParameters];
    pDescOut->NumParameters = pDescIn->NumParameters;
    memset((void *)pDescOut->pParameters, 0, pDescOut->NumParameters*sizeof(OUT_DXIL_ROOT_PARAMETER));
  }

  for (unsigned iRP = 0; iRP < pDescIn->NumParameters; iRP++) {
    const auto &ParamIn = pDescIn->pParameters[iRP];
    OUT_DXIL_ROOT_PARAMETER &ParamOut = (OUT_DXIL_ROOT_PARAMETER &)pDescOut->pParameters[iRP];

    ParamOut.ParameterType = ParamIn.ParameterType;
    ParamOut.ShaderVisibility = ParamIn.ShaderVisibility;

    switch (ParamIn.ParameterType) {
    case DxilRootParameterType::DescriptorTable: {
      ParamOut.DescriptorTable.pDescriptorRanges = nullptr;
      unsigned NumRanges = ParamIn.DescriptorTable.NumDescriptorRanges;
      if (NumRanges > 0) {
        ParamOut.DescriptorTable.pDescriptorRanges = new OUT_DXIL_DESCRIPTOR_RANGE[NumRanges];
        ParamOut.DescriptorTable.NumDescriptorRanges = NumRanges;
      }

      for (unsigned i = 0; i < NumRanges; i++) {
        const auto &RangeIn = ParamIn.DescriptorTable.pDescriptorRanges[i];
        OUT_DXIL_DESCRIPTOR_RANGE &RangeOut = (OUT_DXIL_DESCRIPTOR_RANGE &)ParamOut.DescriptorTable.pDescriptorRanges[i];

        RangeOut.RangeType = RangeIn.RangeType;
        RangeOut.NumDescriptors = RangeIn.NumDescriptors;
        RangeOut.BaseShaderRegister = RangeIn.BaseShaderRegister;
        RangeOut.RegisterSpace = RangeIn.RegisterSpace;
        RangeOut.OffsetInDescriptorsFromTableStart = RangeIn.OffsetInDescriptorsFromTableStart;
        DxilDescriptorRangeFlags Flags = GetFlags(RangeIn);
        SetFlags(RangeOut, Flags);
      }
      break;
    }
    case DxilRootParameterType::Constants32Bit: {
      ParamOut.Constants.Num32BitValues = ParamIn.Constants.Num32BitValues;
      ParamOut.Constants.ShaderRegister = ParamIn.Constants.ShaderRegister;
      ParamOut.Constants.RegisterSpace = ParamIn.Constants.RegisterSpace;
      break;
    }
    case DxilRootParameterType::CBV:
    case DxilRootParameterType::SRV:
    case DxilRootParameterType::UAV: {
      ParamOut.Descriptor.ShaderRegister = ParamIn.Descriptor.ShaderRegister;
      ParamOut.Descriptor.RegisterSpace = ParamIn.Descriptor.RegisterSpace;
      DxilRootDescriptorFlags Flags = GetFlags(ParamIn.Descriptor);
      SetFlags(ParamOut.Descriptor, Flags);
      break;
    }
    default:
      IFT(E_FAIL);
    }
  }

  // Static samplers.
  if (pDescIn->NumStaticSamplers > 0) {
    pDescOut->pStaticSamplers = new DxilStaticSamplerDesc[pDescIn->NumStaticSamplers];
    pDescOut->NumStaticSamplers = pDescIn->NumStaticSamplers;
    memcpy((void*)pDescOut->pStaticSamplers, pDescIn->pStaticSamplers, pDescOut->NumStaticSamplers*sizeof(DxilStaticSamplerDesc));
  }
}

void ConvertRootSignature(const DxilVersionedRootSignatureDesc * pRootSignatureIn,
                          DxilRootSignatureVersion RootSignatureVersionOut,
                          const DxilVersionedRootSignatureDesc ** ppRootSignatureOut) {
  IFTBOOL(pRootSignatureIn != nullptr && ppRootSignatureOut != nullptr, E_INVALIDARG);
  *ppRootSignatureOut = nullptr;

  if (pRootSignatureIn->Version == RootSignatureVersionOut){
    // No conversion. Return the original root signature pointer; no cloning.
    *ppRootSignatureOut = pRootSignatureIn;
    return;
  }

  DxilVersionedRootSignatureDesc *pRootSignatureOut = nullptr;

  try {
    pRootSignatureOut = new DxilVersionedRootSignatureDesc();
    memset(pRootSignatureOut, 0, sizeof(*pRootSignatureOut));

    // Convert root signature.
    switch (RootSignatureVersionOut) {
    case DxilRootSignatureVersion::Version_1_0:
      switch (pRootSignatureIn->Version) {
      case DxilRootSignatureVersion::Version_1_1:
        pRootSignatureOut->Version = DxilRootSignatureVersion::Version_1_0;
        ConvertRootSignatureTemplate<
          DxilRootSignatureDesc1,
          DxilRootSignatureDesc,
          DxilRootParameter,
          DxilRootDescriptor,
          DxilDescriptorRange>(pRootSignatureIn->Desc_1_1,
            DxilRootSignatureVersion::Version_1_0,
            pRootSignatureOut->Desc_1_0);
        break;
      default:
        IFT(E_INVALIDARG);
      }
      break;

    case DxilRootSignatureVersion::Version_1_1:
      switch (pRootSignatureIn->Version) {
      case DxilRootSignatureVersion::Version_1_0:
        pRootSignatureOut->Version = DxilRootSignatureVersion::Version_1_1;
        ConvertRootSignatureTemplate<
          DxilRootSignatureDesc,
          DxilRootSignatureDesc1,
          DxilRootParameter1,
          DxilRootDescriptor1,
          DxilDescriptorRange1>(pRootSignatureIn->Desc_1_0,
            DxilRootSignatureVersion::Version_1_1,
            pRootSignatureOut->Desc_1_1);
        break;
      default:
        IFT(E_INVALIDARG);
      }
      break;

    default:
      IFT(E_INVALIDARG);
      break;
    }
  }
  catch (...) {
    DeleteRootSignature(pRootSignatureOut);
    throw;
  }

  *ppRootSignatureOut = pRootSignatureOut;
}

template<typename T_ROOT_SIGNATURE_DESC,
  typename T_ROOT_PARAMETER,
  typename T_ROOT_DESCRIPTOR_INTERNAL,
  typename T_DESCRIPTOR_RANGE_INTERNAL>
void SerializeRootSignatureTemplate(_In_ const T_ROOT_SIGNATURE_DESC* pRootSignature,
                                    DxilRootSignatureVersion DescVersion,
                                    _COM_Outptr_ IDxcBlob** ppBlob,
                                    DiagnosticPrinter &DiagPrinter,
                                    _In_ bool bAllowReservedRegisterSpace) {
  DxilContainerRootSignatureDesc RS;
  uint32_t Offset;
  SimpleSerializer Serializer;
  IFT(Serializer.AddBlock(&RS, sizeof(RS), &Offset));
  IFTBOOL(Offset == 0, E_FAIL);

  const T_ROOT_SIGNATURE_DESC *pRS = pRootSignature;
  RS.Version = (uint32_t)DescVersion;
  RS.Flags = (uint32_t)pRS->Flags;
  RS.NumParameters = pRS->NumParameters;
  RS.NumStaticSamplers = pRS->NumStaticSamplers;

  DxilContainerRootParameter *pRP;
  IFT(Serializer.ReserveBlock((void**)&pRP,
    sizeof(DxilContainerRootParameter)*RS.NumParameters, &RS.RootParametersOffset));

  for (uint32_t iRP = 0; iRP < RS.NumParameters; iRP++) {
    const T_ROOT_PARAMETER *pInRP = &pRS->pParameters[iRP];
    DxilContainerRootParameter *pOutRP = &pRP[iRP];
    pOutRP->ParameterType = (uint32_t)pInRP->ParameterType;
    pOutRP->ShaderVisibility = (uint32_t)pInRP->ShaderVisibility;
    switch (pInRP->ParameterType) {
    case DxilRootParameterType::DescriptorTable: {
      DxilContainerRootDescriptorTable *p1;
      IFT(Serializer.ReserveBlock((void**)&p1,
                                  sizeof(DxilContainerRootDescriptorTable),
                                  &pOutRP->PayloadOffset));
      p1->NumDescriptorRanges = pInRP->DescriptorTable.NumDescriptorRanges;

      T_DESCRIPTOR_RANGE_INTERNAL *p2;
      IFT(Serializer.ReserveBlock((void**)&p2,
                                  sizeof(T_DESCRIPTOR_RANGE_INTERNAL)*p1->NumDescriptorRanges,
                                  &p1->DescriptorRangesOffset));

      for (uint32_t i = 0; i < p1->NumDescriptorRanges; i++) {
        p2[i].RangeType = (uint32_t)pInRP->DescriptorTable.pDescriptorRanges[i].RangeType;
        p2[i].NumDescriptors = pInRP->DescriptorTable.pDescriptorRanges[i].NumDescriptors;
        p2[i].BaseShaderRegister = pInRP->DescriptorTable.pDescriptorRanges[i].BaseShaderRegister;
        p2[i].RegisterSpace = pInRP->DescriptorTable.pDescriptorRanges[i].RegisterSpace;
        p2[i].OffsetInDescriptorsFromTableStart = pInRP->DescriptorTable.pDescriptorRanges[i].OffsetInDescriptorsFromTableStart;
        DxilDescriptorRangeFlags Flags = GetFlags(pInRP->DescriptorTable.pDescriptorRanges[i]);
        SetFlags(p2[i], Flags);
      }
      break;
    }
    case DxilRootParameterType::Constants32Bit: {
      DxilRootConstants *p;
      IFT(Serializer.ReserveBlock((void**)&p, sizeof(DxilRootConstants), &pOutRP->PayloadOffset));
      p->Num32BitValues = pInRP->Constants.Num32BitValues;
      p->ShaderRegister = pInRP->Constants.ShaderRegister;
      p->RegisterSpace = pInRP->Constants.RegisterSpace;
      break;
    }
    case DxilRootParameterType::CBV:
    case DxilRootParameterType::SRV:
    case DxilRootParameterType::UAV: {
      T_ROOT_DESCRIPTOR_INTERNAL *p;
      IFT(Serializer.ReserveBlock((void**)&p, sizeof(T_ROOT_DESCRIPTOR_INTERNAL), &pOutRP->PayloadOffset));
      p->ShaderRegister = pInRP->Descriptor.ShaderRegister;
      p->RegisterSpace = pInRP->Descriptor.RegisterSpace;
      DxilRootDescriptorFlags Flags = GetFlags(pInRP->Descriptor);
      SetFlags(*p, Flags);
      break;
    }
    default:
      EAT(DiagPrinter << "D3DSerializeRootSignature: unknown root parameter type ("
                      << (uint32_t)pInRP->ParameterType << ")\n");
    }
  }

  DxilStaticSamplerDesc *pSS;
  unsigned StaticSamplerSize = sizeof(DxilStaticSamplerDesc)*RS.NumStaticSamplers;
  IFT(Serializer.ReserveBlock((void**)&pSS, StaticSamplerSize, &RS.StaticSamplersOffset));
  memcpy(pSS, pRS->pStaticSamplers, StaticSamplerSize);

  // Create the result blob.
  CDxcMallocHeapPtr<char> bytes(DxcGetThreadMallocNoRef());
  CComPtr<IDxcBlob> pBlob;
  unsigned cb = Serializer.GetSize();
  DXASSERT_NOMSG((cb & 0x3) == 0);
  IFTBOOL(bytes.Allocate(cb), E_OUTOFMEMORY);
  IFT(Serializer.Compact(bytes.m_pData, cb));
  IFT(DxcCreateBlobOnHeap(bytes.m_pData, cb, ppBlob));
  bytes.Detach(); // Ownership transfered to ppBlob.
}

_Use_decl_annotations_
void SerializeRootSignature(const DxilVersionedRootSignatureDesc *pRootSignature,
                            IDxcBlob **ppBlob, IDxcBlobEncoding **ppErrorBlob,
                            bool bAllowReservedRegisterSpace) {
  DXASSERT_NOMSG(pRootSignature != nullptr);
  DXASSERT_NOMSG(ppBlob != nullptr);
  DXASSERT_NOMSG(ppErrorBlob != nullptr);

  *ppBlob = nullptr;
  *ppErrorBlob = nullptr;

  RootSignatureVerifier RSV;
  // TODO: change SerializeRootSignature to take raw_ostream&
  string DiagString;
  raw_string_ostream DiagStream(DiagString);
  DiagnosticPrinterRawOStream DiagPrinter(DiagStream);

  // Verify root signature.
  RSV.AllowReservedRegisterSpace(bAllowReservedRegisterSpace);
  try {
    RSV.VerifyRootSignature(pRootSignature, DiagPrinter);
    switch (pRootSignature->Version)
    {
    case DxilRootSignatureVersion::Version_1_0:
      SerializeRootSignatureTemplate<
        DxilRootSignatureDesc,
        DxilRootParameter,
        DxilRootDescriptor,
        DxilContainerDescriptorRange>(&pRootSignature->Desc_1_0,
          DxilRootSignatureVersion::Version_1_0,
          ppBlob, DiagPrinter,
          bAllowReservedRegisterSpace);
      break;

    case DxilRootSignatureVersion::Version_1_1:
    default:
      DXASSERT(pRootSignature->Version == DxilRootSignatureVersion::Version_1_1, "else VerifyRootSignature didn't validate");
      SerializeRootSignatureTemplate<
        DxilRootSignatureDesc1,
        DxilRootParameter1,
        DxilContainerRootDescriptor1,
        DxilContainerDescriptorRange1>(&pRootSignature->Desc_1_1,
          DxilRootSignatureVersion::Version_1_1,
          ppBlob, DiagPrinter,
          bAllowReservedRegisterSpace);
      break;
    }
  } catch (...) {
    DiagStream.flush();
    DxcCreateBlobWithEncodingOnHeapCopy(DiagString.c_str(), DiagString.size(), CP_UTF8, ppErrorBlob);
  }
}

//=============================================================================
//
// CVersionedRootSignatureDeserializer.
//
//=============================================================================
class CVersionedRootSignatureDeserializer {
protected:
  const DxilVersionedRootSignatureDesc *m_pRootSignature;
  const DxilVersionedRootSignatureDesc *m_pRootSignature10;
  const DxilVersionedRootSignatureDesc *m_pRootSignature11;

public:
  CVersionedRootSignatureDeserializer();
  ~CVersionedRootSignatureDeserializer();

  void Initialize(_In_reads_bytes_(SrcDataSizeInBytes) const void *pSrcData,
                  _In_ uint32_t SrcDataSizeInBytes);

  const DxilVersionedRootSignatureDesc *GetRootSignatureDescAtVersion(DxilRootSignatureVersion convertToVersion);

  const DxilVersionedRootSignatureDesc *GetUnconvertedRootSignatureDesc();
};

CVersionedRootSignatureDeserializer::CVersionedRootSignatureDeserializer()
  : m_pRootSignature(nullptr)
  , m_pRootSignature10(nullptr)
  , m_pRootSignature11(nullptr) {
}

CVersionedRootSignatureDeserializer::~CVersionedRootSignatureDeserializer() {
  DeleteRootSignature(m_pRootSignature10);
  DeleteRootSignature(m_pRootSignature11);
}

void CVersionedRootSignatureDeserializer::Initialize(_In_reads_bytes_(SrcDataSizeInBytes) const void *pSrcData,
                                                     _In_ uint32_t SrcDataSizeInBytes) {
  const DxilVersionedRootSignatureDesc *pRootSignature = nullptr;
  DeserializeRootSignature(pSrcData, SrcDataSizeInBytes, &pRootSignature);

  switch (pRootSignature->Version) {
  case DxilRootSignatureVersion::Version_1_0:
    m_pRootSignature10 = pRootSignature;
    break;

  case DxilRootSignatureVersion::Version_1_1:
    m_pRootSignature11 = pRootSignature;
    break;

  default:
    DeleteRootSignature(pRootSignature);
    return;
  }

  m_pRootSignature = pRootSignature;
}

const DxilVersionedRootSignatureDesc *
CVersionedRootSignatureDeserializer::GetUnconvertedRootSignatureDesc() {
  return m_pRootSignature;
}

const DxilVersionedRootSignatureDesc *
CVersionedRootSignatureDeserializer::GetRootSignatureDescAtVersion(DxilRootSignatureVersion ConvertToVersion) {
  switch (ConvertToVersion) {
  case DxilRootSignatureVersion::Version_1_0:
    if (m_pRootSignature10 == nullptr) {
      ConvertRootSignature(m_pRootSignature,
                           ConvertToVersion,
                           (const DxilVersionedRootSignatureDesc **)&m_pRootSignature10);
    }
    return m_pRootSignature10;

  case DxilRootSignatureVersion::Version_1_1:
    if (m_pRootSignature11 == nullptr) {
      ConvertRootSignature(m_pRootSignature,
                           ConvertToVersion,
                           (const DxilVersionedRootSignatureDesc **)&m_pRootSignature11);
    }
    return m_pRootSignature11;

  default:
    IFTBOOL(false, E_FAIL);
  }

  return nullptr;
}

template<typename T_ROOT_SIGNATURE_DESC,
         typename T_ROOT_PARAMETER,
         typename T_ROOT_DESCRIPTOR,
         typename T_ROOT_DESCRIPTOR_INTERNAL,
         typename T_DESCRIPTOR_RANGE,
         typename T_DESCRIPTOR_RANGE_INTERNAL>
void DeserializeRootSignatureTemplate(_In_reads_bytes_(SrcDataSizeInBytes) const void *pSrcData,
                                      _In_ uint32_t SrcDataSizeInBytes,
                                      DxilRootSignatureVersion DescVersion,
                                      T_ROOT_SIGNATURE_DESC &RootSignatureDesc) {
  // Note that in case of failure, outside code must deallocate memory.
  T_ROOT_SIGNATURE_DESC *pRootSignature = &RootSignatureDesc;
  const char *pData = (const char *)pSrcData;
  const char *pMaxPtr = pData + SrcDataSizeInBytes;
  UNREFERENCED_PARAMETER(DescVersion);
  DXASSERT_NOMSG(((const uint32_t*)pData)[0] == (uint32_t)DescVersion);

  // Root signature.
  IFTBOOL(pData + sizeof(DxilContainerRootSignatureDesc) <= pMaxPtr, E_FAIL);
  const DxilContainerRootSignatureDesc *pRS = (const DxilContainerRootSignatureDesc *)pData;
  pRootSignature->Flags = (DxilRootSignatureFlags)pRS->Flags;
  pRootSignature->NumParameters = pRS->NumParameters;
  pRootSignature->NumStaticSamplers = pRS->NumStaticSamplers;
  // Intialize all pointers early so that clean up works properly.
  pRootSignature->pParameters = nullptr;
  pRootSignature->pStaticSamplers = nullptr;

  size_t s = sizeof(DxilContainerRootParameter)*pRS->NumParameters;
  const DxilContainerRootParameter *pInRTS = (const DxilContainerRootParameter *)(pData + pRS->RootParametersOffset);
  IFTBOOL(((const char*)pInRTS) + s <= pMaxPtr, E_FAIL);
  if (pRootSignature->NumParameters) {
    pRootSignature->pParameters = new T_ROOT_PARAMETER[pRootSignature->NumParameters];
  }
  memset((void *)pRootSignature->pParameters, 0, s);

  for(unsigned iRP = 0; iRP < pRootSignature->NumParameters; iRP++) {
    DxilRootParameterType ParameterType = (DxilRootParameterType)pInRTS[iRP].ParameterType;
    T_ROOT_PARAMETER *pOutRTS = (T_ROOT_PARAMETER *)&pRootSignature->pParameters[iRP];
    pOutRTS->ParameterType = ParameterType;
    pOutRTS->ShaderVisibility = (DxilShaderVisibility)pInRTS[iRP].ShaderVisibility;
    switch(ParameterType) {
    case DxilRootParameterType::DescriptorTable: {
      const DxilContainerRootDescriptorTable *p1 = (const DxilContainerRootDescriptorTable*)(pData + pInRTS[iRP].PayloadOffset);
      IFTBOOL((const char*)p1 + sizeof(DxilContainerRootDescriptorTable) <= pMaxPtr, E_FAIL);
      pOutRTS->DescriptorTable.NumDescriptorRanges = p1->NumDescriptorRanges;
      pOutRTS->DescriptorTable.pDescriptorRanges = nullptr;
      const T_DESCRIPTOR_RANGE_INTERNAL *p2 = (const T_DESCRIPTOR_RANGE_INTERNAL*)(pData + p1->DescriptorRangesOffset);
      IFTBOOL((const char*)p2 + sizeof(T_DESCRIPTOR_RANGE_INTERNAL) <= pMaxPtr, E_FAIL);
      if (p1->NumDescriptorRanges) {
        pOutRTS->DescriptorTable.pDescriptorRanges = new T_DESCRIPTOR_RANGE[p1->NumDescriptorRanges];
      }
      for (unsigned i = 0; i < p1->NumDescriptorRanges; i++) {
        T_DESCRIPTOR_RANGE *p3 = (T_DESCRIPTOR_RANGE *)&pOutRTS->DescriptorTable.pDescriptorRanges[i];
        p3->RangeType                         = (DxilDescriptorRangeType)p2[i].RangeType;
        p3->NumDescriptors                    = p2[i].NumDescriptors;
        p3->BaseShaderRegister                = p2[i].BaseShaderRegister;
        p3->RegisterSpace                     = p2[i].RegisterSpace;
        p3->OffsetInDescriptorsFromTableStart = p2[i].OffsetInDescriptorsFromTableStart;
        DxilDescriptorRangeFlags Flags = GetFlags(p2[i]);
        SetFlags(*p3, Flags);
      }
      break;
    }
    case DxilRootParameterType::Constants32Bit: {
      const DxilRootConstants *p = (const DxilRootConstants*)(pData + pInRTS[iRP].PayloadOffset);
      IFTBOOL((const char*)p + sizeof(DxilRootConstants) <= pMaxPtr, E_FAIL);
      pOutRTS->Constants.Num32BitValues = p->Num32BitValues;
      pOutRTS->Constants.ShaderRegister = p->ShaderRegister;
      pOutRTS->Constants.RegisterSpace  = p->RegisterSpace;
      break;
    }
    case DxilRootParameterType::CBV:
    case DxilRootParameterType::SRV:
    case DxilRootParameterType::UAV: {
      const T_ROOT_DESCRIPTOR *p = (const T_ROOT_DESCRIPTOR *)(pData + pInRTS[iRP].PayloadOffset);
      IFTBOOL((const char*)p + sizeof(T_ROOT_DESCRIPTOR) <= pMaxPtr, E_FAIL);
      pOutRTS->Descriptor.ShaderRegister = p->ShaderRegister;
      pOutRTS->Descriptor.RegisterSpace  = p->RegisterSpace;
      DxilRootDescriptorFlags Flags = GetFlags(*p);
      SetFlags(pOutRTS->Descriptor, Flags);
      break;
    }
    default:
      IFT(E_FAIL);
    }
  }

  s = sizeof(DxilStaticSamplerDesc)*pRS->NumStaticSamplers;
  const DxilStaticSamplerDesc *pInSS = (const DxilStaticSamplerDesc *)(pData + pRS->StaticSamplersOffset);
  IFTBOOL(((const char*)pInSS) + s <= pMaxPtr, E_FAIL);
  if (pRootSignature->NumStaticSamplers) {
    pRootSignature->pStaticSamplers = new DxilStaticSamplerDesc[pRootSignature->NumStaticSamplers];
  }
  memcpy((void*)pRootSignature->pStaticSamplers, pInSS, s);
}

_Use_decl_annotations_
void DeserializeRootSignature(const void *pSrcData,
                              uint32_t SrcDataSizeInBytes,
                              const DxilVersionedRootSignatureDesc **ppRootSignature) {
  DxilVersionedRootSignatureDesc *pRootSignature = nullptr;
  IFTBOOL(pSrcData != nullptr && SrcDataSizeInBytes != 0 && ppRootSignature != nullptr, E_INVALIDARG);
  IFTBOOL(*ppRootSignature == nullptr, E_INVALIDARG);
  const char *pData = (const char *)pSrcData;
  IFTBOOL(pData + sizeof(uint32_t) < pData + SrcDataSizeInBytes, E_FAIL);

  DxilRootSignatureVersion Version = (const DxilRootSignatureVersion)((const uint32_t*)pData)[0];

  pRootSignature = new DxilVersionedRootSignatureDesc();

  try {
    switch (Version) {
    case DxilRootSignatureVersion::Version_1_0:
      pRootSignature->Version = DxilRootSignatureVersion::Version_1_0;
      DeserializeRootSignatureTemplate<
         DxilRootSignatureDesc,
         DxilRootParameter,
         DxilRootDescriptor,
         DxilRootDescriptor,
         DxilDescriptorRange,
         DxilContainerDescriptorRange>(pSrcData,
                                       SrcDataSizeInBytes,
                                       DxilRootSignatureVersion::Version_1_0,
                                       pRootSignature->Desc_1_0);
      break;

    case DxilRootSignatureVersion::Version_1_1:
      pRootSignature->Version = DxilRootSignatureVersion::Version_1_1;
      DeserializeRootSignatureTemplate<
         DxilRootSignatureDesc1,
         DxilRootParameter1,
         DxilRootDescriptor1,
         DxilContainerRootDescriptor1,
         DxilDescriptorRange1,
         DxilContainerDescriptorRange1>(pSrcData,
                                        SrcDataSizeInBytes,
                                        DxilRootSignatureVersion::Version_1_1,
                                        pRootSignature->Desc_1_1);
      break;

    default:
      IFT(E_FAIL);
      break;
    }
  } catch(...) {
    DeleteRootSignature(pRootSignature);
    throw;
  }

  *ppRootSignature = pRootSignature;
}

static DxilShaderVisibility GetVisibilityType(DXIL::ShaderKind ShaderKind) {
  switch(ShaderKind) {
  case DXIL::ShaderKind::Pixel:       return DxilShaderVisibility::Pixel;
  case DXIL::ShaderKind::Vertex:      return DxilShaderVisibility::Vertex;
  case DXIL::ShaderKind::Geometry:    return DxilShaderVisibility::Geometry;
  case DXIL::ShaderKind::Hull:        return DxilShaderVisibility::Hull;
  case DXIL::ShaderKind::Domain:      return DxilShaderVisibility::Domain;
  default:                            return DxilShaderVisibility::All;
  }
}

_Use_decl_annotations_
bool VerifyRootSignatureWithShaderPSV(const DxilVersionedRootSignatureDesc *pDesc,
                                      DXIL::ShaderKind ShaderKind,
                                      const void *pPSVData,
                                      uint32_t PSVSize,
                                      llvm::raw_ostream &DiagStream) {
  try {
    RootSignatureVerifier RSV;
    DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
    RSV.VerifyRootSignature(pDesc, DiagPrinter);
    RSV.VerifyShader(GetVisibilityType(ShaderKind), pPSVData, PSVSize, DiagPrinter);
  } catch (...) {
    return false;
  }

  return true;
}

bool VerifyRootSignature(_In_ const DxilVersionedRootSignatureDesc *pDesc,
                         _In_ llvm::raw_ostream &DiagStream) {
  try {
    RootSignatureVerifier RSV;
    DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
    RSV.VerifyRootSignature(pDesc, DiagPrinter);
  } catch (...) {
    return false;
  }

  return true;
}

} // namespace hlsl
