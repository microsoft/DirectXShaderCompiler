// RUN: %dxc -T ps_6_1 -E main  %s -spirv -Od 2>&1 | FileCheck %s

struct S {
  float4 a : COLOR;
};

float compute(float4 a) {
  return GetAttributeAtVertex(a, 2)[0];
}

float4 main(nointerpolation S s) : SV_TARGET
{
  return float4(0, 0, 0, compute(s.a));
}

// CHECK: [[param_var_a:%[a-zA-Z0-9_]+]] = OpVariable %_ptr_Function__arr_v4float_uint_3 Function
// CHECK: [[inst32:%[0-9_]+]] = OpAccessChain %_ptr_Function_v4float [[param_var_a]] %uint_0
// CHECK: [[inst33:%[0-9_]+]] = OpAccessChain %_ptr_Function_v4float [[s:%[a-zA-Z0-9_]+]] %int_0 %uint_0
// CHECK: [[inst34:%[0-9_]+]] = OpLoad %v4float [[inst33]]
// CHECK: OpStore [[inst32]] [[inst34]]
// CHECK: [[inst35:%[0-9_]+]] = OpFunctionCall %float %compute [[param_var_a]]