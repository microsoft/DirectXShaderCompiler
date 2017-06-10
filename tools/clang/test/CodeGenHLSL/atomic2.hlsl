// RUN: %dxc -E main -T cs_6_0 %s | FileCheck %s

// CHECK: dx.op.atomicBinOp.i32(i32 78
// Make sure offset is 0
// CHECK: i32 0, i32 undef, i32 1)

// CHECK: dx.op.atomicBinOp.i32(i32 78
// Make sure offset is 0
// CHECK: i32 0, i32 undef, i32 %

RWStructuredBuffer<uint> structBuf1 : register( u1 );

[numthreads( 8, 8, 1 )]
void main( uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID )
{
    uint v;
    InterlockedAdd( structBuf1[DTid.z], 1, v);
    InterlockedAdd( structBuf1[DTid.z], v);
}
