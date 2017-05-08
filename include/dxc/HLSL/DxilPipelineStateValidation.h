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

// How many dwords are required for mask with one bit per component, 4 components per vector
inline uint32_t ComputeMaskDwordsFromVectors(uint32_t Vectors) { return (Vectors + 7) >> 3; }

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
struct PSVRuntimeInfo1 : public PSVRuntimeInfo0
{
  char UsesViewID;
  unsigned char SigInputVectors;
  unsigned char SigOutputVectors;
  unsigned char SigPCOutputVectors;       // HS only
  unsigned char SigPCInputVectors;        // DS only
};

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

// Helpers for output dependencies (ViewID and Input-Output tables)
struct ComponentMasks {
  uint32_t *Masks;
  uint32_t NumVectors;
  ComponentMasks() : Masks(nullptr), NumVectors(0) {}
  ComponentMasks(uint32_t *pMasks, uint32_t outputVectors)
    : Masks(pMasks), NumVectors(outputVectors) {}
  const ComponentMasks &operator|=(const ComponentMasks &other) {
    _Analysis_assume_(NumVectors == other.NumVectors && Masks && other.Masks);
    uint32_t dwords = ComputeMaskDwordsFromVectors(NumVectors);
    for (uint32_t i = 0; i < dwords; ++i) {
      Masks[i] |= other.Masks[i];
    }
  }
  uint32_t Get(uint32_t ComponentIndex) const {
    _Analysis_assume_(ComponentIndex < (NumVectors * 4));
    return Masks[ComponentIndex >> 5] & (1 << (ComponentIndex & 0x1F));
  }
  void Set(uint32_t ComponentIndex) {
    _Analysis_assume_(ComponentIndex < (NumVectors * 4));
    Masks[ComponentIndex >> 5] |= (1 << (ComponentIndex & 0x1F));
  }
  void Clear(uint32_t ComponentIndex) {
    _Analysis_assume_(ComponentIndex < (NumVectors * 4));
    Masks[ComponentIndex >> 5] &= ~(1 << (ComponentIndex & 0x1F));
  }
};

struct DependencyTable {
  uint32_t *Table;
  uint32_t InputVectors;
  uint32_t OutputVectors;
  DependencyTable() : Table(nullptr), InputVectors(0), OutputVectors(0) {}
  DependencyTable(uint32_t *pTable, uint32_t inputVectors, uint32_t outputVectors)
    : Table(pTable), InputVectors(inputVectors), OutputVectors(outputVectors) {}
  ComponentMasks GetMasksForInput(uint32_t inputComponentIndex) {
    if (!Table || !InputVectors || !OutputVectors)
      return ComponentMasks();
    return ComponentMasks(Table + (ComputeMaskDwordsFromVectors(OutputVectors) * inputComponentIndex), OutputVectors);
  }
};

class DxilPipelineStateValidation
{
  uint32_t m_uPSVRuntimeInfoSize;
  PSVRuntimeInfo0* m_pPSVRuntimeInfo0;
  PSVRuntimeInfo1* m_pPSVRuntimeInfo1;
  uint32_t m_uResourceCount;
  uint32_t m_uPSVResourceBindInfoSize;
  void* m_pPSVResourceBindInfo;
  uint32_t* m_pViewIDOutputMasks;
  uint32_t* m_pViewIDPCOutputMasks;
  uint32_t* m_pInputToOutputTable;
  uint32_t* m_pInputToPCOutputTable;
  uint32_t* m_pPCInputToOutputTable;

public:
  DxilPipelineStateValidation() : 
    m_uPSVRuntimeInfoSize(0),
    m_pPSVRuntimeInfo0(nullptr),
    m_pPSVRuntimeInfo1(nullptr),
    m_uResourceCount(0),
    m_uPSVResourceBindInfoSize(0),
    m_pPSVResourceBindInfo(nullptr),
    m_pViewIDOutputMasks(nullptr),
    m_pViewIDPCOutputMasks(nullptr),
    m_pInputToOutputTable(nullptr),
    m_pInputToPCOutputTable(nullptr),
    m_pPCInputToOutputTable(nullptr)
  {
  }

