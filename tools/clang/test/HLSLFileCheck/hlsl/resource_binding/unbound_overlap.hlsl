// RUN: %dxc -E main -T ps_6_0 %s  | FileCheck -input-file=stderr %s


// CHECK: error: more than one unbounded resource (Tex1 and Tex2) in space 0


Texture2D<float4> Tex1[] : register(t5);  // unbounded
Texture2D<float4> Tex2[] : register(t6);  // unbounded

SamplerState Samp : register(s0);


float4 main(int4 a : A, float4 coord : TEXCOORD) : SV_TARGET
{
  return (float4)1.0
  * Tex1[0].Sample(Samp, coord.xy)
  * Tex2[0].Sample(Samp, coord.xy)
  ;
}
