// RUN: %dxc -I %hlsl_headers -T lib_6_9 -enable-16bit-types %s -verify

#include <dx/linalg.h>

enum MatLayout {
  RowMajor = 0,
  ColumnMajor = 1,
  MulOptimal = 2,
  OuterProductOptimal = 3,
};

using namespace dx::linalg;

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
  const uint input_interpretation = DataType::DATA_TYPE_FLOAT32;
  const uint matrix_offset = 0;
  const uint matrix_interpretation = DataType::DATA_TYPE_FLOAT32;
  const uint matrix_dimM = 4;
  const uint matrix_dimK = 4;
  const uint matrix_layout = MatrixLayout::MATRIX_LAYOUT_ROW_MAJOR;
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
  const uint input_interpretation = DataType::DATA_TYPE_FLOAT32;
  const uint matrix_offset = 0;
  const uint matrix_interpretation = DataType::DATA_TYPE_FLOAT32;
  const uint matrix_dimM = 4;
  const uint matrix_dimK = 4; 
  const uint matrix_layout = MatrixLayout::MATRIX_LAYOUT_ROW_MAJOR;
  const uint matrix_is_transposed = 0;
  const uint matrix_stride = 0;
  const uint bias_offset = 0;

  const uint bias_interpretation_0 = 0;

  // expected-error@+6 {{0 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_0);

  const uint bias_interpretation_1 = 1;

  // expected-error@+6 {{1 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_1);

  const uint bias_interpretation_2 = 6;

  // expected-error@+6 {{6 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_2);

  const uint bias_interpretation_3 = 7;

  // expected-error@+6 {{7 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_3);

  const uint bias_interpretation_4 = 10;

  // expected-error@+6 {{10 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_4);

  const uint bias_interpretation_5 = 11;

  // expected-error@+6 {{11 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_5);

  const uint bias_interpretation_6 = 12;

  // expected-error@+6 {{12 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,  
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_6);

  const uint bias_interpretation_7 = 13;

  // expected-error@+6 {{13 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_7);

  const uint bias_interpretation_8 = 14;

  // expected-error@+6 {{14 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_8);

  const uint bias_interpretation_9 = 15;

  // expected-error@+6 {{15 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_9);

  const uint bias_interpretation_10 = 16;
  
  
  // expected-error@+6 {{16 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_10);

  const uint bias_interpretation_11 = DataType::DATA_TYPE_SINT8_T4_PACKED;

  // expected-error@+6 {{17 is an invalid Memory Interpretation value}}
  __builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector,
                         is_input_unsigned, input_interpretation, matrix_buffer,
                         matrix_offset, matrix_interpretation, matrix_dimM,
                         matrix_dimK, matrix_layout, matrix_is_transposed,
                         matrix_stride, bias_buffer, bias_offset,
                         bias_interpretation_11);

  const uint bias_interpretation_12 = DataType::DATA_TYPE_UINT8_T4_PACKED;

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
