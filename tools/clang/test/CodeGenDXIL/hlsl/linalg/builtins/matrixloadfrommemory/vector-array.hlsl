// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -HV 202x -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -HV 202x -E main -fcgl %s | FileCheck %s --check-prefix=CHECK2

// The groupshared array may hold vectors of the matrix component type. The
// DXIL operation is overloaded on the array's element type, so the memory
// operand is a pointer to a vector.

// CHECK: @"\01?SharedArr@@3PAV?$vector@M$03@@A" = external addrspace(3) global [64 x <4 x float>], align 4
groupshared float4 SharedArr[64];

// The array may also reach the builtin through a function parameter.
void LoadIndirect(groupshared float4 Arr[64]) {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(9, 5, 4, 1, 2)]] mat;
  __builtin_LinAlg_MatrixLoadFromMemory(mat, Arr, 128, 16, 3);
}

[numthreads(4,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call %dx.types.LinAlgMatrixC9M5N4U1S2 @dx.op.linAlgMatrixLoadFromMemory.mC9M5N4U1S2.v4f32
  // CHECK-SAME: (i32 -2147483633, <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>],
  // CHECK-SAME: [64 x <4 x float>] addrspace(3)* @"\01?SharedArr@@3PAV?$vector@M$03@@A", i32 0, i32 0),
  // CHECK-SAME: i32 128, i32 16, i32 3)
  // CHECK-SAME: ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  // CHECK2: call void @"dx.hl.op..void (i32, %dx.types.LinAlgMatrixC9M5N4U1S2*, [64 x <4 x float>] addrspace(3)*,
  // CHECK2-SAME: i32, i32, i32)"(i32 407, %dx.types.LinAlgMatrixC9M5N4U1S2* %mat, [64 x <4 x float>] addrspace(3)*
  // CHECK2-SAME: @"\01?SharedArr@@3PAV?$vector@M$03@@A", i32 128, i32 16, i32 3)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(9, 5, 4, 1, 2)]] mat;
  __builtin_LinAlg_MatrixLoadFromMemory(mat, SharedArr, 128, 16, 3);

  // CHECK: call %dx.types.LinAlgMatrixC9M5N4U1S2 @dx.op.linAlgMatrixLoadFromMemory.mC9M5N4U1S2.v4f32
  // CHECK-SAME: (i32 -2147483633, <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>],
  // CHECK-SAME: [64 x <4 x float>] addrspace(3)* @"\01?SharedArr@@3PAV?$vector@M$03@@A", i32 0, i32 0),
  // CHECK-SAME: i32 128, i32 16, i32 3)
  // CHECK-SAME: ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)
  LoadIndirect(SharedArr);
}
