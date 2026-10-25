// RUN: not %dxc -T ms_6_5 -fspv-target-env=vulkan1.3 -E main -fcgl %s -spirv 2>&1 | FileCheck %s

struct MeshPerVertex {
  float4 position : SV_Position;
};

struct MeshPerPrimitive {
// CHECK: :[[@LINE+1]]:15: error: semantic CullPrimitive must be bool
  uint cull : SV_CullPrimitive;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(out vertices MeshPerVertex verts[3],
          out indices uint3 tris[1],
          out primitives MeshPerPrimitive prims[1]) {
  SetMeshOutputCounts(3, 1);
  verts[0].position = 0;
  verts[1].position = 0;
  verts[2].position = 0;
  tris[0] = uint3(0, 1, 2);
  prims[0].cull = 1;
}
