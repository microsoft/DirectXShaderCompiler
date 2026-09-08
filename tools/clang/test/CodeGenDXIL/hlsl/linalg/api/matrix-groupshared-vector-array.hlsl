// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 %s | FileCheck %s

// dx::linalg::Matrix's groupshared memory methods accept arrays of vectors of
// the matrix component type, in addition to arrays of scalars. The DXIL
// operations are overloaded on the array element type, so the memory operand
// is a pointer to a vector.

#include <dx/linalg.h>
using namespace dx::linalg;

using MatrixBTy = Matrix<ComponentType::F32, 4, 4, MatrixUse::B, MatrixScope::Wave>;
using MatrixAccumTy = Matrix<ComponentType::F32, 4, 4, MatrixUse::Accumulator, MatrixScope::Wave>;

// CHECK: @"\01?SharedVecArr@@3PAV?$vector@M$03@@A" = external addrspace(3) global [64 x <4 x float>]
groupshared float4 SharedVecArr[64];
// CHECK: @"\01?PackedVecArr@@3PAV?$vector@$ui8_4pk@$01@@A" = external addrspace(3) global [64 x <2 x i32>]
groupshared vector<uint8_t4_packed, 2> PackedVecArr[64];

[numthreads(4, 4, 4)]
void main() {

// Matrix::Load from an array of vectors
//
// CHECK: %[[MATB:.*]] = call %dx.types.LinAlgMatrixC9M4N4U1S1
// CHECK-SAME: @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.v4f32(i32 -2147483633,
// CHECK-SAME: <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>],
// CHECK-SAME: [64 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0),
// CHECK-SAME: i32 0, i32 16, i32 1)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)
  MatrixBTy MatB = MatrixBTy::Load(SharedVecArr, 0, 16, MatrixLayoutEnum::ColMajor);

// Matrix::Store to an array of vectors
//
// CHECK: call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.v4f32(i32 -2147483627,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U1S1 %[[MATB]],
// CHECK-SAME: <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>],
// CHECK-SAME: [64 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0),
// CHECK-SAME: i32 0, i32 16, i32 1)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)
  MatB.Store(SharedVecArr, 0, 16, MatrixLayoutEnum::ColMajor);

// Matrix::Load from an array of packed vectors
//
// CHECK: %[[MATP:.*]] = call %dx.types.LinAlgMatrixC9M4N4U1S1
// CHECK-SAME: @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.v2i32(i32 -2147483633,
// CHECK-SAME: <2 x i32> addrspace(3)* getelementptr inbounds ([64 x <2 x i32>],
// CHECK-SAME: [64 x <2 x i32>] addrspace(3)* @"\01?PackedVecArr@@3PAV?$vector@$ui8_4pk@$01@@A", i32 0, i32 0),
// CHECK-SAME: i32 0, i32 16, i32 1)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)
  MatrixBTy MatP = MatrixBTy::Load(PackedVecArr, 0, 16, MatrixLayoutEnum::ColMajor);

// Matrix::Store to an array of packed vectors
//
// CHECK: call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.v2i32(i32 -2147483627,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U1S1 %[[MATP]],
// CHECK-SAME: <2 x i32> addrspace(3)* getelementptr inbounds ([64 x <2 x i32>],
// CHECK-SAME: [64 x <2 x i32>] addrspace(3)* @"\01?PackedVecArr@@3PAV?$vector@$ui8_4pk@$01@@A", i32 0, i32 0),
// CHECK-SAME: i32 0, i32 16, i32 1)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)
  MatP.Store(PackedVecArr, 0, 16, MatrixLayoutEnum::ColMajor);

  MatrixAccumTy Acc = MatrixAccumTy::Splat(0);

// Matrix::InterlockedAccumulate to an array of vectors
//
// CHECK: call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.v4f32(i32 -2147483620,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U2S1 %[[ACC:[0-9]+]],
// CHECK-SAME: <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>],
// CHECK-SAME: [64 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0),
// CHECK-SAME: i32 9, i32 0, i32 16, i32 1)
// CHECK-SAME: ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)
  Acc.InterlockedAccumulate(SharedVecArr, 0, 16, MatrixLayoutEnum::ColMajor);

// Matrix::InterlockedAccumulate to an array of packed vectors
//
// CHECK: call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.v2i32(i32 -2147483620,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U2S1 %[[ACC]],
// CHECK-SAME: <2 x i32> addrspace(3)* getelementptr inbounds ([64 x <2 x i32>],
// CHECK-SAME: [64 x <2 x i32>] addrspace(3)* @"\01?PackedVecArr@@3PAV?$vector@$ui8_4pk@$01@@A", i32 0, i32 0),
// CHECK-SAME: i32 4, i32 0, i32 16, i32 1)
// CHECK-SAME: ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)
  Acc.InterlockedAccumulate<ComponentType::I32>(PackedVecArr, 0, 16, MatrixLayoutEnum::ColMajor);
}
