// REQUIRES: dxil-1-10
// RUN: %dxc -enable-16bit-types -E main -T cs_6_10 %s -Fo %t
// RUN: %dxa %t -dumppsv | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

RWByteAddressBuffer Output : register(u0);
groupshared half SharedHalf[64];
groupshared float SharedFloat[64];

using ThreadHalfAccumulator = Matrix<ComponentType::F16, 2, 3,
                                     MatrixUse::Accumulator,
                                     MatrixScope::Thread>;
using ThreadFloatAccumulator = Matrix<ComponentType::F32, 3, 2,
                                      MatrixUse::Accumulator,
                                      MatrixScope::Thread>;
using ThreadIntAccumulator = Matrix<ComponentType::I32, 4, 4,
                                    MatrixUse::Accumulator,
                                    MatrixScope::Thread>;
using WaveHalfAccumulator = Matrix<ComponentType::F16, 2, 2,
                                   MatrixUse::Accumulator, MatrixScope::Wave>;
using WaveFloatAccumulator = Matrix<ComponentType::F32, 2, 2,
                                    MatrixUse::Accumulator, MatrixScope::Wave>;

[numthreads(4, 4, 1)]
void main(uint Index : SV_GroupIndex) {
  ThreadHalfAccumulator HalfOuter =
      OuterProduct<ComponentType::F16>((vector<half, 2>)1.0h,
                                       (vector<half, 3>)2.0h);
  HalfOuter.InterlockedAccumulate(Output, 0);

  ThreadFloatAccumulator FloatOuter =
      OuterProduct<ComponentType::F32>((vector<half, 3>)3.0h,
                                       (vector<half, 2>)4.0h);
  FloatOuter.InterlockedAccumulate(Output, 64);

  ThreadIntAccumulator IntOuter =
      OuterProduct<ComponentType::I32>((int4)5, (int4)6);
  IntOuter.InterlockedAccumulate(Output, 128);

  WaveHalfAccumulator WaveHalf = WaveHalfAccumulator::Splat(7.0h);
  WaveHalf.InterlockedAccumulate(Output, 192, 4, MatrixLayout::RowMajor);
  WaveHalf.InterlockedAccumulate(SharedHalf, 0, 8, MatrixLayout::RowMajor);

  WaveFloatAccumulator WaveFloat = WaveFloatAccumulator::Splat(8.0f);
  WaveFloat.InterlockedAccumulate(SharedFloat, 0, 4, MatrixLayout::RowMajor);

  InterlockedAccumulate(Output, 256, (vector<int64_t, 2>)Index);
}

// CHECK: LinAlgRuntimeInfoPresent: true
// CHECK: PSVLinAlgRuntimeInfo:
// CHECK-NEXT: MatrixOperationShapeCount: 1
// CHECK-NEXT: MatrixConstructionCount: 2
// CHECK-NEXT: ThreadMatrixVectorMultiplyCount: 0
// CHECK-NEXT: WaveMatrixMultiplyCount: 0
// CHECK-NEXT: ThreadGroupMatrixMultiplyCount: 0
// CHECK-NEXT: OuterProductCount: 3
// CHECK-NEXT: AccumulateStoreCount: 5
// CHECK-NEXT: MatrixConstruction[0]: MatrixType=8, Shapes=[(2,2,0)]
// CHECK-NEXT: MatrixConstruction[1]: MatrixType=9, Shapes=[(2,2,0)]
// CHECK-NEXT: OuterProduct[0]: ResultType=8, VectorInputType=8
// CHECK-NEXT: OuterProduct[1]: ResultType=9, VectorInputType=8
// CHECK-NEXT: OuterProduct[2]: ResultType=4, VectorInputType=4
// CHECK-NEXT: AccumulateStore[0]: AccumulatorType=8, Flags=1
// CHECK-NEXT: AccumulateStore[1]: AccumulatorType=9, Flags=1
// CHECK-NEXT: AccumulateStore[2]: AccumulatorType=4, Flags=1
// CHECK-NEXT: AccumulateStore[3]: AccumulatorType=0, Flags=2
// CHECK-NEXT: AccumulateStore[4]: AccumulatorType=6, Flags=1
