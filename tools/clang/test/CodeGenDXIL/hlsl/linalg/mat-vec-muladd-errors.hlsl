// RUN: not %dxc -T lib_6_9 %s 2>&1 | FileCheck %s

#include "linalg.h"

ByteAddressBuffer Buf;

vector<float, 128> MixUpVectorAndMatrixArguments(vector<float, 128> Input) {
  using namespace dx::linalg;

  MatrixRef<1, 128, 128, MATRIX_LAYOUT_MUL_OPTIMAL> Matrix = {
      Buf, 0, 0};

  // clang-format off
  // CHECK: error: no matching function for call to 'MulAdd'
  // CHECK: note: candidate template ignored: could not match 'MatrixRefImpl' against 'InterpretedVector'
  // clang-format on
  return MulAdd<float>(MakeInterpretedVector<1>(Input), Matrix, MakeInterpretedVector<1>(Input));
}
