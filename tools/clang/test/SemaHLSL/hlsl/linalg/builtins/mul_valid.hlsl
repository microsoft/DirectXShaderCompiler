// RUN: %dxc -T lib_6_9 -enable-16bit-types %s -verify

ByteAddressBuffer input_vector_buffer;
ByteAddressBuffer matrix_buffer; 
RWByteAddressBuffer output_vector_buffer;
ByteAddressBuffer const_buffer;

// Output vector, isUnsigned mismatch
void test_invalid_output_vector_type() {

    vector<float, 4> input_vector = input_vector_buffer.Load<vector<float, 4> >(0);
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
    const uint is_output_unsigned_0 = 1;

    // expected-no-diagnostics@+1
    __builtin_MatVecMul(output_vector_0, is_output_unsigned_0, input_vector,
        is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride);

    vector<int64_t, 4> output_vector_1;
    const uint is_output_unsigned_1 = 0;

    // expected-no-diagnostics@+1
    __builtin_MatVecMul(output_vector_1, is_output_unsigned_1, input_vector,
        is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride);

    vector<float, 4> output_vector_2;
    const uint is_output_unsigned_2 = 0;

    // expected-no-diagnostics@+1
    __builtin_MatVecMul(output_vector_2, is_output_unsigned_2, input_vector,
        is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset,
        matrix_interpretation, matrix_dimM, matrix_dimK, matrix_layout,
        matrix_is_transposed, matrix_stride);
}

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

  const uint is_output_unsigned_0 = 1;

  // expected-no-diagnostics@+1
  __builtin_MatVecMul(output_vector_0, is_output_unsigned_0, input_vector,
                      is_input_unsigned, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);
}

// Input vector is incorrect type
void test_valid_input_vector_type() {

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

    vector<int32_t, 4> input_vector_0 =
      input_vector_buffer.Load<vector<int32_t, 4> >(0);
    const uint is_input_unsigned_0 = 0;

 // expected-no-diagnostics@+1
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector_0,
                      is_input_unsigned_0, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);

    vector<uint32_t, 4> input_vector_1 =
      input_vector_buffer.Load<vector<uint32_t, 4> >(0);
    const uint is_input_unsigned_1 = 1;

 // expected-no-diagnostics@+1 
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector_1,
                      is_input_unsigned_1, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);

    vector<float16_t, 4> input_vector_2 =
      input_vector_buffer.Load<vector<float16_t, 4> >(0);
    const uint is_input_unsigned_2 = 0;

 // expected-no-diagnostics@+1
  __builtin_MatVecMul(output_vector, is_output_unsigned, input_vector_2,
                      is_input_unsigned_2, input_interpretation, matrix_buffer,
                      matrix_offset, matrix_interpretation, matrix_dimM,
                      matrix_dimK, matrix_layout, matrix_is_transposed,
                      matrix_stride);
}
