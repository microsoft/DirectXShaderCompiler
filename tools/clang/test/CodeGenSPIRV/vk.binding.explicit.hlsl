// Run: %dxc -T ps_6_0 -E main

// CHECK:      OpDecorate %sampler1 DescriptorSet 0
// CHECK-NEXT: OpDecorate %sampler1 Binding 0
[[vk::binding(0)]]
SamplerState sampler1      : register(s1, space1);

// CHECK:      OpDecorate %sampler2 DescriptorSet 1
// CHECK-NEXT: OpDecorate %sampler2 Binding 3
[[vk::binding(3, 1)]]
SamplerState sampler2      : register(s2);

// CHECK:      OpDecorate %texture1 DescriptorSet 0
// CHECK-NEXT: OpDecorate %texture1 Binding 5
[[vk::binding(5)]]
Texture2D<float4> texture1;

// CHECK:      OpDecorate %texture2 DescriptorSet 2
// CHECK-NEXT: OpDecorate %texture2 Binding 2
[[vk::binding(2, 2)]]
Texture3D<float4> texture2 : register(t0, space0);

float4 main() : SV_Target {
    return 1.0;
}