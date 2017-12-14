// Run: %dxc -T ps_6_0 -E main

struct S {
    float4 f;
};

ConstantBuffer<S>     myCbuffer1 : register(b0);
ConstantBuffer<S>     myCbuffer2 : register(b0, space1);

RWStructuredBuffer<S> mySBuffer1 : register(u0);         // reuse - disallowed
RWStructuredBuffer<S> mySBuffer2 : register(u0, space1); // reuse - disallowed
RWStructuredBuffer<S> mySBuffer3 : register(u0, space2);

SamplerState mySampler1 : register(s5, space1);
Texture2D    myTexture1 : register(t5, space1); // reuse - allowed

Texture2D    myTexture2 : register(t6, space6);
[[vk::binding(6, 6)]] // reuse - allowed
SamplerState mySampler2;

float4 main() : SV_Target {
    return 1.0;
}

// CHECK: :10:36: warning: resource binding #0 in descriptor set #0 already assigned
// CHECK:  :7:36: note: binding number previously assigned here
// CHECK: :11:36: warning: resource binding #0 in descriptor set #1 already assigned
// CHECK-NOT: :15:{{%\d+}}: warning: resource binding #5 in descriptor set #1 already assigned
// CHECK-NOT: :18:{{%\d+}}: warning: resource binding #6 in descriptor set #6 already assigned
