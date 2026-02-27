// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;

  // CHECK: call i32 @dx.op.linAlgMatrixGetElement.i32.mC4M5N4U1S2(i32 -2147483630, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, i32 0)  ; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  uint elem1;
  __builtin_LinAlg_MatrixGetElement(elem1, mat, 0);
  // CHECK: call float @dx.op.linAlgMatrixGetElement.f32.mC4M5N4U1S2(i32 -2147483630, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, i32 1)  ; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  float elem2;
  __builtin_LinAlg_MatrixGetElement(elem2, mat, 1);
}
