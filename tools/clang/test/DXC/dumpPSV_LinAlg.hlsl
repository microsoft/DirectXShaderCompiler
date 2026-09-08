// REQUIRES: dxil-1-10
// RUN: %dxc -enable-16bit-types -E main -T cs_6_10 %s -Fo %t
// RUN: %dxa %t -dumppsv | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

ByteAddressBuffer Input : register(t0);
RWByteAddressBuffer Output : register(u0);
RWStructuredBuffer<vector<half, 8> > VectorOutput : register(u1);
groupshared uint8_t4_packed SharedOutput[64];

using ThreadA =
    Matrix<ComponentType::F16, 8, 4, MatrixUse::A, MatrixScope::Thread>;
using WaveA =
    Matrix<ComponentType::F16, 3, 4, MatrixUse::A, MatrixScope::Wave>;
using WaveB =
    Matrix<ComponentType::I32, 4, 5, MatrixUse::B, MatrixScope::Wave>;
using WaveAccumulator =
    Matrix<ComponentType::F32, 3, 5, MatrixUse::Accumulator, MatrixScope::Wave>;
using ThreadAccumulator = Matrix<ComponentType::F32, 4, 4,
                                 MatrixUse::Accumulator, MatrixScope::Thread>;

[numthreads(4, 4, 1)]
void main(uint Index : SV_GroupIndex) {
  ThreadA TA =
      ThreadA::Load<MatrixLayout::MulOptimalTranspose>(Input, 0, 0);
  VectorOutput[Index] = Multiply<half>(TA, (vector<half, 4>)1.0h);

  WaveA A = WaveA::Splat(1.0h);
  WaveB B = WaveB::Splat(2);
  WaveAccumulator C = Multiply<ComponentType::F32>(A, B);
  C.Store(Output, 0, 20, MatrixLayout::RowMajor);
  C.InterlockedAccumulate<ComponentType::I32>(
      SharedOutput, 0, 16, MatrixLayout::RowMajor);

  ThreadAccumulator Outer =
      OuterProduct<ComponentType::F32>((float4)1.0f, (float4)2.0f);
  Outer.InterlockedAccumulate(Output, 256);
  InterlockedAccumulate(Output, 512, (int4)Index);
}

// CHECK: LinAlgRuntimeInfoPresent: true
// CHECK: PSVLinAlgRuntimeInfo:
// CHECK: MatrixOperationShapeCount:
// CHECK: MatrixConstructionCount:
// CHECK: ThreadMatrixVectorMultiplyCount: 1
// CHECK: WaveMatrixMultiplyCount: 1
// CHECK: ThreadGroupMatrixMultiplyCount: 0
// CHECK: OuterProductCount: 1
// CHECK: AccumulateStoreCount: 2
// CHECK: ThreadMatrixVectorMultiply[0]: ResultType=8, MatrixType=8, VectorInputType=8, Flags=1
// CHECK: WaveMatrixMultiply[0]: AccumulatorType=9, MatrixAType=8, MatrixBType=4, Shapes=[(3,5,4)]
// CHECK: OuterProduct[0]: ResultType=9, VectorInputType=9
// CHECK-DAG: AccumulateStore[{{[0-9]+}}]: AccumulatorType=9, Flags=1
// CHECK-DAG: AccumulateStore[{{[0-9]+}}]: AccumulatorType=4, Flags=3
