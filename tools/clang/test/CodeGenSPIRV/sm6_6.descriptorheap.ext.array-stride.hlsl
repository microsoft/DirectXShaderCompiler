// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 64 -fvk-sampler-heap-stride 32 -spirv %s | FileCheck %s

// Verifies: each distinct descriptor element type yields exactly one OpTypeRuntimeArray, 
//  and each array gets the correct literal ArrayStride based on CLI.
//
// SB / RWSB / BAB -> OpTypeRuntimeArray %[[SBDesc]] (OpTypeBufferEXT StorageBuffer) -> ArrayStride 64
// ConstantBuffer  -> OpTypeRuntimeArray %[[UBDesc]] (OpTypeBufferEXT Uniform)       -> ArrayStride 64
// Texture2D       -> OpTypeRuntimeArray %[[Tex2DType]] (OpTypeImage sampled)        -> ArrayStride 64
// RWTexture2D     -> OpTypeRuntimeArray %[[RWTexType]] (OpTypeImage storage)        -> ArrayStride 64
// SamplerState    -> OpTypeRuntimeArray %[[SamplerType]] (OpTypeSampler)            -> ArrayStride 32

// CHECK-DAG:       %[[SBDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer
// CHECK-DAG:       %[[UBDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT Uniform
// CHECK-DAG:    %[[Tex2DType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:    %[[RWTexType:[a-zA-Z0-9_]+]] = OpTypeImage %uint 2D 2 0 0 2 R32ui
// CHECK-DAG:  %[[SamplerType:[a-zA-Z0-9_]+]] = OpTypeSampler

// CHECK-DAG:      %[[SBArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SBDesc]]{{$}}
// CHECK-DAG:      %[[UBArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[UBDesc]]{{$}}
// CHECK-DAG:   %[[Tex2DArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex2DType]]{{$}}
// CHECK-DAG:   %[[RWTexArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RWTexType]]{{$}}
// CHECK-DAG: %[[SamplerArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SamplerType]]{{$}}

// CHECK-DAG:                                   OpDecorate %[[SBArray]]      ArrayStride 64
// CHECK-DAG:                                   OpDecorate %[[UBArray]]      ArrayStride 64
// CHECK-DAG:                                   OpDecorate %[[Tex2DArray]]   ArrayStride 64
// CHECK-DAG:                                   OpDecorate %[[RWTexArray]]   ArrayStride 64
// CHECK-DAG:                                   OpDecorate %[[SamplerArray]] ArrayStride 32

struct CBData { uint value; };

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  StructuredBuffer<uint>   sb   = ResourceDescriptorHeap[0];
  RWStructuredBuffer<uint> rwsb = ResourceDescriptorHeap[1];
  ByteAddressBuffer        bab  = ResourceDescriptorHeap[2];
  ConstantBuffer<CBData>   cb   = ResourceDescriptorHeap[3];
  Texture2D<float4>        tex  = ResourceDescriptorHeap[4];
  RWTexture2D<uint>        rwtex = ResourceDescriptorHeap[5];
  SamplerState             samp  = SamplerDescriptorHeap[0];

  uint bufVal = sb.Load(tid.x) + bab.Load(tid.x * 4) + cb.value;
  rwsb[tid.x] = bufVal;
  float4 texVal = tex.SampleLevel(samp, float2(0, 0), 0);
  uint rwOrig;
  InterlockedAdd(rwtex[tid.xy], 1, rwOrig);
  outputBytes.Store(0, rwsb.Load(tid.x) + asuint(texVal.r) + rwOrig);
}
