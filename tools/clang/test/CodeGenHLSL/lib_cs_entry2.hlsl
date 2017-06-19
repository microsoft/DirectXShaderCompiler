// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: redefinition of entry

[numthreads(8,8,1)]
void entry( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
}

[numthreads(8,8,1)]
void entry( uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
}