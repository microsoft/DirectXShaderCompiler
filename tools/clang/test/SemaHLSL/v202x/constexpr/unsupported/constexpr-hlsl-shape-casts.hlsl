// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

constexpr int vector_to_scalar() { return (int)int4(1, 2, 3, 4); } // expected-error{{constexpr function never produces a constant expression}} expected-note 2 {{subexpression not valid in a constant expression}}
constexpr int vector_truncation() { return ((int2)int4(1, 2, 3, 4)).y; } // expected-error{{constexpr function never produces a constant expression}} expected-note 2 {{subexpression not valid in a constant expression}}
constexpr int vector_to_matrix() { // expected-error{{constexpr function never produces a constant expression}}
  return ((int2x2)int4(1, 2, 3, 4))[1][1]; // expected-note 2 {{subexpression not valid in a constant expression}}
}
constexpr int matrix_to_vector() { // expected-error{{constexpr function never produces a constant expression}}
  return ((int4)int2x2(1, 2, 3, 4)).w; // expected-note 2 {{subexpression not valid in a constant expression}}
}
constexpr int scalar_splat() { return ((int4)7).z; } // expected-error{{constexpr function never produces a constant expression}} expected-note 2 {{subexpression not valid in a constant expression}}

static_assert(vector_to_scalar() == 1, "vector-to-scalar truncation"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'vector_to_scalar()'}}
static_assert(vector_truncation() == 2, "vector truncation"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'vector_truncation()'}}
static_assert(vector_to_matrix() == 4, "vector-to-matrix flattened cast"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'vector_to_matrix()'}}
static_assert(matrix_to_vector() == 4, "matrix-to-vector flattened cast"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'matrix_to_vector()'}}
static_assert(scalar_splat() == 7, "scalar-to-vector splat"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'scalar_splat()'}}
