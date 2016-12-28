// RUN: %dxc -E main -T ps_5_0 %s

cbuffer Foo
{
  float4 g0;
  float4 g1;
  float4 g2;
};

SamplerState samp1 : register(s5);
Texture2D<float2> text1 : register(t3);

float2 main(float2 a : A) : SV_Target
{
  return text1.Sample(samp1, a) + g2.xy;
}
