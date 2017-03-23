// RUN: %dxc -E main -Zi -Od -T cs_6_0 %s | FileCheck %s

// Make sure createHandle has debug info.
// CHECK: @dx.op.createHandle(i32 59, i8 1, i32 0, i32 0, i1 false), !dbg
// CHECK: @dx.op.createHandle(i32 59, i8 1, i32 1, i32 1, i1 false), !dbg

RWBuffer<uint> uav1;
RWBuffer<uint> uav2;

[ numthreads( 64, 2, 2 ) ]
void main( uint GI : SV_GroupIndex)
{
    RWBuffer<uint> u = uav1;
    u[GI] = GI;
    u = uav2;
    u[GI] = GI+1;
}
