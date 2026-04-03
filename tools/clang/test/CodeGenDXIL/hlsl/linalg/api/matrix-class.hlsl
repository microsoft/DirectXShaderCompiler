// REQUIRES: dxil-1-10
// RUN: %dxc -I %hlsl_headers -T cs_6_10 %s | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

using MatrixATy = Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave>;
using MatrixBTy = Matrix<ComponentType::F32, 4, 4, MatrixUse::B, MatrixScope::Wave>;
using MatrixBTyInt = Matrix<ComponentType::I32, 4, 4, MatrixUse::B, MatrixScope::Wave>;
using MatrixAccumTy = Matrix<ComponentType::F32, 4, 4, MatrixUse::Accumulator, MatrixScope::Wave>;
using TSMatrixATy = Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Thread>;
using TSMatrixAccumTy = Matrix<ComponentType::F32, 4, 4, MatrixUse::Accumulator, MatrixScope::Thread>;

ByteAddressBuffer BAB : register(t0);
RWByteAddressBuffer RWBAB : register(u0);
groupshared float SharedArr[256];

[numthreads(4, 4, 4)]
void main(uint ID : SV_GroupID)
{

// CHECK: %[[GROUP_ID:.*]] = call i32 @dx.op.groupId.i32(i32 94, i32 0)  ; GroupId(component)

// Matrix::Splat
//
// CHECK: %[[MATA1:.*]] = call %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgFillMatrix.mC9M4N4U0S1.f32(
// CHECK-SAME: i32 -2147483636, float 1.000000e+00)
  MatrixATy MatA1 = MatrixATy::Splat(1.0f);

// CHECK: %[[MATB1:.*]] = call %dx.types.LinAlgMatrixC9M4N4U1S1 @dx.op.linAlgFillMatrix.mC9M4N4U1S1.f32(
// CHECK-SAME: i32 -2147483636, float 2.000000e+00)
  MatrixBTy MatB1;
  MatB1 = MatrixBTy::Splat(2.0f);

// Matrix::Cast
//
// CHECK: call %dx.types.LinAlgMatrixC4M4N4U1S1 @dx.op.linAlgCopyConvertMatrix.mC4M4N4U1S1.mC9M4N4U0S1(
// CHECK-SAME: i32 -2147483635, %dx.types.LinAlgMatrixC9M4N4U0S1 %[[MATA1]], i1 false)
// CHECK-SAME: ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  MatrixBTyInt MatBInt1 = MatA1.Cast<ComponentType::I32, MatrixUse::B>();

// CHECK: call %dx.types.LinAlgMatrixC4M4N4U1S1 @dx.op.linAlgCopyConvertMatrix.mC4M4N4U1S1.mC9M4N4U1S1(
// CHECK-SAME: i32 -2147483635, %dx.types.LinAlgMatrixC9M4N4U1S1 %[[MATB1]], i1 true)
// CHECK-SAME: ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  MatrixBTyInt MatBInt2;
  MatBInt2 = MatB1.Cast<ComponentType::I32, MatrixUse::B, true>();

// Matrix::Load from ByteAddressBuffer
//
// CHECK: %[[MATA2:.*]] = call %dx.types.LinAlgMatrixC9M4N4U0S1
// CHECK-SAME: @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S1(i32 -2147483634,
// CHECK-SAME: %dx.types.Handle %{{[0-9]+}}, i32 0, i32 16, i32 1, i32 4)
// CHECK-SAME: ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  MatrixATy MatA2 = MatrixATy::Load(BAB, 0, 16, MatrixLayoutEnum::ColMajor);

// Matrix::Load from RWByteAddressBuffer
//
// CHECK: %[[MATB2:.*]] = call %dx.types.LinAlgMatrixC9M4N4U1S1
// CHECK-SAME: @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U1S1(i32 -2147483634,
// CHECK-SAME: %dx.types.Handle %{{[0-9]+}}, i32 256, i32 16, i32 1, i32 4)
// CHECK-SAME: ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)  
  MatrixBTy MatB2;
  MatB2 = MatrixBTy::Load(RWBAB, 256, 16, MatrixLayoutEnum::ColMajor);

// Matrix::Load from groupshared memory
//
// CHECK: %[[MATB3:.*]] = call %dx.types.LinAlgMatrixC9M4N4U1S1
// CHECK-SAME: @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.f32(i32 -2147483633,
// CHECK-SAME: float addrspace(3)* getelementptr inbounds ([256 x float],
// CHECK-SAME: [256 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0),
// CHECK-SAME: i32 0, i32 16, i32 1)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)
  MatrixBTy MatB3 = MatrixBTy::Load(SharedArr, 0, 16, MatrixLayoutEnum::ColMajor);

// Matrix::Length
//
// CHECK: call i32 @dx.op.linAlgMatrixLength.mC9M4N4U0S1(i32 -2147483632,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U0S1 %[[MATA1]])  ; LinAlgMatrixLength
  uint len = MatA1.Length();

// Matrix::GetCoordinate
//
// CHECK: call <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC9M4N4U1S1(i32 -2147483631,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U1S1 %[[MATB1]], i32 %[[GROUP_ID]]) 
// CHECK-SAME:; LinAlgMatrixGetCoordinate(matrix,threadLocalIndex)
  uint2 coord = MatB1.GetCoordinate(ID);

// Matrix::Get
//
// CHECK: %[[VAL:.*]] = call float @dx.op.linAlgMatrixGetElement.f32.mC9M4N4U0S1(i32 -2147483630,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U0S1 %[[MATA1]], i32 %[[GROUP_ID]])
// CHECK-SAME:; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  float val = MatA1.Get(ID);

// Matrix::Set
//
// CHECK: %[[MATB1_2:.*]] = call %dx.types.LinAlgMatrixC9M4N4U1S1
// CHECK-SAME: @dx.op.linAlgMatrixSetElement.mC9M4N4U1S1.mC9M4N4U1S1.f32(
// CHECK-SAME: i32 -2147483629, %dx.types.LinAlgMatrixC9M4N4U1S1 %[[MATB1]],
// CHECK-SAME: i32 %[[GROUP_ID]], float %[[VAL]])  ; LinAlgMatrixSetElement(matrix,threadLocalIndex,value)
  MatB1.Set(ID, val);

// Matrix::Store to resource descriptor
//
// CHECK: call void @dx.op.linAlgMatrixStoreToDescriptor.mC9M4N4U1S1(i32 -2147483628,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U1S1 %[[MATB1_2]], %dx.types.Handle %{{[0-9]+}},
// CHECK-SAME: i32 256, i32 16, i32 1, i32 4)  ;
// CHECK-SAME: LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout,align)
  MatB1.Store(RWBAB, 256, 16, MatrixLayoutEnum::ColMajor);

// Matrix::Store to groupshared memory
//
// CHECK: call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.f32(i32 -2147483627,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U1S1 %[[MATB2]], float addrspace(3)* getelementptr inbounds
 // CHECK-SAME: ([256 x float], [256 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0),
// CHECK-SAME: i32 0, i32 16, i32 1)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)
  MatB2.Store(SharedArr, 0, 16, MatrixLayoutEnum::ColMajor);

// CHECK: %[[ACCUM0:.*]] = call %dx.types.LinAlgMatrixC9M4N4U2S1 @dx.op.linAlgFillMatrix.mC9M4N4U2S1.f32(
// CHECK-SAME: i32 -2147483636, float 1.400000e+01)  ; LinAlgFillMatrix(value)
  MatrixAccumTy AccMat1 = MatrixAccumTy::Splat(14.0f);

// Matrix::InterlockedAccumulate to resource descriptor
//
// CHECK: call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC9M4N4U2S1(i32 -2147483621,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U2S1 %[[ACCUM0]], %dx.types.Handle %{{[0-9]+}}, i32 0, i32 16, i32 1, i32 4)
// CHECK-SAME: ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout,align)
  AccMat1.InterlockedAccumulate(RWBAB, 0, 16, MatrixLayoutEnum::ColMajor);

// Matrix::InterlockedAccumulate to groupshared memory
//
// CHECK: call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32(i32 -2147483620,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U2S1 %[[ACCUM0]],
// CHECK-SAME: float addrspace(3)* getelementptr inbounds ([256 x float],
// CHECK-SAME: [256 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 0, i32 16, i32 1)
// CHECK-SAME: ; LinAlgMatrixAccumulateToMemory(matrix,memory,offset,stride,layout)
  AccMat1.InterlockedAccumulate(SharedArr, 0, 16, MatrixLayoutEnum::ColMajor);

// Matrix::Accumulate
//
// CHECK: %[[ACCUM1:.*]] = call %dx.types.LinAlgMatrixC9M4N4U2S1 @dx.op.linAlgFillMatrix.mC9M4N4U2S1.f32(
// CHECK-SAME: i32 -2147483636, float 0.000000e+00)  ; LinAlgFillMatrix(value)
  MatrixAccumTy AccMat2 = MatrixAccumTy::Splat(0.0f);

// CHECK: %[[ACCUM2:.*]] = call %dx.types.LinAlgMatrixC9M4N4U2S1
// CHECK-SAME: @dx.op.linAlgMatrixAccumulate.mC9M4N4U2S1.mC9M4N4U2S1.mC9M4N4U0S1(i32 -2147483624,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U2S1 %[[ACCUM1]],
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U0S1 %[[MATA2]])  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  AccMat2.Accumulate(MatA2);

