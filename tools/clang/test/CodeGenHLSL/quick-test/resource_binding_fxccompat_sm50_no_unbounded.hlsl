// RUN: %dxc /T ps_6_0 /E main -flegacy-resource-reservation %s | FileCheck %s

// CHECK: error: unbounded resources are not supported with -flegacy-resource-reservation

Texture2D Tex[];
float4 main() : SV_Target
{
	return 0;
}