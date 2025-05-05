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
  vector<uint, 4> input_vector_1_0 = input_vector_buffer.Load<vector<uint, 4> >(0);

  // expected-error@+1 {{input vectors of outerproductaccumulate must have the same element type}}
  __builtin_OuterProductAccumulate(input_vector_0_0, input_vector_1_0,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);

  vector<int, 4> input_vector_0_1 = input_vector_buffer.Load<vector<int, 4> >(0);
  vector<float, 4> input_vector_1_1 = input_vector_buffer.Load<vector<float, 4> >(0);

  // expected-error@+1 {{input vectors of outerproductaccumulate must have the same element type}}
  __builtin_OuterProductAccumulate(input_vector_0_1, input_vector_1_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);
}

// Check for non constant matrix interpretation
void test_non_constant_matrix_interpretation() {

  vector<float, 4> input_vector_0 = input_vector_buffer.Load<vector<float, 4> >(0);
  vector<float, 4> input_vector_1 = input_vector_buffer.Load<vector<float, 4> >(0);
  const uint matrix_offset = 0;
  const uint matrix_layout = MatLayout::OuterProductOptimal;
  const uint matrix_stride = 0;

  const uint matrix_interpretation = constants_buffer.Load<uint>(0);

  // expected-error@+3 {{'MatrixInterpretation' must be a constant parameter}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);
}

// Check for matrix interpretation is not a valid value
void test_invalid_matrix_interpretation() {

  vector<float, 4> input_vector_0 = input_vector_buffer.Load<vector<float, 4> >(0);
  vector<float, 4> input_vector_1 = input_vector_buffer.Load<vector<float, 4> >(0);
  const uint matrix_offset = 0;
  const uint matrix_layout = MatLayout::OuterProductOptimal;
  const uint matrix_stride = 0;

  const uint matrix_interpretation = CompType::Invalid;

  // expected-error@+3 {{0 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_2 = CompType::I1;

  // expected-error@+3 {{1 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_2, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_3 = CompType::I64;

  // expected-error@+3 {{6 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_3, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_4 = CompType::U64;

  // expected-error@+3 {{7 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_4, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_5 = CompType::F64;

  // expected-error@+3 {{10 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_5, matrix_layout,
                                  matrix_stride); 

  const uint matrix_interpretation_6 = CompType::SNormF16;

  // expected-error@+3 {{11 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_6, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_7 = CompType::UNormF16;

  // expected-error@+3 {{12 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_7, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_8 = CompType::SNormF32;

  // expected-error@+3 {{13 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_8, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_9 = CompType::UNormF32;

  // expected-error@+3 {{14 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_9, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_10 = CompType::SNormF64;

  // expected-error@+3 {{15 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_10, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_11 = CompType::UNormF64;

  // expected-error@+3 {{16 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_11, matrix_layout,
                                  matrix_stride); 

  const uint matrix_interpretation_12 = CompType::PackedS8x32;

  // expected-error@+3 {{17 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_12, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_13 = CompType::PackedU8x32;

  // expected-error@+3 {{18 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_13, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_14 = 23;

  // expected-error@+3 {{23 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_14, matrix_layout,
                                  matrix_stride);

  const uint matrix_interpretation_15 = 100;

  // expected-error@+3 {{100 is an invalid Memory Interpretation value}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation_15, matrix_layout,
                                  matrix_stride);                   
                              
}

// Check for matrix layout is not a constant parameter
void test_non_constant_matrix_layout() {

  vector<float, 4> input_vector_0 = input_vector_buffer.Load<vector<float, 4> >(0);
  vector<float, 4> input_vector_1 = input_vector_buffer.Load<vector<float, 4> >(0);
  const uint matrix_offset = 0;
  const uint matrix_interpretation = CompType::F32;
  const uint matrix_stride = 0;

  const uint matrix_layout = constants_buffer.Load<uint>(0);

  // expected-error@+3 {{'MatrixLayout' must be a constant parameter}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);
}

// Check for matrix layout is not a valid value
void test_invalid_matrix_layout() {

  vector<float, 4> input_vector_0 = input_vector_buffer.Load<vector<float, 4> >(0);
  vector<float, 4> input_vector_1 = input_vector_buffer.Load<vector<float, 4> >(0);
  const uint matrix_offset = 0;
  const uint matrix_interpretation = CompType::F32; 
  const uint matrix_stride = 0;

  const uint matrix_layout = MatLayout::RowMajor;

  // expected-error@+3 {{matrix layout for outerproductaccumulate must be 3}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);

  const uint matrix_layout_2 = MatLayout::ColumnMajor;

  // expected-error@+3 {{matrix layout for outerproductaccumulate must be 3}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout_2,
                                  matrix_stride);

  const uint matrix_layout_3 = MatLayout::MulOptimal;

  // expected-error@+3 {{matrix layout for outerproductaccumulate must be 3}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout_3,
                                  matrix_stride);                               
                                  
}

// Check for matrix stride is zero, if constant
void test_zero_matrix_stride() {

  vector<float, 4> input_vector_0 = input_vector_buffer.Load<vector<float, 4> >(0);
  vector<float, 4> input_vector_1 = input_vector_buffer.Load<vector<float, 4> >(0);
  const uint matrix_offset = 0;
  const uint matrix_interpretation = CompType::F32;
  const uint matrix_layout = MatLayout::OuterProductOptimal;

  const uint matrix_stride = 16;

  // expected-error@+4 {{for optimal matrix layout, matrix stride must be zero}}
  __builtin_OuterProductAccumulate(input_vector_0, input_vector_1,
                                  accumulate_buffer, matrix_offset,
                                  matrix_interpretation, matrix_layout,
                                  matrix_stride);
}
