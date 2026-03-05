// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 3, 4, 0, 0)]] mat1;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 1, 1, 0, 0)]] mat2;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(2, 2, 2, 2, 2)]] mat3;

  // CHECK: call %dx.types.LinAlgMatrixC2M2N2U2S2 @dx.op.linAlgMatrixAccumulate.mC2M2N2U2S2.mC1M1N1U0S0.mC5M3N4U0S0(i32 -2147483624, %dx.types.LinAlgMatrixC1M1N1U0S0 {{.*}}, %dx.types.LinAlgMatrixC5M3N4U0S0 {{.*}})  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  __builtin_LinAlg_MatrixAccumulate(mat3, mat2, mat1);
}
