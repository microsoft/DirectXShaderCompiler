// Run: %dxc -T vs_6_0 -E main

SamplerState gSampler : register(s5);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1D   <float4> t1 : register(t1);
Texture2D   <float4> t2 : register(t2);
Texture3D   <float4> t3 : register(t3);
TextureCube <float4> t4 : register(t4);

float4 main(int2 offset: A) : SV_Position {
// CHECK: Use constant value for offset (SPIR-V spec does not accept a variable offset for OpImage* instructions other than OpImage*Gather)
    float4 val2 = t2.Sample(gSampler, float2(0.5, 0.25), offset);

    float clamp;
// CHECK: Use constant value for offset (SPIR-V spec does not accept a variable offset for OpImage* instructions other than OpImage*Gather)
    float4 val5 = t2.Sample(gSampler, float2(0.5, 0.25), offset, clamp);

    uint status;
// CHECK: Use constant value for offset (SPIR-V spec does not accept a variable offset for OpImage* instructions other than OpImage*Gather)
    float4 val7 = t2.Sample(gSampler, float2(0.5, 0.25), offset, clamp, status);

    return float4(0.0, 0.0, 0.0, 1.0);
}
