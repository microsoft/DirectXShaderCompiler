// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -HV 202x -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -HV 202x -E main -fcgl %s | FileCheck %s --check-prefix=CHECK2

// The groupshared array may hold vectors of the matrix component type. The
// DXIL operation is overloaded on the array's element type, so the memory
// operand is a pointer to a vector.

// CHECK: @"\01?SharedArr@@3PAV?$vector@M$01@@A" = external addrspace(3) global [64 x <2 x float>], align 4
groupshared float2 SharedArr[64];

[numthreads(4,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call void @dx.op.linAlgMatrixAccumulateToMemory.mC4M5N4U1S2.v2f32(i32 -2147483620,
  // CHECK-SAME: %dx.types.LinAlgMatrixC4M5N4U1S2 %{{.*}}, <2 x float> addrspace(3)* getelementptr inbounds ([64 x <2 x float>],
  // CHECK-SAME: [64 x <2 x float>] addrspace(3)* @"\01?SharedArr@@3PAV?$vector@M$01@@A", i32 0, i32 0),
  // CHECK-SAME: i32 9, i32 1, i32 2, i32 3)
  // CHECK-SAME: ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  // CHECK2: call void @"dx.hl.op..void (i32, %dx.types.LinAlgMatrixC4M5N4U1S2,
  // CHECK2-SAME: [64 x <2 x float>] addrspace(3)*, i32, i32, i32, i32)"(i32 416,
  // CHECK2-SAME: %dx.types.LinAlgMatrixC4M5N4U1S2 %{{.*}}, [64 x <2 x float>] addrspace(3)*
  // CHECK2-SAME: @"\01?SharedArr@@3PAV?$vector@M$01@@A", i32 9, i32 1, i32 2, i32 3)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  __builtin_LinAlg_FillMatrix(mat, 1);
  __builtin_LinAlg_MatrixAccumulateToMemory(mat, SharedArr, 9, 1, 2, 3);
}