  // Init() from PSV0 blob part that looks like:
  // uint32_t PSVRuntimeInfo_size
  // { PSVRuntimeInfoN structure }
  // uint32_t ResourceCount
  // ---  end of blob if ResourceCount == 0  ---
  // uint32_t PSVResourceBindInfo_size
  // { PSVResourceBindInfoN structure } * ResourceCount
  // Inputs are rows or outputs?  Currently making bits outputs and rows inputs.
  // Optional (UsesViewID and SigOutputVectors non-zero) Outputs affected by ViewID as a bitmask
  // Optional (UsesViewID and SigPCOutputVectors non-zero) PCOutputs affected by ViewID as a bitmask
  // Optional (SigInputVectors and SigOutputVectors non-zero) Outputs affected by inputs as a table of bitmasks
  // Optional (SigPCOutputVectors or SigPCInputVectors non-zero) table for patch constants (Input to PCOutput for HS, PCInput to Output for DS)
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
    if(m_uPSVRuntimeInfoSize >= sizeof(PSVRuntimeInfo1))
      m_pPSVRuntimeInfo1 = const_cast<PSVRuntimeInfo1*>((const PSVRuntimeInfo1*)pCurBits);
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
    pCurBits += m_uPSVResourceBindInfoSize * m_uResourceCount;

