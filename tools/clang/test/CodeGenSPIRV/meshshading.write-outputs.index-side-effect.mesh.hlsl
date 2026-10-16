// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s
// RUN: %dxc -T ms_6_6 -E main -DPAREN -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefix=PAREN
// RUN: %dxc -T ms_6_6 -E main -DPAREN -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefix=PAREN
// RUN: %dxc -T ms_6_6 -E main -DSPLAT -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefix=SPLAT
// RUN: %dxc -T ms_6_6 -E main -DSPLAT -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefix=SPLAT

// CHECK-DAG: OpDecorate [[POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// CHECK-DAG: OpDecorate [[ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// CHECK: [[Z:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[POSITION]] %int_0 %uint_2
// CHECK-NEXT: OpStore [[Z]] %float_5
// CHECK: [[W:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[POSITION]] %int_0 %uint_3
// CHECK-NEXT: OpStore [[W]] %float_6
// CHECK: [[ID0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[ID]] %int_0
// CHECK-NEXT: OpStore [[ID0]] %uint_1
// CHECK: [[EXTRA:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_EXTRA %int_0{{$}}
// CHECK-NEXT: OpStore [[EXTRA]] %float_9

// PAREN-DAG: OpDecorate [[PAREN_POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// PAREN-DAG: OpDecorate [[PAREN_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// PAREN: [[PAREN_VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_0{{$}}
// PAREN-NEXT: OpStore [[PAREN_VALUE]] %float_11
// PAREN: [[PAREN_Y:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[PAREN_POSITION]] %int_1 %uint_1
// PAREN-NEXT: OpStore [[PAREN_Y]] %float_12
// PAREN: [[PAREN_ID0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[PAREN_ID]] %int_0
// PAREN-NEXT: OpStore [[PAREN_ID0]] %uint_1

// SPLAT-DAG: OpDecorate [[SPLAT_POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// SPLAT: [[SCALAR:%[0-9]+]] = OpConvertUToF %float
// SPLAT: [[SPLAT_Z:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[SPLAT_POSITION]] %int_0 %uint_2
// SPLAT-NEXT: OpStore [[SPLAT_Z]] [[SCALAR]]
// SPLAT: [[SPLAT_W:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[SPLAT_POSITION]] %int_0 %uint_3
// SPLAT-NEXT: OpStore [[SPLAT_W]] [[SCALAR]]

struct Vertex {
  float4 position : SV_Position;
  float1 extra : EXTRA;
  float value : VALUE;
};

struct Primitive {
  uint id : SV_PrimitiveID;
};

[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void main(out vertices Vertex vertices[3], out primitives Primitive primitives[1],
          out indices uint3 triangles[1], uint gi : SV_GroupIndex) {
  SetMeshOutputCounts(3, 1);
  int i = 0;
#if defined(PAREN)
  (vertices[i++]).value = 11;
  ((vertices[1]).position).y = 12;
  primitives[0].id = i;
#elif defined(SPLAT)
  vertices[0].position.zw = (float)gi;
#else
  vertices[i++].position.zw = float2(5, 6);
  primitives[0].id = i;
  vertices[0].extra.x = 9;
  triangles[0] = uint3(0, 1, 2);
#endif
}
