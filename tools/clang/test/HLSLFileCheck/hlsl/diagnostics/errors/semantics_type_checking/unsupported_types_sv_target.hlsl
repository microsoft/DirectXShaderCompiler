// RUN: %dxc -E main -T ps_6_4 -DTY=float %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=float2 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=float3 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=float4 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=uint %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=uint2 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=uint3 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=uint4 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=int2x1 %s | FileCheck %s -check-prefix=CHK_ERR

// CHK_NO_ERR: define void @main
// CHK_ERR: error: invalid type used for 'SV_Target' semantic

TY main() : SV_Target
{
  return 0;
}