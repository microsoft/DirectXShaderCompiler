// Run: %dxc -T vs_6_0 -E main

struct S {
    float  f;
};

struct T {
    float    a;
    float2   b;
    float3x4 c;
    S        s;
    float    t[4];
};


ConstantBuffer<T> MyCbuffer : register(b1);

float main() : A {
// CHECK:      [[a:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[a]]

// CHECK:      [[b:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %MyCbuffer %int_1
// CHECK-NEXT: [[b0:%\d+]] = OpAccessChain %_ptr_Uniform_float [[b]] %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[b0]]

// CHECK:      [[c:%\d+]] = OpAccessChain %_ptr_Uniform_mat3v4float %MyCbuffer %int_2
// CHECK-NEXT: [[c12:%\d+]] = OpAccessChain %_ptr_Uniform_float [[c]] %uint_1 %uint_2
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[c12]]

// CHECK:      [[s:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_3 %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[s]]

// CHECK:      [[t:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_4 %int_3
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[t]]
    return MyCbuffer.a + MyCbuffer.b.x + MyCbuffer.c[1][2] + MyCbuffer.s.f + MyCbuffer.t[3];
}

