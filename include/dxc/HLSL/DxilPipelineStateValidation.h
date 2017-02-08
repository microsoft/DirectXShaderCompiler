///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPipelineStateValidation.h                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Defines data used by the D3D runtime for PSO validation.                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXIL_PIPELINE_STATE_VALIDATION__H__
#define __DXIL_PIPELINE_STATE_VALIDATION__H__

#include <stdint.h>
#include <string.h>

// Versioning is additive and based on size
struct PSVRuntimeInfo0
{
  union {
    struct VSInfo {
      char OutputPositionPresent;
    } VS;
    struct HSInfo {
      uint32_t InputControlPointCount;      // max control points == 32
      uint32_t OutputControlPointCount;     // max control points == 32
      uint32_t TessellatorDomain;           // hlsl::DXIL::TessellatorDomain/D3D11_SB_TESSELLATOR_DOMAIN
      uint32_t TessellatorOutputPrimitive;  // hlsl::DXIL::TessellatorOutputPrimitive/D3D11_SB_TESSELLATOR_OUTPUT_PRIMITIVE
    } HS;
    struct DSInfo {
      uint32_t InputControlPointCount;      // max control points == 32
      char OutputPositionPresent;
      uint32_t TessellatorDomain;           // hlsl::DXIL::TessellatorDomain/D3D11_SB_TESSELLATOR_DOMAIN
    } DS;
    struct GSInfo {
      uint32_t InputPrimitive;              // hlsl::DXIL::InputPrimitive/D3D10_SB_PRIMITIVE
      uint32_t OutputTopology;              // hlsl::DXIL::PrimitiveTopology/D3D10_SB_PRIMITIVE_TOPOLOGY
      uint32_t OutputStreamMask;            // max streams == 4
      char OutputPositionPresent;
    } GS;
    struct PSInfo {
      char DepthOutput;
      char SampleFrequency;
    } PS;
  };
  uint32_t MinimumExpectedWaveLaneCount;  // minimum lane count required, 0 if unused
  uint32_t MaximumExpectedWaveLaneCount;  // maximum lane count required, 0xffffffff if unused
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
  uint32_t ResType;     // PSVResourceType
  uint32_t Space;
  uint32_t LowerBound;
  uint32_t UpperBound;
};
// PSVResourceBindInfo1 would derive and extend

class DxilPipelineStateValidation
{
  uint32_t m_uPSVRuntimeInfoSize;
  PSVRuntimeInfo0* m_pPSVRuntimeInfo0;
  uint32_t m_uResourceCount;
  uint32_t m_uPSVResourceBindInfoSize;
  void* m_pPSVResourceBindInfo;
  uint32_t m_uSize;

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
  // uint32_t PSVRuntimeInfo_size
  // { PSVRuntimeInfoN structure }
  // uint32_t ResourceCount
  // ---  end of blob if ResourceCount == 0  ---
  // uint32_t PSVResourceBindInfo_size
  // { PSVResourceBindInfoN structure } * ResourceCount
  // returns true if no errors occurred.
  bool InitFromPSV0(const void* pBits, uint32_t size) {
    if(!(pBits != nullptr)) return false;
    const uint8_t* pCurBits = (uint8_t*)pBits;
    uint32_t minsize = sizeof(PSVRuntimeInfo0) + sizeof(uint32_t) * 2;
    if(!(size >= minsize)) return false;
    m_uPSVRuntimeInfoSize = *((const uint32_t*)pCurBits);
    if(!(m_uPSVRuntimeInfoSize >= sizeof(PSVRuntimeInfo0))) return false;
    pCurBits += sizeof(uint32_t);
    minsize = m_uPSVRuntimeInfoSize + sizeof(uint32_t) * 2;
    if(!(size >= minsize)) return false;
    m_pPSVRuntimeInfo0 = const_cast<PSVRuntimeInfo0*>((const PSVRuntimeInfo0*)pCurBits);
    pCurBits += m_uPSVRuntimeInfoSize;
    m_uResourceCount = *(const uint32_t*)pCurBits;
    pCurBits += sizeof(uint32_t);
    if (m_uResourceCount > 0) {
      minsize += sizeof(uint32_t);
      if(!(size >= minsize)) return false;
      m_uPSVResourceBindInfoSize = *(const uint32_t*)pCurBits;
      pCurBits += sizeof(uint32_t);
      minsize += m_uPSVResourceBindInfoSize * m_uResourceCount;
      if(!(m_uPSVResourceBindInfoSize >= sizeof(PSVResourceBindInfo0))) return false;
      if(!(size >= minsize)) return false;
      m_pPSVResourceBindInfo = static_cast<void*>(const_cast<uint8_t*>(pCurBits));
    }
    return true;
  }

  // Initialize a new buffer
  // call with null pBuffer to get required size
  bool InitNew(uint32_t ResourceCount, void *pBuffer, uint32_t *pSize) {
    if(!(pSize)) return false;
    uint32_t size = sizeof(PSVRuntimeInfo0) + sizeof(uint32_t) * 2;
    if (ResourceCount) {
      size += sizeof(uint32_t) + (sizeof(PSVResourceBindInfo0) * ResourceCount);
    }
    if (pBuffer) {
      if(!(*pSize >= size)) return false;
    } else {
      *pSize = size;
      return true;
    }
    memset(pBuffer, 0, size);
    m_uPSVRuntimeInfoSize = sizeof(PSVRuntimeInfo0);
    uint8_t* pCurBits = (uint8_t*)pBuffer;
    *(uint32_t*)pCurBits = sizeof(PSVRuntimeInfo0);
    pCurBits += sizeof(uint32_t);
    m_pPSVRuntimeInfo0 = (PSVRuntimeInfo0*)pCurBits;
    pCurBits += sizeof(PSVRuntimeInfo0);

    // Set resource info:
    m_uResourceCount = ResourceCount;
    *(uint32_t*)pCurBits = ResourceCount;
    pCurBits += sizeof(uint32_t);
    if (ResourceCount > 0) {
      m_uPSVResourceBindInfoSize = sizeof(PSVResourceBindInfo0);
      *(uint32_t*)pCurBits = m_uPSVResourceBindInfoSize;
      pCurBits += sizeof(uint32_t);
      m_pPSVResourceBindInfo = pCurBits;
    }
    return true;
  }

  PSVRuntimeInfo0* GetPSVRuntimeInfo0() {
    return m_pPSVRuntimeInfo0;
  }

  uint32_t GetBindCount() const {
    return m_uResourceCount;
  }

  PSVResourceBindInfo0* GetPSVResourceBindInfo0(uint32_t index) {
    if (index < m_uResourceCount && m_pPSVResourceBindInfo &&
        sizeof(PSVResourceBindInfo0) <= m_uPSVResourceBindInfoSize) {
      return (PSVResourceBindInfo0*)((uint8_t*)m_pPSVResourceBindInfo +
        (index * m_uPSVResourceBindInfoSize));
    }
    return nullptr;
  }
};


#endif  // __DXIL_PIPELINE_STATE_VALIDATION__H__