    // Set ViewID and Input-Output tables
    if (m_pPSVRuntimeInfo1) {
      if (m_pPSVRuntimeInfo1->UsesViewID) {
        if (m_pPSVRuntimeInfo1->SigOutputVectors) {
          m_pViewIDOutputMasks = (uint32_t*)pCurBits;
          pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigOutputVectors);
        }
        if (m_pPSVRuntimeInfo1->SigPCOutputVectors) {
          m_pViewIDPCOutputMasks = (uint32_t*)pCurBits;
          pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigPCOutputVectors);
        }
      }
      if (m_pPSVRuntimeInfo1->SigOutputVectors > 0 && m_pPSVRuntimeInfo1->SigInputVectors > 0) {
        m_pInputToOutputTable = (uint32_t*)pCurBits;
        pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigOutputVectors) * m_pPSVRuntimeInfo1->SigInputVectors * 4;
      }
      if (m_pPSVRuntimeInfo1->SigPCOutputVectors > 0 && m_pPSVRuntimeInfo1->SigInputVectors > 0) {
        m_pInputToPCOutputTable = (uint32_t*)pCurBits;
        pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigPCOutputVectors) * m_pPSVRuntimeInfo1->SigInputVectors * 4;
      }
      if (m_pPSVRuntimeInfo1->SigOutputVectors > 0 && m_pPSVRuntimeInfo1->SigPCInputVectors > 0) {
        m_pPCInputToOutputTable = (uint32_t*)pCurBits;
        pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigOutputVectors) * m_pPSVRuntimeInfo1->SigPCInputVectors * 4;
      }
    }
    return true;
  }

  // Initialize a new buffer
  // call with null pBuffer to get required size
  bool InitNew(uint32_t ResourceCount, void *pBuffer, uint32_t *pSize) {
    return InitNew1(ResourceCount,
                   0, 0, 0, 0, 0,
                   pBuffer, pSize);
  }
  bool InitNew1(uint32_t ResourceCount,
               char UsesViewID,
               uint32_t SigInputVectors,
               uint32_t SigOutputVectors,
               uint32_t SigPCOutputVectors,
               uint32_t SigPCInputVectors,
               void *pBuffer, uint32_t *pSize) {
    if(!(pSize)) return false;
    uint32_t size = sizeof(PSVRuntimeInfo1) + sizeof(uint32_t) * 2;
    if (ResourceCount) {
      size += sizeof(uint32_t) + (sizeof(PSVResourceBindInfo0) * ResourceCount);
    }
    if (UsesViewID) {
      size += ComputeMaskDwordsFromVectors(SigOutputVectors);
      size += ComputeMaskDwordsFromVectors(SigPCOutputVectors);
    }
    if (SigPCOutputVectors > 0 && SigPCInputVectors > 0) {
      return false;   // Invalid to have both
    }
    if (SigOutputVectors > 0 && SigInputVectors > 0) {
      size += ComputeMaskDwordsFromVectors(SigOutputVectors) * SigInputVectors * 4;
    }
    if (SigPCOutputVectors > 0 && SigInputVectors > 0) {
      size += ComputeMaskDwordsFromVectors(SigPCOutputVectors) * SigInputVectors * 4;
    }
    if (SigOutputVectors > 0 && SigPCInputVectors > 0) {
      size += ComputeMaskDwordsFromVectors(SigOutputVectors) * SigPCInputVectors * 4;
    }
    if (pBuffer) {
      if(!(*pSize >= size)) return false;
    } else {
      *pSize = size;
      return true;
    }
    memset(pBuffer, 0, size);
    m_uPSVRuntimeInfoSize = sizeof(PSVRuntimeInfo1);
    uint8_t* pCurBits = (uint8_t*)pBuffer;
    *(uint32_t*)pCurBits = sizeof(PSVRuntimeInfo1);
    pCurBits += sizeof(uint32_t);
    m_pPSVRuntimeInfo0 = (PSVRuntimeInfo0*)pCurBits;
    m_pPSVRuntimeInfo1 = (PSVRuntimeInfo1*)pCurBits;
    pCurBits += sizeof(PSVRuntimeInfo1);

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
    pCurBits += m_uPSVResourceBindInfoSize * m_uResourceCount;

    // Set ViewID and Input-Output tables
    m_pPSVRuntimeInfo1->UsesViewID = UsesViewID;
    m_pPSVRuntimeInfo1->SigInputVectors = SigInputVectors;
    m_pPSVRuntimeInfo1->SigOutputVectors = SigOutputVectors;
    m_pPSVRuntimeInfo1->SigPCOutputVectors = SigPCOutputVectors;
    m_pPSVRuntimeInfo1->SigPCInputVectors = SigPCInputVectors;
    if (UsesViewID) {
      if (SigOutputVectors) {
        m_pViewIDOutputMasks = (uint32_t*)pCurBits;
        pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(SigOutputVectors);
      }
      if (SigPCOutputVectors) {
        m_pViewIDPCOutputMasks = (uint32_t*)pCurBits;
        pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(SigPCOutputVectors);
      }
    }
    if (SigOutputVectors > 0 && SigInputVectors > 0) {
      m_pInputToOutputTable = (uint32_t*)pCurBits;
      pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(SigOutputVectors) * SigInputVectors * 4;
    }
    if (SigPCOutputVectors > 0 && SigInputVectors > 0) {
      m_pInputToPCOutputTable = (uint32_t*)pCurBits;
      pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(SigPCOutputVectors) * SigInputVectors * 4;
    }
    if (SigOutputVectors > 0 && SigPCInputVectors > 0) {
      m_pPCInputToOutputTable = (uint32_t*)pCurBits;
      pCurBits += sizeof(uint32_t) * ComputeMaskDwordsFromVectors(SigOutputVectors) * SigPCInputVectors * 4;
    }

    return true;
  }

  PSVRuntimeInfo0* GetPSVRuntimeInfo0() {
    return m_pPSVRuntimeInfo0;
  }

  PSVRuntimeInfo1* GetPSVRuntimeInfo1() {
    return m_pPSVRuntimeInfo1;
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

  ComponentMasks GetViewIDOutputMasks() {
    if (!m_pViewIDOutputMasks || !m_pPSVRuntimeInfo1 || !m_pPSVRuntimeInfo1->SigOutputVectors)
      return ComponentMasks();
    return ComponentMasks(m_pViewIDOutputMasks, m_pPSVRuntimeInfo1->SigOutputVectors);
  }
  ComponentMasks GetViewIDPCOutputMasks() {
    if (!m_pViewIDPCOutputMasks || !m_pPSVRuntimeInfo1 || !m_pPSVRuntimeInfo1->SigPCOutputVectors)
      return ComponentMasks();
    return ComponentMasks(m_pViewIDPCOutputMasks, m_pPSVRuntimeInfo1->SigPCOutputVectors);
  }

  DependencyTable GetInputToOutputTable() {
    if (m_pInputToOutputTable) {
      return DependencyTable(m_pInputToOutputTable, m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigOutputVectors);
    }
    return DependencyTable();
  }
  DependencyTable GetInputToPCOutputTable() {
    if (m_pInputToPCOutputTable) {
      return DependencyTable(m_pInputToPCOutputTable, m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigPCOutputVectors);
    }
    return DependencyTable();
  }
  DependencyTable GetPCInputToOutputTable() {
    if (m_pPCInputToOutputTable) {
      return DependencyTable(m_pPCInputToOutputTable, m_pPSVRuntimeInfo1->SigPCInputVectors, m_pPSVRuntimeInfo1->SigOutputVectors);
    }
    return DependencyTable();
  }
};


#endif  // __DXIL_PIPELINE_STATE_VALIDATION__H__
