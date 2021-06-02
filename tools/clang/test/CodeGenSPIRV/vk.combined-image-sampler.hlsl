// Run: %dxc -T ps_6_0 -E main

// CHECK: OpDecorate %gSampler DescriptorSet 3
// CHECK: OpDecorate %gSampler Binding 2
// CHECK: OpDecorate %t2 DescriptorSet 0
// CHECK: OpDecorate %t2 Binding 2

[[vk::combinedImageSampler]] [[vk::binding(2, 3)]]
SamplerState gSampler : register(s5);

[[vk::combinedImageSampler]]
Texture2D<float4> t2 : register(t2);

float4 main(int3 offset: A) : SV_Target {
    return t2.SampleLevel(gSampler, float2(1, 2), 10, 2);
}
