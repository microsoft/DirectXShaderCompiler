// RUN: %dxc -E main -T cs_6_0 %s | FileCheck %s

// CHECK: Atomic operation on typed buffer is not supported

RWBuffer<uint4> bufA;

[numthreads(8,8,1)]
void main( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
    bufA[tid.x] = gid.x;
    bufA[tid.y].z = gid.y;
    InterlockedOr(bufA[tid.y].y, 2);
 }
