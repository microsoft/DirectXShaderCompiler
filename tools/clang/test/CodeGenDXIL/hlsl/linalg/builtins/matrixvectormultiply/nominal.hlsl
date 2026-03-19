// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  float4 vec = {1,2,3,4};
  float4 result;

  // CHECK: call <4 x float> @dx.op.linAlgMatVecMul.v4f32.mC4M5N4U1S2.v4f32(i32 -2147483623, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, <4 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, i32 1)  ; LinAlgMatVecMul(matrix,inputVector,interpretation)
  __builtin_LinAlg_MatrixVectorMultiply(result, mat, vec, 1);
}
