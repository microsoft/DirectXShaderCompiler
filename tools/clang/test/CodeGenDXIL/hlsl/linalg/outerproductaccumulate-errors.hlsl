// RUN: not %dxc -T lib_6_9 -enable-16bit-types %s 2>&1 | FileCheck %s

RWByteAddressBuffer RWBuf;

// test for inputs of different size
export void Test4(vector<half, 128> Input1, vector<half, 64> Input2) {
  using namespace dx::linalg;

  RWMatrixRef<DATA_TYPE_FLOAT16, 128, 64, MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL, true>
      matrix = {RWBuf, 0, 0};

  // clang-format off
  // CHECK: error: no matching function for call to 'OuterProductAccumulate'
  // CHECK: note: candidate template ignored: could not match 0 against 1
  // __builtin_OuterProductAccumulate(Input1, Input2, RWBuf, 0, DATA_TYPE_FLOAT16, MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL, 0);
  // clang-format on

  OuterProductAccumulate(Input1, Input2, matrix);  
}

// now test for an error when element types differ
export void Test5(vector<int, 128> Input1, vector<uint, 128> Input2) {
  using namespace dx::linalg;

  RWMatrixRef<DATA_TYPE_FLOAT16, 128, 128, MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL, true>
      matrix = {RWBuf, 0, 0};

  // clang-format off
  // CHECK: error: no matching function for call to 'OuterProductAccumulate'
  // CHECK: note: candidate template ignored: could not match 0 against 1
  // __builtin_OuterProductAccumulate(Input1, Input2, RWBuf, 0, DATA_TYPE_FLOAT16, MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL, 0);
  // clang-format on

  OuterProductAccumulate(Input1, Input2, matrix);  
}

// now test for an error when matrix transpose parameter is true
export void Test4(vector<half, 64> Input1, vector<half, 64> Input2) {
  using namespace dx::linalg;

  RWMatrixRef<DATA_TYPE_FLOAT16, 64, 64, MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL, true>
      matrix = {RWBuf, 0, 0};

  // clang-format off
  // CHECK: error: no matching function for call to 'OuterProductAccumulate'
  // CHECK: note: candidate template ignored: could not match 0 against 1
  // __builtin_OuterProductAccumulate(Input1, Input2, RWBuf, 0, DATA_TYPE_FLOAT16, MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL, 0);
  // clang-format on

  OuterProductAccumulate(Input1, Input2, matrix);  
}
