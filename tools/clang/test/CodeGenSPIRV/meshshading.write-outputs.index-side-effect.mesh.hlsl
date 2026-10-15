// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s

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

struct Vertex {
  float4 position : SV_Position;
  float1 extra : EXTRA;
};

struct Primitive {
  uint id : SV_PrimitiveID;
};

[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void main(out vertices Vertex vertices[3], out primitives Primitive primitives[1],
          out indices uint3 triangles[1]) {
  SetMeshOutputCounts(3, 1);
  int i = 0;
  vertices[i++].position.zw = float2(5, 6);
  primitives[0].id = i;
  vertices[0].extra.x = 9;
  triangles[0] = uint3(0, 1, 2);
}
