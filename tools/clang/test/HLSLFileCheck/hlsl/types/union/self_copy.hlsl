// RUN: %dxc -E main -HV 202x -T ps_6_0 %s | FileCheck %s

// CHECK: %union.S = type { %union.N }
// CHECK: %union.N = type { float }
union N {
  float n;
};

union S {
  N  n;
  int s;
};

S s0;

// CHECK: @dx.op.storeOutput.f32
float4 main(float4 a : A, float4 b:B) : SV_TARGET
{
  S s1 = s0;
  s1 = s1;
  return s1.n.n;
}
