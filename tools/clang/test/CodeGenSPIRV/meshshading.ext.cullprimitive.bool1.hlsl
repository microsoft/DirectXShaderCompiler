// RUN: %dxc -T ms_6_5 -fspv-target-env=vulkan1.3 -E main -fcgl %s -spirv -DCULL_TYPE=bool1 | FileCheck %s
// RUN: %dxc -T ms_6_5 -fspv-target-env=vulkan1.3 -E main -fcgl %s -spirv -DCULL_TYPE=bool1x1 | FileCheck %s

// CHECK: OpDecorate [[cullprim:%[0-9]+]] BuiltIn CullPrimitiveEXT
// CHECK: [[cullprim]] = OpVariable %_ptr_Output__arr_bool_uint_1 Output

struct MeshPerVertex {
  float4 position : SV_Position;
};

struct MeshPerPrimitive {
  CULL_TYPE cull : SV_CullPrimitive;
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
  prims[0].cull = true;
}
