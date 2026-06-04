// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: Heap-stride spec constants coexist with a user [[vk::constant_id]] without
//  SpecId collision. All three emit distinct OpSpecConstant instructions, and
//  the user spec constant remains usable in shader arithmetic (OpSpecConstantOp).

// CHECK-DAG:            OpDecorate %[[USC:[a-zA-Z0-9_]+]] SpecId 0
// CHECK-DAG:            OpDecorate %[[RSC:[a-zA-Z0-9_]+]] SpecId 5
// CHECK-DAG:            OpDecorate %[[SSC:[a-zA-Z0-9_]+]] SpecId 6
// CHECK-DAG: %[[USC]] = OpSpecConstant %uint 16
// CHECK-DAG: %[[RSC]] = OpSpecConstant %uint 64
// CHECK-DAG: %[[SSC]] = OpSpecConstant %uint 32

// Heap arrays use their respective stride spec constants
// CHECK-DAG:            OpDecorateId %{{[a-zA-Z0-9_]+}} ArrayStrideIdEXT %[[RSC]]
// CHECK-DAG:            OpDecorateId %{{[a-zA-Z0-9_]+}} ArrayStrideIdEXT %[[SSC]]

// The user spec constant still participates in constant folding
// CHECK:                OpSpecConstantOp %uint IMul %[[USC]]

[[vk::constant_id(0)]]                      const uint TILE_SIZE           = 16;
[[vk::resource_heap_stride_constant_id(5)]] const uint kResourceHeapStride = 64;
[[vk::sampler_heap_stride_constant_id(6)]]  const uint kSamplerHeapStride  = 32;

RWByteAddressBuffer outputBytes : register(u0);

static const uint kTileArea = TILE_SIZE * TILE_SIZE; // forces OpSpecConstantOp

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    StructuredBuffer<uint> sb   = ResourceDescriptorHeap[0];
    SamplerState           samp = SamplerDescriptorHeap[0];
    Texture2D<float4>      tex  = ResourceDescriptorHeap[1];
    outputBytes.Store(0, sb.Load(tid.x) + kTileArea);
    outputBytes.Store(4, (uint)tex.SampleLevel(samp, float2(0, 0), 0).r);
}
