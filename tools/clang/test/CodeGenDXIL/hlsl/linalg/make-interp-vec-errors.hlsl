// RUN: not %dxc -T lib_6_9 %s 2>&1 | FileCheck %s

#include "linalg.h"

ByteAddressBuffer Buf;

export float4 Test1(vector<float, 4> Input) {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_UINT16, 4, 4, MATRIX_LAYOUT_MUL_OPTIMAL, true> Matrix = {
      Buf, 0, 0};

  // clang-format off
  // CHECK: error: no matching function for call to 'MakeInterpretedVector'
  // CHECK: note: candidate template ignored: invalid explicitly-specified argument for template parameter 'DT'
  return Mul<float>(    
      Matrix, MakeInterpretedVector<2>(Input));
  // clang-format on
}

enum DataType {
  DATA_TYPE_InvalidType = 40
};

export float4 Test2(vector<float, 4> Input) {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_UINT16, 4, 4, MATRIX_LAYOUT_MUL_OPTIMAL, true> Matrix = {
      Buf, 0, 0};

  // clang-format off
  // CHECK: error: no matching function for call to 'MakeInterpretedVector'
  // CHECK: note: candidate template ignored: invalid explicitly-specified argument for template parameter 'DT'
  return Mul<float>(    
      Matrix, MakeInterpretedVector<DATA_TYPE_InvalidType>(Input));
  // clang-format on
}

