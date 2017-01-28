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

#include "dxc/HLSL/DxilRootSignature.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/dxcapi.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace hlsl {

DEFINE_ENUM_FLAG_OPERATORS(DxilRootSignatureFlags)
DEFINE_ENUM_FLAG_OPERATORS(DxilRootDescriptorFlags)
DEFINE_ENUM_FLAG_OPERATORS(DxilDescriptorRangeType)
DEFINE_ENUM_FLAG_OPERATORS(DxilDescriptorRangeFlags)

//////////////////////////////////////////////////////////////////////////////
// Error handling helper.

static void ErrorRootSignature(IStream *pStream, const char *pFormat, ...) {
  va_list ArgList;
  char Buf[2048];
  if (pStream == nullptr)
    return;

  va_start(ArgList, pFormat);
  HRESULT hr = StringCchVPrintfA(Buf, _countof(Buf), pFormat, ArgList);
  va_end(ArgList);

  ULONG written;
  IFT(hr);
  IFT(pStream->Write(Buf, strlen(Buf), &written));
}

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

void RootSignatureHandle::LoadSerialized(const uint8_t *pData,
                                         unsigned length) {
  DXASSERT_NOMSG(IsEmpty());
  IFT(DxcCreateBlobOnHeapCopy(pData, length, &m_pSerialized));
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

  HRESULT Compact(__out_bcount(cbSize) char *pData, unsigned cbSize);
  unsigned GetSize();

protected:
  unsigned m_cbSegments;
  Segment *m_pSegment;
  Segment **m_ppSegment;
};

SimpleSerializer::SimpleSerializer() {
  m_cbSegments = 0;
  m_pSegment = NULL;
  m_ppSegment = &m_pSegment;
}

SimpleSerializer::~SimpleSerializer() {
  while (m_pSegment) {
    Segment *pSegment = m_pSegment;
    m_pSegment = pSegment->pNext;

    if (pSegment->bOwner) {
      delete pSegment->pData;
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
  pSegment->pNext = NULL;

  m_cbSegments += pSegment->cbSize;
  *m_ppSegment = pSegment;
  m_ppSegment = &pSegment->pNext;

  if (pOffset != NULL) {
    *pOffset = pSegment->Offset;
  }

  return S_OK;
}

HRESULT SimpleSerializer::ReserveBlock(void **ppData, unsigned cbSize,
                                       unsigned *pOffset) {
  HRESULT hr = S_OK;
  Segment *pSegment = NULL;
  void *pClonedData = NULL;

  IFCOOM(pSegment = new (std::nothrow) Segment);
  pSegment->pData = NULL;

  IFCOOM(pClonedData = new (std::nothrow) char[cbSize]);
  pSegment->pData = pClonedData;

  m_cbSegments = (m_cbSegments + 3) & ~3;
  pSegment->Offset = m_cbSegments;
  pSegment->cbSize = cbSize;
  pSegment->bOwner = true;
  pSegment->pNext = NULL;

  m_cbSegments += pSegment->cbSize;
  *m_ppSegment = pSegment;
  m_ppSegment = &pSegment->pNext;

  *ppData = pClonedData;
  if (pOffset) {
    *pOffset = pSegment->Offset;
  }

Cleanup:
  if (FAILED(hr)) {
    delete[] pClonedData;
    delete pSegment;
  }
  return hr;
}

HRESULT SimpleSerializer::Compact(__out_bcount(cbSize) char *pData,
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
  std::vector<T> m_set;
public:
  T* FindIntersectingInterval(const T &I) {
    DXASSERT(m_set.size() < INT_MAX,
             "else too many interval entries, and min<max check can undeflow");
    int mid, min = 0, max = (int)m_set.size();
    while (min < max) {
      mid = (min + max) / 2;
      T &R = m_set[mid];
      int order = I.overlap(R);
      if (order == 0) return &R;
      if (order < 0)
        max = mid - 1;
      else
        min = mid + 1;
    }
    return nullptr;
  }
  void Insert(const T& value) {
    // Find the first element that is greater or equal to value.
    auto it = std::lower_bound(m_set.begin(), m_set.end(), value);
    if (it == m_set.end()) {
      m_set.push_back(value);
    }
    else {
      m_set.insert(it, value);
#if DBG
      // Verify that the insertion didn't violate disjoint range assumptions.
      for (size_t i = 1; i < m_set.size(); ++i) {
        DXASSERT_NOMSG(m_set[i - 1].overlap(m_set[i]));
        DXASSERT_NOMSG(m_set[i - 1].space < m_set[i].space ||
                       m_set[i - 1].ub < m_set[i].lb);
      }
#endif
    }
  }
};

//////////////////////////////////////////////////////////////////////////////
// Verifier classes.

class DescriptorTableVerifier {
public:
  HRESULT Verify(const DxilDescriptorRange1 *pRanges, unsigned NumRanges,
                 unsigned iRTS, IStream *pErrors);
};

class StaticSamplerVerifier {
public:
  HRESULT Verify(const DxilStaticSamplerDesc *pDesc, IStream *pErrors);
};

class RootSignatureVerifier {
public:
  RootSignatureVerifier();
  ~RootSignatureVerifier();

  // Call this before calling VerifyShader, as it accumulates root signature
  // state.
  HRESULT
  VerifyRootSignature(const DxilVersionedRootSignatureDesc *pRootSignature,
                      IStream *pErrors);
  void AllowReservedRegisterSpace(bool bAllow);

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
        (space == other.space && lb < other.lb);
    }
    // Like a regular -1,0,1 comparison, but 0 indicates overlap.
    int overlap(const RegisterRange& other) const {
      if (space < other.space) return -1;
      if (space > other.space) return 1;
      if (ub < other.lb) return -1;
      if (lb > other.ub) return 1;
      return 0;
    }
  };
  typedef CIntervalCollection<RegisterRange> RegisterRanges;

  HRESULT AddRegisterRange(unsigned iRTS, NODE_TYPE nt, unsigned iDTS,
                           DxilDescriptorRangeType DescType,
                           DxilShaderVisibility VisType,
                           unsigned NumRegisters, unsigned BaseRegister,
                           unsigned RegisterSpace, IStream *pErrors);

  RegisterRanges &
  GetRanges(DxilShaderVisibility VisType, DxilDescriptorRangeType DescType) {
    return RangeKinds[(unsigned)VisType][(unsigned)DescType];
  }

  RegisterRanges RangeKinds[kMaxVisType + 1][kMaxDescType + 1];
  bool m_bAllowReservedRegisterSpace;
  DxilRootSignatureFlags m_RootSignatureFlags;
};

