// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -E main -fcgl %s | FileCheck %s --check-prefix=CHECK2

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 4, 4, 0, 0)]] mat1;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 4, 4, 0, 0)]] mat2;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 4, 4, 0, 0)]] mat3;

  __builtin_LinAlg_FillMatrix(mat1, 1);
  __builtin_LinAlg_FillMatrix(mat2, 2);

  // CHECK: call %dx.types.LinAlgMatrixC5M4N4U0S0 @dx.op.linAlgMatrixAccumulate.mC5M4N4U0S0.mC5M4N4U0S0.mC5M4N4U0S0
  // CHECK-SAME: (i32 -2147483624, %dx.types.LinAlgMatrixC5M4N4U0S0 %{{.*}}, %dx.types.LinAlgMatrixC5M4N4U0S0 %{{.*}}) ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)

  // CHECK2: call void @"dx.hl.op..void (i32, %dx.types.LinAlgMatrixC5M4N4U0S0*, %dx.types.LinAlgMatrixC5M4N4U0S0,
  // CHECK2-SAME:  %dx.types.LinAlgMatrixC5M4N4U0S0)"(i32 411, %dx.types.LinAlgMatrixC5M4N4U0S0* %mat3,
  // CHECK2-SAME: %dx.types.LinAlgMatrixC5M4N4U0S0 %{{[0-9]+}}, %dx.types.LinAlgMatrixC5M4N4U0S0 %{{[0-9]+}})
  __builtin_LinAlg_MatrixAccumulate(mat3, mat2, mat1);
}
