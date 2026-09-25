// REQUIRES: dxil-1-10
// RUN: %dxc -enable-16bit-types -T cs_6_10 %s | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

using MatrixATy =
    Matrix<ComponentType::F8_E4M3FN, 4, 8, MatrixUse::A,
           MatrixScope::Thread>;

ByteAddressBuffer Input : register(t0);
RWStructuredBuffer<vector<half, 4> > Output : register(u0);

[numthreads(1, 1, 1)]
void main(uint Index : SV_GroupIndex) {
  MatrixATy Mat =
      MatrixATy::Load<MatrixLayout::RowMajor>(Input, 0, 8);
  vector<half, 8> Vec = 10.3h;

  // CHECK: call <4 x half> @dx.op.linAlgMatVecMul.v4f16.mC21M4N8U0S0.v8f16(
  // CHECK-SAME: i1 true, <8 x half>
  // CHECK-SAME: i32 8)
  Output[Index] = Multiply<half>(Mat, Vec);
}
