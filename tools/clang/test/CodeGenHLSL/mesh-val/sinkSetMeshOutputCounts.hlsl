// RUN: %dxc -E main -T ms_6_5 %s | FileCheck %s

// Test that SetMeshOutputCounts has noduplicate attribute, preventing the
// optimizer from sinking/duplicating the call into branches that compute
// the count values.

// CHECK: dx.op.setMeshOutputCounts
// CHECK: noduplicate

#define MAX_VERT 32
#define MAX_PRIM 16
#define NUM_THREADS 32

struct MeshPerVertex {
    float4 position : SV_Position;
};

struct MeshPayload {
    float normal;
};

[numthreads(NUM_THREADS, 1, 1)]
[outputtopology("triangle")]
void main(
            out indices uint3 primIndices[MAX_PRIM],
            out vertices MeshPerVertex verts[MAX_VERT],
            in payload MeshPayload mpl,
            in uint tig : SV_GroupIndex,
            in uint vid : SV_ViewID
         )
{
    // Compute counts in a branch - optimizer used to sink SetMeshOutputCounts
    // into each branch, producing two copies and failing validation.
    uint nverts, nprims;
    if (vid % 2) {
        nverts = MAX_VERT;
        nprims = MAX_PRIM;
    } else {
        nverts = MAX_VERT / 2;
        nprims = MAX_PRIM / 2;
    }
    SetMeshOutputCounts(nverts, nprims);

    if (tig < nverts) {
        verts[tig].position = float4(0, 0, 0, 1);
    }
    if (tig < nprims) {
        primIndices[tig] = uint3(tig * 3, tig * 3 + 1, tig * 3 + 2);
    }
}