HRESULT DescriptorTableVerifier::Verify(const DxilDescriptorRange1 *pRanges,
                                        UINT NumRanges, UINT iRP,
                                        IStream *pErrors) {
  bool bHasSamplers = false;
  bool bHasResources = false;

  UINT64 iAppendStartSlot = 0;
  for (UINT iDTS = 0; iDTS < NumRanges; iDTS++) {
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
      ErrorRootSignature(pErrors,
        "Unsupported RangeType value %u (descriptor table slot [%u], root parameter [%u]).\n",
        pRange->RangeType, iDTS, iRP);
      return E_FAIL;
    }

    // Samplers cannot be mixed with other resources.
    if (bHasResources && bHasSamplers) {
      ErrorRootSignature(pErrors, "Samplers cannot be mixed with other "
                                  "resource types in a descriptor table (root "
                                  "parameter [%u]).\n",
                         iRP);
      return E_FAIL;
    }

    // NumDescriptors is not 0.
    if (pRange->NumDescriptors == 0) {
      ErrorRootSignature(pErrors, "NumDescriptors cannot be 0 (descriptor "
                                  "table slot [%u], root parameter [%u]).\n",
                         iDTS, iRP);
      return E_FAIL;
    }

    // Range start.
    UINT64 iStartSlot = iAppendStartSlot;
    if (pRange->OffsetInDescriptorsFromTableStart !=
        DxilDescriptorRangeOffsetAppend) {
      iStartSlot = pRange->OffsetInDescriptorsFromTableStart;
    }
    if (iStartSlot > UINT_MAX) {
      ErrorRootSignature(pErrors, "Cannot append range with implicit lower "
                                  "bound after an unbounded range (descriptor "
                                  "table slot [%u], root parameter [%u]).\n",
                         iDTS, iRP);
      return E_FAIL;
    }

    // Descriptor range and shader register range overlow.
    if (pRange->NumDescriptors != UINT_MAX) {
      // Bounded range.
      UINT64 ub1 = (UINT64)pRange->BaseShaderRegister +
                   (UINT64)pRange->NumDescriptors - 1ull;
      if (ub1 > UINT_MAX) {
        ErrorRootSignature(
            pErrors, "Overflow for shader register range: "
                     "BaseShaderRegister=%u, NumDescriptor=%u; (descriptor "
                     "table slot [%u], root parameter [%u]).\n",
            pRange->BaseShaderRegister, pRange->NumDescriptors, iDTS, iRP);
        return E_FAIL;
      }

      UINT64 ub2 = (UINT64)iStartSlot + (UINT64)pRange->NumDescriptors - 1ull;
      if (ub2 > UINT_MAX) {
        ErrorRootSignature(pErrors, "Overflow for descriptor range (descriptor "
                                    "table slot [%u], root parameter [%u])\n",
                           iDTS, iRP);
        return E_FAIL;
      }

      iAppendStartSlot = iStartSlot + (UINT64)pRange->NumDescriptors;
    } else {
      // Unbounded range.
      iAppendStartSlot = 1ull + (UINT64)UINT_MAX;
    }
  }

  return S_OK;
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

