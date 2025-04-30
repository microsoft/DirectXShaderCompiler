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

#pragma once

//= DXR Utility
//============================================================================
#define SHADER_ID_SIZE_IN_BYTES 32

#ifndef ROUND_UP
#define ROUND_UP(v, PowerOf2Alignment)                                         \
  (((v) + (PowerOf2Alignment)-1) & ~((PowerOf2Alignment)-1))
#endif
struct SceneConsts {
  DirectX::XMFLOAT4 Eye;
  DirectX::XMFLOAT4 U;
  DirectX::XMFLOAT4 V;
  DirectX::XMFLOAT4 W;
  float SceneScale;
  unsigned WindowSize[2];
  int RayFlags;
};

struct Instance {
  D3D12_RAYTRACING_GEOMETRY_TYPE Type;
  DirectX::XMFLOAT4X4 Matrix;
  UINT GeometryCount;
  UINT BottomASIdx;
  UINT InstanceID;
  UINT Mask;
  UINT Flags;
};

class ShaderTable {
public:
  void Init(ID3D12Device *Device, int RaygenCount, int MissCount,
            int HitGroupCount, int RayTypeCount, int RootTableDwords) {
    RayTypeCount = RayTypeCount;
    RaygenCount = RaygenCount;
    MissCount = MissCount * RayTypeCount;
    HitGroupCount = HitGroupCount * RayTypeCount;
    RootTableSizeInBytes = RootTableDwords * 4;
    ShaderRecordSizeInBytes =
        ROUND_UP(RootTableSizeInBytes + SHADER_ID_SIZE_IN_BYTES,
                 D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    MissStartIdx = RaygenCount;
    HitGroupStartIdx = MissStartIdx + MissCount;

    const int TotalSizeInBytes =
        (RaygenCount + MissCount + HitGroupCount) * ShaderRecordSizeInBytes;

    D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(
        TotalSizeInBytes, D3D12_RESOURCE_FLAG_NONE,
        std::max(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT,
                 D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));
    CD3DX12_HEAP_PROPERTIES Heap =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    VERIFY_SUCCEEDED(Device->CreateCommittedResource(
        &Heap, D3D12_HEAP_FLAG_NONE, &Desc,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
        IID_PPV_ARGS(&SBTResource)));
    SBTResource->SetName(L"SBT Resource Heap");
    CD3DX12_HEAP_PROPERTIES Upload =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    VERIFY_SUCCEEDED(Device->CreateCommittedResource(
        &Upload, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&SBTUploadResource)));
    SBTUploadResource->SetName(L"SBT Upload Heap");

    VERIFY_SUCCEEDED(SBTUploadResource->Map(0, nullptr, (void **)&HostPtr));
  }

