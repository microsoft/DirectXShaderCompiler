// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC4M5N4U1S2(i32 -2147483631, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, i32 1) ; LinAlgMatrixGetCoordinate(matrix,threadLocalIndex)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  uint2 coord = __builtin_LinAlg_MatrixGetCoordinate(mat, 1);
}
