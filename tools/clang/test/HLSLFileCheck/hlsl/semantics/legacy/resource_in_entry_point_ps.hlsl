// RUN: %dxc /E main -T ps_6_0 %s | FileCheck %s
// CHECK: warning: effect state block ignored - effect syntax is deprecated. To use braces as an initializer use them with equal signs. [-Weffects-syntax]
// CHECK: error: Resource types only valid for non-entry-point functions.


#define ROOT_SIG [RootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), DescriptorTable( SRV(t0)) ")]

SamplerState PointSampler { Filter = MIN_MAG_MIP_POINT; AddressU = Clamp; AddressV = Clamp; };

Texture2D<float> g_fallback_tex              : register(t0);

void main_vertex
(
	float4 position	: POSITION,
	float4 color	: COLOR,
	float2 texCoord : TEXCOORD0,

	uniform float4x4 modelViewProj,

	out float4 oPosition : POSITION,
	out float4 oColor    : COLOR,
	out float2 otexCoord : TEXCOORD
)
{
	oPosition = mul(modelViewProj, position);
	oColor = color;
	otexCoord = texCoord;
}

ROOT_SIG
int main(float2 texCoord : TEXCOORD0, Texture2D<float> g_fallback_tex  : TEXUNIT0) : SV_Target
{
	return 3;
}  