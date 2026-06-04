// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -fvk-sampler-heap-stride 16 -spirv %s | FileCheck %s --check-prefix=CLI
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -spirv -DWITHATTR %s 2>&1 | FileCheck %s --check-prefix=WARN
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -spirv -DWITHATTR %s | FileCheck %s --check-prefix=OVERRIDE

// Verifies: a command-line literal stride has the HIGHEST precedence.
//   1) -fvk-resource-heap-stride / -fvk-sampler-heap-stride emit a literal ArrayStride.
//   2) the CLI value BEATS a [[vk::*_heap_stride_constant_id]] attribute.
//   3) the overridden attribute is ignored (with a warning); no ArrayStrideIdEXT.

// ---- Command-line literal strides, no attribute ----
// CLI-DAG: OpDecorate %{{[a-zA-Z0-9_]+}} ArrayStride 128
// CLI-DAG: OpDecorate %{{[a-zA-Z0-9_]+}} ArrayStride 16
// CLI-NOT: ArrayStrideIdEXT

// ---- Attribute present but overridden by the command line ----
// WARN: warning: {{.*}}resource_heap_stride_constant_id{{.*}} is ignored because -fvk-resource-heap-stride

// OVERRIDE: OpDecorate %{{[a-zA-Z0-9_]+}} ArrayStride 128
// OVERRIDE-NOT: ArrayStrideIdEXT

#ifdef WITHATTR
[[vk::resource_heap_stride_constant_id(2)]] const uint kResourceHeapStride = 64;
#endif

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    StructuredBuffer<uint> sb   = ResourceDescriptorHeap[0];
    SamplerState           samp = SamplerDescriptorHeap[0];
    Texture2D<float4>      tex  = ResourceDescriptorHeap[1];
    outputBytes.Store(0, sb.Load(tid.x));
    outputBytes.Store(4, (uint)tex.SampleLevel(samp, float2(0, 0), 0).r);
}
