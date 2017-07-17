// Run: %dxc -T vs_6_0 -E main

struct VSOut {
    float4 out1: C;
    float4 out2: D;
};

static float4 sgVar; // Private

// CHECK: [[input:%\d+]] = OpVariable %_ptr_Input_v4float Input

VSOut main(float4 input: A /* Input */, uint index: B /* Input */) {
    static float4 slVar; // Private

    VSOut ret; // Function

// CHECK:      {{%\d+}} = OpAccessChain %_ptr_Input_float [[input]] {{%\d+}}
// CHECK:      {{%\d+}} = OpAccessChain %_ptr_Private_float %sgVar {{%\d+}}
// CHECK:      {{%\d+}} = OpAccessChain %_ptr_Private_float %slVar {{%\d+}}
// CHECK:      [[lhs:%\d+]] = OpAccessChain %_ptr_Function_v4float %ret %int_0
// CHECK-NEXT: {{%\d+}} = OpAccessChain %_ptr_Function_float [[lhs]] {{%\d+}}
    ret.out1[index] = input[index] + sgVar[index] + slVar[index];

    return ret;
}
