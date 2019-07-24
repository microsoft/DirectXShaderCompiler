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
#ifndef UINT_MAX
#define UINT_MAX 0xffffffff
#endif
// How many dwords are required for mask with one bit per component, 4 components per vector
inline uint32_t PSVComputeMaskDwordsFromVectors(uint32_t Vectors) { return (Vectors + 7) >> 3; }
inline uint32_t PSVComputeInputOutputTableSize(uint32_t InputVectors, uint32_t OutputVectors) {
  return sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(OutputVectors) * InputVectors * 4;
}
#define PSVALIGN(ptr, alignbits) (((ptr) + ((1 << (alignbits))-1)) & ~((1 << (alignbits))-1))
#define PSVALIGN4(ptr) (((ptr) + 3) & ~3)

struct VSInfo {
  char OutputPositionPresent;
};
struct HSInfo {
  uint32_t InputControlPointCount;      // max control points == 32
  uint32_t OutputControlPointCount;     // max control points == 32
  uint32_t TessellatorDomain;           // hlsl::DXIL::TessellatorDomain/D3D11_SB_TESSELLATOR_DOMAIN
  uint32_t TessellatorOutputPrimitive;  // hlsl::DXIL::TessellatorOutputPrimitive/D3D11_SB_TESSELLATOR_OUTPUT_PRIMITIVE
};
struct DSInfo {
  uint32_t InputControlPointCount;      // max control points == 32
  char OutputPositionPresent;
  uint32_t TessellatorDomain;           // hlsl::DXIL::TessellatorDomain/D3D11_SB_TESSELLATOR_DOMAIN
};
struct GSInfo {
  uint32_t InputPrimitive;              // hlsl::DXIL::InputPrimitive/D3D10_SB_PRIMITIVE
  uint32_t OutputTopology;              // hlsl::DXIL::PrimitiveTopology/D3D10_SB_PRIMITIVE_TOPOLOGY
  uint32_t OutputStreamMask;            // max streams == 4
  char OutputPositionPresent;
};
struct PSInfo {
  char DepthOutput;
  char SampleFrequency;
};
struct MSInfo {
  uint32_t GroupSharedBytesUsed;
  uint32_t GroupSharedBytesDependentOnViewID;
  uint32_t PayloadSizeInBytes;
  uint16_t MaxOutputVertices;
  uint16_t MaxOutputPrimitives;
};
struct ASInfo {
  uint32_t PayloadSizeInBytes;
};
struct MSInfo1 {
  uint8_t SigPrimVectors;     // Primitive output for MS
  uint8_t MeshOutputTopology;
};

// Versioning is additive and based on size
struct PSVRuntimeInfo0
{
  union {
    VSInfo VS;
    HSInfo HS;
    DSInfo DS;
    GSInfo GS;
    PSInfo PS;
    MSInfo MS;
    ASInfo AS;
  };
  uint32_t MinimumExpectedWaveLaneCount;  // minimum lane count required, 0 if unused
  uint32_t MaximumExpectedWaveLaneCount;  // maximum lane count required, 0xffffffff if unused
};

enum class PSVShaderKind : uint8_t    // DXIL::ShaderKind
{
  Pixel = 0,
  Vertex,
  Geometry,
  Hull,
  Domain,
  Compute,
  Library,
  RayGeneration,
  Intersection,
  AnyHit,
  ClosestHit,
  Miss,
  Callable,
  Mesh,
  Amplification,
  Invalid,
};

struct PSVRuntimeInfo1 : public PSVRuntimeInfo0
{
  uint8_t ShaderStage;              // PSVShaderKind
  uint8_t UsesViewID;
  union {
    uint16_t MaxVertexCount;          // MaxVertexCount for GS only (max 1024)
    uint8_t SigPatchConstOrPrimVectors;  // Output for HS; Input for DS; Primitive output for MS (overlaps MS1::SigPrimVectors)
    struct MSInfo1 MS1;
  };

  // PSVSignatureElement counts
  uint8_t SigInputElements;
  uint8_t SigOutputElements;
  uint8_t SigPatchConstOrPrimElements;

