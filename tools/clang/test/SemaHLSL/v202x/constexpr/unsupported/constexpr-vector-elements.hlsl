// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

constexpr int vector_subscript(int4 value) { return value[2]; }

static_assert(vector_subscript(int4(1, 2, 3, 4)) == 3, // expected-error{{static_assert expression is not an integral constant expression}}
              "constexpr vector subscript");
