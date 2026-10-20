// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

static_assert(determinant(float2x2(1.0f, 2.0f, 3.0f, 4.0f)) == -2.0f, // expected-error{{static_assert expression is not an integral constant expression}}
              "determinant");
static_assert(mul(2, 3) == 6, "scalar mul"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(mul(int2(1, 2), int2(3, 4)) == 11, "vector dot-product mul"); // expected-error{{static_assert expression is not an integral constant expression}}

constexpr int2x3 transposed = transpose(int3x2(1, 2, 3, 4, 5, 6)); // expected-error{{constexpr variable 'transposed' must be initialized by a constant expression}}
static_assert(transposed[0][0] == 1 && transposed[0][1] == 3 && // expected-error{{static_assert expression is not an integral constant expression}}
                  transposed[0][2] == 5 && transposed[1][0] == 2 &&
                  transposed[1][1] == 4 && transposed[1][2] == 6,
              "non-square transpose");

constexpr int2 matrix_vector = mul(int2x3(1, 2, 3, 4, 5, 6), int3(1, 2, 3)); // expected-error{{constexpr variable 'matrix_vector' must be initialized by a constant expression}}
static_assert(matrix_vector.x == 14 && matrix_vector.y == 32, // expected-error{{static_assert expression is not an integral constant expression}}
              "matrix-vector mul");

constexpr int3 vector_matrix = mul(int2(2, 3), int2x3(1, 2, 3, 4, 5, 6)); // expected-error{{constexpr variable 'vector_matrix' must be initialized by a constant expression}}
static_assert(vector_matrix.x == 14 && vector_matrix.y == 19 && // expected-error{{static_assert expression is not an integral constant expression}}
                  vector_matrix.z == 24,
              "vector-matrix mul");

constexpr int2x2 matrix_matrix = // expected-error{{constexpr variable 'matrix_matrix' must be initialized by a constant expression}}
    mul(int2x3(1, 2, 3, 4, 5, 6), int3x2(7, 8, 9, 10, 11, 12));
static_assert(matrix_matrix[0][0] == 58 && matrix_matrix[0][1] == 64 && // expected-error{{static_assert expression is not an integral constant expression}}
                  matrix_matrix[1][0] == 139 && matrix_matrix[1][1] == 154,
              "matrix-matrix mul");
