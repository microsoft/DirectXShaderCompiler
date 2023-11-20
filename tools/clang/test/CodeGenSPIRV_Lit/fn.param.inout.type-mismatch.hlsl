// RUN: %dxc -T ps_6_2 -E main -enable-16bit-types
void foo(const half3 input, out half3 output) {
  output = input;
}

float4 main() : SV_Target0 {
  float3 output;
// CHECK:       %param_var_input = OpVariable %_ptr_Function_v3half Function
// CHECK-NEXT: %param_var_output = OpVariable %_ptr_Function_v3half Function

// CHECK:      [[outputFloat3:%\d+]] = OpLoad %v3float %output
// CHECK-NEXT:  [[outputHalf3:%\d+]] = OpFConvert %v3half [[outputFloat3]]
// CHECK-NEXT:                         OpStore %param_var_output [[outputHalf3]]
// CHECK-NEXT:              {{%\d+}} = OpFunctionCall %void %foo %param_var_input %param_var_output
  foo(float3(1, 0, 0), output);
// CHECK-NEXT:  [[outputHalf3:%\d+]] = OpLoad %v3half %param_var_output
// CHECK-NEXT: [[outputFloat3:%\d+]] = OpFConvert %v3float [[outputHalf3]]
// CHECK-NEXT:                         OpStore %output [[outputFloat3]]

// CHECK-NEXT: [[outputFloat3:%\d+]] = OpLoad %v3float %output
// CHECK-NEXT: OpCompositeExtract %float [[outputFloat3:%\d+]] 0
// CHECK-NEXT: OpCompositeExtract %float [[outputFloat3:%\d+]] 1
// CHECK-NEXT: OpCompositeExtract %float [[outputFloat3:%\d+]] 2
// CHECK-NEXT: OpCompositeConstruct %v4float
  return float4(output, 1.0f);
}

