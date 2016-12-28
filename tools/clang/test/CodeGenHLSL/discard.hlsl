// RUN: %dxc -E main -T ps_5_0 %s | FileCheck %s

// CHECK: discard

float main(float a : A, int b : B, float r : R) : SV_Target
{
  if (b > 100) {
    discard;
  }

  return r;
}
