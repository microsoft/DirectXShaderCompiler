///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPipelineStateValidation.h                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Defines data used by the D3D runtime for PSO validation.                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXIL_PIPELINE_STATE_VALIDATION__H__
#define __DXIL_PIPELINE_STATE_VALIDATION__H__

// Versioning is additive and based on size
struct PSVRuntimeInfo0
{
  union {
    struct VSInfo {
      char OutputPositionPresent;
    } VS;
    struct HSInfo {
      UINT InputControlPointCount;      // max control points == 32
      UINT OutputControlPointCount;     // max control points == 32
      UINT TessellatorDomain;           // hlsl::DXIL::TessellatorDomain/D3D11_SB_TESSELLATOR_DOMAIN
      UINT TessellatorOutputPrimitive;  // hlsl::DXIL::TessellatorOutputPrimitive/D3D11_SB_TESSELLATOR_OUTPUT_PRIMITIVE
    } HS;
    struct DSInfo {
      UINT InputControlPointCount;      // max control points == 32
      char OutputPositionPresent;
      UINT TessellatorDomain;           // hlsl::DXIL::TessellatorDomain/D3D11_SB_TESSELLATOR_DOMAIN
    } DS;
    struct GSInfo {
      UINT InputPrimitive;              // hlsl::DXIL::InputPrimitive/D3D10_SB_PRIMITIVE
      UINT OutputTopology;              // hlsl::DXIL::PrimitiveTopology/D3D10_SB_PRIMITIVE_TOPOLOGY
      UINT OutputStreamMask;            // max streams == 4
      char OutputPositionPresent;
    } GS;
    struct PSInfo {
      char DepthOutput;
      char SampleFrequency;
    } PS;
  };
  UINT MinimumExpectedWaveLaneCount;  // minimum lane count required, 0 if unused
  UINT MaximumExpectedWaveLaneCount;  // maximum lane count required, 0xffffffff if unused
};
// PSVRuntimeInfo1 would derive and extend

enum class PSVResourceType
{
  Invalid = 0,

  Sampler,
  CBV,
  SRVTyped,
  SRVRaw,
  SRVStructured,
  UAVTyped,
  UAVRaw,
  UAVStructured,
  UAVStructuredWithCounter,

  NumEntries
};

// Versioning is additive and based on size
struct PSVResourceBindInfo0
{
  UINT ResType;     // PSVResourceType
  UINT Space;
  UINT LowerBound;
  UINT UpperBound;
};
// PSVResourceBindInfo1 would derive and extend

class DxilPipelineStateValidation
{
  UINT m_uPSVRuntimeInfoSize;
  PSVRuntimeInfo0* m_pPSVRuntimeInfo0;
  UINT m_uResourceCount;
  UINT m_uPSVResourceBindInfoSize;
  void* m_pPSVResourceBindInfo;
  UINT m_uSize;

public:
  DxilPipelineStateValidation() : 
    m_uPSVRuntimeInfoSize(0),
    m_pPSVRuntimeInfo0(nullptr),
    m_uResourceCount(0),
    m_uPSVResourceBindInfoSize(0),
    m_pPSVResourceBindInfo(nullptr)
  {
  }

  // Init() from PSV0 blob part that looks like:
  // UINT PSVRuntimeInfo_size
  // { PSVRuntimeInfoN structure }
  // UINT ResourceCount
  // ---  end of blob if ResourceCount == 0  ---
  // UINT PSVResourceBindInfo_size
  // { PSVResourceBindInfoN structure } * ResourceCount
  // returns true if no errors occurred.
  bool InitFromPSV0(const void* pBits, UINT size) {
    if(!(pBits != nullptr)) return false;
    const BYTE* pCurBits = (BYTE*)pBits;
    UINT minsize = sizeof(PSVRuntimeInfo0) + sizeof(UINT) * 2;
    if(!(size >= minsize)) return false;
    m_uPSVRuntimeInfoSize = *((const UINT*)pCurBits);
    if(!(m_uPSVRuntimeInfoSize >= sizeof(PSVRuntimeInfo0))) return false;
    pCurBits += sizeof(UINT);
    minsize = m_uPSVRuntimeInfoSize + sizeof(UINT) * 2;
    if(!(size >= minsize)) return false;
    m_pPSVRuntimeInfo0 = const_cast<PSVRuntimeInfo0*>((const PSVRuntimeInfo0*)pCurBits);
    pCurBits += m_uPSVRuntimeInfoSize;
    m_uResourceCount = *(const UINT*)pCurBits;
    pCurBits += sizeof(UINT);
    if (m_uResourceCount > 0) {
      minsize += sizeof(UINT);
      if(!(size >= minsize)) return false;
      m_uPSVResourceBindInfoSize = *(const UINT*)pCurBits;
      pCurBits += sizeof(UINT);
      minsize += m_uPSVResourceBindInfoSize * m_uResourceCount;
      if(!(m_uPSVResourceBindInfoSize >= sizeof(PSVResourceBindInfo0))) return false;
      if(!(size >= minsize)) return false;
      m_pPSVResourceBindInfo = static_cast<void*>(const_cast<BYTE*>(pCurBits));
    }
    return true;
  }

  // Initialize a new buffer
  // call with null pBuffer to get required size
  bool InitNew(UINT ResourceCount, void *pBuffer, UINT *pSize) {
    if(!(pSize)) return false;
    UINT size = sizeof(PSVRuntimeInfo0) + sizeof(UINT) * 2;
    if (ResourceCount) {
      size += sizeof(UINT) + (sizeof(PSVResourceBindInfo0) * ResourceCount);
    }
    if (pBuffer) {
      if(!(*pSize >= size)) return false;
    } else {
      *pSize = size;
      return true;
    }
    ::ZeroMemory(pBuffer, size);
    m_uPSVRuntimeInfoSize = sizeof(PSVRuntimeInfo0);
    BYTE* pCurBits = (BYTE*)pBuffer;
    *(UINT*)pCurBits = sizeof(PSVRuntimeInfo0);
    pCurBits += sizeof(UINT);
    m_pPSVRuntimeInfo0 = (PSVRuntimeInfo0*)pCurBits;
    pCurBits += sizeof(PSVRuntimeInfo0);

    // Set resource info:
    m_uResourceCount = ResourceCount;
    *(UINT*)pCurBits = ResourceCount;
    pCurBits += sizeof(UINT);
    if (ResourceCount > 0) {
      m_uPSVResourceBindInfoSize = sizeof(PSVResourceBindInfo0);
      *(UINT*)pCurBits = m_uPSVResourceBindInfoSize;
      pCurBits += sizeof(UINT);
      m_pPSVResourceBindInfo = pCurBits;
    }
    return true;
  }

  PSVRuntimeInfo0* GetPSVRuntimeInfo0() {
    return m_pPSVRuntimeInfo0;
  }

  UINT GetBindCount() const {
    return m_uResourceCount;
  }

  PSVResourceBindInfo0* GetPSVResourceBindInfo0(UINT index) {
    if (index < m_uResourceCount && m_pPSVResourceBindInfo &&
        sizeof(PSVResourceBindInfo0) <= m_uPSVResourceBindInfoSize) {
      return (PSVResourceBindInfo0*)((BYTE*)m_pPSVResourceBindInfo +
        (index * m_uPSVResourceBindInfoSize));
    }
    return nullptr;
  }
};


#endif  // __DXIL_PIPELINE_STATE_VALIDATION__H__
