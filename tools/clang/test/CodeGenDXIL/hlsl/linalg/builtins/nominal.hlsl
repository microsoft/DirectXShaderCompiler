// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

ByteAddressBuffer inbuf;
RWByteAddressBuffer outbuf;

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: %{{.*}} = call %dx.types.LinAlgMatrixC4M5N4U1S2 @dx.op.linAlgFillMatrix.mC4M5N4U1S2.i32(i32 -2147483636, i32 {{.*}})  ; LinAlgFillMatrix(value)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat1;
  __builtin_LinAlg_FillMatrix(mat1, 5);
  // CHECK: %{{.*}} = call %dx.types.LinAlgMatrixC5M3N4U0S0 @dx.op.linAlgFillMatrix.mC5M3N4U0S0.f32(i32 -2147483636, float {{.*}})  ; LinAlgFillMatrix(value)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 3, 4, 0, 0)]] mat2;
  __builtin_LinAlg_FillMatrix(mat2, 3.14);

  // CHECK: call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U1S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U1S2 %{{.*}}, %dx.types.Handle %{{.*}}, i32 1, i32 1, i32 0)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  __builtin_LinAlg_MatrixStoreToDescriptor(mat1, outbuf, 1, 1, 0);

  // CHECK: %{{.*}} = call %dx.types.LinAlgMatrixC1M1N1U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC1M1N1U0S0(i32 -2147483634, %dx.types.Handle %{{.*}}, i32 0, i32 0, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 1, 1, 0, 0)]] mat3;
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat3, inbuf, 0, 0, 0);

  // CHECK: call <4 x float> @dx.op.linAlgMatVecMul.v4f32.mC4M5N4U1S2.v4f32(i32 -2147483623, %dx.types.LinAlgMatrixC4M5N4U1S2 %{{.*}}, <4 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, i32 1)  ; LinAlgMatVecMul(matrix,inputVector,interpretation)
  float4 vec = {1,2,3,4};
  float4 result;
  __builtin_LinAlg_MatrixVectorMultiply(result, mat1, vec, 1);

  // CHECK: call <4 x float> @dx.op.linAlgMatVecMulAdd.v4f32.mC5M3N4U0S0.v4f32.v4f32(i32 -2147483622, %dx.types.LinAlgMatrixC5M3N4U0S0 %{{.*}}, <4 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, i32 1, <4 x float> %{{.*}}, i32 0)  ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)
  __builtin_LinAlg_MatrixVectorMultiplyAdd(result, mat2, vec, 1, result, 0);


  // CHECK: call %dx.types.LinAlgMatrixC2M2N2U2S2 @dx.op.linAlgMatrixOuterProduct.mC2M2N2U2S2.v4f32.v4f32(i32 -2147483619, <4 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, <4 x float> %{{.*}})  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(2, 2, 2, 2, 2)]] mat4;
  __builtin_LinAlg_MatrixOuterProduct(mat4, vec, result);


  // CHECK: call %dx.types.LinAlgMatrixC2M2N2U2S2 @dx.op.linAlgMatrixAccumulate.mC2M2N2U2S2.mC1M1N1U0S0.mC5M3N4U0S0(i32 -2147483624, %dx.types.LinAlgMatrixC1M1N1U0S0 %{{.*}}, %dx.types.LinAlgMatrixC5M3N4U0S0 %{{.*}})  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  __builtin_LinAlg_MatrixAccumulate(mat4, mat3, mat2);

  // CHECK: call i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32 -2147483626)  ; LinAlgMatrixQueryAccumulatorLayout()
  uint layout = __builtin_LinAlg_MatrixQueryAccumulatorLayout();

  // CHECK: call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC4M5N4U1S2(i32 -2147483621, %dx.types.LinAlgMatrixC4M5N4U1S2 %{{.*}}, %dx.types.Handle %{{.*}}, i32 5, i32 5, i32 5)  ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout)
  __builtin_LinAlg_MatrixAccumulateToDescriptor(mat1, outbuf, 5, 5, 5);

}
