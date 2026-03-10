// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -HV 202x -E main %s | FileCheck %s

// CHECK: @{{.*}} = external addrspace(3) global [64 x float]
groupshared float SharedArr[64];

[numthreads(4,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call void @dx.op.linAlgMatrixAccumulateToMemory.mC4M5N4U1S2.f32(i32 -2147483620, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, [64 x float] addrspace(3)* nonnull @{{.*}}, i32 1, i32 2, i32 3)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,offset,stride,layout)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  __builtin_LinAlg_MatrixAccumulateToMemory(mat, SharedArr, 1, 2, 3);
}
