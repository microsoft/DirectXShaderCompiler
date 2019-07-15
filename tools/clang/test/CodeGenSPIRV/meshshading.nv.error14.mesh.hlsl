// Run: %dxc -T ms_6_5 -E main

// CHECK:  18:20: error: invalid usage of semantic 'USER_OUT' in shader profile ms

struct MeshPerVertex {
    float4 position : SV_Position;
};

#define MAX_VERT 64
#define MAX_PRIM 81
#define NUM_THREADS 128

[outputtopology("point")]
[numthreads(NUM_THREADS, 1, 1)]
void main(
        out vertices MeshPerVertex verts[MAX_VERT],
        out indices uint primitiveInd[MAX_PRIM],
        out float3 userAttrOUT : USER_OUT
        )
{
}
