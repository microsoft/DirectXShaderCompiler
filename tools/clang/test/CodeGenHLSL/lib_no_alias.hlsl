// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure out param has no-alias.
// CHECK: void @"\01?test@@YAMMUT@@AIAV?$matrix@M$01$01@@M@Z"(float, float* noalias nocapture, i32* noalias nocapture, [4 x float]* noalias nocapture dereferenceable(16), float, float* noalias nocapture)

struct T {
  float a;
  int b;
};

float test(float a, out T t, out float2x2 m, float b) {
  t.a = 1;
  t.b = 6;
  m = 3;
  return a + t.a - t.b - b;
}