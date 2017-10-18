// Run: %dxc -T ps_6_0 -E main

struct S {
    float4 f;
};

ConstantBuffer<S>     myCbuffer1 : register(b0);
ConstantBuffer<S>     myCbuffer2 : register(b0, space1);

RWStructuredBuffer<S> mySBuffer1 : register(u0);         // duplicate
RWStructuredBuffer<S> mySBuffer2 : register(u0, space1); // duplicate
RWStructuredBuffer<S> mySBuffer3 : register(u0, space2);

float4 main() : SV_Target {
    return 1.0;
}

// CHECK: :10:36: error: resource binding #0 in descriptor set #0 already assigned
// CHECK: :11:36: error: resource binding #0 in descriptor set #1 already assigned