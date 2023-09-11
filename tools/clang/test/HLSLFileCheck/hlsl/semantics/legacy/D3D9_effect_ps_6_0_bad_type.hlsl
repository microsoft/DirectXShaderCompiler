// RUN: %dxc -D PARAMTYPE=Tex2D /E main_fragment -T ps_6_0 %s | FileCheck %s -check-prefix=CHECK1
// CHECK1: warning: effect state block ignored - effect syntax is deprecated. To use braces as an initializer use them with equal signs. [-Weffects-syntax]
// CHECK1: attribute uniform only valid for non-entry-point functions.

// RUN: %dxc -D PARAMTYPE=RW /E main_fragment -T ps_6_0 %s | FileCheck %s -check-prefix=CHECK2
// CHECK2: warning: effect state block ignored - effect syntax is deprecated. To use braces as an initializer use them with equal signs. [-Weffects-syntax]
// CHECK2: attribute uniform only valid for non-entry-point functions.


// RUN: %dxc -D PARAMTYPE=SamplerState /E main_fragment -T ps_6_0 %s | FileCheck %s -check-prefix=CHECK3
// CHECK3: warning: effect state block ignored - effect syntax is deprecated. To use braces as an initializer use them with equal signs. [-Weffects-syntax]
// CHECK3: attribute uniform only valid for non-entry-point functions.



#define Tex2D Texture2D<float4>
#define RW RWStructuredBuffer<float4>

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

int main_fragment(float2 texCoord : TEXCOORD0, uniform PARAMTYPE decal : TEXUNIT0) : SV_Target
{
	return 3;
} 