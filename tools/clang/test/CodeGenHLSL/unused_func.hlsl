// RUN: %dxc -T ps_6_1 -fcgl %s | FileCheck %s

// Make sure unused function not generated.
// CHECK-NOT: unused

float unused() {
  return 3;
}


float4 main(float4 a : A) : SV_TARGET
{
  return a;
}

