// Run: %dxc -T vs_6_0 -E main

struct VSOut {
    float4 out1: C;
    float4 out2: D;
};

// CHECK: [[input:%\d+]] = OpVariable %_ptr_Input_v4float Input

VSOut main(float4 input: A, uint index: B) {
    VSOut ret;

// CHECK:      {{%\d+}} = OpAccessChain %_ptr_Input_float [[input]] {{%\d+}}
// CHECK:      [[lhs:%\d+]] = OpAccessChain %_ptr_Function_v4float %ret %int_0
// CHECK-NEXT: {{%\d+}} = OpAccessChain %_ptr_Function_float [[lhs]] {{%\d+}}
    ret.out1[index] = input[index];

    return ret;
}
