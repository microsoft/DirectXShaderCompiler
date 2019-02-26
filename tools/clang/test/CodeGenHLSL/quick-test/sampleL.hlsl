// RUN: %dxc -E main -T ps_6_0 %s   | FileCheck %s

// CHECK: sampleLevel


SamplerState samp1 : register(s5);
Texture2D<float4> text1 : register(t3);
int LOD;
float4 main(float2 a : A) : SV_Target
{
  uint status;
  float4 r = 0;
  r += text1.SampleLevel(samp1, a, LOD);
  r += text1.SampleLevel(samp1, a, LOD, uint2(-5, 7));
  r += text1.SampleLevel(samp1, a, LOD, uint2(-3, 2), status); r += status;
  return r;
}
