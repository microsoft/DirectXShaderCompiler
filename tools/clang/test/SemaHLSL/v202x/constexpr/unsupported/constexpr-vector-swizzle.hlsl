// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

constexpr int vector_swizzle() { return int4(1, 2, 3, 4).y; } // expected-error{{constexpr function never produces a constant expression}} expected-note 2 {{subexpression not valid in a constant expression}}

static_assert(vector_swizzle() == 2, // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'vector_swizzle()'}}
              "constexpr vector swizzle");
