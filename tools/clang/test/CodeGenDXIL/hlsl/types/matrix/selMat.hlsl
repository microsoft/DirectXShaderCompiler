// RUN: %dxc -E main -T ps_6_0 -HV 2018 %s | FileCheck %s
// RUN: %dxc -E main -T ps_6_0 -HV 2018 -Zpr %s | FileCheck %s

// CHECK-COUNT-16: fcmp fast olt float %{{.+}}, 3.000000e+00

// CHECK-COUNT-16: select i1

// CHECK: icmp ne i32 %{{.+}}, 0


// CHECK-COUNT-16: select i1

float4x4 x4;
float4x4 xa;
float4x4 xb;

uint i;

float4 main(uint4 a : A) : SV_TARGET
{
  float4x4 x = (x4 < 3)?xa:xb;
  x += (i > 0)?xa:xb;
  return x[i];
}
