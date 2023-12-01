// RUN: %dxc -T lib_6_3 -D e1 %s | FileCheck -check-prefix=CHECK_ENTRY1 %s
// RUN: %dxc -T lib_6_3 -D e2 %s
// RUN: %dxc -T lib_6_3 -D e3 %s | FileCheck -check-prefix=CHECK_ENTRY3 %s


#ifdef e1
// CHECK_ENTRY1: error: Invalid system value semantic 'SV_DispatchThreadID' for launchtype 'Thread'
// CHECK_ENTRY1: error: Invalid system value semantic 'SV_GroupID' for launchtype 'Thread'
// CHECK_ENTRY1: error: Invalid system value semantic 'SV_GroupThreadID' for launchtype 'Thread'
// CHECK_ENTRY1: error: Invalid system value semantic 'SV_GroupIndex' for launchtype 'Thread'
[shader("node")]
[NodeLaunch("thread")]
[numthreads(1,1,1)]
void entry( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
}
#endif

#ifdef e2
// no expected errors
[shader("node")]
[NodeDispatchGrid(1,1,1)]
[NodeLaunch("broadcasting")]
[numthreads(1,1,1)]
void entry2( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
}
#endif

#ifdef e3
// CHECK_ENTRY3: error: Invalid system value semantic 'SV_DispatchThreadID' for launchtype 'Coalescing'
// CHECK_ENTRY3: error: Invalid system value semantic 'SV_GroupID' for launchtype 'Coalescing'
[shader("node")]
[NodeLaunch("coalescing")]
[numthreads(1,1,1)]
void entry3( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
}
#endif
