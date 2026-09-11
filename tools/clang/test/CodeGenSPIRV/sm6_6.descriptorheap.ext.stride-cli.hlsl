// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -fvk-sampler-heap-stride 16 -spirv %s | FileCheck %s

// Verifies: -fvk-resource-heap-stride / -fvk-sampler-heap-stride emit a literal
// ArrayStride and suppress the default OpConstantSizeOfEXT stride.

// CHECK-DAG: OpDecorate %{{[a-zA-Z0-9_]+}} ArrayStride 128
// CHECK-DAG: OpDecorate %{{[a-zA-Z0-9_]+}} ArrayStride 16
// CHECK-NOT: ArrayStrideIdEXT

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    StructuredBuffer<uint> sb   = ResourceDescriptorHeap[0];
    SamplerState           samp = SamplerDescriptorHeap[0];
    Texture2D<float4>      tex  = ResourceDescriptorHeap[1];
    outputBytes.Store(0, sb.Load(tid.x));
    outputBytes.Store(4, (uint)tex.SampleLevel(samp, float2(0, 0), 0).r);
}
