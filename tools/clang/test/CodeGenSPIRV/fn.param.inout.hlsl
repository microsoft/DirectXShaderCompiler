// RUN: %dxc -T vs_6_0 -E main

struct Pixel {
  float4 color;
};

float fnInOut(uniform float a, in float b, out float c, inout float d, inout Pixel e) {
    float v = a + b + c + d;
    d = c = a;
    return v;
}

float main(float val: A) : B {
    float m, n;
    Pixel p;

// CHECK:      %param_var_a = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %param_var_b = OpVariable %_ptr_Function_float Function

// CHECK-NEXT:                OpStore %param_var_a %float_5
// CHECK-NEXT: [[val:%\d+]] = OpLoad %float %val
// CHECK-NEXT:                OpStore %param_var_b [[val]]

// CHECK-NEXT: [[ret:%\d+]] = OpFunctionCall %float %fnInOut %param_var_a %param_var_b %m %n %p

// CHECK-NEXT:                OpReturnValue [[ret]]
    return fnInOut(5., val, m, n, p);
}
