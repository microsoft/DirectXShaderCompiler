// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -HV 202x -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -HV 202x -E main -fcgl %s | FileCheck %s --check-prefix=CHECK2

// The groupshared array may hold vectors of the matrix component type. The
// DXIL operation is overloaded on the array's element type, so the memory
// operand is a pointer to a vector.

// CHECK: @"\01?SharedArr@@3PAV?$vector@M$03@@A" = external addrspace(3) global [64 x <4 x float>]
groupshared float4 SharedArr[64];

[numthreads(4,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call void @dx.op.linAlgMatrixStoreToMemory.mC9M5N4U1S2.v4f32(i32 -2147483627,
  // CHECK-SAME: %dx.types.LinAlgMatrixC9M5N4U1S2 %{{.*}}, <4 x float> addrspace(3)* getelementptr
  // CHECK-SAME: inbounds ([64 x <4 x float>], [64 x <4 x float>] addrspace(3)*
  // CHECK-SAME: @"\01?SharedArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 128, i32 16, i32 3)
  // CHECK-SAME: ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  // CHECK2: call void @"dx.hl.op..void (i32, %dx.types.LinAlgMatrixC9M5N4U1S2, [64 x <4 x float>] addrspace(3)*,
  // CHECK2-SAME: i32, i32, i32)"(i32 410, %dx.types.LinAlgMatrixC9M5N4U1S2 %{{.*}}, [64 x <4 x float>] addrspace(3)*
  // CHECK2-SAME: @"\01?SharedArr@@3PAV?$vector@M$03@@A", i32 128, i32 16, i32 3)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(9, 5, 4, 1, 2)]] mat;
  __builtin_LinAlg_FillMatrix(mat, 1);
  __builtin_LinAlg_MatrixStoreToMemory(mat, SharedArr, 128, 16, 3);
}
