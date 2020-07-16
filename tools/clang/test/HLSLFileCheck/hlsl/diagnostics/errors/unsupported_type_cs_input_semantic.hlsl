// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=float3 -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_TID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=float1x1 -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_TID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=float -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_TID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=bool -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_TID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=min16float -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_TID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=float3 -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GI_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=float1x1 -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GI_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=float -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GI_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=bool -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GI_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=min16float -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GI_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=float3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=float1x1 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=float -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=bool -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=min16float -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_GID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=float3 %s | FileCheck %s -check-prefix=CHK_GTID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=float1x1 %s | FileCheck %s -check-prefix=CHK_GTID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=float %s | FileCheck %s -check-prefix=CHK_GTID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=bool %s | FileCheck %s -check-prefix=CHK_GTID_TY_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=min16float %s | FileCheck %s -check-prefix=CHK_GTID_TY_ERR

// Verify error gets reported for non-integer types on CS input semantics on integeral types

// CHK_NO_ERR: define void @main
// CHK_TID_TY_ERR: error: invalid type used for 'SV_DispatchThreadID' input semantics, must be of 'integer' type
// CHK_GI_TY_ERR: error: invalid type used for 'SV_GroupIndex' input semantics, must be of 'integer' type
// CHK_GID_TY_ERR: error: invalid type used for 'SV_GroupID' input semantics, must be of 'integer' type
// CHK_GTID_TY_ERR: error: invalid type used for 'SV_GroupThreadID' input semantics, must be of 'integer' type


[numthreads(1, 1, 1)]
void main(TID_TY tid : SV_DispatchThreadID, 
GI_TY gi : SV_GroupIndex, 
GID_TY gid : SV_GroupID, 
GTID_TY gtid : SV_GroupThreadID ) {}