// CHECK: %[[ACCUM3:.*]] = call %dx.types.LinAlgMatrixC9M4N4U2S1
// CHECK-SAME: @dx.op.linAlgMatrixAccumulate.mC9M4N4U2S1.mC9M4N4U2S1.mC9M4N4U1S1(i32 -2147483624,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U2S1 %[[ACCUM2]],
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U1S1 %[[MATB2]])
// CHECK-SAME: ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  AccMat2.Accumulate(MatB2);
 
// Matrix::MultiplyAccumulate
//
// CHECK: %[[ACCUM4:.*]] = call %dx.types.LinAlgMatrixC9M4N4U2S1
// CHECK-SAME: @dx.op.linAlgMatrixMultiplyAccumulate.mC9M4N4U2S1.mC9M4N4U0S1.mC9M4N4U1S1.mC9M4N4U2S1(i32 -2147483637,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U0S1 %[[MATA1]],
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U1S1 %[[MATB1_2]],
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U2S1 %[[ACCUM3]])
// CHECK-SAME: ; LinAlgMatrixMultiplyAccumulate(matrixA,matrixB,matrixC)
  AccMat2.MultiplyAccumulate(MatA1, MatB1);

// Matrix::Load for thread-scope matrix
//
// CHECK: %[[TSMATA:.*]] = call %dx.types.LinAlgMatrixC9M4N4U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S0(
// CHECK-SAME: i32 -2147483634, %dx.types.Handle %{{[0-9]+}}, i32 0, i32 16, i32 1, i32 4) 
// CHECK-SAME: ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  TSMatrixATy TSMatA = TSMatrixATy::Load<MatrixLayoutEnum::ColMajor>(BAB, 0, 16);

// Matrix::InterlockedAccumulate for thread-scope matrix
//
// CHECK: %[[TSACCUM:.*]] = call %dx.types.LinAlgMatrixC9M4N4U2S0 @dx.op.linAlgMatrixOuterProduct.mC9M4N4U2S0.v4f32.v4f32
// CHECK: call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC9M4N4U2S0(i32 -2147483621,
// CHECK-SAME: %dx.types.LinAlgMatrixC9M4N4U2S0 %[[TSACCUM]], %dx.types.Handle %{{[0-9]+}}, i32 0, i32 16, i32 1, i32 4)
// CHECK-SAME: ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout,align)
  vector<float, 4> vec1 = 1.0f;
  vector<float, 4> vec2 = 2.0f;
  TSMatrixAccumTy TSMatAccum = OuterProduct<ComponentType::F32>(vec1, vec2);
  TSMatAccum.InterlockedAccumulate(RWBAB, 0, 16, MatrixLayoutEnum::ColMajor);

// CHECK: call i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32 -2147483626)  ; LinAlgMatrixQueryAccumulatorLayout()
  MatrixUseEnum layout = AccumulatorLayout();
}
