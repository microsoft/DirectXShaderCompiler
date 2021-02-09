// RUN: %dxc -E main -T ps_6_2 -DTY=uint %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ps_6_2 -DTY=float2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_2 -DTY=int2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_2 -DTY=int2x2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_2 -DTY=float %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ps_6_2 -DTY=bool2 %s | FileCheck %s -check-prefix=CHK_ERR

// CHK_NO_ERR: define void @main
// CHK_ERR: error: invalid type used for 'SV_Coverage' semantic

void main(inout TY c: SV_Coverage)
{
   c=0;
}
