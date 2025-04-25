//===------------ DXRUtil.h - DXR Utility Functions ------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DXRUtil.h                                                                 //
// Copyright (C) Nvidia Corporation. All rights reserved.                    //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This file contains the utility functions for DXR execution tests.        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

//= DXR Utility
//============================================================================
#define SHADER_ID_SIZE_IN_BYTES 32

#ifndef ROUND_UP
#define ROUND_UP(v, powerOf2Alignment)                                         \
  (((v) + (powerOf2Alignment)-1) & ~((powerOf2Alignment)-1))
#endif
struct SceneConsts {
  DirectX::XMFLOAT4 eye;
  DirectX::XMFLOAT4 U;
  DirectX::XMFLOAT4 V;
  DirectX::XMFLOAT4 W;
  float sceneScale;
  unsigned windowSize[2];
  int rayFlags;
};

struct Instance {
  D3D12_RAYTRACING_GEOMETRY_TYPE type;
  DirectX::XMFLOAT4X4 matrix;
  UINT geometryCount;
  UINT bottomASIdx;
  UINT instanceID;
  UINT mask;
  UINT flags;
};

class ShaderTable {
public:
  void Init(ID3D12Device *device, int raygenCount, int missCount,
            int hitGroupCount, int rayTypeCount, int rootTableDwords) {
    m_rayTypeCount = rayTypeCount;
    m_raygenCount = raygenCount;
    m_missCount = missCount * rayTypeCount;
    m_hitGroupCount = hitGroupCount * rayTypeCount;
    m_rootTableSizeInBytes = rootTableDwords * 4;
    m_shaderRecordSizeInBytes =
        ROUND_UP(m_rootTableSizeInBytes + SHADER_ID_SIZE_IN_BYTES,
                 D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    m_missStartIdx = m_raygenCount;
    m_hitGroupStartIdx = m_missStartIdx + m_missCount;

    const int m_totalSizeInBytes =
        (m_raygenCount + m_missCount + m_hitGroupCount) *
        m_shaderRecordSizeInBytes;

    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(
        m_totalSizeInBytes, D3D12_RESOURCE_FLAG_NONE,
        std::max(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT,
                 D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));
    CD3DX12_HEAP_PROPERTIES heap =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    VERIFY_SUCCEEDED(device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
        IID_PPV_ARGS(&m_sbtResource)));
    m_sbtResource->SetName(L"SBT Resource Heap");
    CD3DX12_HEAP_PROPERTIES upload =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    VERIFY_SUCCEEDED(device->CreateCommittedResource(
        &upload, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_sbtUploadResource)));
    m_sbtUploadResource->SetName(L"SBT Upload Heap");

