// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure subscript on inout vector works.
// CHECK: noalias

struct A {
  float2 s;
  int2  i;
};

float test(inout A a) {
  return a.s[1];
}