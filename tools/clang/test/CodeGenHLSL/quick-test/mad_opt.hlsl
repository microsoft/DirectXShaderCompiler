// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s


// Make sure a is not used.
// CHECK-NOT: dx.op.loadInput.f32(i32 4, i32 0

float main(float a : A, float b :B) : SV_Target {
  return mad(a, 0, b);
}