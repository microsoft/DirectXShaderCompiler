// RUN: %dxc -E main -T cs_6_0 %s | FileCheck %s

// CHECK: atomicrmw add
// CHECK: atomicrmw add
// CHECK: cmpxchg
// CHECK: cmpxchg
// CHECK: atomicBinOp
// CHECK: atomicBinOp
// CHECK: atomicCompareExchange
// CHECK: atomicCompareExchange
// CHECK: atomicBinOp
// CHECK: atomicBinOp
// CHECK: atomicCompareExchange
// CHECK: atomicCompareExchange
// CHECK: atomicBinOp
// CHECK: atomicCompareExchange
// CHECK: atomicCompareExchange
// CHECK: AtomicAdd

RWByteAddressBuffer rawBuf0 : register( u0 );

struct Foo
{
  float2 a;
  float3 b;
  uint   u;
  int2 c[4];
};
RWStructuredBuffer<Foo> structBuf1 : register( u1 );
RWTexture2D<uint> rwTex2: register( u2 );


groupshared uint shareMem[256];

[numthreads( 8, 8, 1 )]
void main( uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID )
{
    shareMem[GI] = 0;

    GroupMemoryBarrierWithGroupSync();
    uint v;

    InterlockedAdd( shareMem[DTid.x], 1 );
    InterlockedAdd( shareMem[DTid.x], 1, v );
    InterlockedCompareStore( shareMem[DTid.x], 1, v );
    InterlockedCompareExchange( shareMem[DTid.x], 1, 2, v );

    InterlockedAdd( rwTex2[DTid.xy], v );
    InterlockedAdd( rwTex2[DTid.xy], 1, v );
    InterlockedCompareStore( rwTex2[DTid.xy], 1, v );
    InterlockedCompareExchange( rwTex2[DTid.xy], 1, 2, v );

    InterlockedAdd( structBuf1[DTid.z].u, v);
    InterlockedAdd( structBuf1[DTid.z].u, 1, v);
    InterlockedCompareStore( structBuf1[DTid.z].u, 1, v);
    InterlockedCompareExchange( structBuf1[DTid.z].u, 1, 2, v);

    GroupMemoryBarrierWithGroupSync();

    rawBuf0.InterlockedAdd( GI * 4, shareMem[GI], v );
    rawBuf0.InterlockedCompareStore( GI * 4, shareMem[GI], v );
    rawBuf0.InterlockedCompareExchange( GI * 4, shareMem[GI], 2, v );
    rawBuf0.InterlockedAdd( GI * 4, v );
}
