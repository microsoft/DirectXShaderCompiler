// RUN: %dxc -E main -T ps_6_0 -HV 202x -O2 %s | FileCheck %s -check-prefix=CHECK
// RUN: %dxc -E main -T ps_6_0 -HV 202x -O3 %s | FileCheck %s -check-prefix=CHECK

// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK-NOT: call float @dx.op.dot3
// CHECK-NOT: br i1
// CHECK-NOT: !llvm.loop

float main(float3 a : A, float3 b : B) : SV_Target {
  float result = 0;
  [unroll]
  for (int i = 0; i < 10; i++) {
    result += dot(a * i, b);
  }
  return result;
}
