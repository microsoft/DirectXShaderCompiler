// RUN: %dxc -I %hlsl_headers -T cs_6_10 -verify %s

#include <dx/linalg.h>
using namespace dx::linalg;

using MatrixATy = Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave>;

[nodiscard] int fn() { return 42; }
[[nodiscard]] int fn2() { return 42; }

[numthreads(4, 4, 4)]
void main(uint ID : SV_GroupID)
{
  MatrixATy MatA1;
  MatA1.Splat(1.0f); // expected-warning {{ignoring return value of function declared with 'nodiscard' attribute}}
  fn(); // expected-warning {{ignoring return value of function declared with 'nodiscard' attribute}}
  fn2(); // expected-warning {{ignoring return value of function declared with 'nodiscard' attribute}}
}
