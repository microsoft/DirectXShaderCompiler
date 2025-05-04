// RUN: %dxc -T lib_6_ -E main %s | FileCheck %s

ByteAddressBuffer input_vector_buffer;
ByteAddressBuffer matrix_buffer;
RWByteAddressBuffer output_vector_buffer;
ByteAddressBuffer constants_buffer;

// Output vector, isUnsigned mismatch
void test_invalid_output_vector_type() {

  vector<float, 4> input_vector =
      input_vector_buffer.Load<vector<float, 4> >(0);
  const uint is_input_unsigned = 0;
  const uint input_interpretation = 9; // F32
  const uint matrix_offset = 0;
  const uint matrix_interpretation = 9; // F32
  const uint matrix_dimM = 4;
  const uint matrix_dimK = 4;
  const uint matrix_layout = 0; // RowMajor
  const bool matrix_is_transposed = false;
  const uint matrix_stride = 64;

  vector<uint, 4> output_vector_0;
  const uint is_output_unsigned_0 = 0;

  // expected-error@+1 {{IsOuputUnsigned must be true for a unsigned int vector type}}
  __builtin_MatVecMul(output_vector_0, is_output_unsigned_0, input_vector,
                      is_input_unsigned, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);

  vector<int64_t, 4> output_vector_1;
  const uint is_output_unsigned_1 = 1;

  // expected-error@+1 {{IsOuputUnsigned must be false for a signed int vector type}}
  __builtin_MatVecMul(output_vector_1, is_output_unsigned_1, input_vector,
                      is_input_unsigned, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);

  vector<float, 4> output_vector_2;
  const uint is_output_unsigned_2 = 1;

  // expected-error@+1 {{IsOuputUnsigned must be false for a float vector type}}
  __builtin_MatVecMul(output_vector_2, is_output_unsigned_2, input_vector,
                      is_input_unsigned, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);
}

// IsOutputUnsigned is not a constant parameter
void test_invalid_is_output_unsigned_non_const() {

  vector<uint, 4> output_vector_0;
  vector<float, 4> input_vector =
      input_vector_buffer.Load<vector<float, 4> >(0);
  const uint is_input_unsigned = 0;
  const uint input_interpretation = 9; // F32
  const uint matrix_offset = 0;
  const uint matrix_interpretation = 9; // F32
  const uint matrix_dimM = 4;
  const uint matrix_dimK = 4;
  const uint matrix_layout = 0; // RowMajor
  const bool matrix_is_transposed = false;
  const uint matrix_stride = 64;

  const uint is_output_unsigned_0 = constants_buffer.Load<uint>(0);

  // expected-error@+1 {{IsOutputUnsigned' must be a constant parameter}}
  __builtin_MatVecMul(output_vector_0, is_output_unsigned_0, input_vector,
                      is_input_unsigned, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);
}

// Input vector is incorrect type - 64 bit types
void test_invalid_input_vector_type() {

  vector<uint, 4> output_vector;
  const uint is_output_unsigned = 1;
  const uint input_interpretation = 9; // F32
  const uint matrix_offset = 0;
  const uint matrix_interpretation = 9; // F32
  const uint matrix_dimM = 4;
  const uint matrix_dimK = 4;
  const uint matrix_layout = 0; // RowMajor
  const bool matrix_is_transposed = false;
  const uint matrix_stride = 64;

    vector<int64_t, 4> input_vector_0 =
      input_vector_buffer.Load<vector<int64_t, 4> >(0);
    const uint is_input_unsigned_0 = 0;

// expected-error@+1 {{Input Vector is incorrect type, must be 16-bit or 32-bit 'unsigned int', 'signed int' or 'float'}}
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector_0,
                      is_input_unsigned_0, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);

    vector<uint64_t, 4> input_vector_1 =
      input_vector_buffer.Load<vector<uint64_t, 4> >(0);
    const uint is_input_unsigned_1 = 1;

// expected-error@+1 {{Input Vector is incorrect type, must be 16-bit or 32-bit 'unsigned int', 'signed int' or 'float'}}   
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector_1,
                      is_input_unsigned_1, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);

    vector<float64_t, 4> input_vector_2 =
      input_vector_buffer.Load<vector<float64_t, 4> >(0);
    const uint is_input_unsigned_2 = 0;

// expected-error@+1 {{Input Vector is incorrect type, must be 16-bit or 32-bit 'unsigned int', 'signed int' or 'float'}}
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector_2,
                      is_input_unsigned_2, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);
}

