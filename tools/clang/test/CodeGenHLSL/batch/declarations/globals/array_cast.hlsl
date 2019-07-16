// RUN: %dxc -E main -T ps_6_0 > %s | FileCheck %s

// Make sure cast to nest struct works.

// Test that no variable initializers are emitted, especially for cbuffers globals.
// CHECK-NOT: {{.*}} = constant

// Check the offset calculate.
// CHECK: add {{.+}}, 2


float4 cb[4*4];

struct N {
  float4 b;
  float4 c;
};

struct A {
  float4 a;
  N      n;
  float4 d;
};

static const A a[4] = cb;


float4 main(int i:I) : SV_Target {
  return a[i].n.c + cb[i];
}