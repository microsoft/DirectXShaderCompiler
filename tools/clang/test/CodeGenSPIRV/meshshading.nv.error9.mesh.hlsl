// Run: %dxc -T ms_6_5 -E main

// CHECK:  14:6: error: expected indices object declaration

struct MeshPerVertex {
    float4 position : SV_Position;
};

#define MAX_VERT 64
#define NUM_THREADS 128

[outputtopology("point")]
[numthreads(NUM_THREADS, 1, 1)]
void main(
        out vertices MeshPerVertex verts[MAX_VERT])
{
}
