// RUN: %dxc -T lib_6_ -E main %s | FileCheck %s

ByteAddressBuffer input_vector_buffer;
ByteAddressBuffer matrix_buffer; 
RWByteAddressBuffer output_vector_buffer;

// CHECK: error: __builtin_MatVecMul: first parameter must be numeric vector
void test_mul_non_numeric() {
    vector<int, 4> output_vector; // Changed from bool to int vector
    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float, 4>>(0);
    const uint is_output_unsigned = 0;
    const uint is_input_unsigned = 0;
    const uint input_interpretation = 9; // F32
    const uint matrix_offset = 0;
    const uint matrix_interpretation = 9; // F32
    const uint matrix_dimM = 4;
    const uint matrix_dimK = 4;
    const uint matrix_layout = 0; // RowMajor
    const bool matrix_is_transposed = false;
    const uint matrix_stride = 64;

    __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector,
        is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride); // expected-error {{__builtin_MatVecMul: first parameter must be numeric vector}}
}

// CHECK: error: __builtin_MatVecMul: input vector must be numeric
void test_mul_non_numeric2() {
    vector<float, 4> output_vector;
    vector<bool, 4> input_vector; // Using bool vector to trigger error
    const uint is_output_unsigned = 0;
    const uint is_input_unsigned = 0;
    const uint input_interpretation = 9; // F32
    const uint matrix_offset = 0;
    const uint matrix_interpretation = 9; // F32
    const uint matrix_dimM = 4;
    const uint matrix_dimK = 4;
    const uint matrix_layout = 0; // RowMajor
    const bool matrix_is_transposed = false;
    const uint matrix_stride = 64;

    __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector,
        is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride); // expected-error {{__builtin_MatVecMul: input vector must be numeric}}
}

// CHECK: error: __builtin_MatVecMul: matrix buffer must be valid
struct MyStruct { int x; };
void test_mul_invalid_buffer() {
    vector<float, 4> output_vector;
    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float, 4>>(0);
    MyStruct s;
    const uint is_output_unsigned = 0;
    const uint is_input_unsigned = 0;
    const uint input_interpretation = 9; // F32
    const uint matrix_offset = 0;
    const uint matrix_interpretation = 9; // F32
    const uint matrix_dimM = 4;
    const uint matrix_dimK = 4;
    const uint matrix_layout = 0; // RowMajor
    const bool matrix_is_transposed = false;
    const uint matrix_stride = 64;

    __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector,
        is_input_unsigned, input_interpretation, s, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride); // expected-error {{__builtin_MatVecMul: matrix buffer must be valid}}
}

// CHECK: error: __builtin_MatVecMul: matrix dimensions must be positive
void test_mul_invalid_dims() {
    vector<float, 4> output_vector;
    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float, 4>>(0);
    const uint is_output_unsigned = 0;
    const uint is_input_unsigned = 0;
    const uint input_interpretation = 9; // F32
    const uint matrix_offset = 0;
    const uint matrix_interpretation = 9; // F32
    const uint matrix_dimM = 0;
    const uint matrix_dimK = 0;
    const uint matrix_layout = 0; // RowMajor
    const bool matrix_is_transposed = false;
    const uint matrix_stride = 64;

    __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector,
        is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride); // expected-error {{__builtin_MatVecMul: matrix dimensions must be positive}}
}

// CHECK: error: __builtin_MatVecMul: matrix stride must be positive
void test_mul_invalid_stride() {
    vector<float, 4> output_vector;
    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float, 4>>(0);
    const uint is_output_unsigned = 0;
    const uint is_input_unsigned = 0;
    const uint input_interpretation = 9; // F32
    const uint matrix_offset = 0;
    const uint matrix_interpretation = 9; // F32
    const uint matrix_dimM = 4;
    const uint matrix_dimK = 4;
    const uint matrix_layout = 0; // RowMajor
    const bool matrix_is_transposed = false;
    const uint matrix_stride = 0;

    __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector,
        is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride); // expected-error {{__builtin_MatVecMul: matrix stride must be positive}}
}

[numthreads(1,1,1)]
void main() {
    vector<float, 4> output_vector;
    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float, 4>>(0);
    const uint is_output_unsigned = 0;
    const uint is_input_unsigned = 0;
    const uint input_interpretation = 9; // F32
    const uint matrix_offset = 0;
    const uint matrix_interpretation = 9; // F32
    const uint matrix_dimM = 4;
    const uint matrix_dimK = 4;
    const uint matrix_layout = 0; // RowMajor
    const bool matrix_is_transposed = false;
    const uint matrix_stride = 64;

    __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector,
        is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride);
    output_vector_buffer.Store(0, output_vector);
}