    VERIFY_SUCCEEDED(m_sbtUploadResource->Map(0, nullptr, (void **)&m_hostPtr));
  }

  void Upload(ID3D12GraphicsCommandList *cmdlist) {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_sbtResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COPY_DEST);
    cmdlist->ResourceBarrier(1, &barrier);
    cmdlist->CopyResource(m_sbtResource, m_sbtUploadResource);
    CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        m_sbtResource, D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    cmdlist->ResourceBarrier(1, &barrier2);
  }

  int GetShaderRecordSizeInBytes() { return m_shaderRecordSizeInBytes; }

  int GetRaygenShaderRecordIdx(int idx) { return idx; }
  int GetMissShaderRecordIdx(int idx, int rayType) {
    return m_missStartIdx + idx * m_rayTypeCount + rayType;
  }
  int GetHitGroupShaderRecordIdx(int idx, int rayType) {
    return m_hitGroupStartIdx + idx * m_rayTypeCount + rayType;
  }

  void *GetRaygenShaderIdPtr(int idx) {
    return m_hostPtr +
           GetRaygenShaderRecordIdx(idx) * m_shaderRecordSizeInBytes;
  }
  void *GetMissShaderIdPtr(int idx, int rayType) {
    return m_hostPtr +
           GetMissShaderRecordIdx(idx, rayType) * m_shaderRecordSizeInBytes;
  }
  void *GetHitGroupShaderIdPtr(int idx, int rayType) {
    return m_hostPtr +
           GetHitGroupShaderRecordIdx(idx, rayType) * m_shaderRecordSizeInBytes;
  }

  void *GetRaygenRootTablePtr(int idx) {
    return (char *)GetRaygenShaderIdPtr(idx) + SHADER_ID_SIZE_IN_BYTES;
  }
  void *GetMissRootTablePtr(int idx, int rayType) {
    return (char *)GetMissShaderIdPtr(idx, rayType) + SHADER_ID_SIZE_IN_BYTES;
  }
  void *GetHitGroupRootTablePtr(int idx, int rayType) {
    return (char *)GetHitGroupShaderIdPtr(idx, rayType) +
           SHADER_ID_SIZE_IN_BYTES;
  }

  int GetRaygenRangeInBytes() {
    return m_raygenCount * m_shaderRecordSizeInBytes;
  }
  int GetMissRangeInBytes() { return m_missCount * m_shaderRecordSizeInBytes; }
  int GetHitGroupRangeInBytes() {
    return m_hitGroupCount * m_shaderRecordSizeInBytes;
  }

  D3D12_GPU_VIRTUAL_ADDRESS GetRaygenStartGpuVA() {
    return m_sbtResource->GetGPUVirtualAddress() +
           GetRaygenShaderRecordIdx(0) * m_shaderRecordSizeInBytes;
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetMissStartGpuVA() {
    return m_sbtResource->GetGPUVirtualAddress() +
           GetMissShaderRecordIdx(0, 0) * m_shaderRecordSizeInBytes;
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetHitGroupStartGpuVA() {
    return m_sbtResource->GetGPUVirtualAddress() +
           GetHitGroupShaderRecordIdx(0, 0) * m_shaderRecordSizeInBytes;
  }

private:
  CComPtr<ID3D12Resource> m_sbtResource;
  CComPtr<ID3D12Resource> m_sbtUploadResource;
  char *m_hostPtr = nullptr;
  int m_rayTypeCount = 0;
  int m_raygenCount = 0;
  int m_missCount = 0;
  int m_hitGroupCount = 0;
  int m_rootTableSizeInBytes = 0;
  int m_shaderRecordSizeInBytes = 0;
  int m_missStartIdx = 0;
  int m_hitGroupStartIdx = 0;
};

//-----------------------------------------------------------------------------
void AllocateBuffer(
    ID3D12Device *pDevice, UINT64 bufferSize, ID3D12Resource **ppResource,
    bool allowUAV = false,
    D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON,
    const wchar_t *resourceName = nullptr) {
  auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
      bufferSize, allowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                           : D3D12_RESOURCE_FLAG_NONE);
  VERIFY_SUCCEEDED(pDevice->CreateCommittedResource(
      &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
      initialResourceState, nullptr, IID_PPV_ARGS(ppResource)));
  if (resourceName) {
    (*ppResource)->SetName(resourceName);
  }
}

//-----------------------------------------------------------------------------
void ReallocScratchResource(ID3D12Device *pDevice, ID3D12Resource **ppResource,
                            UINT64 nbytes) {

  if (!(*ppResource) || (*ppResource)->GetDesc().Width < nbytes) {
    AllocateBuffer(pDevice, nbytes, ppResource, true,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"scratchResource");
  }
}

//-----------------------------------------------------------------------------
void AllocateUploadBuffer(ID3D12Device *pDevice, const void *pData,
                          UINT64 datasize, ID3D12Resource **ppResource,
                          const wchar_t *resourceName = nullptr) {
  auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(datasize);
  VERIFY_SUCCEEDED(pDevice->CreateCommittedResource(
      &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(ppResource)));
  if (resourceName) {
    (*ppResource)->SetName(resourceName);
  }
  void *pMappedData;
  VERIFY_SUCCEEDED((*ppResource)->Map(0, nullptr, &pMappedData));
  memcpy(pMappedData, pData, datasize);
  (*ppResource)->Unmap(0, nullptr);
}

//-----------------------------------------------------------------------------
void AllocateBufferFromUpload(ID3D12Device *pDevice,
                              ID3D12GraphicsCommandList *pCommandList,
                              ID3D12Resource *uploadSource,
                              ID3D12Resource **ppResource,
                              D3D12_RESOURCE_STATES targetResourceState,
                              const wchar_t *resourceName = nullptr) {
  const bool allowUAV =
      targetResourceState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  AllocateBuffer(pDevice, uploadSource->GetDesc().Width, ppResource, allowUAV,
                 D3D12_RESOURCE_STATE_COPY_DEST, resourceName);
  pCommandList->CopyResource(*ppResource, uploadSource);
  CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      *ppResource, D3D12_RESOURCE_STATE_COPY_DEST, targetResourceState);
  pCommandList->ResourceBarrier(1, (const D3D12_RESOURCE_BARRIER *)&barrier);
}

//= DXR Utility
//============================================================================