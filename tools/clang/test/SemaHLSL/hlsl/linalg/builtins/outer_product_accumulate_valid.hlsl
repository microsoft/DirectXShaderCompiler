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
RWByteAddressBuffer accumulate_buffer;
ByteAddressBuffer constants_buffer;

// Check ofr input vectors aren't the same component type
void test_invalid_input_vector_component_type() {

  const uint matrix_offset = 0;
  const uint matrix_interpretation = CompType::F32;
  const uint matrix_layout = MatLayout::OuterProductOptimal;
  const uint matrix_stride = 0;

  vector<float, 4> input_vector_0_0 = input_vector_buffer.Load<vector<float, 4> >(0);
  vector<float, 16> input_vector_1_0 = input_vector_buffer.Load<vector<float, 16> >(0);

      // expected-no-diagnostics@+1
  __builtin_OuterProductAccumulate(input_vector_0_0, input_vector_1_0,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);

  vector<int, 32> input_vector_0_1 = input_vector_buffer.Load<vector<int, 32> >(0);
  vector<int ,16> input_vector_1_1 = input_vector_buffer.Load<vector<int, 16> >(0);

     // expected-no-diagnostics@+1
  __builtin_OuterProductAccumulate(input_vector_0_1, input_vector_1_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);

  vector<uint, 4> input_vector_0_2 = input_vector_buffer.Load<vector<uint, 4> >(0);
  vector<uint, 16> input_vector_1_2 = input_vector_buffer.Load<vector<uint, 16> >(0);

  // expected-no-diagnostics@+1
  __builtin_OuterProductAccumulate(input_vector_0_2, input_vector_1_2,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);
}

// Check for non constant matrix stride
void test_non_constant_matrix_stride() {

  vector<float, 4> input_vector_0 = input_vector_buffer.Load<vector<float, 4> >(0);
  vector<float, 4> input_vector_1 = input_vector_buffer.Load<vector<float, 4> >(0);
  const uint matrix_offset = 0;
  const uint matrix_interpretation = CompType::F32;
  const uint matrix_layout = MatLayout::OuterProductOptimal;

  const uint matrix_stride = constants_buffer.Load<uint>(0);

  // expected-no-diagnostics@+4
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);
}

// Check for matrix stride is not a valid value