HRESULT RootSignatureVerifier::AddRegisterRange(unsigned iRP,
  NODE_TYPE nt,
  unsigned iDTS,
  DxilDescriptorRangeType DescType,
  DxilShaderVisibility VisType,
  unsigned NumRegisters,
  unsigned BaseRegister,
  unsigned RegisterSpace,
  IStream *pErrors)
{
  RegisterRange interval;
  interval.space = RegisterSpace;
  interval.lb = BaseRegister;
  interval.ub = (NumRegisters != UINT_MAX) ? BaseRegister + NumRegisters - 1 : UINT_MAX;
  interval.nt = nt;
  interval.iDTS = iDTS;
  interval.iRP = iRP;

  if (!m_bAllowReservedRegisterSpace &&
    (RegisterSpace >= DxilSystemReservedRegisterSpaceValuesStart) &&
    (RegisterSpace <= DxilSystemReservedRegisterSpaceValuesEnd))
  {
    if (nt == DESCRIPTOR_TABLE_ENTRY)
    {
      ErrorRootSignature(pErrors,
        "Root parameter [%u] descriptor table entry [%u] specifies RegisterSpace=%#x, which is invalid since RegisterSpace values in the range [%#x,%#x] are reserved for system use.\n",
        iRP, iDTS, RegisterSpace, DxilSystemReservedRegisterSpaceValuesStart, DxilSystemReservedRegisterSpaceValuesEnd);
      return E_FAIL;
    }
    else
    {
      ErrorRootSignature(pErrors,
        "Root parameter [%u] specifies RegisterSpace=%#x, which is invalid since RegisterSpace values in the range [%#x,%#x] are reserved for system use.\n",
        iRP, RegisterSpace, DxilSystemReservedRegisterSpaceValuesStart, DxilSystemReservedRegisterSpaceValuesEnd);
      return E_FAIL;
    }
  }

  RegisterRange *pNode = NULL;
  DxilShaderVisibility NodeVis = VisType;
  if (VisType == DxilShaderVisibility::All) {
    // Check for overlap with each visibility type.
    for (unsigned iVT = kMinVisType; iVT <= kMaxVisType; iVT++) {
      pNode = GetRanges((DxilShaderVisibility)iVT, DescType)
                  .FindIntersectingInterval(interval);
      if (pNode != NULL)
        break;
    }
  } else {
    // Check for overlap with the same visibility.
    pNode = GetRanges(VisType, DescType).FindIntersectingInterval(interval);

    // Check for overlap with ALL visibility.
    if (pNode == NULL) {
      pNode = GetRanges(DxilShaderVisibility::All, DescType)
                  .FindIntersectingInterval(interval);
      NodeVis = DxilShaderVisibility::All;
    }
  }

  if (pNode != NULL)
  {
    const int strSize = 132;
    char testString[strSize];
    char nodeString[strSize];
    switch (nt)
    {
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
    ErrorRootSignature(pErrors,
      "Shader register range of type %s %s overlaps with another shader register range %s.\n",
      RangeTypeString(DescType), testString, nodeString);
    return E_FAIL;
  }

  // Insert node.
  GetRanges(VisType, DescType).Insert(interval);

  return S_OK;
}

static DxilDescriptorRangeType GetRangeType(DxilRootParameterType RPT) {
  switch (RPT) {
  case DxilRootParameterType::CBV: return DxilDescriptorRangeType::CBV;
  case DxilRootParameterType::SRV: return DxilDescriptorRangeType::SRV;
  case DxilRootParameterType::UAV: return DxilDescriptorRangeType::UAV;
  }

  DXASSERT_NOMSG(false);
  return DxilDescriptorRangeType::SRV;
}

HRESULT RootSignatureVerifier::VerifyRootSignature(const DxilVersionedRootSignatureDesc *pVersionedRootSignature,
  IStream *pErrors)
{
  const DxilVersionedRootSignatureDesc *pUpconvertedRS = NULL;
  if (pVersionedRootSignature == nullptr) {
    return E_INVALIDARG;
  }

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
  if ((unsigned)(pRootSignature->Flags & ~DxilRootSignatureFlags::ValidFlags) != 0)
  {
    ErrorRootSignature(pErrors,
      "Unsupported bit-flag set (root signature flags %x).\n", pRootSignature->Flags);
    return E_FAIL;
  }

  m_RootSignatureFlags = pRootSignature->Flags;

  for (unsigned iRP = 0; iRP < pRootSignature->NumParameters; iRP++)
  {
    const DxilRootParameter1 *pSlot = &pRootSignature->pParameters[iRP];
    // Shader visibility.
    DxilShaderVisibility Visibility = pSlot->ShaderVisibility;
    if (!IsDxilShaderVisibility(Visibility)) {
      ErrorRootSignature(pErrors,
        "Unsupported ShaderVisibility value %u (root parameter [%u]).\n", Visibility, iRP);
      return E_FAIL;
    }

    DxilRootParameterType ParameterType = pSlot->ParameterType;
    switch (ParameterType)
    {
    case DxilRootParameterType::DescriptorTable:
    {
      DescriptorTableVerifier DTV;
      IFR(DTV.Verify(pSlot->DescriptorTable.pDescriptorRanges,
                     pSlot->DescriptorTable.NumDescriptorRanges, iRP, pErrors));

      for (unsigned iDTS = 0; iDTS < pSlot->DescriptorTable.NumDescriptorRanges; iDTS++)
      {
        const DxilDescriptorRange1 *pRange = &pSlot->DescriptorTable.pDescriptorRanges[iDTS];
        unsigned RangeFlags = (unsigned)pRange->Flags;

        // Verify range flags.
        if (RangeFlags & ~(unsigned)DxilDescriptorRangeFlags::ValidFlags)
        {
          ErrorRootSignature(pErrors,
            "Unsupported bit-flag set (descriptor range flags %x).\n", pRange->Flags);
          return E_FAIL;
        }
        switch (pRange->RangeType)
        {
        case DxilDescriptorRangeType::Sampler:
        {
          if (RangeFlags & (unsigned)(
            DxilDescriptorRangeFlags::DataVolatile |
            DxilDescriptorRangeFlags::DataStatic |
            DxilDescriptorRangeFlags::DataStaticWhileSetAtExecute))
          {
            ErrorRootSignature(pErrors,
              "Sampler descriptor ranges can't specify DATA_* flags since there is no data pointed to by samplers (descriptor range flags %x).\n", pRange->Flags);
            return E_FAIL;
          }
          break;
        }
        default:
        {
          unsigned NumDataFlags = 0;
          if (RangeFlags & (unsigned)DxilDescriptorRangeFlags::DataVolatile) { NumDataFlags++; }
          if (RangeFlags & (unsigned)DxilDescriptorRangeFlags::DataStatic) { NumDataFlags++; }
          if (RangeFlags & (unsigned)DxilDescriptorRangeFlags::DataStaticWhileSetAtExecute) { NumDataFlags++; }
          if (NumDataFlags > 1)
          {
            ErrorRootSignature(pErrors,
              "Descriptor range flags cannot specify more than one DATA_* flag at a time (descriptor range flags %x).\n", pRange->Flags);
            return E_FAIL;
          }
          if ((RangeFlags & (unsigned)DxilDescriptorRangeFlags::DataStatic) && (RangeFlags & (unsigned)DxilDescriptorRangeFlags::DescriptorsVolatile))
          {
            ErrorRootSignature(pErrors,
              "Descriptor range flags cannot specify DESCRIPTORS_VOLATILE with the DATA_STATIC flag at the same time (descriptor range flags %x). "
              "DATA_STATIC_WHILE_SET_AT_EXECUTE is fine to combine with DESCRIPTORS_VOLATILE, since DESCRIPTORS_VOLATILE still requires descriptors don't change during execution. \n", pRange->Flags);
            return E_FAIL;
          }
          break;
        }
        }

        IFR(AddRegisterRange(iRP,
          DESCRIPTOR_TABLE_ENTRY,
          iDTS,
          pRange->RangeType,
          Visibility,
          pRange->NumDescriptors,
          pRange->BaseShaderRegister,
          pRange->RegisterSpace,
          pErrors));
      }

      break;
    }

    case DxilRootParameterType::Constants32Bit:
      IFR(AddRegisterRange(iRP,
        ROOT_CONSTANT,
        (unsigned)-1,
        DxilDescriptorRangeType::CBV,
        Visibility,
        1,
        pSlot->Constants.ShaderRegister,
        pSlot->Constants.RegisterSpace,
        pErrors));
      break;

    case DxilRootParameterType::CBV:
    case DxilRootParameterType::SRV:
    case DxilRootParameterType::UAV:
    {
      // Verify root descriptor flags.
      unsigned Flags = (unsigned)pSlot->Descriptor.Flags;
      if (Flags & ~(unsigned)DxilRootDescriptorFlags::ValidFlags)
      {
        ErrorRootSignature(pErrors,
          "Unsupported bit-flag set (root descriptor flags %x).\n", Flags);
        return E_FAIL;
      }

      unsigned NumDataFlags = 0;
      if (Flags & (unsigned)DxilRootDescriptorFlags::DataVolatile) { NumDataFlags++; }
      if (Flags & (unsigned)DxilRootDescriptorFlags::DataStatic) { NumDataFlags++; }
      if (Flags & (unsigned)DxilRootDescriptorFlags::DataStaticWhileSetAtExecute) { NumDataFlags++; }
      if (NumDataFlags > 1) {
        ErrorRootSignature(pErrors, "Root descriptor flags cannot specify more "
                                    "than one DATA_* flag at a time (root "
                                    "descriptor flags %x).\n",
                           Flags);
        return E_FAIL;
      }

      IFR(AddRegisterRange(iRP, ROOT_DESCRIPTOR, (unsigned)-1,
                           GetRangeType(ParameterType), Visibility, 1,
                           pSlot->Descriptor.ShaderRegister,
                           pSlot->Descriptor.RegisterSpace, pErrors));
      break;
    }

    default:
      ErrorRootSignature(pErrors,
        "Unsupported ParameterType value %u (root parameter %u)\n", ParameterType, iRP);
      return E_FAIL;
    }
  }

  for (unsigned iSS = 0; iSS < pRootSignature->NumStaticSamplers; iSS++)
  {
    const DxilStaticSamplerDesc *pSS = &pRootSignature->pStaticSamplers[iSS];
    // Shader visibility.
    DxilShaderVisibility Visibility = pSS->ShaderVisibility;
    if (!IsDxilShaderVisibility(Visibility)) {
      ErrorRootSignature(pErrors,
        "Unsupported ShaderVisibility value %u (static sampler [%u]).\n", Visibility, iSS);
      return E_FAIL;
    }

    StaticSamplerVerifier SSV;
    IFR(SSV.Verify(pSS, pErrors));
    IFR(AddRegisterRange(iSS, STATIC_SAMPLER, (unsigned)-1,
                         DxilDescriptorRangeType::Sampler, Visibility, 1,
                         pSS->ShaderRegister, pSS->RegisterSpace, pErrors));
  }

  return S_OK;
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
HRESULT StaticSamplerVerifier::Verify(const DxilStaticSamplerDesc* pDesc, IStream* pErrors)
{
  if (!pDesc) {
    ErrorRootSignature(pErrors,
                       "Static sampler: A NULL pSamplerDesc was specified.");
    return E_INVALIDARG;
  }

  bool bIsComparison = false;
  switch (pDesc->Filter)
  {
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
    ErrorRootSignature(pErrors, "Static sampler: Filter unrecognized.");
    return E_FAIL;
  }

  if (!IsDxilTextureAddressMode(pDesc->AddressU)) {
    ErrorRootSignature(pErrors, "Static sampler: AddressU unrecognized.");
    return E_FAIL;
  }
  if (!IsDxilTextureAddressMode(pDesc->AddressV)) {
    ErrorRootSignature(pErrors, "Static sampler: AddressV unrecognized.");
    return E_FAIL;
  }
  if (!IsDxilTextureAddressMode(pDesc->AddressW)) {
    ErrorRootSignature(pErrors, "Static sampler: AddressW unrecognized.");
    return E_FAIL;
  }

  if (isNaN(pDesc->MipLODBias) || (pDesc->MipLODBias < DxilMipLodBiaxMin) ||
      (pDesc->MipLODBias > DxilMipLodBiaxMax)) {
    ErrorRootSignature(pErrors, "Static sampler: MipLODBias must be in the "
                                "range [%f to %f].  %f specified.",
                       DxilMipLodBiaxMin, DxilMipLodBiaxMax, pDesc->MipLODBias);
    return E_FAIL;
  }

  if (pDesc->MaxAnisotropy > DxilMapAnisotropy)
  {
    ErrorRootSignature(pErrors,
      "Static sampler: MaxAnisotropy must be in the range [0 to %d].  %d specified.",
      DxilMapAnisotropy, pDesc->MaxAnisotropy);
    return E_FAIL;
  }

  if (bIsComparison && !IsDxilComparisonFunc(pDesc->ComparisonFunc)) {
    ErrorRootSignature(pErrors, "Static sampler: ComparisonFunc unrecognized.");
    return E_FAIL;
  }

  if (isNaN(pDesc->MinLOD)) {
    ErrorRootSignature(
        pErrors,
        "Static sampler: MinLOD be in the range [-INF to +INF].  %f specified.",
        pDesc->MinLOD);
    return E_FAIL;
  }

  if (isNaN(pDesc->MaxLOD)) {
    ErrorRootSignature(
        pErrors,
        "Static sampler: MaxLOD be in the range [-INF to +INF].  %f specified.",
        pDesc->MaxLOD);
    return E_FAIL;
  }

  return S_OK;
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
static DxilRootDescriptorFlags GetFlags(const DxilContainerRootDescriptor1 &D)
{
  return (DxilRootDescriptorFlags)D.Flags;
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
HRESULT ConvertRootSignatureTemplate(const IN_DXIL_ROOT_SIGNATURE_DESC & DescIn,
    DxilRootSignatureVersion DescVersionOut,
    OUT_DXIL_ROOT_SIGNATURE_DESC & DescOut)
{
  HRESULT hr = S_OK;
  const IN_DXIL_ROOT_SIGNATURE_DESC * pDescIn = &DescIn;
  OUT_DXIL_ROOT_SIGNATURE_DESC * pDescOut = &DescOut;

  // Root signature descriptor.
  pDescOut->Flags = pDescIn->Flags;
  pDescOut->NumParameters = 0;
  pDescOut->NumStaticSamplers = 0;
  // Intialize all pointers early so that clean up works properly.
  pDescOut->pParameters = NULL;
  pDescOut->pStaticSamplers = NULL;

  // Root signature parameters.
  if (pDescIn->NumParameters > 0)
  {
    IFCOOM(pDescOut->pParameters = new (std::nothrow) OUT_DXIL_ROOT_PARAMETER[pDescIn->NumParameters]);
    pDescOut->NumParameters = pDescIn->NumParameters;
    memset((void *)pDescOut->pParameters, 0, pDescOut->NumParameters*sizeof(OUT_DXIL_ROOT_PARAMETER));
  }

  for (unsigned iRP = 0; iRP < pDescIn->NumParameters; iRP++)
  {
    const auto & ParamIn = pDescIn->pParameters[iRP];
    OUT_DXIL_ROOT_PARAMETER & ParamOut = (OUT_DXIL_ROOT_PARAMETER &)pDescOut->pParameters[iRP];

    ParamOut.ParameterType = ParamIn.ParameterType;
    ParamOut.ShaderVisibility = ParamIn.ShaderVisibility;

    switch (ParamIn.ParameterType)
    {
    case DxilRootParameterType::DescriptorTable:
    {
      ParamOut.DescriptorTable.pDescriptorRanges = NULL;
      unsigned NumRanges = ParamIn.DescriptorTable.NumDescriptorRanges;
      if (NumRanges > 0)
      {
        IFCOOM(ParamOut.DescriptorTable.pDescriptorRanges = new (std::nothrow) OUT_DXIL_DESCRIPTOR_RANGE[NumRanges]);
        ParamOut.DescriptorTable.NumDescriptorRanges = NumRanges;
      }

      for (unsigned i = 0; i < NumRanges; i++)
      {
        const auto & RangeIn = ParamIn.DescriptorTable.pDescriptorRanges[i];
        OUT_DXIL_DESCRIPTOR_RANGE & RangeOut = (OUT_DXIL_DESCRIPTOR_RANGE &)ParamOut.DescriptorTable.pDescriptorRanges[i];

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
    case DxilRootParameterType::Constants32Bit:
    {
      ParamOut.Constants.Num32BitValues = ParamIn.Constants.Num32BitValues;
      ParamOut.Constants.ShaderRegister = ParamIn.Constants.ShaderRegister;
      ParamOut.Constants.RegisterSpace = ParamIn.Constants.RegisterSpace;
      break;
    }
    case DxilRootParameterType::CBV:
    case DxilRootParameterType::SRV:
    case DxilRootParameterType::UAV:
    {
      ParamOut.Descriptor.ShaderRegister = ParamIn.Descriptor.ShaderRegister;
      ParamOut.Descriptor.RegisterSpace = ParamIn.Descriptor.RegisterSpace;
      DxilRootDescriptorFlags Flags = GetFlags(ParamIn.Descriptor);
      SetFlags(ParamOut.Descriptor, Flags);
      break;
    }
    default:
      IFC(E_FAIL);
    }
  }

  // Static samplers.
  if (pDescIn->NumStaticSamplers > 0)
  {
    IFCOOM(pDescOut->pStaticSamplers = new (std::nothrow) DxilStaticSamplerDesc[pDescIn->NumStaticSamplers]);
    pDescOut->NumStaticSamplers = pDescIn->NumStaticSamplers;
    memcpy((void*)pDescOut->pStaticSamplers, pDescIn->pStaticSamplers, pDescOut->NumStaticSamplers*sizeof(DxilStaticSamplerDesc));
  }

Cleanup:
  // Note that in case of failure, outside code must deallocate memory.
  return hr;
}

void ConvertRootSignature(const DxilVersionedRootSignatureDesc * pRootSignatureIn,
  DxilRootSignatureVersion RootSignatureVersionOut,
  const DxilVersionedRootSignatureDesc ** ppRootSignatureOut)
{
  HRESULT hr = S_OK;
  DxilVersionedRootSignatureDesc *pRootSignatureOut = NULL;
  if (pRootSignatureIn == NULL || ppRootSignatureOut == NULL)
  {
    IFC(E_INVALIDARG);
  }
  *ppRootSignatureOut = NULL;

  if (pRootSignatureIn->Version == RootSignatureVersionOut)
  {
    // No conversion. Return the original root signature pointer; no cloning.
    *ppRootSignatureOut = pRootSignatureIn;
    goto Cleanup;
  }

  IFCOOM(pRootSignatureOut = new (std::nothrow) DxilVersionedRootSignatureDesc());
  memset(pRootSignatureOut, 0, sizeof(*pRootSignatureOut));

  // Convert root signature.
  switch (RootSignatureVersionOut)
  {
  case DxilRootSignatureVersion::Version_1_0:
    switch (pRootSignatureIn->Version)
    {
    case DxilRootSignatureVersion::Version_1_1:
      pRootSignatureOut->Version = DxilRootSignatureVersion::Version_1_0;
      hr = ConvertRootSignatureTemplate<
        DxilRootSignatureDesc1,
        DxilRootSignatureDesc,
        DxilRootParameter,
        DxilRootDescriptor,
        DxilDescriptorRange>(pRootSignatureIn->Desc_1_1,
          DxilRootSignatureVersion::Version_1_0,
          pRootSignatureOut->Desc_1_0);
      IFC(hr);
      break;
    default:
      IFC(E_INVALIDARG);
    }
    break;

  case DxilRootSignatureVersion::Version_1_1:
    switch (pRootSignatureIn->Version)
    {
    case DxilRootSignatureVersion::Version_1_0:
      pRootSignatureOut->Version = DxilRootSignatureVersion::Version_1_1;
      hr = ConvertRootSignatureTemplate<
        DxilRootSignatureDesc,
        DxilRootSignatureDesc1,
        DxilRootParameter1,
        DxilRootDescriptor1,
        DxilDescriptorRange1>(pRootSignatureIn->Desc_1_0,
          DxilRootSignatureVersion::Version_1_1,
          pRootSignatureOut->Desc_1_1);
      IFC(hr);
      break;
    default:
      IFC(E_INVALIDARG);
    }
    break;

  default:
    IFC(E_INVALIDARG);
    break;
  }
  *ppRootSignatureOut = pRootSignatureOut;

Cleanup:
  if (FAILED(hr)) {
    DeleteRootSignature(pRootSignatureOut);
    IFT(hr);
  }
}

template<typename T_ROOT_SIGNATURE_DESC,
  typename T_ROOT_PARAMETER,
  typename T_ROOT_DESCRIPTOR_INTERNAL,
  typename T_DESCRIPTOR_RANGE_INTERNAL>
HRESULT SerializeRootSignatureTemplate(__in const T_ROOT_SIGNATURE_DESC* pRootSignature,
    DxilRootSignatureVersion DescVersion,
    _COM_Outptr_ IDxcBlob** ppBlob,
    _COM_Outptr_ IStream* pErrors,
    __in bool bAllowReservedRegisterSpace)
{
  DxilContainerRootSignatureDesc RS;
  UINT Offset;
  SimpleSerializer Serializer;
  IFR(Serializer.AddBlock(&RS, sizeof(RS), &Offset));
  IFRBOOL(Offset == 0, E_FAIL);

  const T_ROOT_SIGNATURE_DESC *pRS = pRootSignature;
  RS.Version = (uint32_t)DescVersion;
  RS.Flags = (uint32_t)pRS->Flags;
  RS.NumParameters = pRS->NumParameters;
  RS.NumStaticSamplers = pRS->NumStaticSamplers;

  DxilContainerRootParameter *pRP;
  IFR(Serializer.ReserveBlock((void**)&pRP,
    sizeof(DxilContainerRootParameter)*RS.NumParameters, &RS.RootParametersOffset));
  for (UINT iRP = 0; iRP < RS.NumParameters; iRP++)
  {
    const T_ROOT_PARAMETER *pInRP = &pRS->pParameters[iRP];
    DxilContainerRootParameter *pOutRP = &pRP[iRP];
    pOutRP->ParameterType = (uint32_t)pInRP->ParameterType;
    pOutRP->ShaderVisibility = (uint32_t)pInRP->ShaderVisibility;
    switch (pInRP->ParameterType)
    {
    case DxilRootParameterType::DescriptorTable:
    {
      DxilContainerRootDescriptorTable *p1;
      IFR(Serializer.ReserveBlock((void**)&p1,
        sizeof(DxilContainerRootDescriptorTable), &pOutRP->PayloadOffset));
      p1->NumDescriptorRanges = pInRP->DescriptorTable.NumDescriptorRanges;

      T_DESCRIPTOR_RANGE_INTERNAL *p2;
      IFR(Serializer.ReserveBlock((void**)&p2,
        sizeof(T_DESCRIPTOR_RANGE_INTERNAL)*p1->NumDescriptorRanges, &p1->DescriptorRangesOffset));
      for (UINT i = 0; i < p1->NumDescriptorRanges; i++)
      {
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
    case DxilRootParameterType::Constants32Bit:
    {
      DxilRootConstants *p;
      IFR(Serializer.ReserveBlock((void**)&p, sizeof(DxilRootConstants), &pOutRP->PayloadOffset));
      p->Num32BitValues = pInRP->Constants.Num32BitValues;
      p->ShaderRegister = pInRP->Constants.ShaderRegister;
      p->RegisterSpace = pInRP->Constants.RegisterSpace;
      break;
    }
    case DxilRootParameterType::CBV:
    case DxilRootParameterType::SRV:
    case DxilRootParameterType::UAV:
    {
      T_ROOT_DESCRIPTOR_INTERNAL *p;
      IFR(Serializer.ReserveBlock((void**)&p, sizeof(T_ROOT_DESCRIPTOR_INTERNAL), &pOutRP->PayloadOffset));
      p->ShaderRegister = pInRP->Descriptor.ShaderRegister;
      p->RegisterSpace = pInRP->Descriptor.RegisterSpace;
      DxilRootDescriptorFlags Flags = GetFlags(pInRP->Descriptor);
      SetFlags(*p, Flags);
      break;
    }
    default:
      ErrorRootSignature(pErrors,
        "D3DSerializeRootSignature: unknown root parameter type (%u)\n", pInRP->ParameterType);
      return E_FAIL;
    }
  }

  DxilStaticSamplerDesc *pSS;
  unsigned StaticSamplerSize = sizeof(DxilStaticSamplerDesc)*RS.NumStaticSamplers;
  IFR(Serializer.ReserveBlock((void**)&pSS, StaticSamplerSize, &RS.StaticSamplersOffset));
  memcpy(pSS, pRS->pStaticSamplers, StaticSamplerSize);

  // Create the result blob.
  CComHeapPtr<char> bytes;
  CComPtr<IDxcBlob> pBlob;
  unsigned cb = Serializer.GetSize();
  DXASSERT_NOMSG((cb & 0x3) == 0);
  if (!bytes.AllocateBytes(cb))
    return E_OUTOFMEMORY;
  IFR(Serializer.Compact(bytes.m_pData, cb));
  IFR(DxcCreateBlobOnHeap(bytes.m_pData, cb, ppBlob));
  bytes.Detach(); // Ownership transfered to ppBlob.

  return S_OK;
}

_Use_decl_annotations_
void
SerializeRootSignature(const DxilVersionedRootSignatureDesc *pRootSignature,
                       IDxcBlob **ppBlob, IDxcBlobEncoding **ppErrorBlob,
                       bool bAllowReservedRegisterSpace) {
  DXASSERT_NOMSG(pRootSignature != nullptr);
  DXASSERT_NOMSG(ppBlob != nullptr);
  DXASSERT_NOMSG(ppErrorBlob != nullptr);

  *ppBlob = nullptr;
  *ppErrorBlob = nullptr;

  RootSignatureVerifier RSV;
  CComPtr<AbstractMemoryStream> pErrors;
  CComPtr<IMalloc> pMalloc;

  IFT(CoGetMalloc(1, &pMalloc));
  IFT(CreateMemoryStream(pMalloc, &pErrors));

  // Verify root signature.
  RSV.AllowReservedRegisterSpace(bAllowReservedRegisterSpace);
  if (FAILED(RSV.VerifyRootSignature(pRootSignature, pErrors))) {
    IFT(DxcCreateBlobWithEncodingFromStream(pErrors, true, CP_UTF8, ppErrorBlob));
    return;
  }

  switch (pRootSignature->Version)
  {
  case DxilRootSignatureVersion::Version_1_0:
    SerializeRootSignatureTemplate<
      DxilRootSignatureDesc,
      DxilRootParameter,
      DxilRootDescriptor,
      DxilContainerDescriptorRange>(&pRootSignature->Desc_1_0,
        DxilRootSignatureVersion::Version_1_0,
        ppBlob, pErrors,
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
        ppBlob, pErrors,
        bAllowReservedRegisterSpace);
    break;
  }
}

} // namespace hlsl
