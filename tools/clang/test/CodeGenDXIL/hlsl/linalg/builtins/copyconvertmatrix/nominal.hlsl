// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call %dx.types.LinAlgMatrixC4M5N4U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M5N4U1S2.mC2M5N4U1S2(i32 -2147483635, %dx.types.LinAlgMatrixC2M5N4U1S2 {{.*}}, i1 false)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(2, 5, 4, 1, 2)]] mat1;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat2;
  __builtin_LinAlg_CopyConvertMatrix(mat2, mat1, false);
}
