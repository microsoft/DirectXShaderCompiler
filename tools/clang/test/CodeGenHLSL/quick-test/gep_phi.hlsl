// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// Make sure no phi of pointers
// CHECK-NOT: phi float*

static float a[4] = {1,2,3,4};
static float b[4] = {5,6,7,8};

float main(float x : X, uint y : Y) : SV_Target {
  float c[4] = {x, x, x, x};
  if (x > 2.5)
    return a[y];
  else if (x > 1.0)
    return b[y];
  else
    return c[y];
}