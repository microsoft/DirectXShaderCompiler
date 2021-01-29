// RUN: %dxc -E main -T ps_6_0 %s  | FileCheck %s
// CHECK:tx0.s                             sampler      NA          NA      S0             s0     1
// CHECK:tx1.s                             sampler      NA          NA      S1             s1     1
// CHECK:tx0.t                             texture     f32          2d      T0             t0     1
// CHECK:tx1.t                             texture     f32          2d      T1             t1     1

struct LegacyTex
{
	Texture2D t;
	SamplerState  s;
};

LegacyTex tx0;
LegacyTex tx1;

float4 tex2D(LegacyTex tx, float2 uv)
{
	return tx.t.Sample(tx.s,uv);
}

float4 main(float2 uv:UV) : SV_Target
{
	return tex2D(tx0,uv) + tex2D(tx1,uv);
}