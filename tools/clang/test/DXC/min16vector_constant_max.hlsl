// Validate that the offsets for a min16float vector is correct if 16-bit types aren't enabled
// RUN: %dxc -T ps_6_0 -Od -Zi -Qembed_debug %s | FileCheck %s

void main( out min16float4 outColor : SV_Target0)
{
	outColor = max( min16float4 (0,0,0,0), min16float4 (0,0,0,0));
}