  // Number of packed vectors per signature
  uint8_t SigInputVectors;
  uint8_t SigOutputVectors[4];      // Array for GS Stream Out Index
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

enum class PSVResourceKind
{
  Invalid = 0,
  Texture1D,
  Texture2D,
  Texture2DMS,
  Texture3D,
  TextureCube,
  Texture1DArray,
  Texture2DArray,
  Texture2DMSArray,
  TextureCubeArray,
  TypedBuffer,
  RawBuffer,
  StructuredBuffer,
  CBuffer,
  Sampler,
  TBuffer,
  RTAccelerationStructure,
  NumEntries
};

// Table of null-terminated strings, overall size aligned to dword boundary, last byte must be null
struct PSVStringTable {
  const char *Table;
  uint32_t Size;
  PSVStringTable() : Table(nullptr), Size(0) {}
  PSVStringTable(const char *table, uint32_t size) : Table(table), Size(size) {}
  const char *Get(uint32_t offset) const {
    _Analysis_assume_(offset < Size && Table && Table[Size-1] == '\0');
    return Table + offset;
  }
};

// Versioning is additive and based on size
struct PSVResourceBindInfo0
{
  uint32_t ResType;     // PSVResourceType
  uint32_t Space;
  uint32_t LowerBound;
  uint32_t UpperBound;
};

// Helpers for output dependencies (ViewID and Input-Output tables)
struct PSVComponentMask {
  uint32_t *Mask;
  uint32_t NumVectors;
  PSVComponentMask() : Mask(nullptr), NumVectors(0) {}
  PSVComponentMask(const PSVComponentMask &other) : Mask(other.Mask), NumVectors(other.NumVectors) {}
  PSVComponentMask(uint32_t *pMask, uint32_t outputVectors)
  : Mask(pMask),
    NumVectors(outputVectors)
  {}
  const PSVComponentMask &operator|=(const PSVComponentMask &other) {
    uint32_t dwords = PSVComputeMaskDwordsFromVectors(NumVectors < other.NumVectors ? NumVectors : other.NumVectors);
    for (uint32_t i = 0; i < dwords; ++i) {
      Mask[i] |= other.Mask[i];
    }
    return *this;
  }
  bool Get(uint32_t ComponentIndex) const {
    if(ComponentIndex < NumVectors * 4)
      return (bool)(Mask[ComponentIndex >> 5] & (1 << (ComponentIndex & 0x1F)));
    return false;
  }
  void Set(uint32_t ComponentIndex) {
    if (ComponentIndex < NumVectors * 4)
      Mask[ComponentIndex >> 5] |= (1 << (ComponentIndex & 0x1F));
  }
  void Clear(uint32_t ComponentIndex) {
    if (ComponentIndex < NumVectors * 4)
      Mask[ComponentIndex >> 5] &= ~(1 << (ComponentIndex & 0x1F));
  }
  bool IsValid() { return Mask != nullptr; }
};

struct PSVDependencyTable {
  uint32_t *Table;
  uint32_t InputVectors;
  uint32_t OutputVectors;
  PSVDependencyTable() : Table(nullptr), InputVectors(0), OutputVectors(0) {}
  PSVDependencyTable(const PSVDependencyTable &other) : Table(other.Table), InputVectors(other.InputVectors), OutputVectors(other.OutputVectors) {}
  PSVDependencyTable(uint32_t *pTable, uint32_t inputVectors, uint32_t outputVectors)
  : Table(pTable),
    InputVectors(inputVectors),
    OutputVectors(outputVectors)
  {}
  PSVComponentMask GetMaskForInput(uint32_t inputComponentIndex) {
    if (!Table || !InputVectors || !OutputVectors)
      return PSVComponentMask();
    return PSVComponentMask(Table + (PSVComputeMaskDwordsFromVectors(OutputVectors) * inputComponentIndex), OutputVectors);
  }
  bool IsValid() { return Table != nullptr; }
};

struct PSVString {
  uint32_t Offset;
  PSVString() : Offset(0) {}
  PSVString(uint32_t offset) : Offset(offset) {}
  const char *Get(const PSVStringTable &table) const { return table.Get(Offset); }
};

struct PSVSemanticIndexTable {
  const uint32_t *Table;
  uint32_t Entries;
  PSVSemanticIndexTable() : Table(nullptr), Entries(0) {}
  PSVSemanticIndexTable(const uint32_t *table, uint32_t entries) : Table(table), Entries(entries) {}
  const uint32_t *Get(uint32_t offset) const {
    _Analysis_assume_(offset < Entries && Table);
    return Table + offset;
  }
};

struct PSVSemanticIndexes {
  uint32_t Offset;
  PSVSemanticIndexes() : Offset(0) {}
  PSVSemanticIndexes(uint32_t offset) : Offset(offset) {}
  const uint32_t *Get(const PSVSemanticIndexTable &table) const { return table.Get(Offset); }
};

enum class PSVSemanticKind : uint8_t    // DXIL::SemanticKind
{
  Arbitrary,
  VertexID,
  InstanceID,
  Position,
  RenderTargetArrayIndex,
  ViewPortArrayIndex,
  ClipDistance,
  CullDistance,
  OutputControlPointID,
  DomainLocation,
  PrimitiveID,
  GSInstanceID,
  SampleIndex,
  IsFrontFace,
  Coverage,
  InnerCoverage,
  Target,
  Depth,
  DepthLessEqual,
  DepthGreaterEqual,
  StencilRef,
  DispatchThreadID,
  GroupID,
  GroupIndex,
  GroupThreadID,
  TessFactor,
  InsideTessFactor,
  ViewID,
  Barycentrics,
  Invalid,
};

struct PSVSignatureElement0
{
  uint32_t SemanticName;          // Offset into StringTable
  uint32_t SemanticIndexes;       // Offset into PSVSemanticIndexTable, count == Rows
  uint8_t Rows;                   // Number of rows this element occupies
  uint8_t StartRow;               // Starting row of packing location if allocated
  uint8_t ColsAndStart;           // 0:4 = Cols, 4:6 = StartCol, 6:7 == Allocated
  uint8_t SemanticKind;           // PSVSemanticKind
  uint8_t ComponentType;          // DxilProgramSigCompType
  uint8_t InterpolationMode;      // DXIL::InterpolationMode or D3D10_SB_INTERPOLATION_MODE
  uint8_t DynamicMaskAndStream;   // 0:4 = DynamicIndexMask, 4:6 = OutputStream (0-3)
  uint8_t Reserved;
};

// Provides convenient access to packed PSVSignatureElementN structure
class PSVSignatureElement
{
private:
  const PSVStringTable &m_StringTable;
  const PSVSemanticIndexTable &m_SemanticIndexTable;
  const PSVSignatureElement0 *m_pElement0;
public:
  PSVSignatureElement(const PSVStringTable &stringTable, const PSVSemanticIndexTable &semanticIndexTable, const PSVSignatureElement0 *pElement0)
  : m_StringTable(stringTable), m_SemanticIndexTable(semanticIndexTable), m_pElement0(pElement0) {}
  const char *GetSemanticName() const { return !m_pElement0 ? "" : m_StringTable.Get(m_pElement0->SemanticName); }
  const uint32_t *GetSemanticIndexes() const { return !m_pElement0 ? nullptr: m_SemanticIndexTable.Get(m_pElement0->SemanticIndexes); }
  uint32_t GetRows() const { return !m_pElement0 ? 0 : ((uint32_t)m_pElement0->Rows); }
  uint32_t GetCols() const { return !m_pElement0 ? 0 : ((uint32_t)m_pElement0->ColsAndStart & 0xF); }
  bool IsAllocated() const { return !m_pElement0 ? false : !!(m_pElement0->ColsAndStart & 0x40); }
  int32_t GetStartRow() const { return !m_pElement0 ? 0 : !IsAllocated() ? -1 : (int32_t)m_pElement0->StartRow; }
  int32_t GetStartCol() const { return !m_pElement0 ? 0 : !IsAllocated() ? -1 : (int32_t)((m_pElement0->ColsAndStart >> 4) & 0x3); }
  PSVSemanticKind GetSemanticKind() const { return !m_pElement0 ? (PSVSemanticKind)0 : (PSVSemanticKind)m_pElement0->SemanticKind; }
  uint32_t GetComponentType() const { return !m_pElement0 ? 0 : (uint32_t)m_pElement0->ComponentType; }
  uint32_t GetInterpolationMode() const { return !m_pElement0 ? 0 : (uint32_t)m_pElement0->InterpolationMode; }
  uint32_t GetOutputStream() const { return !m_pElement0 ? 0 : (uint32_t)(m_pElement0->DynamicMaskAndStream >> 4) & 0x3; }
  uint32_t GetDynamicIndexMask() const { return !m_pElement0 ? 0 : (uint32_t)m_pElement0->DynamicMaskAndStream & 0xF; }
};

#define MAX_PSV_VERSION 1

struct PSVInitInfo
{
  PSVInitInfo(uint32_t psvVersion)
    : PSVVersion(psvVersion),
    ResourceCount(0),
    ShaderStage(PSVShaderKind::Invalid),
    StringTable(),
    SemanticIndexTable(),
    UsesViewID(0),
    SigInputElements(0),
    SigOutputElements(0),
    SigPatchConstOrPrimElements(0),
    SigInputVectors(0),
    SigPatchConstOrPrimVectors(0)
  {}
  uint32_t PSVVersion;
  uint32_t ResourceCount;
  PSVShaderKind ShaderStage;
  PSVStringTable StringTable;
  PSVSemanticIndexTable SemanticIndexTable;
  uint8_t UsesViewID;
  uint8_t SigInputElements;
  uint8_t SigOutputElements;
  uint8_t SigPatchConstOrPrimElements;
  uint8_t SigInputVectors;
  uint8_t SigPatchConstOrPrimVectors;
  uint8_t SigOutputVectors[4] = {0, 0, 0, 0};
};

class DxilPipelineStateValidation
{
  uint32_t m_uPSVRuntimeInfoSize;
  PSVRuntimeInfo0* m_pPSVRuntimeInfo0;
  PSVRuntimeInfo1* m_pPSVRuntimeInfo1;
  uint32_t m_uResourceCount;
  uint32_t m_uPSVResourceBindInfoSize;
  void* m_pPSVResourceBindInfo;
  PSVStringTable m_StringTable;
  PSVSemanticIndexTable m_SemanticIndexTable;
  uint32_t m_uPSVSignatureElementSize;
  void* m_pSigInputElements;
  void* m_pSigOutputElements;
  void* m_pSigPatchConstOrPrimElements;
  uint32_t* m_pViewIDOutputMask;
  uint32_t* m_pViewIDPCOrPrimOutputMask;
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
    m_StringTable(),
    m_SemanticIndexTable(),
    m_uPSVSignatureElementSize(0),
    m_pSigInputElements(nullptr),
    m_pSigOutputElements(nullptr),
    m_pSigPatchConstOrPrimElements(nullptr),
    m_pViewIDOutputMask(nullptr),
    m_pViewIDPCOrPrimOutputMask(nullptr),
    m_pInputToOutputTable(nullptr),
    m_pInputToPCOutputTable(nullptr),
    m_pPCInputToOutputTable(nullptr)
  {
  }

