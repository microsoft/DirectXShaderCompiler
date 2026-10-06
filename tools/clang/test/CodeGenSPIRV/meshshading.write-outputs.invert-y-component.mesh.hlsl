// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fvk-invert-y -fcgl %s | FileCheck %s
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fvk-invert-y %s | FileCheck %s --check-prefix=OPT --implicit-check-not=OpUndef
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader -fvk-invert-y -fcgl %s | FileCheck %s
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader -fvk-invert-y %s | FileCheck %s --check-prefix=OPT --implicit-check-not=OpUndef

// Only the Y component is negated; X, Z and W are stored unchanged.
// OPT: [[Y:%[0-9]+]] = OpAccessChain %_ptr_Output_float %gl_Position %int_0 %uint_1
// OPT-NEXT: OpStore [[Y]] %float_n5
// OPT-NEXT: [[X:%[0-9]+]] = OpAccessChain %_ptr_Output_float %gl_Position %int_0 %uint_0
// OPT-NEXT: OpStore [[X]] %float_6
// OPT-NEXT: [[Z:%[0-9]+]] = OpAccessChain %_ptr_Output_float %gl_Position %int_0 %uint_2
// OPT-NEXT: OpStore [[Z]] %float_7
// OPT-NEXT: [[W:%[0-9]+]] = OpAccessChain %_ptr_Output_float %gl_Position %int_0 %uint_3
// OPT-NEXT: OpStore [[W]] %float_8
// OPT-NEXT: [[XY_X:%[0-9]+]] = OpAccessChain %_ptr_Output_float %gl_Position %int_1 %uint_0
// OPT-NEXT: OpStore [[XY_X]] %float_10
// OPT-NEXT: [[XY_Y:%[0-9]+]] = OpAccessChain %_ptr_Output_float %gl_Position %int_1 %uint_1
// OPT-NEXT: OpStore [[XY_Y]] %float_n11
// OPT-NEXT: [[ZY_Z:%[0-9]+]] = OpAccessChain %_ptr_Output_float %gl_Position %int_2 %uint_2
// OPT-NEXT: OpStore [[ZY_Z]] %float_12
// OPT-NEXT: [[ZY_Y:%[0-9]+]] = OpAccessChain %_ptr_Output_float %gl_Position %int_2 %uint_1
// OPT-NEXT: OpStore [[ZY_Y]] %float_n13

struct Vertex {
  float4 position : SV_Position;
};

[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void main(out vertices Vertex vertices[3], out indices uint3 triangles[1]) {
  SetMeshOutputCounts(3, 1);
  vertices[0].position = float4(1, 2, 3, 4);
  // CHECK: [[PTR:%[0-9]+]] = OpAccessChain %_ptr_Output_float %gl_Position %int_0 %uint_1
  // CHECK-NEXT: [[IS_Y:%[0-9]+]] = OpIEqual %bool %uint_1 %uint_1
  // CHECK-NEXT: [[NEGATED:%[0-9]+]] = OpFNegate %float %float_5
  // CHECK-NEXT: [[VALUE:%[0-9]+]] = OpSelect %float [[IS_Y]] [[NEGATED]] %float_5
  // CHECK-NEXT: OpStore [[PTR]] [[VALUE]]
  vertices[0].position.y = 5;
  vertices[0].position.x = 6;
  vertices[0].position.z = 7;
  vertices[0].position.w = 8;
  vertices[1].position.xy = float2(10, 11);
  vertices[2].position.zy = float2(12, 13);
  triangles[0] = uint3(0, 1, 2);
}
