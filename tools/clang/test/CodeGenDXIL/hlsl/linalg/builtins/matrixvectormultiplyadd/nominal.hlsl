// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 3, 4, 0, 0)]] mat;
  float4 vec = {1,2,3,4};
  float4 result;

  // CHECK: call <4 x float> @dx.op.linAlgMatVecMulAdd.v4f32.mC5M3N4U0S0.v4f32.v4f32(i32 -2147483622, %dx.types.LinAlgMatrixC5M3N4U0S0 {{.*}}, <4 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, i32 1, <4 x float> {{.*}}, i32 0)  ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)
  __builtin_LinAlg_MatrixVectorMultiplyAdd(result, mat, vec, 1, result, 0);
}
