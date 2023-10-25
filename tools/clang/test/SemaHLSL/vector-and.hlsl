// RUN: %dxc -Tlib_6_3 -HV 2021 -verify %s

RWStructuredBuffer<int3> rw;

[numthreads(1,1,1)]
void main() {
// expected-error@+1 {{operands for short-circuiting logical binary operator must be scalar, for non-scalar types use 'and'}}
  rw[0] = rw[0] && rw[0];
}
