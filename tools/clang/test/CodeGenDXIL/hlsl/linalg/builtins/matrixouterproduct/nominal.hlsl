// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  float4 lhs = {1,2,3,4};
  float4 rhs = {4,3,2,1};
  // CHECK: call %dx.types.LinAlgMatrixC2M2N2U2S2 @dx.op.linAlgMatrixOuterProduct.mC2M2N2U2S2.v4f32.v4f32(i32 -2147483619, <4 x float> {{.*}}, <4 x float> {{.*}})  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(2, 2, 2, 2, 2)]] mat;
  __builtin_LinAlg_MatrixOuterProduct(mat, lhs, rhs);
}
