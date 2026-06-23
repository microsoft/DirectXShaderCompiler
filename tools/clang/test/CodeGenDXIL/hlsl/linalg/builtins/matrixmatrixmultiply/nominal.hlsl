// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -E main -fcgl %s | FileCheck %s --check-prefix=CHECK2

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call %dx.types.LinAlgMatrixC4M5N4U1S2 @dx.op.linAlgMatrixMultiply.mC4M5N4U1S2.mC4M5N4U1S2.mC4M5N4U1S2(i32 -2147483625,
  // CHECK-SAME: %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}) ; LinAlgMatrixMultiply(matrixA,matrixB)

  // CHECK2: call void @"dx.hl.op..void (i32, %dx.types.LinAlgMatrixC4M5N4U1S2*, %dx.types.LinAlgMatrixC4M5N4U1S2,
  // CHECK2-SAME: %dx.types.LinAlgMatrixC4M5N4U1S2)"(i32 412, %dx.types.LinAlgMatrixC4M5N4U1S2* %mat2,
  // CHECK2-SAME: %dx.types.LinAlgMatrixC4M5N4U1S2 %{{[0-9]+}}, %dx.types.LinAlgMatrixC4M5N4U1S2 %{{[0-9]+}})
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat1;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat2;
  __builtin_LinAlg_MatrixMatrixMultiply(mat2, mat1, mat1);
}
