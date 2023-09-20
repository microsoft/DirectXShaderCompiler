// RUN: not %dxc -T ms_6_5 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s

// CHECK: 14:6: error: MS entry point must have the numthreads attribute

struct MeshPerVertex {
    float4 position : SV_Position;
};

#define MAX_VERT 64
#define MAX_PRIM 81
#define NUM_THREADS 128

[outputtopology("triangle")]
void main(
        out vertices MeshPerVertex verts[MAX_VERT],
        out indices uint3 primitiveInd[MAX_PRIM],
        in uint tig : SV_GroupIndex)
{
}
