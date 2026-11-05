// RUN: %dxc -T ms_6_5 -E main -fspv-target-env=vulkan1.3 -fcgl %s -spirv | FileCheck %s

// Mesh shader outputs whose attributes are inside nested structs.

struct VertexAttributes {
  float3 PositionVS : POSITION0;
  float3 Normal : NORMAL0;
};

struct VertexOut {
  float4 PositionHS : SV_Position;
  VertexAttributes attributes;
  uint MeshletIndex : COLOR0;
};

struct PrimAttributes {
  uint Id : PRIMID0;
};

// The fields inherit the semantic of the struct: PSEM0 and PSEM1.
struct PrimExtra {
  uint Flags;
  float Weight;
};

struct PrimOut {
  PrimAttributes attrs;
  PrimExtra extra : PSEM0;
};

// CHECK: OpDecorate %out_var_POSITION0 Location 0
// CHECK: OpDecorate %out_var_NORMAL0 Location 1
// CHECK: OpDecorate %out_var_COLOR0 Location 2
// CHECK: OpDecorate %out_var_PRIMID0 Location 3
// CHECK: OpDecorate %out_var_PSEM0 Location 4
// CHECK: OpDecorate %out_var_PSEM1 Location 5

[outputtopology("triangle")]
[numthreads(3, 1, 1)]
void main(in uint tid : SV_GroupThreadID,
          out vertices VertexOut verts[3],
          out primitives PrimOut prims[1],
          out indices uint3 tris[1]) {
  SetMeshOutputCounts(3, 1);

  VertexOut vout = (VertexOut)0;

// Assigning the whole vertex writes each field of the nested struct.
// CHECK:      [[attrs:%[0-9]+]] = OpCompositeExtract %VertexAttributes {{%[0-9]+}} 1
// CHECK:      [[pos:%[0-9]+]] = OpCompositeExtract %v3float [[attrs]] 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v3float %out_var_POSITION0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] [[pos]]
// CHECK-NEXT: [[nrm:%[0-9]+]] = OpCompositeExtract %v3float [[attrs]] 1
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v3float %out_var_NORMAL0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] [[nrm]]
  verts[tid] = vout;

// Assigning the nested struct member.
// CHECK:      [[pos:%[0-9]+]] = OpCompositeExtract %v3float {{%[0-9]+}} 0
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v3float %out_var_POSITION0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] [[pos]]
// CHECK-NEXT: [[nrm:%[0-9]+]] = OpCompositeExtract %v3float {{%[0-9]+}} 1
// CHECK-NEXT: [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v3float %out_var_NORMAL0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] [[nrm]]
  verts[tid].attributes = vout.attributes;

// Assigning a field inside the nested struct.
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_v3float %out_var_NORMAL0 {{%[0-9]+}}
// CHECK-NEXT:                   OpStore [[ptr]] {{%[0-9]+}}
  verts[tid].attributes.Normal = float3(0, 0, 1);

// And one of its components.
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_NORMAL0 {{%[0-9]+}} %uint_1
// CHECK-NEXT:                   OpStore [[ptr]] %float_2
  verts[tid].attributes.Normal.y = 2.0;

// Per-primitive outputs in a nested struct.
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_uint %out_var_PRIMID0 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] %uint_7
  prims[0].attrs.Id = 7;

// A field of a struct with a semantic.
// CHECK:      [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_PSEM1 %int_0
// CHECK-NEXT:                   OpStore [[ptr]] %float_0_5
  prims[0].extra.Weight = 0.5;

  tris[0] = uint3(0, 1, 2);
}
