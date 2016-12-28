// RUN: %dxc -E main -T ps_5_0 %s | FileCheck %s

// CHECK: !"dx.controlflow.hints", i32 2

float main(float2 a : A, int b : B) : SV_Target
{
  [flatten]
  if (b == 1)
    return a.x;
  return a.y;
}
