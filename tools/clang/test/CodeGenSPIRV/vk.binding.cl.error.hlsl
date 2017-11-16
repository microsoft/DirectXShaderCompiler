// Run: %dxc -T ps_6_0 -E main -fvk-b-shift 2 0 -fvk-t-shift 2 0 -fvk-s-shift 3 0 -fvk-u-shift 3 0

struct S {
    float4 f;
};

[[vk::binding(2)]]
ConstantBuffer<S> cbuffer3;

ConstantBuffer<S> cbuffer1 : register(b0); // Collision with cbuffer3 after shift

Texture2D<float4> texture1: register(t0, space1);
Texture2D<float4> texture2: register(t0); // Collision with cbuffer3 after shift

SamplerState sampler1: register(s0);
SamplerState sampler2: register(s0, space2);

RWBuffer<float4> rwbuffer1 : register(u0, space3);
RWBuffer<float4> rwbuffer2 : register(u0); // Collision with sampler1 after shift

float4 main() : SV_Target {
    return cbuffer1.f;
}

//CHECK: :10:30: warning: resource binding #2 in descriptor set #0 already assigned
//CHECK:   :7:3: note: binding number previously assigned here

//CHECK: :13:29: warning: resource binding #2 in descriptor set #0 already assigned

//CHECK: :19:30: warning: resource binding #3 in descriptor set #0 already assigned