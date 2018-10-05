// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: :4:40: error: missing register type and number
Buffer<float4> MyBuffer     : register(space1);
// CHECK: :6:40: error: missing register type and number
Texture2D<float4> MyTexture : register(space1);

// CHECK: :9:30: error: missing register type and number
cbuffer MyCbuffer : register(space2) {
    float4 CB_a;
}

// CHECK: :14:30: error: missing register type and number
tbuffer MyTbuffer : register(space2) {
    float4 TB_a;
}

struct S { float4 val; };

// CHECK: :21:41: error: missing register type and number
ConstantBuffer<S> MyCbuffer2 : register(space3);
// CHECK: :23:41: error: missing register type and number
TextureBuffer<S>  MyTbuffer2 : register(space3);

// CHECK: :26:50: error: missing register type and number
StructuredBuffer<S>        MySbuffer1 : register(space4);
// CHECK: :28:50: error: missing register type and number
RWStructuredBuffer<S>      MySbuffer2 : register(space4);
// CHECK: :30:50: error: missing register type and number
AppendStructuredBuffer<S>  MySbuffer3 : register(space4);
// CHECK: :32:50: error: missing register type and number
ConsumeStructuredBuffer<S> MySbuffer4 : register(space4);

float4 main() : SV_Target {
    return MyBuffer[0];
}
