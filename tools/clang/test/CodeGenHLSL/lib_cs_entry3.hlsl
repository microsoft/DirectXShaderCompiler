// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// Make sure entry function exist.
// CHECK: @entry(
// CHECK: @entry2(

// Make sure cloned function exist.
// CHECK: @"\01?entry
// CHECK: @"\01?entry2

// Make sure function props exist.
// CHECK: dx.func.props

// Make sure function props is correct for [numthreads(8,8,1)].
// CHECK: @entry{{.*}}, i32 5, i32 8, i32 8, i32 1
// CHECK: @entry{{.*}}, i32 5, i32 8, i32 8, i32 1

[numthreads(8,8,1)]
void entry( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
}

[numthreads(8,8,1)]
void entry2( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
}