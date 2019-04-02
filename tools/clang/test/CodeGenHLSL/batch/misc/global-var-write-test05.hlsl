// RUN: %dxc -E main -T ps_6_0 /Gec -HV 2016 > %s | FileCheck %s

// CHECK: define void @main()
// CHECK: ret void

RWTexture2D<float3> Color : register(u0);
groupshared uint PixelCountH;

uint main( uint2 a : A, float3 b : B ) : SV_Target
{
 Color[a] = b; 
 PixelCountH = Color[a].x * 1;
 return PixelCountH;
}
