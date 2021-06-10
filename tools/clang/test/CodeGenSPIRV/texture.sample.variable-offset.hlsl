// Run: %dxc -T ps_6_0 -E main
SamplerState      gSampler  : register(s5);
Texture2D<float4> t         : register(t1);

// CHECK:      OpImageSparseSampleImplicitLod
// CHECK-SAME: Offset

float4 sample(int2 offset, float clamp) {
    uint status;
    return t.Sample(gSampler, float2(0.1, 0.2), offset, clamp, status);
}

float4 main(int2 offset: A) : SV_Target {
    float4 val = 0;
    for (int i = 0; i < 3; ++i)
        val = sample(offset, i);
    return val;
}
