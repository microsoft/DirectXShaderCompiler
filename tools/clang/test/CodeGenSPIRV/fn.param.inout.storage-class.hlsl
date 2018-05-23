// Run: %dxc -T vs_6_0 -E main

RWStructuredBuffer<float> Data;

void foo(in float a, inout float b, out float c) {
    b += a;
    c = a + b;
}

void main(float input : INPUT) {
// CHECK: %param_var_a = OpVariable %_ptr_Function_float Function
// CHECK: %param_var_b = OpVariable %_ptr_Function_float Function
// CHECK: %param_var_c = OpVariable %_ptr_Function_float Function

// CHECK: [[val:%\d+]] = OpLoad %float %input
// CHECK:                OpStore %param_var_a [[val]]
// CHECK:  [[p0:%\d+]] = OpAccessChain %_ptr_Uniform_float %Data %int_0 %uint_0
// CHECK: [[val:%\d+]] = OpLoad %float [[p0]]
// CHECK:                OpStore %param_var_b [[val]]
// CHECK:  [[p1:%\d+]] = OpAccessChain %_ptr_Uniform_float %Data %int_0 %uint_1
// CHECK: [[val:%\d+]] = OpLoad %float [[p1]]
// CHECK:                OpStore %param_var_c [[val]]

// CHECK:                OpFunctionCall %void %foo %param_var_a %param_var_b %param_var_c
    foo(input, Data[0], Data[1]);
// CHECK: [[val:%\d+]] = OpLoad %float %param_var_b
// CHECK:                OpStore [[p0]] [[val]]
// CHECK: [[val:%\d+]] = OpLoad %float %param_var_c
// CHECK:                OpStore [[p1]] [[val]]
}
