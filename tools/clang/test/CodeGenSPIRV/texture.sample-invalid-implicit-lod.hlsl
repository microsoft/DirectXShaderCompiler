// Run: %dxc -T vs_6_0 -E main

SamplerState gSampler : register(s5);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1D   <float4> t1 : register(t1);
Texture2D   <float4> t2 : register(t2);
Texture3D   <float4> t3 : register(t3);
TextureCube <float4> t4 : register(t4);

float4 main(int2 offset: A) : SV_Position {
// CHECK: sampling with implicit lod is only allowed in fragment shaders
    float4 val1 = t1.Sample(gSampler, 0.5);

// CHECK: sampling with implicit lod is only allowed in fragment shaders
    float4 val2 = t2.Sample(gSampler, float2(0.5, 0.25), offset);

// CHECK: sampling with implicit lod is only allowed in fragment shaders
    float4 val3 = t3.Sample(gSampler, float3(0.5, 0.25, 0.3), 3);

// CHECK: sampling with implicit lod is only allowed in fragment shaders
    float4 val4 = t4.Sample(gSampler, float3(0.5, 0.25, 0.3));

    float clamp;
// CHECK: sampling with implicit lod is only allowed in fragment shaders
    float4 val5 = t2.Sample(gSampler, float2(0.5, 0.25), offset, clamp);

// CHECK: sampling with implicit lod is only allowed in fragment shaders
    float4 val6 = t4.Sample(gSampler, float3(0.5, 0.25, 0.3), /*clamp*/ 2.0f);

    uint status;
// CHECK: sampling with implicit lod is only allowed in fragment shaders
    float4 val7 = t2.Sample(gSampler, float2(0.5, 0.25), offset, clamp, status);

// CHECK: sampling with implicit lod is only allowed in fragment shaders
    float4 val8 = t4.Sample(gSampler, float3(0.5, 0.25, 0.3), /*clamp*/ 2.0f, status);

    return float4(0.0, 0.0, 0.0, 1.0);
}
