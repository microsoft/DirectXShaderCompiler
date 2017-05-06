// RUN: %dxc -E main -T ps_6_1 %s | FileCheck %s

// CHECK: SV_Barycentric

float4 main(float3 bary : SV_Barycentric) : SV_Target
{
  float4 vcolor0 = float4(1,0,0,1);
  float4 vcolor1 = float4(0,1,0,1);
  float4 vcolor2 = float4(0,0,0,1);
  return bary.x * vcolor0 + bary.y * vcolor1 + bary.z * vcolor2;
}