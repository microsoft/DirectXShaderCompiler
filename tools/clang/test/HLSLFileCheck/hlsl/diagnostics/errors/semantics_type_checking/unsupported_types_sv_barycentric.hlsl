// RUN: %dxc -E main -T ps_6_4 -DTY=float3 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=min16float3 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -enable-16bit-types -DTY=float16_t3 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=float %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=float2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=float4 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=uint %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=bool %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=uint3 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=int2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=int2x1 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_4 -DTY=uint4 %s | FileCheck %s -check-prefix=CHK_ERR

// CHK_NO_ERR: define void @main
// CHK_ERR: error: invalid type used for 'SV_Barycentrics' semantic

float4 main(TY a : SV_Barycentrics) : SV_Target
{
  return 0;
}