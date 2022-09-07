// RUN: %dxc -D PARAMTYPE=Tex2D /E main -T ps_6_0 %s | FileCheck %s

// RUN: %dxc -D PARAMTYPE=RW /E main -T ps_6_0 %s | FileCheck %s

// RUN: %dxc -D PARAMTYPE=SamplerState /E main -T ps_6_0 %s | FileCheck %s

// RUN: %dxc -D PARAMTYPE=float4x4 /E main -T ps_6_0 %s | FileCheck %s
// CHECK: 'uniform' parameter is not allowed on entry function.

#define Tex2D Texture2D<float4>
#define RW RWStructuredBuffer<float4>

SamplerState PointSampler { Filter = MIN_MAG_MIP_POINT; AddressU = Clamp; AddressV = Clamp; };

int main(float2 texCoord : TEXCOORD0, uniform PARAMTYPE decal : TEXUNIT0) : SV_Target
{
	return 3;
}  