// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s --check-prefix=NOLITERAL

// Verifies: stride-constant-id attributes turn each heap's ArrayStride into a spec-constant <id>.
//
// resource_heap_stride_constant_id(2) -> OpSpecConstant %uint 64 + SpecId 2
// sampler_heap_stride_constant_id(3)  -> OpSpecConstant %uint 32 + SpecId 3
// resource spec const                 -> OpDecorateId ArrayStrideIdEXT on every resource-heap array
// sampler spec const                  -> OpDecorateId ArrayStrideIdEXT on sampler-heap array
// both heaps overridden               -> no literal ArrayStride survives on any heap runtime array

// ---- One spec constant per heap, with the requested SpecId and default ----
// CHECK-DAG:            OpDecorate %[[RSC:[a-zA-Z0-9_]+]] SpecId 2
// CHECK-DAG:            OpDecorate %[[SSC:[a-zA-Z0-9_]+]] SpecId 3
// CHECK-DAG: %[[RSC]] = OpSpecConstant %uint 64
// CHECK-DAG: %[[SSC]] = OpSpecConstant %uint 32

// ---- ArrayStrideIdEXT references the spec-constant <id>, never a literal ----
// Resource heap: StructuredBuffer/ByteAddressBuffer array, ConstantBuffer array, image array.
// CHECK-DAG:            OpDecorateId %[[SBArr:[a-zA-Z0-9_]+]]  ArrayStrideIdEXT %[[RSC]]
// CHECK-DAG:            OpDecorateId %[[UBArr:[a-zA-Z0-9_]+]]  ArrayStrideIdEXT %[[RSC]]
// CHECK-DAG:            OpDecorateId %[[TexArr:[a-zA-Z0-9_]+]] ArrayStrideIdEXT %[[RSC]]
// Sampler heap.
// CHECK-DAG:            OpDecorateId %[[SampArr:[a-zA-Z0-9_]+]] ArrayStrideIdEXT %[[SSC]]

// ---- Both heaps overridden => no literal ArrayStride on any heap runtime array ----
// The NOLITERAL run (second RUN line) uses only a NOT directive, so FileCheck scans
// the entire output.  "_runtimearr_type" infix targets only heap runtime arrays;
// "_runtimearr_uint ArrayStride 4" (RWByteAddressBuffer internal array) does not
// match and is not a false positive.  The positive CHECK-DAGs above guard the
// primary regression (ArrayStrideIdEXT present); this guards double-decoration.
// NOLITERAL-NOT: _runtimearr_type{{[a-zA-Z0-9_]+}} ArrayStride {{[0-9]+}}

struct CBData { uint value; };

[[vk::resource_heap_stride_constant_id(2)]] const uint kResourceHeapStride = 64;
[[vk::sampler_heap_stride_constant_id(3)]]  const uint kSamplerHeapStride  = 32;

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    StructuredBuffer<uint>  sb  = ResourceDescriptorHeap[0];
    ByteAddressBuffer       bab = ResourceDescriptorHeap[1];
    ConstantBuffer<CBData>  cb  = ResourceDescriptorHeap[2];
    Texture2D<float4>       tex = ResourceDescriptorHeap[3];
    SamplerState           samp = SamplerDescriptorHeap[0];

    uint v = sb.Load(tid.x) + bab.Load(tid.x * 4) + cb.value;
    outputBytes.Store(0, v);
    outputBytes.Store(4, (uint)tex.SampleLevel(samp, float2(0, 0), 0).r);
}
