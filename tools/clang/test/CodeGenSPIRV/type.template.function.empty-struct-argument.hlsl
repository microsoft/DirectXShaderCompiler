// RUN: %dxc -T ps_6_0 -E main -HV 2021

struct A {};

template <typename T0, typename T1 = A>
struct B {
  T0 m0;
  T1 m1;
};

float4 main() : SV_Target {
  // CHECK: %12 = OpConstantComposite %v4float %float_1 %float_2 %float_3 %float_4
  B<float4> b = { float4(1, 2, 3, 4) };
  return b.m0;
}
