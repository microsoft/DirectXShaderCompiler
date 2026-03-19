// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call i32 @dx.op.linAlgMatrixLength.mC4M5N4U1S2(i32 -2147483632, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}})  ; LinAlgMatrixLength(matrix)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  uint len = __builtin_LinAlg_MatrixLength(mat);
}
