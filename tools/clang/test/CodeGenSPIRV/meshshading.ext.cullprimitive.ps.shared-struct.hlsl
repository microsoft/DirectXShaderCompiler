// RUN: %dxc -T ms_6_5 -E ms_main %s -spirv -fspv-target-env=vulkan1.3 -DCULL_FIRST | FileCheck %s --check-prefix=MS
// RUN: %dxc -T ms_6_5 -E ms_main %s -spirv -fspv-target-env=vulkan1.3 | FileCheck %s --check-prefix=MS
// RUN: %dxc -T ps_6_5 -E ps_main %s -spirv -fspv-target-env=vulkan1.3 -DCULL_FIRST | FileCheck %s --check-prefix=PS
// RUN: %dxc -T ps_6_5 -E ps_main %s -spirv -fspv-target-env=vulkan1.3 | FileCheck %s --check-prefix=PS
// RUN: %dxc -T ps_6_5 -E ps_main %s -spirv -fspv-target-env=vulkan1.3 -fcgl -DCULL_FIRST | FileCheck %s --check-prefix=FIRST
// RUN: %dxc -T ps_6_5 -E ps_main %s -spirv -fspv-target-env=vulkan1.3 -fcgl | FileCheck %s --check-prefix=LATER

// One struct used as a mesh shader primitive output and a pixel shader input,
// with SV_CullPrimitive as the first member and as a later one. The pixel
// shader replaces it with 0, so the other members must keep the locations the
// mesh shader gave them, and the 0 must land in the right struct member.

// MS: OpDecorate {{%[0-9]+}} BuiltIn CullPrimitiveEXT
// MS-DAG: OpDecorate %out_var_PRIMID Location 0
// MS-DAG: OpDecorate %out_var_COLOR0 Location 1

// PS-NOT: CullPrimitiveEXT
// PS-DAG: OpDecorate %in_var_PRIMID Location 0
// PS-DAG: OpDecorate %in_var_COLOR0 Location 1
// PS-NOT: CullPrimitiveEXT

// FIRST: [[null:%[0-9]+]] = OpConstantNull %bool
// FIRST: [[id:%[0-9]+]] = OpLoad %uint %in_var_PRIMID
// FIRST: [[col:%[0-9]+]] = OpLoad %v4float %in_var_COLOR0
// FIRST: OpCompositeConstruct %Prim [[null]] [[id]] [[col]]

// LATER: [[null:%[0-9]+]] = OpConstantNull %bool
// LATER: [[id:%[0-9]+]] = OpLoad %uint %in_var_PRIMID
// LATER: [[col:%[0-9]+]] = OpLoad %v4float %in_var_COLOR0
// LATER: OpCompositeConstruct %Prim [[id]] [[null]] [[col]]

#ifdef CULL_FIRST
struct Prim {
  bool cull : SV_CullPrimitive;
  uint id : PRIMID;
  float4 col : COLOR0;
};
#else
struct Prim {
  uint id : PRIMID;
  bool cull : SV_CullPrimitive;
  float4 col : COLOR0;
};
#endif

struct Vert {
  float4 pos : SV_Position;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void ms_main(out vertices Vert verts[3], out indices uint3 tris[1],
             out primitives Prim prims[1]) {
  SetMeshOutputCounts(3, 1);
  verts[0].pos = 0;
  verts[1].pos = 0;
  verts[2].pos = 0;
  tris[0] = uint3(0, 1, 2);
  prims[0].cull = false;
  prims[0].id = 7;
  prims[0].col = 1;
}

float4 ps_main(Vert v, Prim p) : SV_Target {
  return p.cull ? 0 : p.col * p.id;
}
