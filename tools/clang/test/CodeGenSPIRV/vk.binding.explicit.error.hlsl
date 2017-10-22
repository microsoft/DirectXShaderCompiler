// Run: %dxc -T ps_6_0 -E main

[[vk::binding(1)]]
SamplerState sampler1      : register(s1, space1);

[[vk::binding(3, 1)]]
SamplerState sampler2      : register(s2);

[[vk::binding(1)]] // reuse - allowed for combined image sampler
Texture2D<float4> texture1;

[[vk::binding(3, 1)]] // reuse - allowed for combined image sampler
Texture3D<float4> texture2 : register(t0, space0);

[[vk::binding(3, 1)]] // reuse - disallowed
Texture3D<float4> texture3 : register(t0, space0);

[[vk::binding(1)]] // reuse - disallowed
SamplerState sampler3      : register(s1, space1);

struct S { float f; };

[[vk::binding(5)]]
StructuredBuffer<S> buf1;

[[vk::binding(5)]] // reuse - disallowed
SamplerState sampler4;

[[vk::binding(5)]] // reuse - disallowed
Texture2D<float4> texture4;

float4 main() : SV_Target {
    return 1.0;
}

// CHECK-NOT:  :9:{{%\d+}}: error: resource binding #1 in descriptor set #0 already assigned
// CHECK-NOT: :12:{{%\d+}}: error: resource binding #3 in descriptor set #1 already assigned
// CHECK: :15:3: error: resource binding #3 in descriptor set #1 already assigned
// CHECK: :18:3: error: resource binding #1 in descriptor set #0 already assigned
// CHECK: :26:3: error: resource binding #5 in descriptor set #0 already assigned
// CHECK: :29:3: error: resource binding #5 in descriptor set #0 already assigned
