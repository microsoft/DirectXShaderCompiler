// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint3 -DGI_TY=uint -DGID_TY=uint3 -DGTID_TY=uint3 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint2 -DGI_TY=uint -DGID_TY=uint2 -DGTID_TY=uint2 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=uint -DGI_TY=uint -DGID_TY=uint -DGTID_TY=uint %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=min16uint3 -DGI_TY=min16uint -DGID_TY=min16uint3 -DGTID_TY=min16uint3 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=min16uint2 -DGI_TY=min16uint -DGID_TY=min16uint2 -DGTID_TY=min16uint2 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T cs_6_0 -DTID_TY=min16uint -DGI_TY=min16uint -DGID_TY=min16uint -DGTID_TY=min16uint %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTID_TY=uint16_t3 -DGI_TY=uint16_t -DGID_TY=uint16_t3 -DGTID_TY=uint16_t3 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTID_TY=uint16_t2 -DGI_TY=uint16_t -DGID_TY=uint16_t2 -DGTID_TY=uint16_t2 %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTID_TY=uint16_t -DGI_TY=uint16_t -DGID_TY=uint16_t -DGTID_TY=uint16_t %s | FileCheck %s -check-prefix=CHK_NO_ERR
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
// CHK_TID_TY_ERR: error: invalid type used for 'SV_DispatchThreadID' semantic
// CHK_GI_TY_ERR: error: invalid type used for 'SV_GroupIndex' semantic
// CHK_GID_TY_ERR: error: invalid type used for 'SV_GroupID' semantic
// CHK_GTID_TY_ERR: error: invalid type used for 'SV_GroupThreadID' semantic


[numthreads(1, 1, 1)]
void main(TID_TY tid : SV_DispatchThreadID, 
GI_TY gi : SV_GroupIndex, 
GID_TY gid : SV_GroupID, 
GTID_TY gtid : SV_GroupThreadID ) {}
