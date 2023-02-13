// RUN: %dxilver 1.7 | %dxc -E main -T lib_6_8  %s | %listparts | FileCheck %s -check-prefix=SM68
// RUN: %dxilver 1.7 | %dxc -E main -T lib_6_7  %s | %listparts | FileCheck %s -check-prefix=SM67

// SM68: VERS
// SM67-NOT:VERS

float4 main(float4 f0 : TEXCOORD0, uint2 u1 : U1, float2 f2 : TEXCOORD2) : SV_Target
{
	return f0 + (u1 * f2).xyxy;
}