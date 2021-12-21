// RUN: %clang_cc1 -HV 2021 -fsyntax-only -ffreestanding -verify %s

RWStructuredBuffer<int3> rw;

struct FV {
  int3 f;
};

ConstantBuffer<FV> c;

[numthreads(1,1,1)]
void main() {
// expected-error@+1 {{condition for short-circuiting ternary operator must be scalar, for non-scalar types use 'select'}}
  rw[0] = rw[0] ? rw[0] : c.f;
}
