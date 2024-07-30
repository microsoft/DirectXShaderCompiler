// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// CHECK: call void (i32, [20 x i8]*, i64, i64, i64, ...) @"dx.op.debugPrintf

float4 main(float4 p: Position) : SV_Position {

  printf("numbers: %d, %d, %d", 1, 2, 3);
  return 0.0.xxxx;
}
