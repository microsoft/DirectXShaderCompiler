// RUN: %dxc -T ps_6_2 -enable-16bit-types %s | FileCheck %s

// CHECK: @main

Texture2D<half>  texture1   : register(t0);
Texture2D<half4> texture2   : register(t1);

SamplerState samplerState   : register(s0);

half4 main() : SV_TARGET
{
    half A = texture1.SampleLevel(samplerState, float2(0.5, 0.5), 0.0);
    half4 B = texture2.SampleLevel(samplerState, float2(0.5, 0.5), 0.0);
    return A * B;
}