// REQUIRES: dxil-1-10
// RUN: %dxc -enable-16bit-types -E main -T cs_6_10 %s -Fo %t
// RUN: %dxa %t -dumppsv | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

RWByteAddressBuffer Output : register(u0);

using WaveA0 =
    Matrix<ComponentType::F16, 2, 4, MatrixUse::A, MatrixScope::Wave>;
using WaveB0 =
    Matrix<ComponentType::I32, 4, 4, MatrixUse::B, MatrixScope::Wave>;
using WaveAccumulator0 = Matrix<ComponentType::F32, 2, 4,
                                MatrixUse::Accumulator, MatrixScope::Wave>;
using WaveA1 =
    Matrix<ComponentType::F16, 5, 7, MatrixUse::A, MatrixScope::Wave>;
using WaveB1 =
    Matrix<ComponentType::I32, 7, 6, MatrixUse::B, MatrixScope::Wave>;
using WaveAccumulator1 = Matrix<ComponentType::F32, 5, 6,
                                MatrixUse::Accumulator, MatrixScope::Wave>;

using GroupA =
    Matrix<ComponentType::I32, 3, 4, MatrixUse::A, MatrixScope::ThreadGroup>;
using GroupB =
    Matrix<ComponentType::U32, 4, 5, MatrixUse::B, MatrixScope::ThreadGroup>;
using GroupAccumulator =
    Matrix<ComponentType::I32, 3, 5, MatrixUse::Accumulator,
           MatrixScope::ThreadGroup>;

[numthreads(4, 4, 1)]
void main() {
  WaveA0 A0 = WaveA0::Splat(1.0h);
  WaveB0 B0 = WaveB0::Splat(2);
  WaveAccumulator0 C0 = Multiply<ComponentType::F32>(A0, B0);
  C0.Store(Output, 0, 16, MatrixLayout::RowMajor);

  WaveA1 A1 = WaveA1::Splat(3.0h);
  WaveB1 B1 = WaveB1::Splat(4);
  WaveAccumulator1 C1 = WaveAccumulator1::Splat(5.0f);
  C1.MultiplyAccumulate(A1, B1);
  C1.Store(Output, 128, 24, MatrixLayout::RowMajor);

  GroupA GA = GroupA::Splat(6);
  GroupB GB = GroupB::Splat(7u);
  GroupAccumulator GC = Multiply<ComponentType::I32>(GA, GB);
  GC.Store(Output, 256, 20, MatrixLayout::RowMajor);
}

// CHECK: LinAlgRuntimeInfoPresent: true
// CHECK: PSVLinAlgRuntimeInfo:
// CHECK-NEXT: MatrixOperationShapeCount: 12
// CHECK-NEXT: MatrixConstructionCount: 6
// CHECK-NEXT: ThreadMatrixVectorMultiplyCount: 0
// CHECK-NEXT: WaveMatrixMultiplyCount: 1
// CHECK-NEXT: ThreadGroupMatrixMultiplyCount: 1
// CHECK-NEXT: OuterProductCount: 0
// CHECK-NEXT: AccumulateStoreCount: 0
// CHECK-NEXT: MatrixConstruction[0]: MatrixType=4, Shapes=[(3,0,4)]
// CHECK-NEXT: MatrixConstruction[1]: MatrixType=4, Shapes=[(0,4,4), (0,6,7)]
// CHECK-NEXT: MatrixConstruction[2]: MatrixType=4, Shapes=[(3,5,0)]
// CHECK-NEXT: MatrixConstruction[3]: MatrixType=5, Shapes=[(0,5,4)]
// CHECK-NEXT: MatrixConstruction[4]: MatrixType=8, Shapes=[(2,0,4), (5,0,7)]
// CHECK-NEXT: MatrixConstruction[5]: MatrixType=9, Shapes=[(2,4,0), (5,6,0)]
// CHECK-NEXT: WaveMatrixMultiply[0]: AccumulatorType=9, MatrixAType=8, MatrixBType=4, Shapes=[(2,4,4), (5,6,7)]
// CHECK-NEXT: ThreadGroupMatrixMultiply[0]: AccumulatorType=4, MatrixAType=4, MatrixBType=5, Shapes=[(3,5,4)]
