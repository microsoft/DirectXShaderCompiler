// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefixes=CHECK,MAINEXT
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefixes=CHECK,MAINNV
// RUN: not %dxc -T ms_6_6 -E main -DINOUT -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_SCALAR -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_ARRAY -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_FLOAT1 -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: %dxc -T ms_6_6 -E main -DARRAY -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefixes=ARRAY,EXT
// RUN: %dxc -T ms_6_6 -E main -DARRAY -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefixes=ARRAY,NV
// RUN: %dxc -T ms_6_6 -E main -DSIDE_EFFECT -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefix=SIDE
// RUN: %dxc -T ms_6_6 -E main -DSIDE_EFFECT -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefix=SIDE

// Mesh shader outputs passed as 'out' arguments are written back to the
// Output variables after the call.

// CHECK-DAG: OpDecorate [[POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// CHECK-DAG: OpDecorate [[PRIMITIVE_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// MAINEXT-DAG: OpDecorate [[INDICES:%[0-9A-Za-z_]+]] BuiltIn PrimitiveTriangleIndicesEXT
// MAINNV-DAG: OpDecorate [[INDICES:%[0-9A-Za-z_]+]] BuiltIn PrimitiveIndicesNV

// CHECK: [[VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_1
// CHECK-NEXT: OpStore [[VALUE]] %float_9
// CHECK-NEXT: OpStore [[VALUE]] %float_9
// CHECK: [[Z:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[POSITION]] %int_2 %uint_2
// CHECK-NEXT: OpStore [[Z]] %float_5
// CHECK: [[W:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[POSITION]] %int_2 %uint_3
// CHECK-NEXT: OpStore [[W]] %float_6
// CHECK: [[ID:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[PRIMITIVE_ID]] %int_0
// CHECK-NEXT: OpStore [[ID]] %uint_8

// MAINEXT: [[TRIANGLE:%[0-9]+]] = OpAccessChain %_ptr_Output_v3uint [[INDICES]] %int_0
// MAINEXT-NEXT: OpStore [[TRIANGLE]]
// MAINNV: [[INDEX0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[INDICES]] %uint_0
// MAINNV-NEXT: OpStore [[INDEX0]] %uint_0
// MAINNV: [[INDEX1:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[INDICES]] %uint_1
// MAINNV-NEXT: OpStore [[INDEX1]] %uint_1
// MAINNV: [[INDEX2:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[INDICES]] %uint_2
// MAINNV-NEXT: OpStore [[INDEX2]] %uint_2
// CHECK: [[EXTRA:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_EXTRA %int_0{{$}}
// CHECK-NEXT: OpStore [[EXTRA]] %float_9
// CHECK: [[STATIC_VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_0
// CHECK-NEXT: OpStore [[STATIC_VALUE]] %float_10

// INOUT: error: output arrays of a mesh shader can not be read from

// ARRAY-DAG: OpDecorate [[ARRAY_POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// ARRAY-DAG: OpDecorate [[ARRAY_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// EXT-DAG: OpDecorate [[ARRAY_INDICES:%[0-9A-Za-z_]+]] BuiltIn PrimitiveTriangleIndicesEXT
// NV-DAG: OpDecorate [[ARRAY_INDICES:%[0-9A-Za-z_]+]] BuiltIn PrimitiveIndicesNV
// ARRAY: [[POS0:%[0-9]+]] = OpAccessChain %_ptr_Output_v4float [[ARRAY_POSITION]] %int_0
// ARRAY-NEXT: OpStore [[POS0]]
// ARRAY: [[VALUE0:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_0
// ARRAY-NEXT: OpStore [[VALUE0]] %float_10
// ARRAY: [[POS1:%[0-9]+]] = OpAccessChain %_ptr_Output_v4float [[ARRAY_POSITION]] %int_1
// ARRAY-NEXT: OpStore [[POS1]]
// ARRAY: [[VALUE1:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_1
// ARRAY-NEXT: OpStore [[VALUE1]] %float_11
// ARRAY: [[POS2:%[0-9]+]] = OpAccessChain %_ptr_Output_v4float [[ARRAY_POSITION]] %int_2
// ARRAY-NEXT: OpStore [[POS2]]
// ARRAY: [[VALUE2:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_2
// ARRAY-NEXT: OpStore [[VALUE2]] %float_12
// ARRAY: [[ARRAY_ID0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[ARRAY_ID]] %int_0
// ARRAY-NEXT: OpStore [[ARRAY_ID0]] %uint_8
// EXT: [[ARRAY_TRIANGLE:%[0-9]+]] = OpAccessChain %_ptr_Output_v3uint [[ARRAY_INDICES]] %int_0
// EXT-NEXT: OpStore [[ARRAY_TRIANGLE]]
// NV: [[ARRAY_INDEX0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[ARRAY_INDICES]] %uint_0
// NV-NEXT: OpStore [[ARRAY_INDEX0]] %uint_0
// NV: [[ARRAY_INDEX1:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[ARRAY_INDICES]] %uint_1
// NV-NEXT: OpStore [[ARRAY_INDEX1]] %uint_1
// NV: [[ARRAY_INDEX2:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[ARRAY_INDICES]] %uint_2
// NV-NEXT: OpStore [[ARRAY_INDEX2]] %uint_2

