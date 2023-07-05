// RUN: %dxc -Tlib_6_3 -ast-dump %s 2>&1 | FileCheck %s

// CHECK: error: invalid matrix element type 'Texture2D'
matrix<Texture2D> m0;
// CHECK: error: invalid value, valid range is between 1 and 4 inclusive
matrix<float, 10> m1;
// CHECK: error: invalid value, valid range is between 1 and 4 inclusive
matrix<float, 2, 10> m2;

// CHECK: error: invalid value, valid range is between 1 and 4 inclusive
matrix<float, 0, 0> m3;
// CHECK: error: invalid value, valid range is between 1 and 4 inclusive
matrix<float, 0> m4;
// CHECK: error: invalid value, valid range is between 1 and 4 inclusive
matrix<float, 2, 0> m5;

float2x2 m;
float foo() {
  const float2x2 tm = m;
  // CHECK: error: cannot assign to variable 'tm' with const-qualified type 'const float2x2'
  tm[1] = 2;
  // CHECK: error: cannot assign to variable 'tm' with const-qualified type 'const float2x2'
  tm[1][1] = 3;
  // CHECK: error: matrix row index '-1' is out of bounds
  return tm[-1][0];
}
