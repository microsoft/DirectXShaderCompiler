// Run: %dxc -T ps_6_0 -E main -fvk-ignore-unused-resources

SamplerState gSampler;

// CHECK:      %gRWBuffer1 = OpVariable
// CHECK-NOT:  %gRWBuffer2 = OpVariable
// CHECK-NOT:  %gRWBuffer3 = OpVariable
// CHECK:      %gRWBuffer4 = OpVariable
RWBuffer<float4> gRWBuffer1;
RWBuffer<float4> gRWBuffer2;
RWBuffer<float4> gRWBuffer3;
RWBuffer<float4> gRWBuffer4;

// CHECK-NOT:  %gTex2D1 = OpVariable
// CHECK:      %gTex2D2 = OpVariable
// CHECK:      %gTex2D3 = OpVariable
// CHECK-NOT:  %gTex2D4 = OpVariable
Texture2D gTex2D1;
Texture2D gTex2D2;
Texture2D gTex2D3;
Texture2D gTex2D4;

// CHECK-NOT: %gCBuffer1 = OpVariable
cbuffer gCBuffer1 {
    float4 cb_f;
};

// CHECK:     %gTBuffer1 = OpVariable
tbuffer gTBuffer1 {
    float4 tb_f;
};

struct S {
    float4 f;
};

// CHECK:     %gCBuffer2 = OpVariable
// CHECK-NOT: %gTBuffer1 = OpVariable
ConstantBuffer<S> gCBuffer2;
TextureBuffer<S> gTBuffer2;

// CHECK:     %gASBuffer1 = OpVariable
// CHECK-NOT: %gASBuffer2 = OpVariable
AppendStructuredBuffer<S> gASBuffer1;
AppendStructuredBuffer<S> gASBuffer2;

// CHECK-NOT: %gCSBuffer1 = OpVariable
// CHECK:     %gCSBuffer2 = OpVariable
ConsumeStructuredBuffer<S> gCSBuffer1;
ConsumeStructuredBuffer<S> gCSBuffer2;

float4 foo(float4 param) {
    return param;
}

float4 bar(float4 param) {
    return param;
}

float4 main() : SV_Target {
    S val = {1., 2., 3., 4.};

    float4 ret =
        gRWBuffer1.Load(0) + gRWBuffer4[1] +
        gTex2D2.Sample(gSampler, 2.) + gTex2D3[float2(3., 4.)] +
        tb_f + gCBuffer2.f;

    gASBuffer1.Append(val);

    ret += gCSBuffer2.Consume().f;

    return ret;
}
