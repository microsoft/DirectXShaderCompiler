// RUN: %dxc -E main -T ps_5_0 %s | FileCheck %s

// CHECK: fmul

float main(float a : A, float b : B, float2 c : C) : SV_Target
{
  float r = a;
  r += a;
  r /= a;
  r *= b;
  r = max(r, c.x);
  r = min(r, c.y);
  return r;
}
