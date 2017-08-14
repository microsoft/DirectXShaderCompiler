// Run: %dxc -T vs_6_0 -E main

float fnInOut(uniform float a, in float b, out float c, inout float d) {
    float v = a + b + c + d;
    d = c = a;
    return v;
}

float main(float val: A) : B {
// CHECK-LABEL: %src_main = OpFunction
    float m, n;
// CHECK:      %param_var_a = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %param_var_b = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %param_var_c = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %param_var_d = OpVariable %_ptr_Function_float Function

// CHECK-NEXT:                OpStore %param_var_a %float_5
// CHECK-NEXT: [[val:%\d+]] = OpLoad %float %val
// CHECK-NEXT:                OpStore %param_var_b [[val]]
// CHECK-NEXT:   [[m:%\d+]] = OpLoad %float %m
// CHECK-NEXT:                OpStore %param_var_c [[m]]
// CHECK-NEXT:   [[n:%\d+]] = OpLoad %float %n
// CHECK-NEXT:                OpStore %param_var_d [[n]]

// CHECK-NEXT: [[ret:%\d+]] = OpFunctionCall %float %fnInOut %param_var_a %param_var_b %param_var_c %param_var_d

// CHECK-NEXT:   [[c:%\d+]] = OpLoad %float %param_var_c
// CHECK-NEXT:                OpStore %m [[c]]
// CHECK-NEXT:   [[d:%\d+]] = OpLoad %float %param_var_d
// CHECK-NEXT:                OpStore %n [[d]]

// CHECK-NEXT:                OpReturnValue [[ret]]
    return fnInOut(5., val, m, n);
// CHECK-NEXT: OpFunctionEnd
}
