// RUN: %dxc -Tlib_6_3 -HV 202x -verify %s
// RUN: %dxc -Tcs_6_3 -HV 202x -verify %s
// expected-no-diagnostics

union C {
  static uint f1;
  uint f2;
};

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  C c;
  c.f2 = 1;
  return;
}
