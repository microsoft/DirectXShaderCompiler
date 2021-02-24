// RUN: %dxc -E main -T vs_6_2 -DTY=float %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DTY=float16_t %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T vs_6_2 -DTY=min16float %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T vs_6_2 -DTY=int %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T vs_6_2 -DTY=int2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T vs_6_2 -DTY=int2x2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T vs_6_2 -DTY=uint %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T vs_6_2 -DTY=bool %s | FileCheck %s -check-prefix=CHK_ERR

// CHK_NO_ERR: define void @main
// CHK_ERR: error: invalid type used for 'SV_CullDistance' semantic

void main(TY id : SV_CullDistance) {}
