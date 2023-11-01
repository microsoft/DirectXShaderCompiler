// RUN: %dxc -E main -enable-unions -HV 2021 -T ps_6_0 %s | FileCheck %s

// CHECK: @main
union N {
  float n;
};

union S {
  N  n;
  float s;
};

S s0;

float4 main(float4 a : A, float4 b:B) : SV_TARGET
{
  S s1 = s0;
  s1 = s1;
  return s1.n.n;
}
