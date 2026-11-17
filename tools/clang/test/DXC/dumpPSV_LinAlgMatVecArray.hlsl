// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -enable-16bit-types %s -Fo %t
// RUN: %dxa %t -dumppsv | FileCheck %s --check-prefix=ROW
// RUN: %dxc -T cs_6_10 -enable-16bit-types -DSECOND_LAYOUT=MulOptimalTranspose %s -Fo %t
// RUN: %dxa %t -dumppsv | FileCheck %s --check-prefix=MIXED
// RUN: %dxc -T lib_6_10 -enable-16bit-types %s -Fo %t
// RUN: %dxa %t -dumprdat | FileCheck %s --check-prefix=RDAT

#include <dx/linalg.h>
using namespace dx::linalg;

#ifndef SECOND_LAYOUT
#define SECOND_LAYOUT RowMajor
#endif

using HalfA =
    Matrix<ComponentType::F16, 4, 4, MatrixUse::A, MatrixScope::Thread>;

ByteAddressBuffer Input;
RWStructuredBuffer<vector<half, 4> > Output;

[shader("compute")]
[numthreads(4, 1, 1)]
void main(uint I : SV_GroupIndex) {
  const uint SecondStride =
      MatrixLayout::SECOND_LAYOUT == MatrixLayout::RowMajor ? 16 : 0;
  HalfA M[2] = {
      HalfA::Load<MatrixLayout::RowMajor>(Input, 0, 16),
      HalfA::Load<MatrixLayout::SECOND_LAYOUT>(Input, 64, SecondStride)};
  Output[I] = Multiply<half>(M[I & 1], (vector<half, 4>)1.0h);
}

// ROW: ThreadMatrixVectorMultiplyCount: 1
// ROW: ThreadMatrixVectorMultiply[0]: ResultType=8, MatrixType=8, VectorInputType=8, Flags=2
// MIXED: ThreadMatrixVectorMultiply[0]: ResultType=8, MatrixType=8, VectorInputType=8, Flags=3
// RDAT: LinAlgThreadMatrixVectorMultiplyTable[1]
// RDAT: Flags: (MatrixNonMulOptimalLayout)
