// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

RWStructuredBuffer<int> input;
RWStructuredBuffer<int> output;

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()
  // CHECK: call %dx.types.MatrixRef @dx.op.createMatrix(i32 -2147483637)  ; CreateMatrix()
  // CHECK: call i32 @dx.op.matrixLength(i32 -2147483632, %dx.types.MatrixRef %{{.*}})  ; MatrixLength(matrix)

  __builtin_LinAlg_MatrixRef mat = __builtin_LinAlg_CreateMatrix();
  output[0] = __builtin_LinAlg_MatrixLength(mat);
}
