// RUN: %dxc -T lib_6_9 -enable-16bit-types %s -verify

enum CompType {
  Invalid = 0,
  I1 = 1,
  I16 = 2,
  U16 = 3,
  I32 = 4,
  U32 = 5,
  I64 = 6,
  U64 = 7,
  F16 = 8,
  F32 = 9,
  F64 = 10,
  SNormF16 = 11,
  UNormF16 = 12,
  SNormF32 = 13,
  UNormF32 = 14,
  SNormF64 = 15,
  UNormF64 = 16,
  PackedS8x32 = 17,
  PackedU8x32 = 18,

  // BEGIN NEW FOR SM 6.9
  U8 = 19,
  I8 = 20,
  F8_E4M3 = 21,
  F8_E5M2 = 22,
};

enum MatLayout {
  RowMajor = 0,
  ColumnMajor = 1,
  MulOptimal = 2,
  OuterProductOptimal = 3,
};

ByteAddressBuffer input_vector_buffer;
ByteAddressBuffer matrix_buffer;
ByteAddressBuffer bias_buffer;
RWByteAddressBuffer output_vector_buffer;
ByteAddressBuffer constants_buffer;

// Check bias interpretation is not a constant value
void test_invalid_bias_interpretation() {
  vector<float, 4> output_vector;
  const uint is_output_unsigned = 0;
  vector<float, 4> input_vector =
      input_vector_buffer.Load<vector<float, 4> >(0);
  const uint is_input_unsigned = 0;
  const uint input_interpretation = CompType::F32; // F32
  const uint matrix_offset = 0;
  const uint matrix_interpretation = CompType::F32; // F32
  const uint matrix_dimM = 4;
  const uint matrix_dimK = 4;
  const uint matrix_layout = MatLayout::RowMajor;   // RowMajor
  const uint matrix_is_transposed = 0;
  const uint matrix_stride = 0;
  const uint bias_offset = 0;

  const uint bias_interpretation_0 = constants_buffer.Load<uint>(0);

  // expected-error@+6 {{'BiasInterpretation' must be a constant parameter}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_0);
}

// Check bias interpretation is not a valid value
void test_invalid_bias_interpretation_value() {
  vector<float, 4> output_vector;
  const uint is_output_unsigned = 0;
  vector<float, 4> input_vector =
      input_vector_buffer.Load<vector<float, 4> >(0);
  const uint is_input_unsigned = 0;
  const uint input_interpretation = CompType::F32; // F32
  const uint matrix_offset = 0;
  const uint matrix_interpretation = CompType::F32; // F32
  const uint matrix_dimM = 4;
  const uint matrix_dimK = 4; 
  const uint matrix_layout = MatLayout::RowMajor;   // RowMajor
  const uint matrix_is_transposed = 0;
  const uint matrix_stride = 0;
  const uint bias_offset = 0;

  const uint bias_interpretation_0 = CompType::Invalid;

  // expected-error@+6 {{0 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_0);

  const uint bias_interpretation_1 = CompType::I1;

  // expected-error@+6 {{1 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_1);

  const uint bias_interpretation_2 = CompType::I64;

  // expected-error@+6 {{6 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_2);

  const uint bias_interpretation_3 = CompType::U64;

  // expected-error@+6 {{7 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_3);

  const uint bias_interpretation_4 = CompType::F64;

  // expected-error@+6 {{10 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_4);

  const uint bias_interpretation_5 = CompType::SNormF16;

  // expected-error@+6 {{11 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_5);

  const uint bias_interpretation_6 = CompType::UNormF16;

  // expected-error@+6 {{12 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,  
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_6);

  const uint bias_interpretation_7 = CompType::SNormF32;

  // expected-error@+6 {{13 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_7);

  const uint bias_interpretation_8 = CompType::UNormF32;

  // expected-error@+6 {{14 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_8);

  const uint bias_interpretation_9 = CompType::SNormF64;

  // expected-error@+6 {{15 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_9);

  const uint bias_interpretation_10 = CompType::UNormF64;
  
  
  // expected-error@+6 {{16 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_10);

  const uint bias_interpretation_11 = CompType::PackedS8x32;

  // expected-error@+6 {{17 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_11);

  const uint bias_interpretation_12 = CompType::PackedU8x32;

  // expected-error@+6 {{18 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_12);

  const uint bias_interpretation_13 = 23;

  // expected-error@+6 {{23 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_13);

  const uint bias_interpretation_14 = 100;

  // expected-error@+6 {{100 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_14);
  }     
