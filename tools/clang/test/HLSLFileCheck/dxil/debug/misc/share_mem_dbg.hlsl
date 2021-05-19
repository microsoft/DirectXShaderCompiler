// RUN: %dxc -E main -T cs_6_0 -Zi -Od -DDefineA -DDefineB=0 %s -Qstrip_reflect | FileCheck %s

// CHECK: threadId
// CHECK: groupId
// CHECK: threadIdInGroup
// CHECK: flattenedThreadIdInGroup
// CHECK: addrspace(3)

// Make sure source info exist.
// CHECK-DAG: !dx.source.contents = !{!{{[0-9]+}}
// CHECK-DAG: !dx.source.defines = !{!{{[0-9]+}}
// CHECK-DAG: !dx.source.mainFileName = !{![[MAIN_FILE:[0-9]+]]}
// CHECK-DAG: !dx.source.args = !{!{{[0-9]+}}

// CHECK-DAG: !{{[0-9]+}} = !DIGlobalVariable(name: "dataC"
// CHECK-DAG: !{{[0-9]+}} = !DIDerivedType(tag: DW_TAG_member, name: "d"
// CHECK-DAG: !{{[0-9]+}} = !DIDerivedType(tag: DW_TAG_member, name: "b"

// CHECK-DAG: ![[MAIN_FILE]] = !{
// CHECK-SAME: share_mem_dbg.hlsl"}

// Make sure source info contents exist.
// CHECK: !{{[0-9]+}} = !{!"DefineA=1", !"DefineB=0"}
// CHECK: !{{[0-9]+}} = !{!"-E", !"main", !"-T", !"cs_6_0", !"-Zi", !"-Od", !"-D", !"DefineA", !"-D", !"DefineB=0", !"-Qstrip_reflect", !"-Qembed_debug"}


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
