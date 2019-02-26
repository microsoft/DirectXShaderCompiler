// RUN: %dxc -E main -T ps_6_0  -Zi -Od %s | FileCheck %s

// CHECK: main

SamplerState samp1 : register(s5);
Texture2D<float4> text1 : register(t3);


float4 main(float2 a : A) : SV_Target {
  float4 r = 0;
  for (uint x=0; x<2;x++)
  for (uint y=0; y<2;y++) {
    r += text1.Sample(samp1, a, int2(x+y,x-y));
  }
  return r;
}
