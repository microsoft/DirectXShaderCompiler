// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

RWStructuredBuffer<int> input;
RWStructuredBuffer<int> output;

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()
  // CHECK: call %dx.types.MatrixRef @dx.op.createMatrix(i32 -2147483637)  ; CreateMatrix()
  // CHECK: call void @dx.op.fillMatrix.i32(i32 -2147483636, %dx.types.MatrixRef %{{.*}}, i32 {{.*}})  ; FillMatrix(matrix,value)
  // CHECK: call void @dx.op.fillMatrix.f32(i32 -2147483636, %dx.types.MatrixRef %{{.*}}, float {{.*}})  ; FillMatrix(matrix,value)
  // CHECK: call i32 @dx.op.matrixLength(i32 -2147483632, %dx.types.MatrixRef %{{.*}})  ; MatrixLength(matrix)

  __builtin_LinAlg_MatrixRef mat = __builtin_LinAlg_CreateMatrix();
  __builtin_LinAlg_FillMatrix(mat, 5);
  __builtin_LinAlg_FillMatrix(mat, 3.14);
  output[0] = __builtin_LinAlg_MatrixLength(mat);
}
