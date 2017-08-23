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

// CHECK:      OpDecorate %var_myCbuffer DescriptorSet 0
// CHECK-NEXT: OpDecorate %var_myCbuffer Binding 4
cbuffer myCbuffer {
    float4 stuff;
}

// CHECK:      OpDecorate %myBuffer DescriptorSet 0
// CHECK-NEXT: OpDecorate %myBuffer Binding 5
Buffer<int> myBuffer;

// CHECK:      OpDecorate %myRWBuffer DescriptorSet 0
// CHECK-NEXT: OpDecorate %myRWBuffer Binding 6
RWBuffer<float4> myRWBuffer;

float4 main() : SV_Target {
    return 1.0;
}
