// Run: %dxc -T ps_6_0 -E main

[[vk::binding(1)]]
SamplerState sampler1      : register(s1, space1);

[[vk::binding(3, 1)]]
SamplerState sampler2      : register(s2);

[[vk::binding(1)]] // duplicate
Texture2D<float4> texture1;

[[vk::binding(3, 1)]] // duplicate
Texture3D<float4> texture2 : register(t0, space0);

[[vk::binding(3, 1)]] // duplicate
Texture3D<float4> texture3 : register(t0, space0);

float4 main() : SV_Target {
    return 1.0;
}

// CHECK: :9:3: error: resource binding #1 in descriptor set #0 already assigned
// CHECK: :12:3: error: resource binding #3 in descriptor set #1 already assigned
// CHECK: :15:3: error: resource binding #3 in descriptor set #1 already assigned
