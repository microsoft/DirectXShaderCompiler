// RUN: %dxc -E main -T cs_6_0 %s  | FileCheck %s

// CHECK: sampleCmpLevelZero

SamplerComparisonState samp1 : register(s5);
Texture2D<float4> text1 : register(t3);
float cmpVal;
RWBuffer<float4> buf1;

[numthreads(8, 8, 8)]
void main(uint dtID : SV_DispatchThreadID)
{
  uint status;
  float2 a = buf1[dtID].xy;
  float4 r = 0;
  r += text1.SampleCmpLevelZero(samp1, a, cmpVal);
  r += text1.SampleCmpLevelZero(samp1, a, cmpVal, uint2(-5, 7));
  r += text1.SampleCmpLevelZero(samp1, a, cmpVal, uint2(-3, 2), status); r += status;
  buf1[dtID] = r;
}
