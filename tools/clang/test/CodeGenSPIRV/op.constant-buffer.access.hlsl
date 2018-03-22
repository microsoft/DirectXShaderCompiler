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
ConstantBuffer<T> MyCbufferArray[5] : register(b2);

float main() : A{
  // CHECK:      [[a:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_0
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[a]]

  // CHECK:      [[b:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %MyCbuffer %int_1
  // CHECK-NEXT: [[b0:%\d+]] = OpAccessChain %_ptr_Uniform_float [[b]] %int_0
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[b0]]

  // CHECK:      [[c12:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_2 %uint_1 %uint_2
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[c12]]

  // CHECK:      [[s:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_3 %int_0
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[s]]

  // CHECK:      [[t:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_4 %int_3
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[t]]
  return MyCbuffer.a + MyCbuffer.b.x + MyCbuffer.c[1][2] + MyCbuffer.s.f + MyCbuffer.t[3] +
  // CHECK:      [[a:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbufferArray %int_4 %int_0
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[a]]

  // CHECK:      [[b:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %MyCbufferArray %int_3 %int_1
  // CHECK-NEXT: [[b0:%\d+]] = OpAccessChain %_ptr_Uniform_float [[b]] %int_0
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[b0]]

  // CHECK:      [[c12:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbufferArray %int_2 %int_2 %uint_1 %uint_2
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[c12]]

  // CHECK:      [[s:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbufferArray %int_1 %int_3 %int_0
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[s]]

  // CHECK:      [[t:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyCbufferArray %int_0 %int_4 %int_3
  // CHECK-NEXT: {{%\d+}} = OpLoad %float [[t]]
         MyCbufferArray[4].a + MyCbufferArray[3].b.x + MyCbufferArray[2].c[1][2] + MyCbufferArray[1].s.f + MyCbufferArray[0].t[3];

}
