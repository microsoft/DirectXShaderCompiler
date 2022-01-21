// RUN: %clang_cc1 -HV 2021 -fsyntax-only -ffreestanding -verify %s

RWStructuredBuffer<int3> rw;

[numthreads(1, 1, 1)]
void main()
{
// expected-error@+1 {{operands for short-circuiting logical binary operator must be scalar, for non-scalar types use 'or'}}
    rw[0] = rw[0] || rw[0];
}
