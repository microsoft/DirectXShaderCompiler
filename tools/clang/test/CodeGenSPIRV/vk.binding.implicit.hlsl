// Run: %dxc -T ps_6_0 -E main

// CHECK:      OpDecorate %sampler1 DescriptorSet 0
// CHECK-NEXT: OpDecorate %sampler1 Binding 0
SamplerState sampler1;

// CHECK:      OpDecorate %texture1 DescriptorSet 0
// CHECK-NEXT: OpDecorate %texture1 Binding 1
Texture2D<float4> texture1;

// CHECK:      OpDecorate %texture2 DescriptorSet 0
// CHECK-NEXT: OpDecorate %texture2 Binding 2
Texture3D<float4> texture2;

// CHECK:      OpDecorate %sampler2 DescriptorSet 0
// CHECK-NEXT: OpDecorate %sampler2 Binding 3
SamplerState sampler2;


float4 main() : SV_Target {
    return 1.0;
}
