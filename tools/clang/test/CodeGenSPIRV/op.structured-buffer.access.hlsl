// RUN: %dxc -T ps_6_0 -E main

struct S {
    float  f;
};

struct T {
    float    a;
    float2   b[2];
    float3x4 c[3];
    S        s[2];
    float    t[4];
};


StructuredBuffer<T> MySbuffer;

float4 main(uint index: A) : SV_Target {
// CHECK:      [[a:%\d+]] = OpAccessChain %_ptr_Uniform_float %MySbuffer %int_0 %uint_0 %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[a]]

// CHECK:      [[b1:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %MySbuffer %int_0 %uint_1 %int_1 %int_1
// CHECK-NEXT: [[x:%\d+]] = OpAccessChain %_ptr_Uniform_float [[b1]] %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[x]]

// CHECK:      [[c12:%\d+]] = OpAccessChain %_ptr_Uniform_float %MySbuffer %int_0 %uint_2 %int_2 %int_2 %uint_1 %uint_2
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[c12]]

// CHECK:      [[s:%\d+]] = OpAccessChain %_ptr_Uniform_float %MySbuffer %int_0 %uint_3 %int_3 %int_0 %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[s]]

// CHECK:      [[index:%\d+]] = OpLoad %uint %index
// CHECK-NEXT: [[t:%\d+]] = OpAccessChain %_ptr_Uniform_float %MySbuffer %int_0 [[index]] %int_4 %int_3
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[t]]
    return MySbuffer[0].a + MySbuffer[1].b[1].x + MySbuffer[2].c[2][1][2] +
           MySbuffer[3].s[0].f + MySbuffer[index].t[3];
}