// SIDE-DAG: OpDecorate [[SIDE_POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// SIDE-DAG: OpDecorate [[SIDE_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// SIDE: [[SIDE_VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_0
// SIDE-NEXT: OpStore [[SIDE_VALUE]] %float_9
// SIDE: [[SIDE_Z:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[SIDE_POSITION]] %int_1 %uint_2
// SIDE-NEXT: OpStore [[SIDE_Z]] %float_5
// SIDE: [[SIDE_W:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[SIDE_POSITION]] %int_1 %uint_3
// SIDE-NEXT: OpStore [[SIDE_W]] %float_6
// SIDE: [[SIDE_ID0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[SIDE_ID]] %int_0
// SIDE-NEXT: OpStore [[SIDE_ID0]] %uint_2
// SIDE: OpStore [[SIDE_VALUE]] %float_13
// SIDE: OpStore [[SIDE_W]] %float_7
// SIDE: OpStore [[SIDE_Z]] %float_8
// SIDE: OpStore [[SIDE_ID0]] %uint_12
// SIDE: [[CAST_VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_2
// SIDE-NEXT: OpStore [[CAST_VALUE]] %float_8

struct Vertex {
  float4 position : SV_Position;
  float value : VALUE;
  float1 extra : EXTRA;
  static float M;
};

float Vertex::M = 1;

struct Primitive {
  uint id : SV_PrimitiveID;
};

struct Writer {
  float value;
  void write(out float x, out float2 pair) {
    x = value;
    pair = float2(7, 8);
  }
};

void setFloat(out float x) { x = 9; }
void setPair(out float2 x) { x = float2(5, 6); }
void setUint(out uint x) { x = 8; }
void setTriangle(out uint3 x) { x = uint3(0, 1, 2); }
void bump(inout float x) { x += 1; }
void setVertices(out Vertex v[3]) {
  v[0].position = float4(0, 0, 0, 1);
  v[0].value = 10;
  v[0].extra = 13;
  v[1].position = float4(1, 0, 0, 1);
  v[1].value = 11;
  v[1].extra = 14;
  v[2].position = float4(0, 1, 0, 1);
  v[2].value = 12;
  v[2].extra = 15;
}
void setPrimitives(out Primitive p[1]) { p[0].id = 8; }
void setTriangles(out uint3 t[1]) { t[0] = uint3(0, 1, 2); }
void bumpArr(inout Vertex v[3]) { v[0].value += 1; }

[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void main(out vertices Vertex vertices[3],
          out primitives Primitive primitives[1],
          out indices uint3 triangles[1]) {
  SetMeshOutputCounts(3, 1);
#if defined(ARRAY)
  setVertices(vertices);
  setPrimitives(primitives);
  setTriangles(triangles);
#elif defined(SIDE_EFFECT)
  int i = 0;
  setFloat(vertices[i++].value);
  setPair(vertices[i++].position.zw);
  primitives[0].id = i;
  Writer writer;
  writer.value = 13;
  int j = 0;
  writer.write(vertices[j++].value.x, vertices[j++].position.wz);
  primitives[0].id = j + 10;
  setUint(vertices[2].value);
#else
  setFloat(vertices[1].value);
  setFloat(vertices[1].value.x);
  setPair(vertices[2].position.zw);
  setUint(primitives[0].id);
  setTriangle(triangles[0]);
  setFloat(vertices[0].extra.x);
  setFloat(vertices[0].M);
  bump(vertices[0].M);
  vertices[0].value = Vertex::M;
#ifdef INOUT
  bump(vertices[1].value);
#endif
#ifdef INOUT_SCALAR
  bump(vertices[1].value.x);
#endif
#ifdef INOUT_ARRAY
  bumpArr(vertices);
#endif
#ifdef INOUT_FLOAT1
  bump(vertices[0].extra.x);
#endif
#endif
}