// Input vector type/isInputUnsigned mismatch
void test_invalid_input_vector_type_mismatch() {

  vector<uint, 4> output_vector;
  const uint is_output_unsigned = 1;
  const uint input_interpretation = 9; // F32
  const uint matrix_offset = 0;
  const uint matrix_interpretation = 9; // F32
  const uint matrix_dimM = 4;
  const uint matrix_dimK = 4;
  const uint matrix_layout = 0; // RowMajor
  const bool matrix_is_transposed = false;
  const uint matrix_stride = 64;

  vector<uint, 4> input_vector_0 =
      input_vector_buffer.Load<vector<uint, 4> >(0);    
  const uint is_input_unsigned_0 = 0;

  // expected-error@+2 {{IsInputUnsigned must be true for a unsigned int vector type}}
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector_0,
                      is_input_unsigned_0, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);

  vector<int32_t, 4> input_vector_1 =
      input_vector_buffer.Load<vector<int32_t, 4> >(0);
  const uint is_input_unsigned_1 = 1;

  // expected-error@+2 {{IsInputUnsigned must be false for a signed int vector type}}
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector_1,
                      is_input_unsigned_1, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);

  vector<float16_t, 4> input_vector_2 =
      input_vector_buffer.Load<vector<float16_t, 4> >(0);
  const uint is_input_unsigned_2 = 1;

  // expected-error@+2 {{IsInputUnsigned must be false for a float vector type}}
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector_2,
                      is_input_unsigned_2, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);
}

//  Check is Matrix M dimension is a constant parameter
void test_invalid_matrix_M_dimension() {

  vector<uint, 4> output_vector;
  const uint is_output_unsigned = 1;
  const uint input_interpretation = 9; // F32   
  vector<float, 4> input_vector =
      input_vector_buffer.Load<vector<float, 4> >(0);
  const uint is_input_unsigned = 0;
  const uint matrix_offset = 0;
  const uint matrix_interpretation = 9; // F32
  const uint matrix_dimK = 4;
  const uint matrix_layout = 0; // RowMajor
  const bool matrix_is_transposed = false;
  const uint matrix_stride = 64; 

  const uint matrix_dimM = constants_buffer.Load<uint>(0);   
  
  // expected-error@+3 {{'MatrixM' must be a constant parameter}}
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector,
                      is_input_unsigned, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);
}

//  Check is Matrix K dimension is a constant parameter
void test_invalid_matrix_K_dimension() {

  vector<uint, 4> output_vector;
  const uint is_output_unsigned = 1;
  vector<float, 4> input_vector =
      input_vector_buffer.Load<vector<float, 4> >(0);
  const uint is_input_unsigned = 0; 
  const uint input_interpretation = 9; // F32
  const uint matrix_offset = 0;
  const uint matrix_interpretation = 9; // F32
  const uint matrix_dimM = 4;
  const uint matrix_layout = 0; // RowMajor
  const bool matrix_is_transposed = false;
  const uint matrix_stride = 64;

  const uint matrix_dimK = constants_buffer.Load<uint>(0);
  
  // expected-error@+4 {{'MatrixK' must be a constant parameter}}
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector,
                      is_input_unsigned, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);
}
/*
//

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
        matrix_is_transposed, matrix_stride);
}

// CHECK: error: __builtin_MatVecMul: matrix buffer must be valid
struct MyStruct { int x; };
void test_mul_invalid_buffer() {
    vector<float, 4> output_vector;
    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float,
4>>(0); MyStruct s; const uint is_output_unsigned = 0; const uint
is_input_unsigned = 0; const uint input_interpretation = 9; // F32 const uint
matrix_offset = 0; const uint matrix_interpretation = 9; // F32 const uint
matrix_dimM = 4; const uint matrix_dimK = 4; const uint matrix_layout = 0; //
RowMajor const bool matrix_is_transposed = false; const uint matrix_stride = 64;

    __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector,
        is_input_unsigned, input_interpretation, s, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride);
}

// CHECK: error: __builtin_MatVecMul: matrix dimensions must be positive
void test_mul_invalid_dims() {
    vector<float, 4> output_vector;
    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float,
4>>(0); const uint is_output_unsigned = 0; const uint is_input_unsigned = 0;
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
        matrix_is_transposed, matrix_stride);
}

// CHECK: error: __builtin_MatVecMul: matrix stride must be positive
void test_mul_invalid_stride() {
    vector<float, 4> output_vector;
    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float,
4>>(0); const uint is_output_unsigned = 0; const uint is_input_unsigned = 0;
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
        matrix_is_transposed, matrix_stride);
}

[numthreads(1,1,1)]
void main() {
    vector<float, 4> output_vector;
    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float,
4>>(0); const uint is_output_unsigned = 0; const uint is_input_unsigned = 0;
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

#endif // 0
*/
