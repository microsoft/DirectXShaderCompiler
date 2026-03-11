// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -E main -fcgl %s | FileCheck %s --check-prefix=CHECK2

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 3, 4, 0, 0)]] mat;
  float4 vec = {1,2,3,4};
  float4 result;

  // CHECK: call <4 x float> @dx.op.linAlgMatVecMulAdd.v4f32.mC5M3N4U0S0.v4f32.v4f32(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC5M3N4U0S0 {{.*}}, <4 x float> <float 1.000000e+00, 
  // CHECK-SAME: float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, i32 1, <4 x float> {{.*}}, i32 0)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)

  // CHECK2:  call void @"dx.hl.op..void (i32, <4 x float>*, %dx.types.LinAlgMatrixC5M3N4U0S0, <4 x float>, i32,
  // CHECK2-SAME: <4 x float>, i32)"(i32 423, <4 x float>* %result, %dx.types.LinAlgMatrixC5M3N4U0S0 %6,
  // CHECK2-SAME: <4 x float> %4, i32 1, <4 x float> %3, i32 0)
  __builtin_LinAlg_MatrixVectorMultiplyAdd(result, mat, vec, 1, result, 0);
}
