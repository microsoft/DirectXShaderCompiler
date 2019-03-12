// Make sure same cbuffer decalred in different lib works.

cbuffer A {
  float a;
  float v;
}

Texture2D	tex;
SamplerState	samp;

float GetV();

[shader("pixel")]
float4 main() : SV_Target
{
   return tex.Sample(samp, float2(a, GetV()));
}