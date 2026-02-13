// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call %dx.types.LinAlgMatrixC4M5N4U1S2 @dx.op.linAlgMatrixMultiplyAccumulate.mC4M5N4U1S2.mC4M5N4U1S2.mC4M5N4U1S2.mC4M5N4U1S2(i32 -2147483637, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}})  ; LinAlgMatrixMultiplyAccumulate(matrixA,matrixB,matrixC)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat1;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat2;
  __builtin_LinAlg_MatrixMatrixMultiplyAccumulate(mat2, mat1, mat1, mat1);
}
