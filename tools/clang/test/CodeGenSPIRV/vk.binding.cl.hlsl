// Run: %dxc -T ps_6_0 -E main -fvk-b-shift=100 -fvk-t-shift=200 -fvk-s-shift=300 -fvk-u-shift=400

struct S {
    float4 f;
};

// Explicit binding assignment is unaffected.

// CHECK: OpDecorate %cbuffer3 DescriptorSet 0
// CHECK: OpDecorate %cbuffer3 Binding 42
[[vk::binding(42)]]
ConstantBuffer<S> cbuffer3;

// CHECK: OpDecorate %cbuffer1 DescriptorSet 0
// CHECK: OpDecorate %cbuffer1 Binding 100
ConstantBuffer<S> cbuffer1 : register(b0);
// CHECK: OpDecorate %cbuffer2 DescriptorSet 2
// CHECK: OpDecorate %cbuffer2 Binding 100
ConstantBuffer<S> cbuffer2 : register(b0, space2);

// CHECK: OpDecorate %texture1 DescriptorSet 1
// CHECK: OpDecorate %texture1 Binding 201
Texture2D<float4> texture1: register(t1, space1);
// CHECK: OpDecorate %texture2 DescriptorSet 0
// CHECK: OpDecorate %texture2 Binding 201
Texture2D<float4> texture2: register(t1);

// CHECK: OpDecorate %sampler1 DescriptorSet 0
// CHECK: OpDecorate %sampler1 Binding 300
// CHECK: OpDecorate %sampler2 DescriptorSet 2
// CHECK: OpDecorate %sampler2 Binding 300
SamplerState sampler1: register(s0);
SamplerState sampler2: register(s0, space2);

// CHECK: OpDecorate %rwbuffer1 DescriptorSet 3
// CHECK: OpDecorate %rwbuffer1 Binding 403
RWBuffer<float4> rwbuffer1 : register(u3, space3);
// CHECK: OpDecorate %rwbuffer2 DescriptorSet 0
// CHECK: OpDecorate %rwbuffer2 Binding 403
RWBuffer<float4> rwbuffer2 : register(u3);

// Lacking binding assignment is unaffacted.

// CHECK: OpDecorate %cbuffer4 DescriptorSet 0
// CHECK: OpDecorate %cbuffer4 Binding 0
ConstantBuffer<S> cbuffer4;
// CHECK: OpDecorate %cbuffer5 DescriptorSet 0
// CHECK: OpDecorate %cbuffer5 Binding 1
ConstantBuffer<S> cbuffer5;

float4 main() : SV_Target {
    return cbuffer1.f;
}