  // Init() from PSV0 blob part that looks like:
  // uint32_t PSVRuntimeInfo_size
  // { PSVRuntimeInfoN structure }
  // uint32_t ResourceCount
  // If ResourceCount > 0:
  //    uint32_t PSVResourceBindInfo_size
  //    { PSVResourceBindInfoN structure } * ResourceCount
  // If PSVRuntimeInfo1:
  //    uint32_t StringTableSize (dword aligned)
  //    If StringTableSize:
  //      { StringTableSize bytes, null terminated }
  //    uint32_t SemanticIndexTableEntries (number of dwords)
  //    If SemanticIndexTableEntries:
  //      { semantic index } * SemanticIndexTableEntries
  //    If SigInputElements || SigOutputElements || SigPatchConstOrPrimElements:
  //      uint32_t PSVSignatureElement_size
  //      { PSVSignatureElementN structure } * SigInputElements
  //      { PSVSignatureElementN structure } * SigOutputElements
  //      { PSVSignatureElementN structure } * SigPatchConstOrPrimElements
  //    If (UsesViewID):
  //      For (i : each stream index 0-3):
  //        If (SigOutputVectors[i] non-zero):
  //          { uint32_t * PSVComputeMaskDwordsFromVectors(SigOutputVectors[i]) }
  //            - Outputs affected by ViewID as a bitmask
  //      If (HS and SigPatchConstOrPrimVectors non-zero):
  //        { uint32_t * PSVComputeMaskDwordsFromVectors(SigPatchConstOrPrimVectors) }
  //          - PCOutputs affected by ViewID as a bitmask
  //    For (i : each stream index 0-3):
  //      If (SigInputVectors and SigOutputVectors[i] non-zero):
  //        { PSVComputeInputOutputTableSize(SigInputVectors, SigOutputVectors[i]) }
  //          - Outputs affected by inputs as a table of bitmasks
  //    If (HS and SigPatchConstOrPrimVectors and SigInputVectors non-zero):
  //      { PSVComputeInputOutputTableSize(SigInputVectors, SigPatchConstOrPrimVectors) }
  //        - Patch constant outputs affected by inputs as a table of bitmasks
  //    If (DS and SigOutputVectors[0] and SigPatchConstOrPrimVectors non-zero):
  //      { PSVComputeInputOutputTableSize(SigPatchConstOrPrimVectors, SigOutputVectors[0]) }
  //        - Outputs affected by patch constant inputs as a table of bitmasks
  // returns true if no errors occurred.
  bool InitFromPSV0(const void* pBits, uint32_t size) {
    if(!(pBits != nullptr)) return false;
    uint8_t* pCurBits = (uint8_t*)pBits;
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

    if (m_pPSVRuntimeInfo1) {
      minsize += sizeof(uint32_t) * 2;    // m_StringTable.Size and m_SemanticIndexTable.Entries
      if (!(size >= minsize)) return false;
      m_StringTable.Size = PSVALIGN4(*(uint32_t*)pCurBits);
      if (m_StringTable.Size != *(uint32_t*)pCurBits)
        return false;   // Illegal: Size not aligned
      minsize += m_StringTable.Size;
      if (!(size >= minsize)) return false;
      pCurBits += sizeof(uint32_t);
      m_StringTable.Table = (const char *)pCurBits;
      pCurBits += m_StringTable.Size;

      m_SemanticIndexTable.Entries = *(uint32_t*)pCurBits;
      minsize += sizeof(uint32_t) * m_SemanticIndexTable.Entries;
      if (!(size >= minsize)) return false;
      pCurBits += sizeof(uint32_t);
      m_SemanticIndexTable.Table = (uint32_t*)pCurBits;
      pCurBits += sizeof(uint32_t) * m_SemanticIndexTable.Entries;

      // Dxil Signature Elements
      if (m_pPSVRuntimeInfo1->SigInputElements || m_pPSVRuntimeInfo1->SigOutputElements || m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements) {
        minsize += sizeof(uint32_t);
        if (!(size >= minsize)) return false;
        m_uPSVSignatureElementSize = *(uint32_t*)pCurBits;
        if (m_uPSVSignatureElementSize < sizeof(PSVSignatureElement0))
          return false;   // Illegal: Size smaller than first version
        pCurBits += sizeof(uint32_t);
        minsize += m_uPSVSignatureElementSize * (m_pPSVRuntimeInfo1->SigInputElements + m_pPSVRuntimeInfo1->SigOutputElements + m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements);
        if (!(size >= minsize)) return false;
      }
      if (m_pPSVRuntimeInfo1->SigInputElements) {
        m_pSigInputElements = (PSVSignatureElement0*)pCurBits;
        pCurBits += m_uPSVSignatureElementSize * m_pPSVRuntimeInfo1->SigInputElements;
      }
      if (m_pPSVRuntimeInfo1->SigOutputElements) {
        m_pSigOutputElements = (PSVSignatureElement0*)pCurBits;
        pCurBits += m_uPSVSignatureElementSize * m_pPSVRuntimeInfo1->SigOutputElements;
      }
      if (m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements) {
        m_pSigPatchConstOrPrimElements = (PSVSignatureElement0*)pCurBits;
        pCurBits += m_uPSVSignatureElementSize * m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements;
      }

      // ViewID dependencies
      if (m_pPSVRuntimeInfo1->UsesViewID) {
        for (unsigned i = 0; i < 4; i++) {
          if (m_pPSVRuntimeInfo1->SigOutputVectors[i]) {
            minsize += sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigOutputVectors[i]);
            if (!(size >= minsize)) return false;
            m_pViewIDOutputMask = (uint32_t*)pCurBits;
            pCurBits += sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigOutputVectors[i]);
          }
          if (!IsGS())
            break;
        }
        if ((IsHS() || IsMS()) && m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors) {
          minsize += sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors);
          if (!(size >= minsize)) return false;
          m_pViewIDPCOrPrimOutputMask = (uint32_t*)pCurBits;
          pCurBits += sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors);
        }
      }

      // Input to Output dependencies
      for (unsigned i = 0; i < 4; i++) {
        if (m_pPSVRuntimeInfo1->SigOutputVectors[i] > 0 && m_pPSVRuntimeInfo1->SigInputVectors > 0) {
          minsize += PSVComputeInputOutputTableSize(m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigOutputVectors[i]);
          if (!(size >= minsize)) return false;
          m_pInputToOutputTable = (uint32_t*)pCurBits;
          pCurBits += PSVComputeInputOutputTableSize(m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigOutputVectors[i]);
        }
        if (!IsGS())
          break;
      }
      if ((IsHS() || IsMS()) && m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors > 0 && m_pPSVRuntimeInfo1->SigInputVectors > 0) {
        minsize += PSVComputeInputOutputTableSize(m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors);
        if (!(size >= minsize)) return false;
        m_pInputToPCOutputTable = (uint32_t*)pCurBits;
        pCurBits += PSVComputeInputOutputTableSize(m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors);
      }
      if (IsDS() && m_pPSVRuntimeInfo1->SigOutputVectors[0] > 0 && m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors > 0) {
        minsize += PSVComputeInputOutputTableSize(m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors, m_pPSVRuntimeInfo1->SigOutputVectors[0]);
        if (!(size >= minsize)) return false;
        m_pPCInputToOutputTable = (uint32_t*)pCurBits;
        pCurBits += PSVComputeInputOutputTableSize(m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors, m_pPSVRuntimeInfo1->SigOutputVectors[0]);
      }
    }
    return true;
  }

  // Initialize a new buffer
  // call with null pBuffer to get required size

  bool InitNew(uint32_t ResourceCount, void *pBuffer, uint32_t *pSize) {
    PSVInitInfo initInfo(0);
    initInfo.ResourceCount = ResourceCount;
    return InitNew(initInfo, pBuffer, pSize);
  }

  bool InitNew(const PSVInitInfo &initInfo, void *pBuffer, uint32_t *pSize) {
    if(!(pSize)) return false;
    if (initInfo.PSVVersion > MAX_PSV_VERSION) return false;

    // Versioned structure sizes
    m_uPSVRuntimeInfoSize = sizeof(PSVRuntimeInfo0);
    m_uPSVResourceBindInfoSize = sizeof(PSVResourceBindInfo0);
    m_uPSVSignatureElementSize = sizeof(PSVSignatureElement0);
    if (initInfo.PSVVersion > 0) {
      m_uPSVRuntimeInfoSize = sizeof(PSVRuntimeInfo1);
    }

    // PSVVersion 0
    uint32_t size = m_uPSVRuntimeInfoSize + sizeof(uint32_t) * 2;
    if (initInfo.ResourceCount) {
      size += sizeof(uint32_t) + (m_uPSVResourceBindInfoSize * initInfo.ResourceCount);
    }

    // PSVVersion 1
    if (initInfo.PSVVersion > 0) {
      size += sizeof(uint32_t) + PSVALIGN4(initInfo.StringTable.Size);
      size += sizeof(uint32_t) + sizeof(uint32_t) * initInfo.SemanticIndexTable.Entries;
      if (initInfo.SigInputElements || initInfo.SigOutputElements || initInfo.SigPatchConstOrPrimElements) {
        size += sizeof(uint32_t);   // PSVSignatureElement_size
      }
      size += m_uPSVSignatureElementSize * initInfo.SigInputElements;
      size += m_uPSVSignatureElementSize * initInfo.SigOutputElements;
      size += m_uPSVSignatureElementSize * initInfo.SigPatchConstOrPrimElements;

      if (initInfo.UsesViewID) {
        for (unsigned i = 0; i < 4; i++) {
          size += sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(initInfo.SigOutputVectors[i]);
          if (initInfo.ShaderStage != PSVShaderKind::Geometry)
            break;
        }
        if (initInfo.ShaderStage == PSVShaderKind::Hull || initInfo.ShaderStage == PSVShaderKind::Mesh)
          size += sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(initInfo.SigPatchConstOrPrimVectors);
      }
      if (initInfo.SigInputVectors > 0) {
        for (unsigned i = 0; i < 4; i++) {
          if (initInfo.SigOutputVectors[i] > 0) {
            size += PSVComputeInputOutputTableSize(initInfo.SigInputVectors, initInfo.SigOutputVectors[i]);
            if (initInfo.ShaderStage != PSVShaderKind::Geometry)
              break;
          }
        }
        if (initInfo.ShaderStage == PSVShaderKind::Hull && initInfo.SigPatchConstOrPrimVectors > 0 && initInfo.SigInputVectors > 0) {
          size += PSVComputeInputOutputTableSize(initInfo.SigInputVectors, initInfo.SigPatchConstOrPrimVectors);
        }
      }
      if (initInfo.ShaderStage == PSVShaderKind::Domain && initInfo.SigOutputVectors[0] > 0 && initInfo.SigPatchConstOrPrimVectors > 0) {
        size += PSVComputeInputOutputTableSize(initInfo.SigPatchConstOrPrimVectors, initInfo.SigOutputVectors[0]);
      }
    }

    // Validate or return required size
    if (pBuffer) {
      if(!(*pSize >= size)) return false;
    } else {
      *pSize = size;
      return true;
    }

    // PSVVersion 0
    memset(pBuffer, 0, size);
    uint8_t* pCurBits = (uint8_t*)pBuffer;
    *(uint32_t*)pCurBits = m_uPSVRuntimeInfoSize;
    pCurBits += sizeof(uint32_t);
    m_pPSVRuntimeInfo0 = (PSVRuntimeInfo0*)pCurBits;
    if (initInfo.PSVVersion > 0) {
      m_pPSVRuntimeInfo1 = (PSVRuntimeInfo1*)pCurBits;
    }
    pCurBits += m_uPSVRuntimeInfoSize;

    // Set resource info:
    m_uResourceCount = initInfo.ResourceCount;
    *(uint32_t*)pCurBits = m_uResourceCount;
    pCurBits += sizeof(uint32_t);
    if (m_uResourceCount > 0) {
      *(uint32_t*)pCurBits = m_uPSVResourceBindInfoSize;
      pCurBits += sizeof(uint32_t);
      m_pPSVResourceBindInfo = pCurBits;
    }
    pCurBits += m_uPSVResourceBindInfoSize * m_uResourceCount;

    // PSVVersion 1
    if (initInfo.PSVVersion) {
      m_pPSVRuntimeInfo1->ShaderStage = (uint8_t)initInfo.ShaderStage;
      m_pPSVRuntimeInfo1->UsesViewID = initInfo.UsesViewID;
      m_pPSVRuntimeInfo1->SigInputElements = initInfo.SigInputElements;
      m_pPSVRuntimeInfo1->SigOutputElements = initInfo.SigOutputElements;
      m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements = initInfo.SigPatchConstOrPrimElements;
      m_pPSVRuntimeInfo1->SigInputVectors = initInfo.SigInputVectors;
      memcpy(m_pPSVRuntimeInfo1->SigOutputVectors, initInfo.SigOutputVectors, 4);
      if (IsHS() || IsDS() || IsMS()) {
        m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors = initInfo.SigPatchConstOrPrimVectors;
      }

      // Note: if original size was unaligned, padding has already been zero initialized
      m_StringTable.Size = PSVALIGN4(initInfo.StringTable.Size);
      *(uint32_t*)pCurBits = m_StringTable.Size;
      pCurBits += sizeof(uint32_t);
      memcpy(pCurBits, initInfo.StringTable.Table, initInfo.StringTable.Size);
      m_StringTable.Table = (const char *)pCurBits;
      pCurBits += m_StringTable.Size;

      m_SemanticIndexTable.Entries = initInfo.SemanticIndexTable.Entries;
      *(uint32_t*)pCurBits = m_SemanticIndexTable.Entries;
      pCurBits += sizeof(uint32_t);
      memcpy(pCurBits, initInfo.SemanticIndexTable.Table, sizeof(uint32_t) * initInfo.SemanticIndexTable.Entries);
      m_SemanticIndexTable.Table = (const uint32_t*)pCurBits;
      pCurBits += sizeof(uint32_t) * m_SemanticIndexTable.Entries;

      // Dxil Signature Elements
      if (m_pPSVRuntimeInfo1->SigInputElements || m_pPSVRuntimeInfo1->SigOutputElements || m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements) {
        *(uint32_t*)pCurBits = m_uPSVSignatureElementSize;
        pCurBits += sizeof(uint32_t);
      }
      if (m_pPSVRuntimeInfo1->SigInputElements) {
        m_pSigInputElements = (PSVSignatureElement0*)pCurBits;
        pCurBits += m_uPSVSignatureElementSize * m_pPSVRuntimeInfo1->SigInputElements;
      }
      if (m_pPSVRuntimeInfo1->SigOutputElements) {
        m_pSigOutputElements = (PSVSignatureElement0*)pCurBits;
        pCurBits += m_uPSVSignatureElementSize * m_pPSVRuntimeInfo1->SigOutputElements;
      }
      if (m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements) {
        m_pSigPatchConstOrPrimElements = (PSVSignatureElement0*)pCurBits;
        pCurBits += m_uPSVSignatureElementSize * m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements;
      }

      // ViewID dependencies
      if (m_pPSVRuntimeInfo1->UsesViewID) {
        for (unsigned i = 0; i < 4; i++) {
          if (m_pPSVRuntimeInfo1->SigOutputVectors[i]) {
            m_pViewIDOutputMask = (uint32_t*)pCurBits;
            pCurBits += sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigOutputVectors[i]);
          }
          if (!IsGS())
            break;
        }
        if ((IsHS() || IsMS()) && m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors) {
          m_pViewIDPCOrPrimOutputMask = (uint32_t*)pCurBits;
          pCurBits += sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors);
        }
      }

      // Input to Output dependencies
      if (m_pPSVRuntimeInfo1->SigInputVectors > 0) {
        for (unsigned i = 0; i < 4; i++) {
          if (m_pPSVRuntimeInfo1->SigOutputVectors[i] > 0) {
            m_pInputToOutputTable = (uint32_t*)pCurBits;
            pCurBits += PSVComputeInputOutputTableSize(m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigOutputVectors[i]);
          }
          if (!IsGS())
            break;
        }
        if (IsHS() && m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors > 0 && m_pPSVRuntimeInfo1->SigInputVectors > 0) {
          m_pInputToPCOutputTable = (uint32_t*)pCurBits;
          pCurBits += PSVComputeInputOutputTableSize(m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors);
        }
      }
      if (IsDS() && m_pPSVRuntimeInfo1->SigOutputVectors[0] > 0 && m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors > 0) {
        m_pPCInputToOutputTable = (uint32_t*)pCurBits;
        pCurBits += PSVComputeInputOutputTableSize(m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors, m_pPSVRuntimeInfo1->SigOutputVectors[0]);
      }
    }

    return true;
  }

  PSVRuntimeInfo0* GetPSVRuntimeInfo0() const {
    return m_pPSVRuntimeInfo0;
  }

  PSVRuntimeInfo1* GetPSVRuntimeInfo1() const {
    return m_pPSVRuntimeInfo1;
  }

  uint32_t GetBindCount() const {
    return m_uResourceCount;
  }

  PSVResourceBindInfo0* GetPSVResourceBindInfo0(uint32_t index) const {
    if (index < m_uResourceCount && m_pPSVResourceBindInfo &&
        sizeof(PSVResourceBindInfo0) <= m_uPSVResourceBindInfoSize) {
      return (PSVResourceBindInfo0*)((uint8_t*)m_pPSVResourceBindInfo +
        (index * m_uPSVResourceBindInfoSize));
    }
    return nullptr;
  }

  const PSVStringTable &GetStringTable() const { return m_StringTable; }
  const PSVSemanticIndexTable &GetSemanticIndexTable() const { return m_SemanticIndexTable; }

  // Signature element access
  uint32_t GetSigInputElements() const {
    if (m_pPSVRuntimeInfo1)
      return m_pPSVRuntimeInfo1->SigInputElements;
    return 0;
  }
  uint32_t GetSigOutputElements() const {
    if (m_pPSVRuntimeInfo1)
      return m_pPSVRuntimeInfo1->SigOutputElements;
    return 0;
  }
  uint32_t GetSigPatchConstOrPrimElements() const {
    if (m_pPSVRuntimeInfo1)
      return m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements;
    return 0;
  }
  PSVSignatureElement0* GetInputElement0(uint32_t index) const {
    if (m_pPSVRuntimeInfo1 && m_pSigInputElements &&
        index < m_pPSVRuntimeInfo1->SigInputElements &&
        sizeof(PSVSignatureElement0) <= m_uPSVSignatureElementSize) {
      return (PSVSignatureElement0*)((uint8_t*)m_pSigInputElements +
        (index * m_uPSVSignatureElementSize));
    }
    return nullptr;
  }
  PSVSignatureElement0* GetOutputElement0(uint32_t index) const {
    if (m_pPSVRuntimeInfo1 && m_pSigOutputElements &&
        index < m_pPSVRuntimeInfo1->SigOutputElements &&
        sizeof(PSVSignatureElement0) <= m_uPSVSignatureElementSize) {
      return (PSVSignatureElement0*)((uint8_t*)m_pSigOutputElements +
        (index * m_uPSVSignatureElementSize));
    }
    return nullptr;
  }
  PSVSignatureElement0* GetPatchConstOrPrimElement0(uint32_t index) const {
    if (m_pPSVRuntimeInfo1 && m_pSigPatchConstOrPrimElements &&
        index < m_pPSVRuntimeInfo1->SigPatchConstOrPrimElements &&
        sizeof(PSVSignatureElement0) <= m_uPSVSignatureElementSize) {
      return (PSVSignatureElement0*)((uint8_t*)m_pSigPatchConstOrPrimElements +
        (index * m_uPSVSignatureElementSize));
    }
    return nullptr;
  }
  // More convenient wrapper:
  PSVSignatureElement GetSignatureElement(PSVSignatureElement0* pElement0) const {
    return PSVSignatureElement(m_StringTable, m_SemanticIndexTable, pElement0);
  }

  PSVShaderKind GetShaderKind() const {
    if (m_pPSVRuntimeInfo1 && m_pPSVRuntimeInfo1->ShaderStage < (uint8_t)PSVShaderKind::Invalid)
      return (PSVShaderKind)m_pPSVRuntimeInfo1->ShaderStage;
    return PSVShaderKind::Invalid;
  }
  bool IsVS() const { return GetShaderKind() == PSVShaderKind::Vertex; }
  bool IsHS() const { return GetShaderKind() == PSVShaderKind::Hull; }
  bool IsDS() const { return GetShaderKind() == PSVShaderKind::Domain; }
  bool IsGS() const { return GetShaderKind() == PSVShaderKind::Geometry; }
  bool IsPS() const { return GetShaderKind() == PSVShaderKind::Pixel; }
  bool IsCS() const { return GetShaderKind() == PSVShaderKind::Compute; }
  bool IsMS() const { return GetShaderKind() == PSVShaderKind::Mesh; }
  bool IsAS() const { return GetShaderKind() == PSVShaderKind::Amplification; }

  // ViewID dependencies
  PSVComponentMask GetViewIDOutputMask(unsigned streamIndex = 0) const {
    if (!m_pViewIDOutputMask || !m_pPSVRuntimeInfo1 || !m_pPSVRuntimeInfo1->SigOutputVectors[streamIndex])
      return PSVComponentMask();
    return PSVComponentMask(m_pViewIDOutputMask, m_pPSVRuntimeInfo1->SigOutputVectors[streamIndex]);
  }
  PSVComponentMask GetViewIDPCOutputMask() const {
    if ((!IsHS() && !IsMS()) || !m_pViewIDPCOrPrimOutputMask || !m_pPSVRuntimeInfo1 || !m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors)
      return PSVComponentMask();
    return PSVComponentMask(m_pViewIDPCOrPrimOutputMask, m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors);
  }

  // Input to Output dependencies
  PSVDependencyTable GetInputToOutputTable(unsigned streamIndex = 0) const {
    if (m_pInputToOutputTable && m_pPSVRuntimeInfo1) {
      return PSVDependencyTable(m_pInputToOutputTable, m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigOutputVectors[streamIndex]);
    }
    return PSVDependencyTable();
  }
  PSVDependencyTable GetInputToPCOutputTable() const {
    if ((IsHS() || IsMS()) && m_pInputToPCOutputTable && m_pPSVRuntimeInfo1) {
      return PSVDependencyTable(m_pInputToPCOutputTable, m_pPSVRuntimeInfo1->SigInputVectors, m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors);
    }
    return PSVDependencyTable();
  }
  PSVDependencyTable GetPCInputToOutputTable() const {
    if (IsDS() && m_pPCInputToOutputTable && m_pPSVRuntimeInfo1) {
      return PSVDependencyTable(m_pPCInputToOutputTable, m_pPSVRuntimeInfo1->SigPatchConstOrPrimVectors, m_pPSVRuntimeInfo1->SigOutputVectors[0]);
    }
    return PSVDependencyTable();
  }
};

namespace hlsl {

  class ViewIDValidator {
  public:
    enum class Result {
      Success = 0,
      SuccessWithViewIDDependentTessFactor,
      InsufficientInputSpace,
      InsufficientOutputSpace,
      InsufficientPCSpace,
      MismatchedSignatures,
      MismatchedPCSignatures,
      InvalidUsage,
      InvalidPSVVersion,
      InvalidPSV,
    };
    virtual ~ViewIDValidator() {}
    virtual Result ValidateStage(const DxilPipelineStateValidation &PSV,
                                 bool bFinalStage,
                                 bool bExpandInputOnly,
                                 unsigned &mismatchElementId) = 0;
  };

  ViewIDValidator* NewViewIDValidator(unsigned viewIDCount, unsigned gsRastStreamIndex);

}

#endif  // __DXIL_PIPELINE_STATE_VALIDATION__H__
