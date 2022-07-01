// RUN: %dxc /E main_fragment -T ps_6_0 %s | FileCheck %s
// CHECK: warning: effect state block ignored - effect syntax is deprecated. To use braces as an initializer use them with equal signs. [-Weffects-syntax]
// CHECK: (type for TEXUNIT) cannot be used as shader inputs or outputs.
SamplerState PointSampler { Filter = MIN_MAG_MIP_POINT; AddressU = Clamp; AddressV = Clamp; };

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

float4 main_fragment(float2 texCoord : TEXCOORD0, uniform Texture2D<float4> decal : TEXUNIT0) : SV_Target
{
	return decal.Sample(PointSampler, texCoord);
}