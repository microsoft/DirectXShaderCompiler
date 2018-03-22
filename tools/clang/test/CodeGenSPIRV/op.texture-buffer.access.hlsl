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


TextureBuffer<T> MyTB : register(t1);
TextureBuffer<T> MyTBArray[5] : register(t2);

float main() : A {
// CHECK:      [[a:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTB %int_0
// CHECK-NEXT:   {{%\d+}} = OpLoad %float [[a]]

// CHECK:       [[b:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %MyTB %int_1
// CHECK-NEXT: [[b0:%\d+]] = OpAccessChain %_ptr_Uniform_float [[b]] %int_0
// CHECK-NEXT:    {{%\d+}} = OpLoad %float [[b0]]

// CHECK:      [[c12:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTB %int_2 %uint_1 %uint_2
// CHECK-NEXT:     {{%\d+}} = OpLoad %float [[c12]]

// CHECK:      [[s:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTB %int_3 %int_0
// CHECK-NEXT:   {{%\d+}} = OpLoad %float [[s]]

// CHECK:      [[t:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTB %int_4 %int_3
// CHECK-NEXT:   {{%\d+}} = OpLoad %float [[t]]
  return MyTB.a         + MyTB.b.x         + MyTB.c[1][2]         + MyTB.s.f         + MyTB.t[3] +
// CHECK:      [[a:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTBArray %int_4 %int_0
// CHECK-NEXT:   {{%\d+}} = OpLoad %float [[a]]

// CHECK:       [[b:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %MyTBArray %int_3 %int_1
// CHECK-NEXT: [[b0:%\d+]] = OpAccessChain %_ptr_Uniform_float [[b]] %int_0
// CHECK-NEXT:    {{%\d+}} = OpLoad %float [[b0]]

// CHECK:      [[c12:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTBArray %int_2 %int_2 %uint_1 %uint_2
// CHECK-NEXT:     {{%\d+}} = OpLoad %float [[c12]]

// CHECK:      [[s:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTBArray %int_1 %int_3 %int_0
// CHECK-NEXT:   {{%\d+}} = OpLoad %float [[s]]

// CHECK:      [[t:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTBArray %int_0 %int_4 %int_3
// CHECK-NEXT:   {{%\d+}} = OpLoad %float [[t]]
         MyTBArray[4].a + MyTBArray[3].b.x + MyTBArray[2].c[1][2] + MyTBArray[1].s.f + MyTBArray[0].t[3];
}
