// RUN: %dxc -T ps_6_0 -E main %s | FileCheck %s

// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 3.000000e+00)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 3.000000e+00)

float fn(min16int i) { return 3; }
float fn(int i) { return 5; }

float2 main(bool2 a: A, min16int2 b : B) : SV_Target {
  return float2(
    fn(b.x + a.x),    // should pick min16int overload
    fn(a.y + b.y));   // should pick min16int overload
}