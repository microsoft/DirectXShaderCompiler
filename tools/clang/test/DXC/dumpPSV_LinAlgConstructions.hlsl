// REQUIRES: dxil-1-10
// RUN: %dxc -E main -T cs_6_10 %s -Fo %t
// RUN: %dxa %t -dumppsv | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

RWByteAddressBuffer Output : register(u0);

using WaveA0 =
    Matrix<ComponentType::F32, 2, 4, MatrixUse::A, MatrixScope::Wave>;
using GroupA1 =
    Matrix<ComponentType::F32, 4, 5, MatrixUse::A, MatrixScope::ThreadGroup>;
using WaveB0 =
    Matrix<ComponentType::F32, 4, 4, MatrixUse::B, MatrixScope::Wave>;
using GroupB1 =
    Matrix<ComponentType::F32, 5, 6, MatrixUse::B, MatrixScope::ThreadGroup>;
using WaveAccumulator0 = Matrix<ComponentType::F32, 2, 4,
                                MatrixUse::Accumulator, MatrixScope::Wave>;
using GroupAccumulator1 =
    Matrix<ComponentType::F32, 4, 6, MatrixUse::Accumulator,
           MatrixScope::ThreadGroup>;

[numthreads(4, 4, 1)]
void main() {
  WaveA0::Splat(1.0f).Store(Output, 0, 16, MatrixLayout::RowMajor);
  GroupA1::Splat(2.0f).Store(Output, 64, 20, MatrixLayout::RowMajor);
  WaveB0::Splat(3.0f).Store(Output, 128, 16, MatrixLayout::RowMajor);
  GroupB1::Splat(4.0f).Store(Output, 192, 24, MatrixLayout::RowMajor);
  WaveAccumulator0::Splat(5.0f).Store(Output, 256, 16,
                                     MatrixLayout::RowMajor);
  GroupAccumulator1::Splat(6.0f).Store(Output, 320, 24,
                                      MatrixLayout::RowMajor);
}

// CHECK: LinAlgRuntimeInfoPresent: true
// CHECK: PSVLinAlgRuntimeInfo:
// CHECK-NEXT: MatrixOperationShapeCount: 6
// CHECK-NEXT: MatrixConstructionCount: 3
// CHECK-NEXT: ThreadMatrixVectorMultiplyCount: 0
// CHECK-NEXT: WaveMatrixMultiplyCount: 0
// CHECK-NEXT: ThreadGroupMatrixMultiplyCount: 0
// CHECK-NEXT: OuterProductCount: 0
// CHECK-NEXT: AccumulateStoreCount: 0
// CHECK-NEXT: MatrixConstruction[0]: MatrixType=9, Shapes=[(2,0,4), (4,0,5)]
// CHECK-NEXT: MatrixConstruction[1]: MatrixType=9, Shapes=[(0,4,4), (0,6,5)]
// CHECK-NEXT: MatrixConstruction[2]: MatrixType=9, Shapes=[(2,4,0), (4,6,0)]
