// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s
// RUN: not %dxc -T ms_6_6 -E main -DINOUT -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT

// Mesh shader outputs passed as 'out' arguments are written back to the
// Output variables after the call.

// CHECK-DAG: OpDecorate [[POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// CHECK-DAG: OpDecorate [[PRIMITIVE_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// CHECK-DAG: OpDecorate [[INDICES:%[0-9A-Za-z_]+]] BuiltIn PrimitiveTriangleIndicesEXT

// CHECK: [[VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_1
// CHECK-NEXT: OpStore [[VALUE]] %float_9
// CHECK: [[Z:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[POSITION]] %int_2 %uint_2
// CHECK-NEXT: OpStore [[Z]] %float_5
// CHECK: [[W:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[POSITION]] %int_2 %uint_3
// CHECK-NEXT: OpStore [[W]] %float_6
// CHECK: [[ID:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[PRIMITIVE_ID]] %int_0
// CHECK-NEXT: OpStore [[ID]] %uint_8

// CHECK: [[TRIANGLE:%[0-9]+]] = OpAccessChain %_ptr_Output_v3uint [[INDICES]] %int_0
// CHECK-NEXT: OpStore [[TRIANGLE]]

// INOUT: error: output arrays of a mesh shader can not be read from

struct Vertex {
  float4 position : SV_Position;
  float value : VALUE;
};

struct Primitive {
  uint id : SV_PrimitiveID;
};

void setFloat(out float x) { x = 9; }
void setPair(out float2 x) { x = float2(5, 6); }
void setUint(out uint x) { x = 8; }
void setTriangle(out uint3 x) { x = uint3(0, 1, 2); }
void bump(inout float x) { x += 1; }

[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void main(out vertices Vertex vertices[3],
          out primitives Primitive primitives[1],
          out indices uint3 triangles[1]) {
  SetMeshOutputCounts(3, 1);
  setFloat(vertices[1].value);
  setPair(vertices[2].position.zw);
  setUint(primitives[0].id);
  setTriangle(triangles[0]);
#ifdef INOUT
  bump(vertices[1].value);
#endif
}