  void Upload(ID3D12GraphicsCommandList *CmdList) {
    CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        SBTResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COPY_DEST);
    CmdList->ResourceBarrier(1, &Barrier);
    CmdList->CopyResource(SBTResource, SBTUploadResource);
    CD3DX12_RESOURCE_BARRIER Barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        SBTResource, D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    CmdList->ResourceBarrier(1, &Barrier2);
  }

  int GetShaderRecordSizeInBytes() { return ShaderRecordSizeInBytes; }

  int GetRaygenShaderRecordIdx(int Idx) { return Idx; }
  int GetMissShaderRecordIdx(int Idx, int RayType) {
    return MissStartIdx + Idx * RayTypeCount + RayType;
  }
  int GetHitGroupShaderRecordIdx(int Idx, int RayType) {
    return HitGroupStartIdx + Idx * RayTypeCount + RayType;
  }

  void *GetRaygenShaderIdPtr(int Idx) {
    return HostPtr + GetRaygenShaderRecordIdx(Idx) * ShaderRecordSizeInBytes;
  }
  void *GetMissShaderIdPtr(int Idx, int RayType) {
    return HostPtr +
           GetMissShaderRecordIdx(Idx, RayType) * ShaderRecordSizeInBytes;
  }
  void *GetHitGroupShaderIdPtr(int Idx, int RayType) {
    return HostPtr +
           GetHitGroupShaderRecordIdx(Idx, RayType) * ShaderRecordSizeInBytes;
  }

  void *GetRaygenRootTablePtr(int Idx) {
    return (char *)GetRaygenShaderIdPtr(Idx) + SHADER_ID_SIZE_IN_BYTES;
  }
  void *GetMissRootTablePtr(int Idx, int RayType) {
    return (char *)GetMissShaderIdPtr(Idx, RayType) + SHADER_ID_SIZE_IN_BYTES;
  }
  void *GetHitGroupRootTablePtr(int Idx, int RayType) {
    return (char *)GetHitGroupShaderIdPtr(Idx, RayType) +
           SHADER_ID_SIZE_IN_BYTES;
  }

  int GetRaygenRangeInBytes() { return RaygenCount * ShaderRecordSizeInBytes; }
  int GetMissRangeInBytes() { return MissCount * ShaderRecordSizeInBytes; }
  int GetHitGroupRangeInBytes() {
    return HitGroupCount * ShaderRecordSizeInBytes;
  }

  D3D12_GPU_VIRTUAL_ADDRESS GetRaygenStartGpuVA() {
    return SBTResource->GetGPUVirtualAddress() +
           GetRaygenShaderRecordIdx(0) * ShaderRecordSizeInBytes;
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetMissStartGpuVA() {
    return SBTResource->GetGPUVirtualAddress() +
           GetMissShaderRecordIdx(0, 0) * ShaderRecordSizeInBytes;
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetHitGroupStartGpuVA() {
    return SBTResource->GetGPUVirtualAddress() +
           GetHitGroupShaderRecordIdx(0, 0) * ShaderRecordSizeInBytes;
  }

private:
  CComPtr<ID3D12Resource> SBTResource;
  CComPtr<ID3D12Resource> SBTUploadResource;
  char *HostPtr = nullptr;
  int RayTypeCount = 0;
  int RaygenCount = 0;
  int MissCount = 0;
  int HitGroupCount = 0;
  int RootTableSizeInBytes = 0;
  int ShaderRecordSizeInBytes = 0;
  int MissStartIdx = 0;
  int HitGroupStartIdx = 0;
};

//-----------------------------------------------------------------------------
void AllocateBuffer(
    ID3D12Device *Device, UINT64 BufferSize, ID3D12Resource **Resource,
    bool AllowUAV = false,
    D3D12_RESOURCE_STATES InitialResourceState = D3D12_RESOURCE_STATE_COMMON,
    const wchar_t *ResourceName = nullptr) {
  auto UploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
      BufferSize, AllowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                           : D3D12_RESOURCE_FLAG_NONE);
  VERIFY_SUCCEEDED(Device->CreateCommittedResource(
      &UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &BufferDesc,
      InitialResourceState, nullptr, IID_PPV_ARGS(Resource)));
  if (ResourceName) {
    (*Resource)->SetName(ResourceName);
  }
}

//-----------------------------------------------------------------------------
void ReallocScratchResource(ID3D12Device *Device, ID3D12Resource **Resource,
                            UINT64 NBytes) {
  if (!(*Resource) || (*Resource)->GetDesc().Width < NBytes) {
    AllocateBuffer(Device, NBytes, Resource, true,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"scratchResource");
  }
}

//-----------------------------------------------------------------------------
void AllocateUploadBuffer(ID3D12Device *Device, const void *Data,
                          UINT64 DataSize, ID3D12Resource **Resource,
                          const wchar_t *ResourceName = nullptr) {
  auto UploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(DataSize);
  VERIFY_SUCCEEDED(Device->CreateCommittedResource(
      &UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &BufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(Resource)));
  if (ResourceName) {
    (*Resource)->SetName(ResourceName);
  }
  void *MappedData;
  VERIFY_SUCCEEDED((*Resource)->Map(0, nullptr, &MappedData));
  memcpy(MappedData, Data, DataSize);
  (*Resource)->Unmap(0, nullptr);
}

//-----------------------------------------------------------------------------
void AllocateBufferFromUpload(ID3D12Device *Device,
                              ID3D12GraphicsCommandList *CommandList,
                              ID3D12Resource *UploadSource,
                              ID3D12Resource **Resource,
                              D3D12_RESOURCE_STATES TargetResourceState,
                              const wchar_t *ResourceName = nullptr) {
  const bool AllowUAV =
      TargetResourceState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  AllocateBuffer(Device, UploadSource->GetDesc().Width, Resource, AllowUAV,
                 D3D12_RESOURCE_STATE_COPY_DEST, ResourceName);
  CommandList->CopyResource(*Resource, UploadSource);
  CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      *Resource, D3D12_RESOURCE_STATE_COPY_DEST, TargetResourceState);
  CommandList->ResourceBarrier(1, (const D3D12_RESOURCE_BARRIER *)&Barrier);
}

//= DXR Utility
//============================================================================