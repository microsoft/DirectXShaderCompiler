// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure extern function don't need to flat is called.
// CHECK: call void @"\01?test@@YAXMAIAM@Z"

void test(float a, out float b);

float test2(float a) {
  float b;
  test(a, b);
  return b;
}