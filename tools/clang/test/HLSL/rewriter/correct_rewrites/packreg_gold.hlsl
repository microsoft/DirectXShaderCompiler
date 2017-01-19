// Rewrite unchanged result:
float4 f_no_conflict : register(vs, c0) : register(ps, c1);
cbuffer MyFloats {
  float4 f4_simple : packoffset(c0);
}
tbuffer OtherFloats {
  float4 f4_t_simple : packoffset(c10);
  float3 f3_t_simple : packoffset(c11.y);
  float2 f2_t_simple : packoffset(c12.z);
}
sampler myVar : register(ps_6_0, s0);
sampler myVar2 : register(vs, s0[8]);
sampler myVar2_offset : register(vs, s2[8]);
sampler myVar_2 : register(vs, s8);
sampler myVar65536 : register(vs, s65536);
sampler myVar5 : register(vs, s0);
AppendStructuredBuffer<float4> myVar11 : register(ps, u1);
Buffer<float4> myVar11_plain : register(ps, u2);
RWStructuredBuffer<float4> myVar11_rw : register(ps, u0);
sampler myVar_s : register(ps, s0);
Texture2D myVar_t : register(ps, t0);
Texture2D myVar_t_1 : register(ps, t0[1]);
Texture2D myVar_t_1_1 : register(ps, t1[1]);
float4 myVar_b : register(ps, b0);
bool myVar_bool : register(ps, b0) : register(ps, c0);
sampler myVar_1 : register(ps, s0[1]);
sampler myVar_11 : register(ps, s0[2]);
sampler myVar_16 : register(ps, s0[15]);
sampler myVar_n1p5 : register(ps, s0);
sampler myVar_s1 : register(ps, s0[1]);
cbuffer MyBuffer {
  float4 Element1 : packoffset(c0);
  float1 Element2 : packoffset(c1);
  float1 Element3 : packoffset(c1.y);
  float4 Element4 : packoffset(c10) : packoffset(c10);
}
Texture2D<float4> Texture_ : register(t0);
sampler Sampler : register(s0);
cbuffer Parameters : register(b0) {
  float4 DiffuseColor : packoffset(c0) : register(c0);
  float4 AlphaTest : packoffset(c1);
  float3 FogColor : packoffset(c2);
  float4 FogVector : packoffset(c3);
  float4x4 WorldViewProj : packoffset(c4);
}
;
cbuffer cbPerObject : register(b0) {
  float4 g_vObjectColor : packoffset(c0);
}
;
cbuffer cbPerFrame : register(b1) {
  float3 g_vLightDir : packoffset(c0);
  float g_fAmbient : packoffset(c0.w);
}
;
cbuffer OuterBuffer {
  float OuterItem0;
  cbuffer InnerBuffer {
    float InnerItem0;
  }
  ;
  float OuterItem1;
}
;
Texture2D g_txDiffuse : register(t0);
SamplerState g_samLinear : register(s0);
float2 f2() {
  g_txDiffuse.Sample(myVar5, float2(1, 2));
  return 0;
}


float4 main(float4 param4 : TEXCOORD0) : SV_Target0 {
  float f = OuterItem0 + OuterItem1 + InnerItem0;
  return g_txDiffuse.Sample(myVar_s, float2(1, f));
}


