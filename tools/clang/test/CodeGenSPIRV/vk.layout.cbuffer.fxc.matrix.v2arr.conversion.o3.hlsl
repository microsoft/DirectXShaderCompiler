// Run: %dxc -T ps_6_0 -E main -fvk-use-dx-layout -O3

// CHECK: OpDecorate [[arr_f3:%\w+]] ArrayStride 16
// CHECK: OpMemberDecorate {{%\w+}} 0 Offset 0
// CHECK: OpMemberDecorate {{%\w+}} 1 Offset 16
// CHECK: OpMemberDecorate {{%\w+}} 2 Offset 52

// CHECK: [[arr_f3]] = OpTypeArray %float %uint_3
// CHECK: [[type_buffer0:%\w+]] = OpTypeStruct %float [[arr_f3]] %float
// CHECK: [[type_ptr_buffer0:%\w+]] = OpTypePointer Uniform [[type_buffer0]]
// CHECK: [[type_buffer0_clone:%\w+]] = OpTypeStruct %float %v3float %float
// CHECK: [[buffer0:%\w+]] = OpVariable [[type_ptr_buffer0]] Uniform

cbuffer buffer0 {
  float dummy0;                      // Offset:    0 Size:     4 [unused]
  float1x3 foo;                      // Offset:   16 Size:    20 [unused]
  float end;                         // Offset:   36 Size:     4
};

float4 main(float4 color : COLOR) : SV_TARGET
{
// CHECK: [[buffer0_clone:%\w+]] = {{\w+}} [[type_buffer0_clone]]

// CHECK: [[foo_0:%\w+]] = OpAccessChain %_ptr_Uniform_float [[buffer0]] %uint_1 %uint_0
// CHECK: [[foo_0_value:%\w+]] = OpLoad %float [[foo_0]]
// CHECK: [[buffer0_clone:%\w+]] = OpCompositeInsert [[type_buffer0_clone]] [[foo_0_value]] [[buffer0_clone]] 1 0
// CHECK: [[foo_1:%\w+]] = OpAccessChain %_ptr_Uniform_float [[buffer0]] %uint_1 %uint_1
// CHECK: [[foo_1_value:%\w+]] = OpLoad %float [[foo_1]]
// CHECK: [[buffer0_clone:%\w+]] = OpCompositeInsert [[type_buffer0_clone]] [[foo_1_value]] [[buffer0_clone]] 1 1
// CHECK: [[foo_2:%\w+]] = OpAccessChain %_ptr_Uniform_float [[buffer0]] %uint_1 %uint_2
// CHECK: [[foo_2_value:%\w+]] = OpLoad %float [[foo_2]]
// CHECK: [[buffer0_clone:%\w+]] = OpCompositeInsert [[type_buffer0_clone]] [[foo_2_value]] [[buffer0_clone]] 1 2
  float1x2 bar = foo;
  color.x += bar._m00;
  return color;
}
