// RUN: %dxc -T lib_6_3 -auto-binding-space 11 -default-linkage external %s | FileCheck %s

// Make sure no undef in test3.
// CHECK: define <4 x float>
// CHECK: insertelement <2 x float> undef
// CHECK: insertelement <4 x float> undef
// CHECK-NOT: undef
// CHECK: ret <4 x float>

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