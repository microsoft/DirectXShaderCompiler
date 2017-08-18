// Run: %dxc -T ps_6_0 -E main

// CHECK:      OpDecorate %sampler1 DescriptorSet 0
// CHECK-NEXT: OpDecorate %sampler1 Binding 1
SamplerState sampler1: register(s1);

// CHECK:      OpDecorate %sampler2 DescriptorSet 1
// CHECK-NEXT: OpDecorate %sampler2 Binding 2
SamplerState sampler2 : register(s2, space1);

// Note: overlapping set # and binding # for now.
// CHECK:      OpDecorate %texture1 DescriptorSet 1
// CHECK-NEXT: OpDecorate %texture1 Binding 2
Texture2D<float4> texture1: register(t2, space1);

// Note: overlapping set # and binding # for now.
// CHECK:      OpDecorate %texture2 DescriptorSet 0
// CHECK-NEXT: OpDecorate %texture2 Binding 1
Texture3D<float4> texture2: register(t1);

// Note: using the next available binding #
// CHECK:      OpDecorate %sampler3 DescriptorSet 0
// CHECK-NEXT: OpDecorate %sampler3 Binding 0
SamplerState sampler3;

// Note: using the next available binding #
// CHECK:      OpDecorate %sampler4 DescriptorSet 0
// CHECK-NEXT: OpDecorate %sampler4 Binding 2
SamplerState sampler4;


float4 main() : SV_Target {
    return 1.0;
}
