// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -HV 202x -E main %s | FileCheck %s

groupshared float SharedArr[64];

void fn(groupshared float Arr[64]) {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  __builtin_LinAlg_MatrixStoreToMemory(mat, Arr, 0, 0, 0);
}

// CHECK: @{{.*}} = external addrspace(3) global [64 x float]

[numthreads(4,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call void @dx.op.linAlgMatrixStoreToMemory.mC4M5N4U1S2.f32(i32 -2147483627, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, [64 x float] addrspace(3)* nonnull @{{.*}}, i32 0, i32 0, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)
  fn(SharedArr);
}
