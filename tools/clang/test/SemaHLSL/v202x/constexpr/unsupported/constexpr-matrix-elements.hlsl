// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

constexpr int matrix_subscript(int2x2 value) { return value[1][0]; }
constexpr int matrix_named_element(int2x2 value) { return value._m10; } // expected-error{{constexpr function never produces a constant expression}} expected-note{{subexpression not valid in a constant expression}}

static_assert(matrix_subscript(int2x2(1, 2, 3, 4)) == 3, // expected-error{{static_assert expression is not an integral constant expression}}
              "constexpr matrix subscript");
static_assert(matrix_named_element(int2x2(1, 2, 3, 4)) == 3, // expected-error{{static_assert expression is not an integral constant expression}}
              "constexpr matrix named element");
