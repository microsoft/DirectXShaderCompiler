// RUN: %dxc -E main -T lib_6_3 %s | FileCheck %s
// CHECK:Initialize static global g with external function is not supported

float foo();

static float g = foo();

export float bar() {
  return g;
}