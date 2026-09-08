// REQUIRES: dxil-1-10
// RUN: %dxc -enable-16bit-types -E main -T cs_6_10 %s -Fo %t
// RUN: %dxa %t -dumppsv | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

ByteAddressBuffer Input : register(t0);
RWStructuredBuffer<vector<half, 4> > HalfOutput : register(u0);
RWStructuredBuffer<float4> FloatOutput : register(u1);
RWStructuredBuffer<int4> IntOutput : register(u2);
RWStructuredBuffer<uint4> UintOutput : register(u3);

using HalfA =
    Matrix<ComponentType::F16, 4, 4, MatrixUse::A, MatrixScope::Thread>;
using FloatA =
    Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Thread>;
using IntA =
    Matrix<ComponentType::I32, 4, 4, MatrixUse::A, MatrixScope::Thread>;

[numthreads(4, 4, 1)]
void main(uint Index : SV_GroupIndex) {
  HalfA MulOptimal =
      HalfA::Load<MatrixLayout::MulOptimal>(Input, 0, 0);
  HalfOutput[Index] = Multiply<half>(MulOptimal, (vector<half, 4>)1.0h);

  FloatA Transposed =
      FloatA::Load<MatrixLayout::MulOptimalTranspose>(Input, 64, 0);
  FloatOutput[Index] =
      MultiplyAdd<float>(Transposed, (float4)2.0f, (float4)3.0f);

  IntA RowMajor =
      IntA::Load<MatrixLayout::RowMajor>(Input, 128, 16);
  IntOutput[Index] = Multiply<int>(RowMajor, (int4)4);

  IntA MaybeTransposed =
      IntA::Load<MatrixLayout::MulOptimalTranspose>(Input, 192, 0);
  IntA MaybeRowMajor =
      IntA::Load<MatrixLayout::RowMajor>(Input, 256, 16);
  IntA Selected = MaybeTransposed;
  if (Index)
    Selected = MaybeRowMajor;
  InterpretedVector<uint, 4, ComponentType::U32> UnsignedInput =
      MakeInterpretedVector<ComponentType::U32>((uint4)5);
  UintOutput[Index] = Multiply<uint>(Selected, UnsignedInput);
}

// CHECK: LinAlgRuntimeInfoPresent: true
// CHECK: PSVLinAlgRuntimeInfo:
// CHECK-NEXT: MatrixOperationShapeCount: 0
// CHECK-NEXT: MatrixConstructionCount: 0
// CHECK-NEXT: ThreadMatrixVectorMultiplyCount: 4
// CHECK-NEXT: WaveMatrixMultiplyCount: 0
// CHECK-NEXT: ThreadGroupMatrixMultiplyCount: 0
// CHECK-NEXT: OuterProductCount: 0
// CHECK-NEXT: AccumulateStoreCount: 0
// CHECK-NEXT: ThreadMatrixVectorMultiply[0]: ResultType=8, MatrixType=8, VectorInputType=8, Flags=0
// CHECK-NEXT: ThreadMatrixVectorMultiply[1]: ResultType=9, MatrixType=9, VectorInputType=9, Flags=1
// CHECK-NEXT: ThreadMatrixVectorMultiply[2]: ResultType=4, MatrixType=4, VectorInputType=4, Flags=2
// CHECK-NEXT: ThreadMatrixVectorMultiply[3]: ResultType=5, MatrixType=4, VectorInputType=5, Flags=3
