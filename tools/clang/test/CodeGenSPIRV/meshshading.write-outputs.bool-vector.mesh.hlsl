// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fcgl %s | FileCheck %s
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefix=OPT --implicit-check-not=OpUndef
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader -fcgl %s | FileCheck %s
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefix=OPT --implicit-check-not=OpUndef

// CHECK-DAG: [[TRUE_FALSE:%[0-9]+]] = OpConstantComposite %v2bool %true %false
// CHECK-DAG: [[UINT2_1:%[0-9]+]] = OpConstantComposite %v2uint %uint_1 %uint_1
// CHECK-DAG: [[UINT2_0:%[0-9]+]] = OpConstantComposite %v2uint %uint_0 %uint_0
// CHECK-DAG: [[MASK_VALUE:%[0-9]+]] = OpConstantComposite %v3bool %true %false %true
// CHECK-DAG: [[UINT3_1:%[0-9]+]] = OpConstantComposite %v3uint %uint_1 %uint_1 %uint_1
// CHECK-DAG: [[UINT3_0:%[0-9]+]] = OpConstantComposite %v3uint %uint_0 %uint_0 %uint_0

// OPT-DAG: [[FLAGS0:%[0-9]+]] = OpConstantComposite %v2uint %uint_1 %uint_0
// OPT-DAG: [[FLAGS1:%[0-9]+]] = OpConstantComposite %v2uint %uint_0 %uint_1
// OPT-DAG: [[MASK:%[0-9]+]] = OpConstantComposite %v3uint %uint_1 %uint_0 %uint_1
// OPT: [[PTR0:%[0-9]+]] = OpAccessChain %_ptr_Output_v2uint %out_var_FLAGS %int_0
// OPT-NEXT: OpStore [[PTR0]] [[FLAGS0]]
// OPT: [[PTR1:%[0-9]+]] = OpAccessChain %_ptr_Output_v2uint %out_var_FLAGS %int_1
// OPT-NEXT: OpStore [[PTR1]] [[FLAGS1]]
// OPT-NEXT: [[PTR2:%[0-9]+]] = OpAccessChain %_ptr_Output_uint %out_var_FLAGS %int_2 %uint_1
// OPT-NEXT: OpStore [[PTR2]] %uint_1
// OPT-NEXT: [[PTR3:%[0-9]+]] = OpAccessChain %_ptr_Output_v3uint %out_var_MASK %int_0
// OPT-NEXT: OpStore [[PTR3]] [[MASK]]

struct Vertex {
  float4 position : SV_Position;
  bool2 flags : FLAGS;
};

struct Primitive {
  bool3 mask : MASK;
};

[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void main(out vertices Vertex vertices[3], out indices uint3 triangles[1],
          out primitives Primitive primitives[1]) {
  SetMeshOutputCounts(3, 1);
  vertices[0].position = float4(0, 0, 0, 1);
  // CHECK: [[CONVERTED:%[0-9]+]] = OpSelect %v2uint [[TRUE_FALSE]] [[UINT2_1]] [[UINT2_0]]
  // CHECK-NEXT: [[PTR:%[0-9]+]] = OpAccessChain %_ptr_Output_v2uint %out_var_FLAGS %int_0
  // CHECK-NEXT: OpStore [[PTR]] [[CONVERTED]]
  vertices[0].flags = bool2(true, false);
  Vertex vertex;
  vertex.position = float4(1, 1, 1, 1);
  vertex.flags = bool2(false, true);
  // CHECK: [[FLAGS:%[0-9]+]] = OpCompositeExtract %v2bool {{%[0-9]+}} 1
  // CHECK-NEXT: [[CONVERTED:%[0-9]+]] = OpSelect %v2uint [[FLAGS]] [[UINT2_1]] [[UINT2_0]]
  // CHECK-NEXT: [[PTR:%[0-9]+]] = OpAccessChain %_ptr_Output_v2uint %out_var_FLAGS %int_1
  // CHECK-NEXT: OpStore [[PTR]] [[CONVERTED]]
  vertices[1] = vertex;
  // CHECK: [[CONVERTED:%[0-9]+]] = OpSelect %uint %true %uint_1 %uint_0
  // CHECK-NEXT: [[PTR:%[0-9]+]] = OpAccessChain %_ptr_Output_uint %out_var_FLAGS %int_2 %uint_1
  // CHECK-NEXT: OpStore [[PTR]] [[CONVERTED]]
  vertices[2].flags.y = true;
  // CHECK: [[CONVERTED:%[0-9]+]] = OpSelect %v3uint [[MASK_VALUE]] [[UINT3_1]] [[UINT3_0]]
  // CHECK-NEXT: [[PTR:%[0-9]+]] = OpAccessChain %_ptr_Output_v3uint %out_var_MASK %int_0
  // CHECK-NEXT: OpStore [[PTR]] [[CONVERTED]]
  primitives[0].mask = bool3(true, false, true);
  triangles[0] = uint3(0, 1, 2);
}
