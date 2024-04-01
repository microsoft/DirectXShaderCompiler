// RUN: not %dxc -T lib_6_9 %s 2>&1 | FileCheck %s

// REQUIRES: dxil-1-9

// Ensure missing setMeshOutputCounts in a shader that writes to the outputs
// produces a validation error

// One for each out param written to
// CHECK:  error: Missing SetMeshOutputCounts call
// CHECK:  error: Missing SetMeshOutputCounts call
// CHECK:  error: Missing SetMeshOutputCounts call

RWBuffer<float> buf0;

#define MAX_VERT 32
#define MAX_PRIM 16
#define NUM_THREADS 32

struct MeshPerVertex {
    float4 position : SV_Position;
};

struct MeshPerPrimitive {
    float normal : NORMAL;
};

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void node_setmeshoutputcounts(
            out indices uint3 primIndices[MAX_PRIM],
            out vertices MeshPerVertex verts[MAX_VERT],
            out primitives MeshPerPrimitive prims[MAX_PRIM],
            in uint tig : SV_GroupIndex) {

  MeshPerVertex ov;
  ov.position = float4(14.0,15.0,16.0,17.0);

  if (tig % 3) {
    primIndices[tig / 3] = uint3(tig, tig + 1, tig + 2);
    MeshPerPrimitive op;
    op.normal = 1.0;
    prims[tig / 3] = op;
  }
  verts[tig] = ov;
}
