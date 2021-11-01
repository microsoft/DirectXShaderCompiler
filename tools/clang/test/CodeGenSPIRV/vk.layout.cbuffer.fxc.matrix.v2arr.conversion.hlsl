// RUN: %dxc -T ps_6_0 -E main -fvk-use-dx-layout

// CHECK: OpDecorate [[arr_f3:%\w+]] ArrayStride 16
// CHECK: OpMemberDecorate {{%\w+}} 0 Offset 0
// CHECK: OpMemberDecorate {{%\w+}} 1 Offset 16
// CHECK: OpMemberDecorate {{%\w+}} 2 Offset 52

// CHECK: [[arr_f3]] = OpTypeArray %float %uint_3
// CHECK: %type_buffer0 = OpTypeStruct %float [[arr_f3]] %float

cbuffer buffer0 {
  float dummy0;                      // Offset:    0 Size:     4 [unused]
  float1x3 foo;                      // Offset:   16 Size:    20 [unused]
  float end;                         // Offset:   36 Size:     4
};

float4 main(float4 color : COLOR) : SV_TARGET
{
// CHECK: %main = OpFunction %void None
// CHECK:         OpFunctionCall %void %module_init
// CHECK:         OpFunctionCall %v4float %src_main

  float1x2 bar = foo;
  color.x += bar._m00;
  return color;
}

// CHECK: %module_init = OpFunction %void
// CHECK: %module_init_bb = OpLabel
// CHECK: [[dummy0:%\w+]] = OpAccessChain %_ptr_Uniform_float [[buffer0:%\w+]] %uint_0
// CHECK: [[dummy0_clone:%\w+]] = OpAccessChain %_ptr_Private_float [[clone:%\w+]] %uint_0
// CHECK: [[dummy0_value:%\w+]] = OpLoad %float [[dummy0]]
// CHECK:                OpStore [[dummy0_clone]] [[dummy0_value]]
// CHECK: [[foo:%\w+]] = OpAccessChain %_ptr_Uniform__arr_float_uint_3 [[buffer0]] %uint_1
// CHECK: [[foo_clone:%\w+]] = OpAccessChain %_ptr_Private_v3float [[clone]] %uint_1
// CHECK: [[foo_0:%\w+]] = OpAccessChain %_ptr_Uniform_float [[foo]] %uint_0
// CHECK: [[foo_clone_0:%\w+]] = OpAccessChain %_ptr_Private_float [[foo_clone]] %uint_0
// CHECK: [[foo_0_value:%\w+]] = OpLoad %float [[foo_0]]
// CHECK:                OpStore [[foo_clone_0]] [[foo_0_value]]
// CHECK: [[foo_1:%\w+]] = OpAccessChain %_ptr_Uniform_float [[foo]] %uint_1
// CHECK: [[foo_clone_1:%\w+]] = OpAccessChain %_ptr_Private_float [[foo_clone]] %uint_1
// CHECK: [[foo_1_value:%\w+]] = OpLoad %float [[foo_1]]
// CHECK:                OpStore [[foo_clone_1]] [[foo_1_value]]
// CHECK: [[foo_2:%\w+]] = OpAccessChain %_ptr_Uniform_float [[foo]] %uint_2
// CHECK: [[foo_clone_2:%\w+]] = OpAccessChain %_ptr_Private_float [[foo_clone]] %uint_2
// CHECK: [[foo_2_value:%\w+]] = OpLoad %float [[foo_2]]
// CHECK:                OpStore [[foo_clone_2]] [[foo_2_value]]
// CHECK: [[end:%\w+]] = OpAccessChain %_ptr_Uniform_float [[buffer0]] %uint_2
// CHECK: [[end_clone:%\w+]] = OpAccessChain %_ptr_Private_float [[clone]] %uint_2
// CHECK: [[end_value:%\w+]] = OpLoad %float [[end]]
// CHECK:                OpStore [[end_clone]] [[end_value]]
// CHECK:                OpReturn
// CHECK:                OpFunctionEnd
