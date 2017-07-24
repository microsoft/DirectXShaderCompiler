// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure no undef in test3.
// CHECK: define void
// CHECK-NOT: undef
// CHECK: ret void

struct T {
  float2 v;
};

cbuffer M {
  float2 m;
}

float test(T t);

float4 test3(){
  float2 x = m + 2;
  T t = { x };
  float a = test(t);
  t.v.x += 2;
  return a + test(t);
}