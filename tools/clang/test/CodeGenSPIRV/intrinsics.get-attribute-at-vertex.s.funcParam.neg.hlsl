// RUN: not %dxc -T ps_6_1 -E main  %s -spirv  2>&1 | FileCheck %s

struct S {
  float4 a : COLOR;
};

float compute(float4 a) {
  return GetAttributeAtVertex(a, 2)[0];
}

float4 main(nointerpolation S s, float4 b : COLOR2) : SV_TARGET
{
  return float4(0, 0, compute(b), compute(s.a));
}

// CHECK: error: Function 'compute' could only use noninterpolated variable as input.