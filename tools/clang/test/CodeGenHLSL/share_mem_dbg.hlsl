// RUN: %dxc -E main -T cs_6_0 -Zi -Od %s | FileCheck %s

// CHECK: threadId
// CHECK: groupId
// CHECK: threadIdInGroup
// CHECK: flattenedThreadIdInGroup
// CHECK: addrspace(3)

// CHECK: DIGlobalVariable(name: "dataC.1.0"
// CHECK: DIDerivedType(tag: DW_TAG_member, name: ".1.0"
// CHECK: DIGlobalVariable(name: "dataC.1.1"
// CHECK: DIDerivedType(tag: DW_TAG_member, name: ".1.1"
// CHECK: DIGlobalVariable(name: "dataC.0
// CHECK: DIDerivedType(tag: DW_TAG_member, name: ".0"

struct S {
  column_major float2x2 d;
  float2  b;
};

groupshared S dataC[8*8];

RWStructuredBuffer<float2x2> fA;
RWStructuredBuffer<float2> fB;

struct mat {
  row_major float2x2 f2x2;
};

StructuredBuffer<mat> mats;
StructuredBuffer<row_major float2x2> mats2;

[numthreads(8,8,1)]
void main( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
    dataC[tid.x%(8*8)].d = mats.Load(gid.x).f2x2 + mats2.Load(gtid.y);
    dataC[tid.x%(8*8)].b = gid;
    GroupMemoryBarrierWithGroupSync();
    float2x2 f2x2 = dataC[8*8-1-tid.y%(8*8)].d;
    float2 f2 = dataC[8*8-1-tid.y%(8*8)].b;
    fA[gidx] = f2x2;
    fB[gidx] = f2;
}
