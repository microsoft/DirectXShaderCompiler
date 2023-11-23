// RUN: %dxc -T ps_6_2 -E main -enable-16bit-types -fcgl  %s -spirv | FileCheck %s
void foo(const half3 input, out half3 output) {
  output = input;
}

float4 main() : SV_Target0 {
  float3 output;
// CHECK:       %param_var_input = OpVariable %_ptr_Function_v3half Function
// CHECK-NEXT: %param_var_output = OpVariable %_ptr_Function_v3half Function

// CHECK:      [[outputFloat3:%[0-9]+]] = OpLoad %v3float %output
// CHECK-NEXT:  [[outputHalf3:%[0-9]+]] = OpFConvert %v3half [[outputFloat3]]
// CHECK-NEXT:                         OpStore %param_var_output [[outputHalf3]]
// CHECK-NEXT:              {{%[0-9]+}} = OpFunctionCall %void %foo %param_var_input %param_var_output
  foo(float3(1, 0, 0), output);
// CHECK-NEXT:  [[outputHalf3_0:%[0-9]+]] = OpLoad %v3half %param_var_output
// CHECK-NEXT: [[outputFloat3_0:%[0-9]+]] = OpFConvert %v3float [[outputHalf3_0]]
// CHECK-NEXT:                         OpStore %output [[outputFloat3_0]]

// CHECK-NEXT: [[outputFloat3_1:%[0-9]+]] = OpLoad %v3float %output
// CHECK-NEXT: OpCompositeExtract %float [[outputFloat3_2:%[0-9]+]] 0
// CHECK-NEXT: OpCompositeExtract %float [[outputFloat3_3:%[0-9]+]] 1
// CHECK-NEXT: OpCompositeExtract %float [[outputFloat3_4:%[0-9]+]] 2
// CHECK-NEXT: OpCompositeConstruct %v4float
  return float4(output, 1.0f);
}

